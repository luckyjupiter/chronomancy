// © 2012 Psigenics
//
// FtdiRngArray: Manages array of FTDI devices as if it is one device
//
// 2012.09.28    pawilber        Creation: Manages array of FTDI devices as if one device
//                               But will also work on single device


#pragma once


#include "IRng.hpp"
#include "FtdiRng.hpp"

#include <Windows.h>
#include <FTD2XX.h>
#include <vector>


class Buffer;


// manages (or array) FTDI PRD device(s) as a single RNG source
class FtdiRngArray : public IRng
{
public:
    FtdiRngArray(Buffer& outBuffer, Buffer& outAcBuffer, std::string id);
    ~FtdiRngArray();

public:
    // starts device(s)
    virtual void Start();
    // stops device(s)
    virtual void Stop();
    // writes to eligible devices
	virtual int Write(int outBuffLen, char* outBuffer);
    // lists FTDI PRD sources
    static void ListSources(std::vector<std::string>& sourceList);

private:
    unsigned arraySize_;
    // actual FTDI PRD device(s) constituing the array
    std::vector<FtdiRng*> ftdiArray_;
    // id of this device or array
    std::string id_;

friend class FtdiRngArrayTest;
};
