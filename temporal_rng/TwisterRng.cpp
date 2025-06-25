#include "TwisterRng.hpp"
#include "MilliTimer.hpp"
#include "Buffer.hpp"

#include <process.h>


TwisterRng::TwisterRng(Buffer& outBuffer, Buffer& outAcBuffer)
    : IRng(outBuffer, outAcBuffer, "PRNGTwister")
    , milliTimerPulse_(MilliTimer::GetPulseHandle())
    , runnerThread_(0)
    , endEvent_(CreateEvent(NULL, TRUE, FALSE, NULL))
{
    bitRate_ = 308000;

    LARGE_INTEGER hpCount;
    QueryPerformanceCounter(&hpCount);
    twister_.init_genrand(hpCount.LowPart);
}


TwisterRng::~TwisterRng()
{
    Stop();
    CloseHandle(endEvent_);
    endEvent_ = 0;
}


void TwisterRng::Start()
{
    // Start the runner thread
    ResetEvent(endEvent_);
    runnerThread_ = (HANDLE)_beginthreadex(NULL, 0, TwisterRng::Runner, this, 0, NULL);
}


void TwisterRng::Stop()
{
    SetEvent(endEvent_);
    WaitForSingleObject(runnerThread_, INFINITE);
    CloseHandle(runnerThread_);
    runnerThread_ = 0;
}

unsigned __stdcall TwisterRng::Runner(void* self)
{
    if (SetThreadAffinityMask(GetCurrentThread(), 1) == 0)
        throw "PseudoRng: SetThreadAffinityMask FAILED";
    Sleep(1);

    if (SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL) == 0)
        throw "PseudoRng: SetThreadPriority FAILED";

    TwisterRng* obj = (TwisterRng*)self;

    std::vector<uint8_t> buffer(110*4);
//    int leapCount = 0;

    uint32_t prngWord;
	int wrx = 4;

	int tenCount = 0;

	LARGE_INTEGER hpCountNow;
	LARGE_INTEGER hpCountPrev;
	LARGE_INTEGER hpFreq;

	double elapsedMillis = 0;
	QueryPerformanceFrequency(&hpFreq);
	QueryPerformanceCounter(&hpCountPrev);

	while (1)
	{
        // is it time to end this thread?
        if (WaitForSingleObject(obj->endEvent_, 0) == WAIT_OBJECT_0)
            break;

		for (int i=0; i<110; i++) {
        	prngWord = obj->twister_.genrand_int32();
			prngWord |= 0x80808080;
			memcpy(&buffer[i*4], &prngWord, 4);
		}

		wrx = obj->outBuffer_.Write(buffer);
		obj->rawBuffer_->Write(&buffer[0], wrx);
		while (wrx < 440) {
			Sleep(2);
			int written = obj->outBuffer_.Write(&buffer[wrx], 440-wrx);
			obj->rawBuffer_->Write(&buffer[wrx], written);
			wrx += written;
			QueryPerformanceCounter(&hpCountPrev);
		}

/*		static int cntr = 0;
        ++cntr;
        if (cntr >= 100)
        {
            printf("x@ %i\n", 110*7*4*cntr);
            cntr = 0;
        }*/

		elapsedMillis = 0;
		while (elapsedMillis < 10) {
			QueryPerformanceCounter(&hpCountNow);
			elapsedMillis = 1000.0*((double)hpCountNow.QuadPart-(double)hpCountPrev.QuadPart)/(double)hpFreq.QuadPart;
			Sleep(1);
		}
		hpCountPrev = hpCountNow;
	}

/*    while (1)
    {
        // is it time to end this thread?
        if (WaitForSingleObject(obj->endEvent_, 0) == WAIT_OBJECT_0)
            break;

        // wait for multimedia timer fire
        DWORD waitResponse = WaitForSingleObject(obj->milliTimerPulse_, INFINITE);
		tenCount++;
        if (waitResponse == WAIT_OBJECT_0)
        {
			if (tenCount == 10) {
				tenCount = 0;

	            for (int i=0; i<110; ++i)
	            {
					if (wrx == 4) {
	                	prngWord = obj->twister_.genrand_int32();
						prngWord |= 0x80808080;
						uint8_t* buffWords4 = (uint8_t*)&prngWord;
						wrx = obj->outBuffer_.Write(buffWords4, 4);
	    static int cntr = 0;
	                ++cntr;
	                if (cntr >= 11000)
	                {
	                    printf("@ %i\n", 7*4*cntr);
	                    cntr = 0;
	                }
					}
					else {
						uint8_t* buffWords4 = (uint8_t*)&prngWord;
						wrx += obj->outBuffer_.Write(&buffWords4[wrx], 4-wrx);
						tenCount = 9;
						i = 110;
					}

	//                memcpy(&buffer[i*4], &prngWord, 4);
					
	            }
	//            obj->outBuffer_.Write(buffer);
	/*            if (++leapCount == 4)
	            {
	                prngWord = obj->twister_.genrand_int32();
	                obj->outBuffer_.Write((uint8_t*)&prngWord, 4);
	                leapCount = 0;
	            }*/
//			}
//        }
//        else
//        {
//            throw "PcqngCore: MMTimer event FAILED";
//        }
//    }

    return 0;
}
