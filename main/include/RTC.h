#pragma once
#include <RtcDS1302.h>
#include <RtcDateTime.h>

namespace RTC {
    extern ThreeWire wire;
    extern RtcDS1302<ThreeWire> rtc;

    // extern RtcDateTime testDT;

    void init();

    inline void setDateTime(uint16_t year, uint8_t month, uint8_t day,
                            uint8_t hour, uint8_t minute, uint8_t second) {
        rtc.SetDateTime(RtcDateTime(year, month, day, hour, minute, second));
    }

    inline void setDateTime(uint32_t timestamp) {
         RtcDateTime dt;
         dt.InitWithUnix32Time(timestamp);
         rtc.SetDateTime(dt);
     }

    // inline void setDateTime(uint32_t timestamp) {
    //     testDT.InitWithUnix32Time(timestamp);
    // }

    // inline RtcDateTime getDateTime() { return testDT; }
    inline RtcDateTime getDateTime() { return rtc.GetDateTime(); }

    inline uint16_t getYear() { return getDateTime().Year(); }

    inline uint8_t getMonth() { return getDateTime().Month(); }

    inline uint8_t getDay() { return getDateTime().Day(); }

    inline uint8_t getHour() { return getDateTime().Hour(); }

    inline uint8_t getMinute() { return getDateTime().Minute(); }

    inline uint8_t getSecond() { return getDateTime().Second(); }

    inline uint8_t getDayOfWeek() { return getDateTime().DayOfWeek(); }

    inline String getDateString() {
        return String(getYear()) + " - " + String(getMonth()) + " - " +
               String(getDay());
    }

    inline String getTimeString() {
        return String(getHour()) + " : " + String(getMinute()) + " : " +
               String(getSecond());
    }

    inline String getDayOfWeekString() {
        String days[] = {"Sunday",   "Monday", "Tuesday", "Wednesday",
                         "Thursday", "Friday", "Saturday"};
        return days[getDayOfWeek()];
    }
} // namespace RTC