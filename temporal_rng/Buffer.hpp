#ifndef BUFFER_INCLUDED_
#define BUFFER_INCLUDED_


#include "stdint.h"

#include <Windows.h>
#include <deque>
#include <vector>


class Buffer
{
public:
    Buffer(size_t bufferSize, bool blockOnFull);
    ~Buffer();

public:
    size_t Write(std::vector<uint8_t> const& writeBuffer);
    size_t Write(uint8_t const* writeBuffer, size_t writeCount);
    size_t Read(std::vector<uint8_t>& buffer, size_t readCount);
    void Clear();
    void SetBlockOnFull(bool blockOnFull);
    void SetSize(size_t size);
    bool IsFull();

    std::deque<uint8_t> buffer_;
private:    size_t bufferMax_;
    size_t readBufferExtension_;
    bool blockOnFull_;
    HANDLE dataInStoreEvent_;
    CRITICAL_SECTION mutex_;
};


#endif // BUFFER_INCLUDED_
