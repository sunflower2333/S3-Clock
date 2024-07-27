#include "RTC.h"

// Pin definitions
constexpr uint8_t DS_DATA = 42;
constexpr uint8_t DS_CLK = 41;
constexpr uint8_t DS_RST = 1;

namespace RTC {
    ThreeWire wire(DS_DATA, DS_CLK, DS_RST);
    RtcDS1302<ThreeWire> rtc(wire);

    // RtcDateTime testDT;

    void init() {
        rtc.Begin();

        if (!rtc.IsDateTimeValid()) {
            // Common Causes:
            //    1) first time you ran and the device wasn't running yet
            //    2) the battery on the device is low or even missing

            Serial.println("RTC lost confidence in the DateTime!");
            rtc.SetDateTime(RtcDateTime(1919, 8, 10, 11, 45, 14));
        }

        if (rtc.GetIsWriteProtected()) {
            Serial.println("RTC was write protected, enabling writing now");
            rtc.SetIsWriteProtected(false);
        }

        if (!rtc.GetIsRunning()) {
            Serial.println("RTC was not actively running, starting now");
            rtc.SetIsRunning(true);
        }
    }

} // namespace RTC