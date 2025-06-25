#include "LpSpeedFilter.hpp"
#include "LpFilter.hpp"

float LpSpeedFilter::LookUp[(1<<17)];

void LpSpeedFilter::Init(double filtConst)
{
    LpFilter lpFilter;

    for (int i=0; i<(1<<17); ++i)
    {
        lpFilter.Init(0);
        for (int j=16; j>=0; --j)
            lpFilter.Feed(((i>>j)&0x1)*2-1, filtConst);

        LookUp[i] = (float)lpFilter.GetValue();
    }
}
