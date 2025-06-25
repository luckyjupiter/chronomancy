#include "PcqngTargetGen.hpp"
#include "Buffer.hpp"
#include "PcqngCore.hpp"


PcqngTargetGen::PcqngTargetGen(Buffer& inBuffer, size_t targetBuffLen, PcqngCore& pcqngCore)
    : inBuffer_(inBuffer)
    , targetBuffer_(new Buffer(targetBuffLen, false))
    , pcqngCore_(pcqngCore)
    , initCount_(10)
{ }


PcqngTargetGen::~PcqngTargetGen()
{
    delete targetBuffer_;
}


void PcqngTargetGen::SetTargetBufferSize(size_t size)
{
    targetBuffer_->SetSize(size);
    initCount_ = (int)size;
}


uint8_t PcqngTargetGen::GetNextTarget()
{
    std::vector<uint8_t> outBit;
    GenerateTarget();
    targetBuffer_->Read(outBit, 1);

    return outBit[0];
}


uint8_t PcqngTargetGen::GetNextPrecogTarget()
{
    uint8_t outBit;
    uint8_t outWord = 0;

    do
    {
        uint8_t eBits = pcqngCore_.PrecogGetTarget();
        
        for (int i=0; i<7; ++i)
        {
            uint8_t runType   = 0;
            int runLength = 0;
            while (runLength < 5)
            {
                for (int b=0; b<7; ++b)
                {
                    outBit = lfsrCorrector_.Next((eBits>>(6-b))&0x1);
                    if (outBit == runType)
                        ++runLength;
                    else
                    {
                        runType = outBit;
                        runLength = 1;
                    }
                }
            }
            outWord <<= 1;
            outWord  |= outBit&0x1;
        }

        --initCount_;
    } while (initCount_ > 0);
    initCount_ = 1;

    return outWord;
}


void PcqngTargetGen::GenerateTarget()
{
    uint8_t outBit;

    do 
    {
        std::vector<uint8_t> inWord;
        inBuffer_.Read(inWord, 1);
        uint8_t eBits = inWord[0];
        
        uint8_t outWord = 0;
        for (int i=0; i<7; ++i)
        {
            uint8_t runType  = 0;
            int runLength = 0;

            while (runLength < 5)
            {
                for (int b=0; b<7; ++b)
                {
                    outBit = lfsrCorrector_.Next((eBits>>(6-b))&0x1);
                    if (outBit == runType)
                        ++runLength;
                    else
                    {
                        runType = outBit;
                        runLength = 1;
                    }
                }
            }
            outWord <<= 1;
            outWord  |= outBit&0x1;
        }

        targetBuffer_->Write(&outWord, 1);

        --initCount_;
    } while (initCount_ > 0);
    initCount_ = 1;
}


void PcqngTargetGen::ClearBuffers()
{
//    inBuffer_.Clear();
}
