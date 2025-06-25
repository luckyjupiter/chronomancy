#include "PseudoRng.hpp"
#include "MilliTimer.hpp"
#include "Buffer.hpp"

#include <process.h>
#include <stdio.h>


PseudoRng::PseudoRng(Buffer& buffer)
    : milliTimerPulse_(MilliTimer::GetPulseHandle())
    , runnerThread_(0)
    , endEvent_(CreateEvent(NULL, TRUE, FALSE, NULL))
    , buffer_(buffer)
{
    buffer_.SetBlockOnFull(true);
}


PseudoRng::~PseudoRng()
{
    Stop();
}


void PseudoRng::Start()
{
    // Start the runner thread
    ResetEvent(endEvent_);
    runnerThread_ = (HANDLE)_beginthreadex(NULL, 0, PseudoRng::Runner, this, 0, NULL);
}


void PseudoRng::Stop()
{
    SetEvent(endEvent_);
    WaitForSingleObject(runnerThread_, INFINITE);
    CloseHandle(runnerThread_);
    runnerThread_ = 0;
}


unsigned __stdcall PseudoRng::Runner(void* self)
{
    if (SetThreadAffinityMask(GetCurrentThread(), 1) == 0)
        throw "PseudoRng: SetThreadAffinityMask FAILED";
    Sleep(1);

    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL) == 0)
        throw "PseudoRng: SetThreadPriority FAILED";

    PseudoRng* obj = (PseudoRng*)self;

    while (1)
    {
        // is it time to end this thread?
        if (WaitForSingleObject(obj->endEvent_, 0) == WAIT_OBJECT_0)
            break;

        // wait for multimedia timer fire
        DWORD waitResponse = WaitForSingleObject(obj->milliTimerPulse_, INFINITE);
        if (waitResponse == WAIT_OBJECT_0)
        {
            // fill buffer until write blocked
            uint32_t prngWord = 0;
            obj->buffer_.Clear();
            do
            {
//                prngWord = MersenneTwister();

            } while (obj->buffer_.Write((uint8_t*)&prngWord, 4) == 4);
        }
        else
        {
            throw "PcqngCore: MMTimer event FAILED";
        }
    }

    return 0;
}
