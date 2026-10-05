#include "usb_mouse.h"
#include "USB.h"
#include "USBHIDMouse.h"

// Must be globals: their constructors register the USB interfaces before USB.begin().
static USBHIDMouse s_mouse;
static USBCDC s_cdc;

static volatile bool s_suspended = false;

static void onUsbEvent(void*, esp_event_base_t base, int32_t id, void*) {
    if (base != ARDUINO_USB_EVENTS) return;
    switch (id) {
        case ARDUINO_USB_SUSPEND_EVENT: s_suspended = true; break;
        case ARDUINO_USB_RESUME_EVENT:
        case ARDUINO_USB_STARTED_EVENT:
        case ARDUINO_USB_STOPPED_EVENT: s_suspended = false; break;
        default: break;
    }
}

namespace UsbMouse {

void begin(const char* productName) {
    USB.onEvent(onUsbEvent);
    USB.productName(productName);
    USB.manufacturerName("M5Stack");
    s_mouse.begin();
    s_cdc.begin();
    USB.begin();
}

bool connected() {
    // Skip while the host sleeps so a jiggle never wakes a suspended PC.
    return (bool)USB && !s_suspended;
}

void move(int8_t dx, int8_t dy) {
    if (connected()) s_mouse.move(dx, dy);
}

}  // namespace UsbMouse
