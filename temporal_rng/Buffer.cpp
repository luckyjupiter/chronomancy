#include "Buffer.hpp"


Buffer::Buffer(size_t bufferSize, bool blockOnFull)
    : bufferMax_(bufferSize)
    , readBufferExtension_(0)
    , blockOnFull_(blockOnFull)
{
    dataInStoreEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
    InitializeCriticalSection(&mutex_);
}


Buffer::~Buffer()
{
    CloseHandle(dataInStoreEvent_);
    dataInStoreEvent_ = 0;
    DeleteCriticalSection(&mutex_);
}


size_t Buffer::Write(std::vector<uint8_t> const& writeBuffer)
{
    EnterCriticalSection(&mutex_);

    size_t transferSize = writeBuffer.size();
    if (transferSize > bufferMax_)
        transferSize = bufferMax_;

    size_t freeSize = bufferMax_ - buffer_.size();
    if (transferSize > freeSize)
    {
        if (blockOnFull_ == true)
        {
            if (freeSize < 0)
                freeSize = 0;
            transferSize = freeSize;
        }
        else
        {
            std::deque<uint8_t>::iterator beginIter = buffer_.begin();
            buffer_.erase(beginIter, beginIter+(transferSize-freeSize));
        }
    }

    buffer_.insert(buffer_.end(), writeBuffer.begin(), writeBuffer.begin()+transferSize);

    SetEvent(dataInStoreEvent_);
    LeaveCriticalSection(&mutex_);

    return transferSize;
}


size_t Buffer::Write(uint8_t const* writeBuffer, size_t writeCount)
{
    std::vector<uint8_t> writeVector(writeBuffer, writeBuffer+writeCount);
    return Write(writeVector);
}


size_t Buffer::Read(std::vector<uint8_t>& readBuffer, size_t readCount)
{
    size_t bytesLeft = readCount;
    size_t transferBlockSize = 0;
    size_t bytesTransferred = 0;

    EnterCriticalSection(&mutex_);

    bool writeBlockFlag = blockOnFull_;
    blockOnFull_ = true;
    bufferMax_ += readBufferExtension_;

    readBuffer.clear();
    while (bytesLeft > 0)
    {
        if (buffer_.size() == 0)
        {
            LeaveCriticalSection(&mutex_);
            if (WaitForSingleObject(dataInStoreEvent_, INFINITE) != WAIT_OBJECT_0)
                return (readCount - bytesLeft);
            EnterCriticalSection(&mutex_);
        }

        transferBlockSize = (bytesLeft < buffer_.size())? bytesLeft : buffer_.size();

        readBuffer.insert(readBuffer.end(), buffer_.begin(), buffer_.begin()+transferBlockSize);
        buffer_.erase(buffer_.begin(), buffer_.begin()+transferBlockSize);
        bytesLeft -= transferBlockSize;
    }

    bufferMax_ -= readBufferExtension_;
    blockOnFull_ = writeBlockFlag;

    LeaveCriticalSection(&mutex_);

    return readCount;
}


void Buffer::Clear()
{
    EnterCriticalSection(&mutex_);
    buffer_.clear();
    LeaveCriticalSection(&mutex_);
}


void Buffer::SetBlockOnFull(bool blockOnFull)
{
    EnterCriticalSection(&mutex_);
    blockOnFull_ = blockOnFull;
    LeaveCriticalSection(&mutex_);
}


void Buffer::SetSize(size_t size)
{
    EnterCriticalSection(&mutex_);
    if (buffer_.size() > size)
        buffer_.resize(size);
    bufferMax_ = size;
    readBufferExtension_ = size;
    LeaveCriticalSection(&mutex_);
}


bool Buffer::IsFull()
{
    return (bufferMax_ >= buffer_.size());
}
