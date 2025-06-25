// © 2012 Psigenics
//
// PrdCore: DLL interface to PrdCore
//
// 2012.09.28    pawilber              


#ifdef PSIGPRD_EXPORTS
#define PSIGPRD_API extern "C" __declspec(dllexport)
#else
#define PSIGPRD_API extern "C" __declspec(dllimport)
#endif


#include <Windows.h>


HMODULE hModule__;


PSIGPRD_API int Open(char* licenseId);
PSIGPRD_API int Close(int prdHandle);
PSIGPRD_API int ListSources(int srcsBuffLen, char* sources);
PSIGPRD_API int SetSource(int prdHandle, char* source);
PSIGPRD_API int GetSource(int prdHandle, int srcBuffLen, char* source);
PSIGPRD_API int SetPrdMode(int prdHandle, int prdMode);
PSIGPRD_API int GetPrdMode(int prdHandle);
PSIGPRD_API int SetPsiMode(int prdHandle, int psiMode);
PSIGPRD_API int GetPsiMode(int prdHandle);
PSIGPRD_API int GetPrdData(int prdHandle, int count, double* psiData, double* targetData);
PSIGPRD_API int GetRawData(int prdHandle, int count, char* rawData);
PSIGPRD_API int Write(int prdHandle, int outBuffLen, char* outBuffer);
