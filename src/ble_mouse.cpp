#include "ble_mouse.h"
#include <NimBLEDevice.h>
#include <NimBLEHIDDevice.h>

static constexpr uint8_t kReportId = 1;

// 3 buttons, X, Y, wheel - all relative.
static const uint8_t kReportMap[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x85, kReportId,   //   Report ID
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Buttons)
    0x19, 0x01,        //     Usage Minimum (1)
    0x29, 0x03,        //     Usage Maximum (3)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x03,        //     Report Count (3)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data, Variable, Absolute)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x05,        //     Report Size (5)
    0x81, 0x03,        //     Input (Constant) - padding
    0x05, 0x01,        //     Usage Page (Generic Desktop)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x09, 0x38,        //     Usage (Wheel)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x03,        //     Report Count (3)
    0x81, 0x06,        //     Input (Data, Variable, Relative)
    0xC0,              //   End Collection
    0xC0               // End Collection
};

static NimBLEServer* s_server         = nullptr;
static NimBLEHIDDevice* s_hid         = nullptr;
static NimBLECharacteristic* s_input  = nullptr;
static volatile bool s_connected      = false;

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override {
        s_connected = true;
        // 15-30 ms interval, 4 s supervision timeout: responsive yet light on battery.
        server->updateConnParams(info.getConnHandle(), 12, 24, 0, 400);
    }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
        s_connected = s_server && s_server->getConnectedCount() > 0;
    }
};
static ServerCallbacks s_callbacks;

namespace BleMouse {

void begin(const char* deviceName) {
    NimBLEDevice::init(deviceName);
    NimBLEDevice::setSecurityAuth(true, false, true);  // bond, no MITM, secure connections
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    s_server = NimBLEDevice::createServer();
    s_server->setCallbacks(&s_callbacks, false);
    s_server->advertiseOnDisconnect(true);

    s_hid   = new NimBLEHIDDevice(s_server);
    s_input = s_hid->getInputReport(kReportId);
    s_hid->setManufacturer("M5Stack");
    s_hid->setPnp(0x02, 0x303A, 0x8204, 0x0100);
    s_hid->setHidInfo(0x00, 0x02);  // not localized, normally connectable
    s_hid->setReportMap((uint8_t*)kReportMap, sizeof(kReportMap));
    s_hid->setBatteryLevel(100);
    s_server->start();

    NimBLEAdvertisementData adv;
    adv.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    adv.setAppearance(HID_MOUSE);
    adv.setCompleteServices(s_hid->getHidService()->getUUID());

    NimBLEAdvertisementData scan;
    scan.setName(deviceName);

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->enableScanResponse(true);
    advertising->setAdvertisementData(adv);
    advertising->setScanResponseData(scan);
    advertising->start();
}

bool started() { return s_server != nullptr; }

bool connected() { return s_connected; }

void move(int8_t dx, int8_t dy) {
    if (!s_connected || !s_input) return;
    uint8_t report[4] = {0, (uint8_t)dx, (uint8_t)dy, 0};
    s_input->setValue(report, sizeof(report));
    s_input->notify();
}

void setBatteryLevel(uint8_t percent) {
    if (s_hid) s_hid->setBatteryLevel(percent, s_connected);
}

void forgetPairings() {
    if (!s_server) {
        // BLE is off in USB-only mode; bring the host up just long enough to clear the bond store.
        NimBLEDevice::init("");
        NimBLEDevice::deleteAllBonds();
        NimBLEDevice::deinit(true);
        return;
    }
    NimBLEDevice::deleteAllBonds();
    for (uint16_t handle : s_server->getPeerDevices()) s_server->disconnect(handle);
}

}  // namespace BleMouse
