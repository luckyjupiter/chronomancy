#ifndef MILLITIMER_INCLUDED_
#define MILLITIMER_INCLUDED_


#include <Windows.h>


class MilliTimer
{
public:
    ~MilliTimer();

private:
    MilliTimer();
    MilliTimer(MilliTimer const&);
    MilliTimer& operator=(MilliTimer const&);

public:
    static HANDLE GetPulseHandle();

private:
    HANDLE mmTimerEvent_;
    MMRESULT mmTimerId_;
};


#endif // MILLITIMER_INCLUDED_
