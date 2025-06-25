// © 2012 Psigenics
//
// FtdiRngArray: Manages array of FTDI devices as if it is one device
//
// 2012.09.28    pawilber        Creation: Manages array of FTDI devices as if one device
//                               But will also work on single device


#include "FtdiRngArray.hpp"
#include "Buffer.hpp"
#include "stdint.h"

#include <string>
#include <process.h>


FtdiRngArray::FtdiRngArray(Buffer& outBuffer, Buffer& outAcBuffer, std::string id)
    : IRng(outBuffer, outAcBuffer, id)
    , id_(id)
{    
    // find our bitrate
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

    // bitrate if array
    arraySize_ = 1;
    if (id.find("x") == 9)
    {
        arraySize_ = (id.at(10)-48);
        bitRate_ *= arraySize_;
    }
}


FtdiRngArray::~FtdiRngArray()
{
    for (unsigned i=0; i<arraySize_; i++)
        delete ftdiArray_[i];

    ftdiArray_.clear();
    arraySize_ = 0;
}


void FtdiRngArray::Start()
{
    std::vector<std::string> sourceList;
    FtdiRng::ListSources(sourceList);
    std::string prefix = id_.substr(0, 4);
    int sourcesCount = sourceList.size();
    int k = 0;

    for (unsigned i=0; i<arraySize_; i++)
    {
        // verify source availability
        while (1)
        {
            if (sourceList[k].find(prefix) == 0)
                break;
            if (++k == sourcesCount)
                throw "NO HARDWARE"; // did not find an available device
        }
        FtdiRng* newFtdiRng = new FtdiRng(outBuffer_, outAcBuffer_, sourceList[i]);
        newFtdiRng->Start();
        ftdiArray_.push_back(newFtdiRng);
    }
}


void FtdiRngArray::Stop()
{
    for (unsigned i=0; i<arraySize_; i++)
        ftdiArray_[i]->Stop();
}


void FtdiRngArray::ListSources(std::vector<std::string>& sourceList)
{
    FtdiRng::ListSources(sourceList);

    for (unsigned i=0; i<sourceList.size(); i++)
    {
        for (unsigned j=i+1; j<sourceList.size(); j++)
        {
            if (sourceList[j].find(sourceList[i].substr(0, 4)) == 0)
            {
                if (sourceList[i].length() == 8)
                    sourceList[i].append("SN");
                if (sourceList[i].find("SN") == 9)
                    sourceList[i].replace(9, 2, "x2");
                else if (sourceList[i].find("x") == 9)
                    sourceList[i].replace(10, 1, 1, (char)(sourceList[i].at(10)+1));
                sourceList.erase(sourceList.begin()+j);
                j--;
            }
        }
    }

    std::reverse(sourceList.begin(), sourceList.end());
}


int FtdiRngArray::Write(int outBuffLen, char* outBuffer)
{
	DWORD bytesWritten = 0;
	if (id_.find("PD") == 0)
    {
        DWORD returnBytesWritten = INT_MAX;
        for (unsigned i=0; i<arraySize_; i++)
        {
            bytesWritten = ftdiArray_[i]->Write(outBuffLen, outBuffer);
            if (bytesWritten < returnBytesWritten)
                returnBytesWritten = bytesWritten;
        }

		return bytesWritten;
	}
	else
		return 0;
}
