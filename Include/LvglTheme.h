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

#endif // LVGL_THEME_H_
