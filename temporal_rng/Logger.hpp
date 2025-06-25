#ifndef LOGGER_INCLUDED_
#define LOGGER_INCLUDED_


#include "stdint.h"
#include "LfsrCorrector.hpp"
#include <fstream>


class Logger
{
public:
    Logger(std::string userInterface, std::string userId, std::string rngType, std::string processingType);
    ~Logger();

public:
    void LogPoint(double utcTime, uint8_t hitMiss, uint8_t matchIntention, uint8_t psiMode, int streamCount, double* streamData);
    static double TimeNowUtc();

private:
    std::ofstream logFile;
    LfsrCorrector sigLfsr;
};


#endif // LOGGER_INCLUDED_
