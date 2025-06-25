#include "IRng.hpp"
#include "Buffer.hpp"


IRng::IRng(Buffer& outBuffer, Buffer& outAcBuffer, std::string rngId)
    : outBuffer_(outBuffer)
	, outAcBuffer_(outAcBuffer)
	, rawBuffer_(new Buffer(128, false))
    , bitRate_(0)
    , rngId_(rngId)
{ }

IRng::~IRng()
{
	delete rawBuffer_;
}

void IRng::Start()
{ }


void IRng::Stop()
{ }


Buffer& IRng::GetOutBuffer()
{
    return outBuffer_;
}


Buffer& IRng::GetOutAcBuffer()
{
	return outAcBuffer_;
}


Buffer* IRng::GetRawBuffer()
{
	return rawBuffer_;
}

double IRng::GetBitRate()
{
    return bitRate_;
}

void IRng::ClearBuffer()
{
//    outBuffer_.Clear();
}


std::string IRng::GetId()
{
    return rngId_;
}

int IRng::Write(int outBuffLen, char* outBuffer)
{
	return -1;
}
