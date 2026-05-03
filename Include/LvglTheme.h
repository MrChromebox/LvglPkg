/** @file
  LvglTheme.h - User-customizable theme for LvglDisplayEngineDxe.

  Edit the macros in this file and rebuild to restyle the LVGL HII form
  renderer. All colors, fonts, padding, and radius values used by
  LvglFormRenderer.c flow through these macros.

  Fonts referenced here must be enabled in LvglPkg/lv_conf.h
  (LV_FONT_MONTSERRAT_*).
**/

#ifndef LVGL_THEME_H_
#define LVGL_THEME_H_

#include <lvgl.h>

//
// Background colors (24-bit RGB; passed to lv_color_hex())
//
#define THEME_COLOR_BG_SCREEN     0x1A1A2E
#define THEME_COLOR_BG_PANEL      0x16213E
#define THEME_COLOR_BG_DIALOG     0x2A2A4A
#define THEME_COLOR_BG_SEPARATOR  0x555580

//
// Text colors
//
#define THEME_COLOR_TEXT_TITLE    0xFFFFFF
#define THEME_COLOR_TEXT_LABEL    0xCCCCCC
#define THEME_COLOR_TEXT_POPUP    0xCCCCCC

//
// Accent (subtitle / focus highlight) — LV_PALETTE_* enum value
//
#define THEME_ACCENT_PALETTE      LV_PALETTE_BLUE

//
// Fonts — must be enabled in lv_conf.h
//
#define THEME_FONT_TITLE          (&lv_font_montserrat_20)
#define THEME_FONT_BODY           (&lv_font_montserrat_16)
#define THEME_FONT_POPUP          (&lv_font_montserrat_16)

//
// Spacing & shape (pixels)
//
#define THEME_PAD_SCREEN          16
#define THEME_PAD_PANEL           8
#define THEME_PAD_ROW             4
#define THEME_PAD_ROW_TIGHT       2
#define THEME_PAD_DIALOG          16
#define THEME_PAD_DIALOG_ROW_GAP  10
#define THEME_PAD_DIALOG_COL_GAP  8
#define THEME_PAD_SCREEN_ROW_GAP  6
#define THEME_PAD_PANEL_ROW_GAP   4
#define THEME_PAD_LABEL_TOP       8
#define THEME_RADIUS              8
#define THEME_OVERLAY_OPA         LV_OPA_50

//
// Aptio-style chrome (header / subtitle / footer / wallpaper).
// Used by LvglDisplayEngineDxe/LvglAptioChrome.c.
//
#define THEME_COLOR_HEADER_BG_TOP     0x0A1428
#define THEME_COLOR_HEADER_BG_BOTTOM  0x1F3A6E
#define THEME_COLOR_HEADER_TEXT       0xFFFFFF
#define THEME_COLOR_HEADER_DIM        0xA0B8D8
#define THEME_COLOR_SUBTITLE_BG       0x16213E
#define THEME_COLOR_SUBTITLE_TEXT     0xCFE3FF
#define THEME_COLOR_FOOTER_BG         0x0E1A33
#define THEME_COLOR_FOOTER_TEXT       0xCFE3FF
#define THEME_COLOR_FOOTER_DIM        0x7A8FB5
#define THEME_COLOR_HIGHLIGHT_ROW     0x1E66D4

#define THEME_HEADER_HEIGHT       44
#define THEME_SUBTITLE_HEIGHT     28
#define THEME_FOOTER_HEIGHT       32

//
// Static strings shown in the chrome (no SMBIOS lookup yet).
//
#define APTIO_HEADER_TITLE        "EDK II Boot Manager"
#define APTIO_HEADER_VENDOR       "UEFI v2.90  |  EDK II"
#define APTIO_FOOTER_NAV_HINTS    LV_SYMBOL_UP LV_SYMBOL_DOWN " Select   Enter Confirm   Esc Exit   F9 Defaults   F10 Save"
#define APTIO_FOOTER_SYSINFO      "QEMU x86_64  |  OVMF"

#endif // LVGL_THEME_H_
