/** @file
  Shared definitions for the LVGL UI configuration (software UI scaling).

  The scale value is persisted in a non-volatile UEFI variable so the LVGL
  display engine can size its logical canvas before the first form is rendered.
  A dedicated setup driver (LvglSetupDxe) presents the control; the LVGL library
  (LvglLib) consumes the value when it creates the display.

  Copyright (c) 2026, MrChromebox. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef LVGL_UI_CONFIG_H_
#define LVGL_UI_CONFIG_H_

//
// {DDB969F2-E99B-4DD5-8E36-B80B526D3B73}
//
#define LVGL_UI_CONFIG_FORMSET_GUID \
  { 0xddb969f2, 0xe99b, 0x4dd5, { 0x8e, 0x36, 0xb8, 0x0b, 0x52, 0x6d, 0x3b, 0x73 } }

//
// Non-volatile variable that stores the UI scale selection. Both the setup
// driver's efivarstore and the LVGL library reference this exact name/GUID.
//
#define LVGL_UI_CONFIG_VAR_NAME  L"LvglUiScale"

//
// UI scale factors. Values are stored as-is in the variable's UiScale field.
//   1x   : native resolution, no scaling (default, zero overhead)
//   1.5x : logical canvas = physical / 1.5, upscaled on flush
//   2x   : logical canvas = physical / 2,   upscaled on flush
//
#define LVGL_UI_SCALE_1X       0
#define LVGL_UI_SCALE_1_5X     1
#define LVGL_UI_SCALE_2X       2
#define LVGL_UI_SCALE_DEFAULT  LVGL_UI_SCALE_1X

#pragma pack(1)
typedef struct {
  UINT8    UiScale;
} LVGL_UI_CONFIG_VARSTORE_DATA;
#pragma pack()

extern EFI_GUID  gLvglUiConfigFormSetGuid;

#endif // LVGL_UI_CONFIG_H_
