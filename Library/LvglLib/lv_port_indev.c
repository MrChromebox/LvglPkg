/**
 * @file lv_port_indev.c
 *
 * UEFI input device port using LVGL's built-in absolute-pointer indev for the
 * mouse and a custom keypad read callback for the keyboard.
 *
 * Why a custom keyboard callback instead of lv_uefi_simple_text_input_indev:
 * The built-in driver queues a PRESSED+RELEASED pair for every EFI keystroke
 * and delivers them in the same LVGL tick via continue_reading.  LVGL's
 * indev_keypad_proc never sees the key as held across ticks, so its
 * long_press_repeat throttle never engages.  EFI auto-repeat then fires at the
 * firmware's raw repeat rate (~33 Hz), causing runaway navigation.
 *
 * The custom keypad_read returns PRESSED while the EFI buffer has a key and
 * RELEASED when it is empty.  LVGL's long-press timer governs the repeat rate
 * while the user holds a key, and navigation stops once the buffer drains.
 *
 * Pointer: EFI_ABSOLUTE_POINTER_PROTOCOL via lv_uefi_absolute_pointer_indev.
 * The PR#17 lazy-binding pattern (RegisterProtocolNotify) is preserved so USB
 * binding during BDS ConnectAll is handled correctly.
 *
 * NOTE: Mouse wheel scrolling is not yet re-ported; it will return in a
 * follow-up branch.
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
// Custom keyboard read callback.
//
// Returns PRESSED (with the translated key code) when the EFI console buffer
// holds a keystroke, RELEASED when the buffer is empty.  Reading one key per
// call means LVGL sees the key as held across ticks while EFI auto-repeat keeps
// the buffer non-empty, allowing LVGL's long_press_repeat throttle to govern
// the navigation rate.
//
static void
keypad_read (
  lv_indev_t      *indev_drv,
  lv_indev_data_t *data
  )
{
  EFI_STATUS                         Status;
  EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL  *TxtInEx;
  EFI_KEY_DATA                       KeyData;
  UINT32                             KeyShift;

  data->key = 0;

  Status = gBS->HandleProtocol (
                  gST->ConsoleInHandle,
                  &gEfiSimpleTextInputExProtocolGuid,
                  (VOID **)&TxtInEx);
  if (EFI_ERROR (Status)) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  Status = TxtInEx->ReadKeyStrokeEx (TxtInEx, &KeyData);
  if (!EFI_ERROR (Status)) {
    switch (KeyData.Key.UnicodeChar) {
    case CHAR_CARRIAGE_RETURN:
      data->key = LV_KEY_ENTER;
      break;

    case CHAR_BACKSPACE:
      data->key = LV_KEY_BACKSPACE;
      break;

    case CHAR_TAB:
      KeyShift = KeyData.KeyState.KeyShiftState;
      data->key = ((KeyShift & EFI_SHIFT_STATE_VALID) &&
                   (KeyShift & (EFI_RIGHT_SHIFT_PRESSED | EFI_LEFT_SHIFT_PRESSED)))
                  ? LV_KEY_PREV : LV_KEY_NEXT;
      break;

    case CHAR_NULL:
      switch (KeyData.Key.ScanCode) {
      case SCAN_UP:        data->key = LV_KEY_UP;        break;
      case SCAN_DOWN:      data->key = LV_KEY_DOWN;      break;
      case SCAN_RIGHT:     data->key = LV_KEY_RIGHT;     break;
      case SCAN_LEFT:      data->key = LV_KEY_LEFT;      break;
      case SCAN_ESC:       data->key = LV_KEY_ESC;       break;
      case SCAN_DELETE:    data->key = LV_KEY_DEL;       break;
      case SCAN_PAGE_DOWN: data->key = LV_KEY_NEXT;      break;
      case SCAN_PAGE_UP:   data->key = LV_KEY_PREV;      break;
      case SCAN_HOME:      data->key = LV_KEY_HOME;      break;
      case SCAN_END:       data->key = LV_KEY_END;       break;
      case SCAN_F1:        data->key = LV_KEY_F1;        break;
      case SCAN_F2:        data->key = LV_KEY_F2;        break;
      case SCAN_F3:        data->key = LV_KEY_F3;        break;
      case SCAN_F4:        data->key = LV_KEY_F4;        break;
      case SCAN_F5:        data->key = LV_KEY_F5;        break;
      case SCAN_F6:        data->key = LV_KEY_F6;        break;
      case SCAN_F7:        data->key = LV_KEY_F7;        break;
      case SCAN_F8:        data->key = LV_KEY_F8;        break;
      case SCAN_F9:        data->key = LV_KEY_F9;        break;
      case SCAN_F10:       data->key = LV_KEY_F10;       break;
      case SCAN_F11:       data->key = LV_KEY_F11;       break;
      case SCAN_F12:       data->key = LV_KEY_F12;       break;
      default:             break;
      }
      break;

    default:
      data->key = KeyData.Key.UnicodeChar;
      break;
    }

    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

static lv_indev_t *
lv_uefi_keyboard_create (void)
{
  lv_indev_t *indev = lv_indev_create ();
  if (indev == NULL) {
    return NULL;
  }
  lv_indev_set_type (indev, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_read_cb (indev, keypad_read);
  return indev;
}

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
  // Keyboard: custom indev that reads one EFI keystroke per tick and returns
  // PRESSED while the buffer is non-empty, RELEASED when it is empty.
  //
  indev_keypad = lv_uefi_keyboard_create ();

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
