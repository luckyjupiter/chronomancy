#include "Logger.hpp"

#include <Windows.h>
#include <shlobj.h>
#include <strstream>
#include <string>


Logger::Logger(std::string userInterface, std::string userId, std::string rngType, std::string processingType)
{
    std::strstream logFilePathA;
    std::strstream logFilePathB;
    std::strstream logFileName;

    // Documents path
    char homePath[MAX_PATH+1];
    SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, homePath);
    // path to log file
    logFilePathA << homePath << "\\" << "Psi Data" << '\0';
    CreateDirectoryA(logFilePathA.str(), NULL);
    logFilePathB << logFilePathA.str() << "\\" << userId.c_str() << "\\" << '\0';
    CreateDirectoryA(logFilePathB.str(), NULL);
    std::string strttt(logFilePathB.str());

    // file name
    double timeNowUtc = TimeNowUtc();
    logFileName << logFilePathB.str() << userId.c_str() << "_" << (uint32_t)timeNowUtc << ".csv" << '\0';

    logFile.open(logFileName.str());

    char timeStr[16];
    sprintf_s(timeStr, 15, "%10.3f", timeNowUtc);
    logFile << timeStr << ',' << userId.c_str() << ',' << userInterface.c_str() << ',' << rngType.c_str() << ',' << processingType.c_str() << '\n';
    logFile.flush();

    // initialize hash signature with userId
    for (unsigned i=0; i<userId.length(); ++i)
        sigLfsr.Next(userId.at(i)&0x1);
}


Logger::~Logger()
{
    logFile.close();
}


void Logger::LogPoint(double utcTime, uint8_t hitMiss, uint8_t matchIntention, uint8_t psiMode, int streamCount, double* streamData)
{
    char checkSig = 0;
    char timeStr[16];
    sprintf_s(timeStr, 15, "%10.3f", utcTime);
    for (int i=0; i<14; ++i)
    {
        checkSig <<= 1;
        checkSig |= sigLfsr.Next(timeStr[i]&0x1);
    }
    logFile << timeStr << ',' << (int)hitMiss << ',' << (int)matchIntention << ',' << (int)psiMode;
    checkSig <<= 1;
    checkSig |= sigLfsr.Next(0);
    checkSig <<= 1;
    checkSig |= sigLfsr.Next(hitMiss&0x1);
    checkSig <<= 1;
    checkSig |= sigLfsr.Next(0);
    checkSig <<= 1;
    checkSig |= sigLfsr.Next(matchIntention&0x1);
    checkSig <<= 1;
    checkSig |= sigLfsr.Next(0);
    checkSig <<= 1;
    checkSig |= sigLfsr.Next(psiMode&0x1);
    char streamDat[10];
    for (int i=0; i<streamCount; ++i)
    {
        sprintf_s(streamDat, 10, "%+1.2f", streamData[i]);
        logFile << ',' << streamDat;
        checkSig <<= 1;
        checkSig |= sigLfsr.Next(0);
        for (int i=0; i<5; ++i)
        {
            checkSig <<= 1;
            checkSig |= sigLfsr.Next(streamDat[i]&0x1);
        }
    }
    checkSig &= 0x3F;
    checkSig += 0x30;
    logFile << ',' << checkSig;
    logFile << '\n';
    logFile.flush();
}


double Logger::TimeNowUtc()
{
    FILETIME fileTime;
    unsigned __int64 uint64Time;

    GetSystemTimeAsFileTime(&fileTime);

    uint64Time = fileTime.dwHighDateTime;
    uint64Time <<= 32;
    uint64Time += fileTime.dwLowDateTime;

    return ((double)((__int64)(uint64Time/10000))/1000. - 9435484800.000); // seconds from 1601 to 1900    
}
