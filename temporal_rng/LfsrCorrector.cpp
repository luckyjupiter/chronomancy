#include "LfsrCorrector.hpp"


LfsrCorrector::LfsrCorrector()
    : lfsr_(0xAAAAAAAAAAAAAAAA)
{}


char LfsrCorrector::Next(char inBit)
{
//    //                           TAB 11        TAB 7         TAB 17      TAB 13
//    unsigned __int64 outBit = ((lfsr_>>37) ^ (lfsr_>>30) ^ (lfsr_>>13) ^ lfsr_)&0x1;
    //                           TAB 15        TAB 11        TAB 7         TAB 17      TAB 13
    unsigned __int64 outBit = ((lfsr_>>48) ^ (lfsr_>>37) ^ (lfsr_>>30) ^ (lfsr_>>13) ^ lfsr_)&0x1;
    lfsr_ >>= 1;
    lfsr_ |= (outBit^inBit) << 62;

    return (char)outBit;
}
