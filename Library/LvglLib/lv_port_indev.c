/**
 * @file lv_port_indev.c
 *
 * UEFI input device port using LVGL's built-in UEFI indev drivers.
 *
 * Pointer: EFI_ABSOLUTE_POINTER_PROTOCOL via lv_uefi_absolute_pointer_indev.
 * Keyboard: EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL via lv_uefi_simple_text_input_indev.
 *
 * The pointer indev is created empty at init; the ConSplitter aggregate handle
 * (gST->ConsoleInHandle) is added immediately if USB is already bound, and
 * via RegisterProtocolNotify otherwise (PR#17 lazy-binding pattern).
 *
 * NOTE: Mouse wheel scrolling and F1-F12 hotkeys are not yet delivered by the
 * built-in indevs and will be re-ported in a follow-up branch.
 */


/*********************
 *      INCLUDES
 *********************/
#include "lv_port_indev.h"

// Required for lv_uefi_keypad_drain() to poke private keypad state
#include "lvgl/src/indev/lv_indev_private.h"

/*********************
 *      DEFINES
 *********************/

extern const lv_img_dsc_t mouse_cursor_icon;

/**********************
 *  STATIC VARIABLES
 **********************/

static lv_indev_t *indev_mouse  = NULL;
static lv_indev_t *indev_keypad = NULL;

//
// PR#17 lazy-binding state. The pointer indev is created empty; handles are
// added via RegisterProtocolNotify when EFI_ABSOLUTE_POINTER_PROTOCOL appears
// (i.e. when USB binds during BDS ConnectAll, after which ConSplitterDxe
// updates its aggregate and its Mode becomes valid).
//
STATIC EFI_EVENT    mPointerNotifyEvent        = NULL;
STATIC VOID        *mPointerNotifyRegistration = NULL;
STATIC BOOLEAN      mPointerAdded              = FALSE;

/**********************
 *   STATIC FUNCTIONS
 **********************/

//
// Protocol-install notification: fires each time any driver installs
// EFI_ABSOLUTE_POINTER_PROTOCOL (e.g. UsbMouseAbsolutePointerDxe during BDS).
// Keep retrying add_handle on the ConSplitter aggregate until it succeeds —
// it returns FALSE while the aggregate Mode is still zero-range.
//
STATIC
VOID
EFIAPI
OnPointerProtocolInstalled (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  if (!mPointerAdded && indev_mouse != NULL) {
    if (lv_uefi_absolute_pointer_indev_add_handle (indev_mouse, gST->ConsoleInHandle)) {
      mPointerAdded = TRUE;
    }
  }
}

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init (lv_display_t *disp)
{
  lv_point_t res;
  EFI_STATUS Status;

  res.x = (lv_coord_t)lv_display_get_horizontal_resolution (disp);
  res.y = (lv_coord_t)lv_display_get_vertical_resolution (disp);

  //
  // Keyboard: ConSplitterDxe installs EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL on
  // gST->ConsoleInHandle early in DXE — add it immediately.
  //
  indev_keypad = lv_uefi_simple_text_input_indev_create ();
  if (indev_keypad != NULL) {
    lv_uefi_simple_text_input_indev_add_handle (indev_keypad, gST->ConsoleInHandle);
  }

  //
  // Pointer: create an empty indev sized to the display resolution, wire it
  // to the display, and attach the software cursor image.
  //
  indev_mouse = lv_uefi_absolute_pointer_indev_create (&res);
  if (indev_mouse != NULL) {
    lv_indev_set_display (indev_mouse, disp);

    LV_IMG_DECLARE(mouse_cursor_icon);
    lv_obj_t *cursor_obj = lv_image_create (lv_screen_active ());
    lv_image_set_src (cursor_obj, &mouse_cursor_icon);
    lv_indev_set_cursor (indev_mouse, cursor_obj);

    // Try to add the ConSplitter handle now; USB may already be bound.
    if (lv_uefi_absolute_pointer_indev_add_handle (indev_mouse, gST->ConsoleInHandle)) {
      mPointerAdded = TRUE;
    }
  }

  //
  // Arm a protocol-install notification so OnPointerProtocolInstalled retries
  // add_handle once USB binds during BDS (the PR#17 lazy-binding pattern).
  //
  if (mPointerNotifyEvent == NULL) {
    Status = gBS->CreateEvent (
                   EVT_NOTIFY_SIGNAL,
                   TPL_CALLBACK,
                   OnPointerProtocolInstalled,
                   NULL,
                   &mPointerNotifyEvent);
    if (!EFI_ERROR (Status)) {
      gBS->RegisterProtocolNotify (
             &gEfiAbsolutePointerProtocolGuid,
             mPointerNotifyEvent,
             &mPointerNotifyRegistration);
    }
  }
}

void lv_uefi_keypad_drain (void)
{
  EFI_STATUS                         Status;
  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *TxtInEx;
  EFI_KEY_DATA                       KeyData;
  lv_indev_t                        *Indev;

  //
  // Drain all pending keystrokes from the EFI console so none leak into the
  // next LVGL event loop iteration.
  //
  Status = gBS->HandleProtocol (
                  gST->ConsoleInHandle,
                  &gEfiSimpleTextInputExProtocolGuid,
                  (VOID **)&TxtInEx);
  if (!EFI_ERROR (Status)) {
    while (!EFI_ERROR (TxtInEx->ReadKeyStrokeEx (TxtInEx, &KeyData))) {
    }
  }

  //
  // Force every keypad indev to RELEASED so LVGL does not interpret a
  // pending ENTER release as a click on the next focused widget.
  //
  Indev = NULL;
  while ((Indev = lv_indev_get_next (Indev)) != NULL) {
    if (lv_indev_get_type (Indev) == LV_INDEV_TYPE_KEYPAD) {
      Indev->keypad.last_state = LV_INDEV_STATE_RELEASED;
      Indev->keypad.last_key   = 0;
    }
  }
}

void lv_port_indev_close (void)
{
  if (mPointerNotifyEvent != NULL) {
    gBS->CloseEvent (mPointerNotifyEvent);
    mPointerNotifyEvent        = NULL;
    mPointerNotifyRegistration = NULL;
  }

  mPointerAdded = FALSE;
  indev_mouse   = NULL;
  indev_keypad  = NULL;
}
