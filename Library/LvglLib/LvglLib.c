
#include "LvglLibCommon.h"

#include <Library/LvglLib.h>

extern UINT8  mExitBtnYes;

BOOLEAN  mTickSupport = FALSE;
STATIC BOOLEAN  mUefiLvglInitDone = FALSE;

#if LV_USE_LOG
static void efi_lv_log_print(lv_log_level_t level, const char * buf)
{
    static const int priority[LV_LOG_LEVEL_NUM] = {
        DEBUG_VERBOSE|DEBUG_INFO|DEBUG_WARN|DEBUG_ERROR, DEBUG_INFO, DEBUG_WARN, DEBUG_ERROR, DEBUG_INFO
    };

    DebugPrint (priority[level], "[LVGL] %a\n", buf);
}
#endif


static uint32_t tick_get_cb(void)
{
  return (UINT32) DivU64x32 (GetTimeInNanoSecond (GetPerformanceCounter()), 1000 * 1000);
}

VOID
EFIAPI
UefiLvglTickInit (
  VOID
  )
{
  if (GetPerformanceCounter()) {
    mTickSupport = TRUE;
    lv_tick_set_cb(tick_get_cb);
  }
}


EFI_STATUS
EFIAPI
UefiLvglInit (
  VOID
  )
{
  EFI_HANDLE                         GopHandle;
  lv_display_t                       *Display;

  if (mUefiLvglInitDone) {
    return EFI_SUCCESS;
  }

  // lv_uefi_init must be called before lv_init() so the LVGL UEFI backend
  // (lv_uefi_platform_init, invoked from lv_init) finds valid EFI globals.
  lv_uefi_init (gImageHandle, gST);

  lv_init();

#if 0
  // Need real TimerLib
  UefiLvglTickInit();
#endif

#if LV_USE_LOG
  lv_log_register_print_cb (efi_lv_log_print);
#endif

  // Use LVGL's built-in UEFI display driver. lv_uefi_display_get_any()
  // returns the first handle with EFI_GRAPHICS_OUTPUT_PROTOCOL installed.
  GopHandle = lv_uefi_display_get_any ();
  if (GopHandle == NULL) {
    lv_deinit ();
    return EFI_UNSUPPORTED;
  }

  Display = lv_uefi_display_create (GopHandle);
  if (Display == NULL) {
    lv_deinit ();
    return EFI_UNSUPPORTED;
  }

  lv_port_indev_init(Display);

  mUefiLvglInitDone = TRUE;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
UefiLvglDeinit (
  VOID
  )
{

  if (!mUefiLvglInitDone) {
    return EFI_SUCCESS;
  }

  LvglUefiEscExitUnregister ();

  lv_deinit();

  lv_port_indev_close();

  gST->ConOut->ClearScreen (gST->ConOut);
  gST->ConOut->SetCursorPosition (gST->ConOut, 0, 0);
  gST->ConOut->EnableCursor (gST->ConOut, TRUE);

  mUefiLvglInitDone = FALSE;

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
UefiLvglAppRegister (
  IN EFI_LVGL_APP_FUNCTION AppRegister
  )
{
  if (!mUefiLvglInitDone) {
    if (UefiLvglInit() != EFI_SUCCESS) {
      return EFI_UNSUPPORTED;
    }
  }

  if (AppRegister != NULL) {
    gST->ConOut->ClearScreen (gST->ConOut);
    gST->ConOut->EnableCursor (gST->ConOut, FALSE);

    // call user GUI APP
    AppRegister();

    LvglUefiEscExitRegister ();

    while (1) {
      if (mExitBtnYes == EXIT_BTN_YES) {
        break;
      }

      lv_timer_handler();

      gBS->Stall (10 * 1000);
      if (!mTickSupport) {
        lv_tick_inc(10);
      }
    }
  } else {
    UefiLvglDeinit();
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
LvglLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{

  UefiLvglInit ();

  return EFI_SUCCESS;
}


EFI_STATUS
EFIAPI
LvglLibDestructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{

  UefiLvglDeinit();

  return EFI_SUCCESS;
}
