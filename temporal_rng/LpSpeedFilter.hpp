#ifndef LPSPEEDFILTER_INCLUDED_
#define LPSPEEDFILTER_INCLUDED_


class LpSpeedFilter
{
public:
    static void Init(double filtConst);

public:
    static float LookUp[(1<<17)];
};


#endif LPSPEEDFILTER_INCLUDED_
