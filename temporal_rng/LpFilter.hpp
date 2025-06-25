#ifndef INCLUDE_HPP_LPFILTER_
#define INCLUDE_HPP_LPFILTER_


class LpFilter
{
public:
    void Init(double initVal);
    double Feed(double newVal, double length);
    double GetValue();

private:
    double filteredVal_;
};


#endif // INCLUDE_HPP_LPFILTER_
