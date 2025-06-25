// © 2012 Psigenics
//
// PrdCore: DLL interface to PrdCore
//
// 2012.10.02    pawilber        Factored out PrdObject
// 2012.09.28    pawilber        Added FTDI generator array capabilities


#include "PrdCore.h"
#include "Buffer.hpp"
#include "PcqngCore.hpp"
#include "PcqngRng.hpp"
#include "FtdiRngArray.hpp"
#include "TwisterRng.hpp"
#include "BpZAnalogFlipMultiPrd.hpp"
#include "PrdComparator.hpp"
#include "PcqngTargetGen.hpp"
#include "PrdObject.h"

#include <map>
#include <vector>
#include <time.h>
#include <string>

using namespace std;


std::map<int, PrdObject> prdHandles_;


BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
    {
	case DLL_PROCESS_ATTACH:
        hModule__ = hModule;
        break;
	case DLL_THREAD_ATTACH:
        break;
	case DLL_THREAD_DETACH:
        break;
	case DLL_PROCESS_DETACH:
        // find a unique handle number
        for (int i=0; i<(int)prdHandles_.size(); ++i)
            Close(i);
		break;
	}
    return TRUE;
}


PSIGPRD_API int Open(char* licenseId)
{
    int idLen = (int)strlen(licenseId);

    // user validity check
    int pwrdLen = 9;
    int tStampLen = 3;
    int userLen = idLen - pwrdLen - tStampLen - 2;
    if (userLen < 3)
        return (-1);

    char* userId = licenseId;
    char* tStamp = userId + userLen + 1;
    char* password = userId + userLen + tStampLen + 2;

    int userIndex = 0;
    // this is the dummy key, but real copyright
	char* key = "© 2012 Psigenics Corporation";
    // this is the real key
    key = "© 2007 Psigenics Corporation";

    userIndex = 0;
    for (int i=0; i<9; i++)
    {
        unsigned val = 0;
        if (i < 3)
            val = abs((int)((unsigned)password[i]-97) - (int)((unsigned)tStamp[i]%26) + 26)%26;
        else
        {
            val = abs((int)((unsigned)password[i]-97) - (int)((unsigned)userId[userIndex]%26) + 26)%26;
            userIndex++;
            userIndex %= userLen;
        }
        if (val != (unsigned)key[i]%26)
        {
            Sleep(1000);
            return (-1);
        }
    }

    if (tStamp[2] != 'z')
    {
        time_t timeTNow;
        time(&timeTNow);
        tm localTNow;
        localtime_s(&localTNow, &timeTNow);
        if (((localTNow.tm_year%10) >= (tStamp[0]-97)) && ((localTNow.tm_mon) >= (tStamp[1]-97)) && ((localTNow.tm_mday) > (tStamp[2]-97)))
        {
            Sleep(1000);
            return (-1);
        }
    }

    // create default PrdObject
    PrdObject prdObject;
    InitPrdObject(prdObject, NULL);

    // assign a unique prdHandle
    int prdHandle = 0;
    std::map<int, PrdObject>::iterator handleIter;
    do
    {
        prdHandle++;
        handleIter = prdHandles_.find(prdHandle);

    } while (handleIter != prdHandles_.end());

    prdHandles_.insert(std::make_pair(prdHandle, prdObject));
    
    return prdHandle;
}


PSIGPRD_API int Close(int prdHandle)
{
    // verify handle
    std::map<int, PrdObject>::iterator handleIter;
    handleIter = prdHandles_.find(prdHandle);
    if (handleIter == prdHandles_.end())
        return -1;

    PrdObject prdObject = prdHandles_[prdHandle];
    DeinitPrdObject(prdObject);

    prdHandles_.erase(handleIter);

    return 0;
}


PSIGPRD_API int ListSources(int srcsBuffLen, char* sources)
{
    std::vector<std::string> sourceList;
    FtdiRngArray::ListSources(sourceList);

    // sufficient buffer size precondition check
    if (srcsBuffLen < (int)(17*(sourceList.size()+1)+1))
        return -1;
    
    // add FTDI (incliding array) sources
    unsigned stringPos = 0;
    for (unsigned i=0; i<sourceList.size(); i++)
    {
		strcpy_s(&sources[stringPos], sourceList[i].length()+1, sourceList[i].c_str());
        stringPos += (unsigned)sourceList[i].length() + 1;
    }

    // add PCQNG source
    strcpy_s(&sources[stringPos], 17, "PCQNG");
    stringPos += 6;

    // add PRNG source
    strcpy_s(&sources[stringPos], 17, "PRNG");
    stringPos += 5;
    
    // double null termination
    sources[stringPos] = '\0';

    return sourceList.size() + 2;
}


PSIGPRD_API int SetSource(int prdHandle, char* source)
{
    // lookup our object
    PrdObject& prdObject = prdHandles_[prdHandle];

    // check if it is the current source - > do nothing
    if (strcmp(source, prdObject.rng->GetId().c_str()) == 0)
        return 0;

    // check if valid source
    std::vector<std::string> sourceList;
    FtdiRng::ListSources(sourceList);

    bool validSource = false;
    for (unsigned i=0; i<sourceList.size(); ++i)
    {
        if (strcmp(source, sourceList[i].c_str()) == 0)
        {
            validSource = true;
            break;
        }
    }
    if (strcmp(source, "PCQNG") == 0)
        validSource = true;
    if (strcmp(source, "PRNG") == 0)
        validSource = true;
    if (validSource != true)
        return -1;

    // remember modes
    int prdMode = GetPrdMode(prdHandle);
    int psiMode = GetPsiMode(prdHandle);

    // reinit prdObject to new source
    DeinitPrdObject(prdObject);
    InitPrdObject(prdObject, source);

    // set modes of previous source
    SetPrdMode(prdHandle, prdMode);
    SetPsiMode(prdHandle, psiMode);

    return 0;
}


PSIGPRD_API int GetSource(int prdHandle, int srcBuffLen, char* source)
{
    if (srcBuffLen < 17)
        return -1;

    // check if it is the current source - > do nothing
    PrdObject prdObject = prdHandles_[prdHandle];
    strcpy_s(source, srcBuffLen, prdObject.rng->GetId().c_str());

    return 0;
}


PSIGPRD_API int SetPrdMode(int prdHandle, int prdMode)
{
    if ((prdMode < 1) || (prdMode > 3))
        return -1;

    PrdObject prdObject = prdHandles_[prdHandle];
    if (prdMode != 1)
        prdObject.prdComparator->SetPsiMode(PrdComparator::PSIMODE_NOTARGETS);
    else
        prdObject.prdComparator->SetPsiMode(PrdComparator::PSIMODE_PSYCHOKINESIS);
    prdObject.prdGen->SetPrdMode(prdMode);

    return prdObject.prdGen->GetPrdMode();
}


PSIGPRD_API int GetPrdMode(int prdHandle)
{
    PrdObject prdObject = prdHandles_[prdHandle];
    return prdObject.prdGen->GetPrdMode();
}


PSIGPRD_API int SetPsiMode(int prdHandle, int psiMode)
{
    if ((psiMode < 1) || (psiMode > 3))
        return -1;

    PrdObject prdObject = prdHandles_[prdHandle];
    prdObject.prdComparator->SetPsiMode((PrdComparator::PsiMode)psiMode);

    return prdObject.prdComparator->GetPsiMode();
}


PSIGPRD_API int GetPsiMode(int prdHandle)
{
    int psiMode = -1;

    PrdObject prdObject = prdHandles_[prdHandle];
    psiMode = prdObject.prdComparator->GetPsiMode();

    return psiMode;
}


PSIGPRD_API int GetPrdData(int prdHandle, int count, double* psiData, double* targetData)
{
    if ((count < 0) || (count > 40000))
        return -1;

    PrdObject prdObject = prdHandles_[prdHandle];
    prdObject.prdComparator->GenPrdResult(count, psiData, targetData);

    return count;
}


PSIGPRD_API int GetRawData(int prdHandle, int count, char* rawData)
{
    if ((count < 0) || (count > 80000))
        return -1;

	PrdObject prdObject = prdHandles_[prdHandle];
	std::vector<uint8_t> vectData;// = new std::vector<uint8_t>();
	count = prdObject.rng->rawBuffer_->Read(vectData, count);
	if (strcmp(prdObject.rng->GetId().c_str(), "PCQNG") == 0)
    {
		for (int i=0; i<count; i++)
			vectData[i] |= 0x80;
	}
	memcpy(rawData, &vectData[0], count);

	return count;
}


PSIGPRD_API int Write(int prdHandle, int outBuffLen, char* outBuffer)
{
	PrdObject prdObject = prdHandles_[prdHandle];
	int wrx =  prdObject.rng->Write(outBuffLen, outBuffer);
	if (wrx==1 && (outBuffer[0]&0x30)==0x30 && prdObject.prdAcGen==NULL)
    {
		prdObject.prdAcGen = new BpZAnalogFlipMultiPrd(*prdObject.rng, *prdObject.rngAcBuffer);
		prdObject.prdComparator->SetPrdAcGen(prdObject.prdAcGen);
	}
	else
    {
		if (prdObject.prdAcGen != NULL)
        {
			prdObject.prdComparator->SetPrdAcGen(NULL);
			delete prdObject.prdAcGen;
		}
	}

	return wrx;
}
