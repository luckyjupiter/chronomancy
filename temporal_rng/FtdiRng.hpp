// © 2012 Psigenics
//
// FtdiRng: Interface of a single FTDI PRD device
//
// 2012.09.28    pawilber        Interface of a single FTDI PRD device


#pragma once


#include "IRng.hpp"

#include <Windows.h>
#include <FTD2XX.h>
#include <vector>


class Buffer;


class FtdiRng : public IRng
{
public:
    FtdiRng(Buffer& outBuffer, Buffer& outAcBuffer, std::string id);
    ~FtdiRng();

public:
    // start FTDI PRD device
    virtual void Start();
    // stop FTDI PRD device
    virtual void Stop();
    // write to FTDI PRD device
	virtual int Write(int outBuffLen, char* outBuffer);
    // list all available FTDI PRD devices individually
    static void ListSources(std::vector<std::string>& sourceList);

private:
    // reader thread
    static unsigned __stdcall Runner(void* self);

private:
    HANDLE runnerThread_;
    // new data event for reader thread
    HANDLE ftdiEvent_;
    // end reader thread event for stop/shutdown
    HANDLE endEvent_;
    FT_HANDLE ftHandle_;
};
