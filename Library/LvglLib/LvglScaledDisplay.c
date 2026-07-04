/** @file
  Software UI-scaling display backend for LVGL on UEFI.

  LVGL and its UEFI display backend (lv_uefi_display.c) render at the physical
  framebuffer resolution, so on a high-resolution panel every widget/font ends
  up physically tiny. To provide a hi-DPI ("larger UI") experience without any
  knowledge of the true panel size, this backend renders LVGL into a *logical*
  canvas smaller than the physical framebuffer, then upscales each flushed
  region to the panel using nearest-neighbour sampling during the GOP Blt.

  A logical canvas of physical / S (S = 1.5 or 2) makes every element appear S
  times larger. The upscale is integer-per-pixel nearest-neighbour, which keeps
  the flush cheap (a memcpy-like Blt per destination row) and avoids any
  floating point. The pointer indev already rescales to the logical resolution
  (lv_display_get_horizontal_resolution), so no input changes are required.

  1x scaling is handled by the stock lv_uefi_display_create() path and never
  reaches this file, so the no-scale case carries zero overhead.

  Copyright (c) 2026, MrChromebox. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "LvglLibCommon.h"

//
// Per-display context for a scaled display. Freed on LV_EVENT_DELETE.
//
typedef struct {
  EFI_GRAPHICS_OUTPUT_PROTOCOL     *Gop;
  UINT32                           Num;        // scale numerator   (S = Num/Den)
  UINT32                           Den;        // scale denominator
  UINT32                           FbWidth;    // physical framebuffer width
  UINT32                           FbHeight;   // physical framebuffer height
  UINT32                           LogWidth;   // logical canvas width  (= Fb*Den/Num)
  UINT32                           LogHeight;  // logical canvas height
  VOID                             *LvBuf;     // logical draw buffer (LVGL DIRECT)
  UINTN                            LvBufSize;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL    *RowBuf;    // scratch row, FbWidth pixels
} LVGL_SCALED_DISPLAY_CTX;

STATIC
VOID
ScaledDisplayFree (
  LVGL_SCALED_DISPLAY_CTX  *Ctx
  )
{
  if (Ctx == NULL) {
    return;
  }

  if (Ctx->LvBuf != NULL) {
    lv_free (Ctx->LvBuf);
  }

  if (Ctx->RowBuf != NULL) {
    lv_free (Ctx->RowBuf);
  }

  lv_free (Ctx);
}

STATIC
VOID
ScaledDisplayEventCb (
  lv_event_t  *e
  )
{
  lv_display_t             *Display;
  LVGL_SCALED_DISPLAY_CTX  *Ctx;

  if (lv_event_get_code (e) != LV_EVENT_DELETE) {
    return;
  }

  Display = (lv_display_t *)lv_event_get_user_data (e);
  if (Display == NULL) {
    return;
  }

  Ctx = (LVGL_SCALED_DISPLAY_CTX *)lv_display_get_user_data (Display);
  lv_display_set_user_data (Display, NULL);
  ScaledDisplayFree (Ctx);
}

//
// Flush callback.
//
// In LV_DISPLAY_RENDER_MODE_DIRECT, px_map points at the base of the full
// logical canvas and 'area' is the dirty sub-rectangle in logical coordinates.
// Each logical pixel (sx,sy) maps to the destination block
// [sx*Num/Den .. (sx+1)*Num/Den) x [sy*Num/Den .. (sy+1)*Num/Den) in physical
// framebuffer coordinates; the inverse used here is src = dst*Den/Num. The
// destination span for the dirty rect is [x1*Num/Den .. (x2+1)*Num/Den), which
// is contiguous across adjacent areas (no gaps) and globally consistent (any
// overlap writes identical pixels), so the panel ends up as an exact
// nearest-neighbour upscale of the logical canvas.
//
STATIC
VOID
ScaledFlushCb (
  lv_display_t      *Display,
  const lv_area_t   *Area,
  uint8_t           *PxMap
  )
{
  LVGL_SCALED_DISPLAY_CTX        *Ctx;
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *Logical;
  UINT32                         Num;
  UINT32                         Den;
  UINT32                         Dx1;
  UINT32                         Dy1;
  UINT32                         Dx2;
  UINT32                         Dy2;
  UINT32                         Dw;
  UINT32                         Dx;
  UINT32                         Dy;

  Ctx = (LVGL_SCALED_DISPLAY_CTX *)lv_display_get_user_data (Display);
  if ((Ctx == NULL) || (Area->x2 < Area->x1) || (Area->y2 < Area->y1)) {
    lv_display_flush_ready (Display);
    return;
  }

  Logical = (EFI_GRAPHICS_OUTPUT_BLT_PIXEL *)PxMap;
  Num     = Ctx->Num;
  Den     = Ctx->Den;

  Dx1 = ((UINT32)Area->x1 * Num) / Den;
  Dy1 = ((UINT32)Area->y1 * Num) / Den;
  Dx2 = (((UINT32)Area->x2 + 1) * Num) / Den;
  Dy2 = (((UINT32)Area->y2 + 1) * Num) / Den;

  if (Dx2 > Ctx->FbWidth) {
    Dx2 = Ctx->FbWidth;
  }

  if (Dy2 > Ctx->FbHeight) {
    Dy2 = Ctx->FbHeight;
  }

  if ((Dx2 > Dx1) && (Dy2 > Dy1)) {
    Dw = Dx2 - Dx1;

    for (Dy = Dy1; Dy < Dy2; Dy++) {
      UINT32                         Sy;
      EFI_GRAPHICS_OUTPUT_BLT_PIXEL  *SrcRow;

      Sy     = (Dy * Den) / Num;
      SrcRow = Logical + (UINTN)Sy * Ctx->LogWidth;

      for (Dx = Dx1; Dx < Dx2; Dx++) {
        Ctx->RowBuf[Dx - Dx1] = SrcRow[(Dx * Den) / Num];
      }

      Ctx->Gop->Blt (
                  Ctx->Gop,
                  Ctx->RowBuf,
                  EfiBltBufferToVideo,
                  0,
                  0,
                  Dx1,
                  Dy,
                  Dw,
                  1,
                  0
                  );
    }
  }

  lv_display_flush_ready (Display);
}

/**
  Create an LVGL display that renders at a reduced logical resolution and
  upscales to the physical framebuffer on flush.

  @param[in]  Gop  Graphics Output Protocol for the target framebuffer.
  @param[in]  Num  Scale numerator   (S = Num/Den, e.g. 3/2 for 1.5x, 2/1 for 2x).
  @param[in]  Den  Scale denominator.

  @return  The created display, or NULL on failure.
**/
lv_display_t *
LvglCreateScaledDisplay (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL  *Gop,
  IN UINT32                        Num,
  IN UINT32                        Den
  )
{
  LVGL_SCALED_DISPLAY_CTX  *Ctx;
  lv_display_t             *Display;
  UINT32                   FbW;
  UINT32                   FbH;

  if ((Gop == NULL) || (Gop->Mode == NULL) || (Gop->Mode->Info == NULL) ||
      (Num == 0) || (Den == 0) || (Num <= Den))
  {
    return NULL;
  }

  FbW = Gop->Mode->Info->HorizontalResolution;
  FbH = Gop->Mode->Info->VerticalResolution;
  if ((FbW == 0) || (FbH == 0)) {
    return NULL;
  }

  Ctx = lv_calloc (1, sizeof (*Ctx));
  if (Ctx == NULL) {
    return NULL;
  }

  Ctx->Gop       = Gop;
  Ctx->Num       = Num;
  Ctx->Den       = Den;
  Ctx->FbWidth   = FbW;
  Ctx->FbHeight  = FbH;
  Ctx->LogWidth  = (UINT32)(((UINT64)FbW * Den) / Num);
  Ctx->LogHeight = (UINT32)(((UINT64)FbH * Den) / Num);

  if ((Ctx->LogWidth == 0) || (Ctx->LogHeight == 0)) {
    ScaledDisplayFree (Ctx);
    return NULL;
  }

  Ctx->LvBufSize = (UINTN)Ctx->LogWidth * Ctx->LogHeight * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL);
  Ctx->LvBuf     = lv_malloc (Ctx->LvBufSize);
  Ctx->RowBuf    = lv_malloc ((UINTN)FbW * sizeof (EFI_GRAPHICS_OUTPUT_BLT_PIXEL));
  if ((Ctx->LvBuf == NULL) || (Ctx->RowBuf == NULL)) {
    ScaledDisplayFree (Ctx);
    return NULL;
  }

  Display = lv_display_create (Ctx->LogWidth, Ctx->LogHeight);
  if (Display == NULL) {
    ScaledDisplayFree (Ctx);
    return NULL;
  }

  lv_display_add_event_cb (Display, ScaledDisplayEventCb, LV_EVENT_DELETE, Display);
  lv_display_set_flush_cb (Display, ScaledFlushCb);
  lv_display_set_buffers (
    Display,
    Ctx->LvBuf,
    NULL,
    (uint32_t)Ctx->LvBufSize,
    LV_DISPLAY_RENDER_MODE_DIRECT
    );
  lv_display_set_user_data (Display, Ctx);

  return Display;
}
