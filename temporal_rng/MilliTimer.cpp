#include "MilliTimer.hpp"


MilliTimer::MilliTimer()
{
    mmTimerEvent_ = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (mmTimerEvent_ == NULL)
        throw "MilliTimer: CreateEvent FAILED";

    // ensure 1ms timer resolution available on this machine
    TIMECAPS timeCaps;
    timeGetDevCaps(&timeCaps, sizeof(timeCaps));
    if (timeCaps.wPeriodMin != 1)
        throw "MilliTimer: 1ms resolution not available";

    // Start the PCQNG low-speed timer interrupt
    mmTimerId_ = timeSetEvent(1, 0, (LPTIMECALLBACK)mmTimerEvent_, 1, TIME_PERIODIC | TIME_CALLBACK_EVENT_PULSE);
    if (mmTimerId_ == 0)
        throw "MilliTimer: Start FAILED";
}


MilliTimer::~MilliTimer()
{
    timeKillEvent(mmTimerId_);
    CloseHandle(mmTimerEvent_);
    mmTimerEvent_ = 0;
}


HANDLE MilliTimer::GetPulseHandle()
{
    static MilliTimer milliTimer;
    return milliTimer.mmTimerEvent_;
}
