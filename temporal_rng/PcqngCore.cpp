#include "PcqngCore.hpp"
#include "MilliTimer.hpp"
#include "Buffer.hpp"

#include <process.h>


PcqngCore::PcqngCore(Buffer& outBuffer)
    : milliTimerPulse_(MilliTimer::GetPulseHandle())
    , runnerThread_(0)
    , endEvent_(CreateEvent(NULL, TRUE, FALSE, NULL))
    , precogEvent_(CreateEvent(NULL, FALSE, FALSE, NULL))
    , tscDiff_(0)
    , qFactor_(0)
    , procCounter_(0)
    , outBuffer_(outBuffer)
    , precogBuffer_(0)
{ }


PcqngCore::~PcqngCore()
{
    Stop();
    CloseHandle(endEvent_);
    endEvent_ = 0;
    CloseHandle(precogEvent_);
    precogEvent_ = 0;
}


void PcqngCore::Start()
{
    // Start the runner thread
    TscProcessing_ = &PcqngCore::InitTscProcessing;
    ResetEvent(endEvent_);
    runnerThread_ = (HANDLE)_beginthreadex(NULL, 0, PcqngCore::Runner, this, 0, NULL);
}


void PcqngCore::Stop()
{
    SetEvent(endEvent_);
    SetEvent(milliTimerPulse_);
    WaitForSingleObject(runnerThread_, INFINITE);
    CloseHandle(runnerThread_);
    runnerThread_ = 0;
}


unsigned __stdcall PcqngCore::Runner(void* self)
{
    if (SetThreadAffinityMask(GetCurrentThread(), 1) == 0)
        throw "PcqngCore: SetThreadAffinityMask FAILED";
    Sleep(1);

    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL) == 0)
        throw "PcqngCore: SetThreadPriority FAILED";

    PcqngCore* obj = (PcqngCore*)self;
    uint64_t tscWord = 0;
    uint64_t prevTscWord = 0;

    while (1)
    {
        // is it time to end this thread?
        if (WaitForSingleObject(obj->endEvent_, 0) == WAIT_OBJECT_0)
            break;

        // wait for multimedia timer fire
        DWORD waitResponse = WaitForSingleObject(obj->milliTimerPulse_, INFINITE);
        if (waitResponse == WAIT_OBJECT_0)
        {
            // TSC Measurement
            __asm
            {
                rdtsc
                mov dword ptr[tscWord], eax
                mov dword ptr[tscWord]+4, edx
            }

            // TSC difference
            obj->tscDiff_ = (tscWord-prevTscWord);
            prevTscWord = tscWord;

            obj->TscProcessing();
        }
        else
        {
            throw "PcqngCore: MMTimer event FAILED";
        }
    }

    return 0;
}


void PcqngCore::TscProcessing()
{
    (this->*TscProcessing_)();
}


void PcqngCore::InitTscProcessing()
{
    lpFilter_.Init((double)tscDiff_);
    if (++procCounter_ >= 3)
    {
        procCounter_ = 0;
        TscProcessing_ = &PcqngCore::RampUpTscProcessing;
    }
}


void PcqngCore::RampUpTscProcessing()
{
    lpFilter_.Feed((double)tscDiff_, 100);
    if (++procCounter_ >= 1000)
    {
        procCounter_ = 0;
        TscProcessing_ = &PcqngCore::NormalTscProcessing;
    }
}


void PcqngCore::NormalTscProcessing()
{
    double filterRatio = (double)tscDiff_/lpFilter_.GetValue();

    if ((filterRatio > 1.05) || (filterRatio < 0.95))
        lpFilter_.Feed((double)tscDiff_, 1000);
    else
        lpFilter_.Feed((double)tscDiff_, 100);

    qFactor_ = lpFilter_.GetValue()/33333;                       // Quantization
    uint8_t eBits = (char)(((double)tscDiff_ / qFactor_) + 0.5); // Entropic bits
    outBuffer_.Write(&eBits, 1);
}


uint8_t PcqngCore::PrecogGetTarget()
{
    TscProcessing_ = &PcqngCore::PrecogProcessing;

    WaitForSingleObject(precogEvent_, 1000);
    outBuffer_.Write(&precogBuffer_, 1);
    WaitForSingleObject(precogEvent_, 1000);
    TscProcessing_ = &PcqngCore::NormalTscProcessing;

    return precogBuffer_;
}


void PcqngCore::PrecogProcessing()
{
    double filterRatio = (double)tscDiff_/lpFilter_.GetValue();

    if ((filterRatio > 1.05) || (filterRatio < 0.95))
        lpFilter_.Feed((double)tscDiff_, 1000);
    else
        lpFilter_.Feed((double)tscDiff_, 100);

    qFactor_ = lpFilter_.GetValue()/33333;                       // Quantization
    uint8_t eBits = (char)(((double)tscDiff_ / qFactor_) + 0.5); // Entropic bits

    precogBuffer_ = eBits;
    PulseEvent(precogEvent_);
}
