#ifndef INCLUDE_HPP_IBUFFER_
#define INCLUDE_HPP_IBUFFER_


#include <Windows.h>


class IBuffer
{
public:
    IBuffer();
    ~IBuffer();

public:
    void Write(char* buffer, int bufferSize);
    int Read(char* buffer, int bufferSize);
    void Clear();

private:
    char* store_;
    int storeSize_;
    int writeMarker_;
    int readMarker_;

    HANDLE dataInStoreEvent_;
    CRITICAL_SECTION mutex_;
};


#endif // INCLUDE_HPP_IBUFFER_
