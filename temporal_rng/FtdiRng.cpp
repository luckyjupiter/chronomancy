// © 2012 Psigenics
//
// FtdiRng: Interface of a single FTDI PRD device
//
// 2012.09.28    pawilber        Interface of a single FTDI PRD device


#include "FtdiRng.hpp"
#include "Buffer.hpp"
#include "stdint.h"

#include <string>
#include <process.h>


#define FTDIB_DEVICE_LATENCY_MS 4
#define FTDIB_DEVICE_PACKET_USB_SIZE 320
#define FTDIB_DEVICE_TX_TIMEOUT 2000


FtdiRng::FtdiRng(Buffer& outBuffer, Buffer& outAcBuffer, std::string id)
    : IRng(outBuffer, outAcBuffer, id)
    , runnerThread_(0)
    , ftdiEvent_(CreateEvent(NULL, FALSE, FALSE, NULL))
    , endEvent_(CreateEvent(NULL, TRUE, FALSE, NULL))
    , ftHandle_(0)
{
    if (id.find("QWR1") == 0)
        bitRate_ = 875000;
    if (id.find("QWR2") == 0)
        bitRate_ = 875000;
    if (id.find("QWP4") == 0)
        bitRate_ = 250000;
    if (id.find("PD")==0 && id.find("QK")==4)
    {
        bitRate_ = (id[6]-'0') * 100000;
        bitRate_ += (id[7]-'0') * 10000;
        bitRate_ += (id[8]-'0') * 1000;
    }
    if (id.find("PD")==0 && id.find("QM")==4)
    {
        bitRate_ = (id[6]-'0') * 1000000;
        bitRate_ += (id[7]-'0') * 100000;
        bitRate_ += (id[8]-'0') * 10000;
    }
}


FtdiRng::~FtdiRng()
{
    Stop();
    CloseHandle(endEvent_);
    endEvent_ = 0;
    CloseHandle(ftdiEvent_);
    ftdiEvent_ = 0;
}


void FtdiRng::Start()
{
    // already open?
    if (ftHandle_ != 0)
        return;

    // search for an available device
    if (memcmp(GetId().c_str(), "QWR1", 4) == 0)
    {
        if (FT_OpenEx("ComScire QNG Device", FT_OPEN_BY_DESCRIPTION, &ftHandle_) != FT_OK)
            throw "NO HARDWARE"; // did not find an available device
    }
    else
    {
        if (FT_OpenEx((PVOID)GetId().c_str(), FT_OPEN_BY_SERIAL_NUMBER, &ftHandle_) != FT_OK)
            throw "NO HARDWARE"; // did not find an available device
    }

    FT_SetLatencyTimer(ftHandle_, FTDIB_DEVICE_LATENCY_MS);
	FT_SetUSBParameters(ftHandle_, FTDIB_DEVICE_PACKET_USB_SIZE, FTDIB_DEVICE_PACKET_USB_SIZE);
	FT_SetTimeouts(ftHandle_, FTDIB_DEVICE_TX_TIMEOUT, FTDIB_DEVICE_TX_TIMEOUT);

	// Start the device
	DWORD bytesWritten = 0;
	if (memcmp(GetId().c_str(), "PD", 2) == 0)
    {
		char prdStartCommand[1] = {0x66};
		if (FT_Write(ftHandle_, prdStartCommand, 1, &bytesWritten) != FT_OK)
		{
			Stop();
			throw "NO HARDWARE";
		}
		if (bytesWritten != 1)
		{
			Stop();
			throw "NO HARDWARE";
		}
	}
	else
    {
		char prdStartCommand[4] = {(char)0x01, (char)0x00, (char)0xf0, (char)0x96};
		if (FT_Write(ftHandle_, prdStartCommand, 4, &bytesWritten) != FT_OK)
		{
			Stop();
			throw "NO HARDWARE";
		}
		if (bytesWritten != 4)
		{
			Stop();
			throw "NO HARDWARE";
		}
	}
	DWORD rxBytes, txBytes, eventStatus;
	if (FT_GetStatus(ftHandle_, &rxBytes, &txBytes, &eventStatus) != FT_OK)
	{
		Stop();
		throw "NO HARDWARE";
	}

    if (FT_SetEventNotification(ftHandle_, FT_EVENT_RXCHAR, ftdiEvent_) != FT_OK)
    {
        Stop();
        throw "NO HARDWARE";
    }

    // start the reader thread
    ResetEvent(endEvent_);
    runnerThread_ = (HANDLE)_beginthreadex(NULL, 0, FtdiRng::Runner, this, 0, NULL);
}


void FtdiRng::Stop()
{
    // end reader thread
    SetEvent(endEvent_);
    WaitForSingleObject(runnerThread_, INFINITE);
    CloseHandle(runnerThread_);
    runnerThread_ = 0;

    // close hardware
    FT_Close(ftHandle_);
    ftHandle_ = 0;
}


unsigned __stdcall FtdiRng::Runner(void* self)
{
    FtdiRng* obj = (FtdiRng*)self;

    DWORD bytesReturned = 0;
    std::vector<uint8_t> inBuffer(FTDIB_DEVICE_PACKET_USB_SIZE);
    std::vector<uint8_t> tempOutBuffer(FTDIB_DEVICE_PACKET_USB_SIZE);
    std::vector<uint8_t> tempAcBuffer(FTDIB_DEVICE_PACKET_USB_SIZE);
    int tempOutCount = 0;
    int tempAcCount = 0;

    DWORD rxBytes;
    DWORD txBytes;
    DWORD eventDword;

    while (1)
    {
//        if (bytesReturned != FTDIB_DEVICE_PACKET_USB_SIZE)
//            inBuffer.resize(FTDIB_DEVICE_PACKET_USB_SIZE);
        if (WaitForSingleObject(obj->endEvent_, 0) == WAIT_OBJECT_0)
            break;

        WaitForSingleObject(obj->ftdiEvent_, INFINITE);
        FT_GetStatus(obj->ftHandle_, &rxBytes, &txBytes, &eventDword);
        if (rxBytes > 0)
        {
            inBuffer.resize(rxBytes);
            tempOutBuffer.resize(rxBytes);
            tempAcBuffer.resize(rxBytes);

            FT_Read(obj->ftHandle_, &inBuffer[0], rxBytes, &bytesReturned);
//            if (bytesReturned < rxBytes)
//                inBuffer.resize(bytesReturned);

			obj->rawBuffer_->Write(&inBuffer[0], bytesReturned);
            tempOutCount = 0;
            tempAcCount = 0;
			for (DWORD i=0; i<bytesReturned; i++) {
				if (((char)inBuffer[i]&0x80) == 0X80)
                {
                    tempOutBuffer[tempOutCount] = inBuffer[i];
                    tempOutCount++;
                }
                else
                {
                    tempAcBuffer[tempAcCount] = inBuffer[i];
                    tempAcCount++;
                }
			}

            if (tempOutCount > 0)
			    obj->outBuffer_.Write(&tempOutBuffer[0], tempOutCount);
            if (tempAcCount > 0)
			    obj->outAcBuffer_.Write(&tempAcBuffer[0], tempAcCount);
        }
    }

    return 0;
}


void FtdiRng::ListSources(std::vector<std::string>& sourceList)
{
    sourceList.clear();

    DWORD ftDevCount = 0;
    FT_ListDevices(&ftDevCount, NULL, FT_LIST_NUMBER_ONLY);
    if (ftDevCount > 0)
    {
        for (DWORD devIndex=0; devIndex<ftDevCount; ++devIndex)
        {
            char serialNum[18];
            PVOID pDevIndex = 0;
            memcpy(&pDevIndex, &devIndex, sizeof(DWORD));
            serialNum[0] = '\0';
            FT_ListDevices((PVOID)pDevIndex, (PVOID)serialNum, FT_LIST_BY_INDEX | FT_OPEN_BY_SERIAL_NUMBER);
            if (serialNum[0] == '\0')
            {
                FT_HANDLE ftHandle;
                if (FT_OpenEx("ComScire QNG Device", FT_OPEN_BY_DESCRIPTION, &ftHandle) == FT_OK)
                    strcpy_s(serialNum, 18, "QWR10000");
                FT_Close(ftHandle);
            }
            if ((memcmp(serialNum, "QWP", 3) == 0) || (memcmp(serialNum, "QWR", 3) == 0))
            {
                serialNum[8] = '\0';
                sourceList.push_back(serialNum);
            }
            if ((memcmp(serialNum, "PD", 2) == 0) && (serialNum[4] == 'Q'))
            {
                bool isValid = true;
                for (int i=6; i<=8; ++i)
                {
                    if ((serialNum[i] < '0') || (serialNum[i] > '9'))
                        isValid = false;
                }
                if (isValid == true)
                {
                    serialNum[16] = '\0';
                    sourceList.push_back(serialNum);
                }
            }
        }
    }
}


int FtdiRng::Write(int outBuffLen, char* outBuffer) {
	DWORD bytesWritten = 0;
	if (memcmp(GetId().c_str(), "PD", 2) == 0) {
		if (FT_Write(ftHandle_, outBuffer, outBuffLen, &bytesWritten) == FT_OK)
			return bytesWritten;
		else
			return -1;
	}
	else
		return 0;
}
