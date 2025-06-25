#ifndef IRNG_INCLUDED_
#define IRNG_INCLUDED_


#include <string>

class Buffer;


class IRng
{
public:
    IRng(Buffer& outBuffer, Buffer& outAcBuffer, std::string rngId);
	~IRng();

public:
    virtual void Start();
    virtual void Stop();
    virtual Buffer& GetOutBuffer();
	virtual Buffer& GetOutAcBuffer();
	virtual Buffer* GetRawBuffer();
    virtual double GetBitRate();
    virtual void ClearBuffer();
    virtual std::string GetId();
	virtual int Write(int outBuffLen, char* outBuffer);

protected:
    Buffer& outBuffer_;
	Buffer& outAcBuffer_;
    double  bitRate_;
    std::string rngId_;

public:
		Buffer* rawBuffer_;
};


#endif // IRNG_INCLUDED_
