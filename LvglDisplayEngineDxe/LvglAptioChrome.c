/** @file
  AMI Aptio-style chrome implementation.

  Copyright (c) 2024-2026, Hamit Karaca. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "LvglAptioChrome.h"
#include <LvglTheme.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>

STATIC lv_obj_t   *mClockLabel = NULL;
STATIC lv_timer_t *mClockTimer = NULL;

/**
  UCS-2 → UTF-8 (caller frees). Local copy to avoid cross-file linkage.
**/
STATIC
CHAR8 *
ChromeUcs2ToUtf8 (
  IN CONST CHAR16  *Str16
  )
{
  UINTN  Len;
  UINTN  Idx;
  UINTN  Out;
  CHAR8  *Utf8;

  if (Str16 == NULL) {
    return NULL;
  }

  for (Len = 0; Str16[Len] != 0; Len++) {
  }

  Utf8 = AllocatePool (Len * 3 + 1);
  if (Utf8 == NULL) {
    return NULL;
  }

  Out = 0;
  for (Idx = 0; Idx < Len; Idx++) {
    UINT16  Ch = Str16[Idx];

    if (Ch < 0x80) {
      Utf8[Out++] = (CHAR8)Ch;
    } else if (Ch < 0x800) {
      Utf8[Out++] = (CHAR8)(0xC0 | (Ch >> 6));
      Utf8[Out++] = (CHAR8)(0x80 | (Ch & 0x3F));
    } else {
      Utf8[Out++] = (CHAR8)(0xE0 | (Ch >> 12));
      Utf8[Out++] = (CHAR8)(0x80 | ((Ch >> 6) & 0x3F));
      Utf8[Out++] = (CHAR8)(0x80 | (Ch & 0x3F));
    }
  }
  Utf8[Out] = '\0';
  return Utf8;
}

/**
  Refresh clock label with current EFI runtime time.
**/
STATIC
VOID
ClockTimerCb (
  lv_timer_t  *Timer
  )
{
  EFI_TIME    Time;
  EFI_STATUS  Status;
  CHAR8       Buf[32];

  if ((mClockLabel == NULL) || (gRT == NULL)) {
    return;
  }

  Status = gRT->GetTime (&Time, NULL);
  if (EFI_ERROR (Status)) {
    lv_label_set_text (mClockLabel, "--:--:--");
    return;
  }

  AsciiSPrint (
    Buf,
    sizeof (Buf),
    "%02d:%02d:%02d   %04d-%02d-%02d",
    Time.Hour, Time.Minute, Time.Second,
    Time.Year, Time.Month, Time.Day
    );
  lv_label_set_text (mClockLabel, Buf);
}

/**
  Build the gradient header bar with title (left), vendor (center), clock (right).
**/
STATIC
VOID
BuildHeader (
  IN lv_obj_t  *Screen
  )
{
  lv_obj_t  *Bar;
  lv_obj_t  *Title;
  lv_obj_t  *Vendor;

  Bar = lv_obj_create (Screen);
  lv_obj_remove_style_all (Bar);
  lv_obj_set_size (Bar, LV_PCT (100), THEME_HEADER_HEIGHT);
  lv_obj_set_style_bg_color (Bar, lv_color_hex (THEME_COLOR_HEADER_BG_TOP), 0);
  lv_obj_set_style_bg_grad_color (Bar, lv_color_hex (THEME_COLOR_HEADER_BG_BOTTOM), 0);
  lv_obj_set_style_bg_grad_dir (Bar, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa (Bar, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_left (Bar, 16, 0);
  lv_obj_set_style_pad_right (Bar, 16, 0);
  lv_obj_set_style_pad_top (Bar, 0, 0);
  lv_obj_set_style_pad_bottom (Bar, 0, 0);
  lv_obj_clear_flag (Bar, LV_OBJ_FLAG_SCROLLABLE);

  // Left: brand title
  Title = lv_label_create (Bar);
  lv_label_set_text (Title, APTIO_HEADER_TITLE);
  lv_obj_set_style_text_font (Title, THEME_FONT_TITLE, 0);
  lv_obj_set_style_text_color (Title, lv_color_hex (THEME_COLOR_HEADER_TEXT), 0);
  lv_obj_align (Title, LV_ALIGN_LEFT_MID, 0, 0);

  // Center: vendor / version (dimmed)
  Vendor = lv_label_create (Bar);
  lv_label_set_text (Vendor, APTIO_HEADER_VENDOR);
  lv_obj_set_style_text_font (Vendor, THEME_FONT_BODY, 0);
  lv_obj_set_style_text_color (Vendor, lv_color_hex (THEME_COLOR_HEADER_DIM), 0);
  lv_obj_align (Vendor, LV_ALIGN_CENTER, 0, 0);

  // Right: clock
  mClockLabel = lv_label_create (Bar);
  lv_obj_set_style_text_font (mClockLabel, THEME_FONT_BODY, 0);
  lv_obj_set_style_text_color (mClockLabel, lv_color_hex (THEME_COLOR_HEADER_TEXT), 0);
  lv_obj_align (mClockLabel, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_label_set_text (mClockLabel, "");
  ClockTimerCb (NULL);
  mClockTimer = lv_timer_create (ClockTimerCb, 1000, NULL);
}

/**
  Build the subtitle bar showing the current form's title.
**/
STATIC
VOID
BuildSubtitleBar (
  IN lv_obj_t                    *Screen,
  IN FORM_DISPLAY_ENGINE_FORM    *FormData
  )
{
  lv_obj_t  *Bar;
  lv_obj_t  *Label;
  CHAR16    *Str16;
  CHAR8     *Utf8;

  Bar = lv_obj_create (Screen);
  lv_obj_remove_style_all (Bar);
  lv_obj_set_size (Bar, LV_PCT (100), THEME_SUBTITLE_HEIGHT);
  lv_obj_set_style_bg_color (Bar, lv_color_hex (THEME_COLOR_SUBTITLE_BG), 0);
  lv_obj_set_style_bg_opa (Bar, LV_OPA_80, 0);
  lv_obj_set_style_border_color (Bar, lv_color_hex (THEME_COLOR_HIGHLIGHT_ROW), 0);
  lv_obj_set_style_border_width (Bar, 0, 0);
  lv_obj_set_style_border_side (Bar, LV_BORDER_SIDE_LEFT, 0);
  lv_obj_set_style_pad_left (Bar, 16, 0);
  lv_obj_set_style_pad_right (Bar, 16, 0);
  lv_obj_set_style_pad_top (Bar, 0, 0);
  lv_obj_set_style_pad_bottom (Bar, 0, 0);
  lv_obj_clear_flag (Bar, LV_OBJ_FLAG_SCROLLABLE);

  Str16 = NULL;
  Utf8  = NULL;
  if ((FormData != NULL) && (FormData->FormTitle != 0)) {
    Str16 = HiiGetString (FormData->HiiHandle, FormData->FormTitle, NULL);
  }
  if (Str16 != NULL) {
    Utf8 = ChromeUcs2ToUtf8 (Str16);
    FreePool (Str16);
  }

  Label = lv_label_create (Bar);
  lv_label_set_text (Label, Utf8 != NULL ? Utf8 : "Setup");
  lv_obj_set_style_text_font (Label, THEME_FONT_BODY, 0);
  lv_obj_set_style_text_color (Label, lv_color_hex (THEME_COLOR_SUBTITLE_TEXT), 0);
  lv_obj_align (Label, LV_ALIGN_LEFT_MID, 0, 0);

  if (Utf8 != NULL) {
    FreePool (Utf8);
  }
}

/**
  Build the footer hint bar.
**/
STATIC
VOID
BuildFooter (
  IN lv_obj_t                    *Screen,
  IN FORM_DISPLAY_ENGINE_FORM    *FormData
  )
{
  lv_obj_t  *Bar;
  lv_obj_t  *Hints;
  lv_obj_t  *SysInfo;

  (VOID)FormData;  // hotkey list reserved for future use

  Bar = lv_obj_create (Screen);
  lv_obj_remove_style_all (Bar);
  lv_obj_set_size (Bar, LV_PCT (100), THEME_FOOTER_HEIGHT);
  lv_obj_set_style_bg_color (Bar, lv_color_hex (THEME_COLOR_FOOTER_BG), 0);
  lv_obj_set_style_bg_opa (Bar, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_left (Bar, 16, 0);
  lv_obj_set_style_pad_right (Bar, 16, 0);
  lv_obj_set_style_pad_top (Bar, 0, 0);
  lv_obj_set_style_pad_bottom (Bar, 0, 0);
  lv_obj_clear_flag (Bar, LV_OBJ_FLAG_SCROLLABLE);

  Hints = lv_label_create (Bar);
  lv_label_set_text (Hints, APTIO_FOOTER_NAV_HINTS);
  lv_obj_set_style_text_font (Hints, THEME_FONT_BODY, 0);
  lv_obj_set_style_text_color (Hints, lv_color_hex (THEME_COLOR_FOOTER_TEXT), 0);
  lv_obj_align (Hints, LV_ALIGN_LEFT_MID, 0, 0);

  SysInfo = lv_label_create (Bar);
  lv_label_set_text (SysInfo, APTIO_FOOTER_SYSINFO);
  lv_obj_set_style_text_font (SysInfo, THEME_FONT_BODY, 0);
  lv_obj_set_style_text_color (SysInfo, lv_color_hex (THEME_COLOR_FOOTER_DIM), 0);
  lv_obj_align (SysInfo, LV_ALIGN_RIGHT_MID, 0, 0);
}

lv_obj_t *
EFIAPI
AptioBuildChrome (
  IN lv_obj_t                    *Screen,
  IN FORM_DISPLAY_ENGINE_FORM    *FormData
  )
{
  lv_obj_t  *Content;

  // Screen is a vertical flex column with an Aptio-style gradient
  // background (dark navy top → mid navy bottom). The compiled-in
  // RGB565 wallpaper image is bypassed — LVGL's decoder doesn't render
  // it cleanly with LV_COLOR_DEPTH=32 and the asset is also smaller
  // than a typical GOP framebuffer (800x600 vs 1280x800), so it
  // wouldn't tile/scale correctly anyway.
  lv_obj_remove_style_all (Screen);
  lv_obj_set_style_bg_color (Screen, lv_color_hex (THEME_COLOR_HEADER_BG_TOP), 0);
  lv_obj_set_style_bg_grad_color (Screen, lv_color_hex (0x14264A), 0);
  lv_obj_set_style_bg_grad_dir (Screen, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa (Screen, LV_OPA_COVER, 0);
  lv_obj_set_flex_flow (Screen, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all (Screen, 0, 0);
  lv_obj_set_style_pad_row (Screen, 0, 0);
  lv_obj_clear_flag (Screen, LV_OBJ_FLAG_SCROLLABLE);

  // Header / subtitle / content / footer in flex order.
  BuildHeader (Screen);
  BuildSubtitleBar (Screen, FormData);

  // No frame — match Demo/1.png: rows float over the wallpaper with
  // side margins. Content panel is just a transparent flex column.
  Content = lv_obj_create (Screen);
  lv_obj_remove_style_all (Content);
  lv_obj_set_width (Content, LV_PCT (100));
  lv_obj_set_flex_grow (Content, 1);
  lv_obj_set_flex_flow (Content, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_left (Content, 24, 0);
  lv_obj_set_style_pad_right (Content, 24, 0);
  lv_obj_set_style_pad_top (Content, 12, 0);
  lv_obj_set_style_pad_bottom (Content, 12, 0);
  lv_obj_set_style_pad_row (Content, 6, 0);
  lv_obj_set_style_bg_opa (Content, LV_OPA_TRANSP, 0);
  lv_obj_add_flag (Content, LV_OBJ_FLAG_SCROLLABLE);
  // No bounce-past-edge: content stops cleanly at the boundary.
  lv_obj_clear_flag (Content, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_clear_flag (Content, LV_OBJ_FLAG_SCROLL_MOMENTUM);

  BuildFooter (Screen, FormData);

  return Content;
}

VOID
EFIAPI
AptioChromeTeardown (
  VOID
  )
{
  if (mClockTimer != NULL) {
    lv_timer_delete (mClockTimer);
    mClockTimer = NULL;
  }
  mClockLabel = NULL;
}
