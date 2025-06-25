#ifndef INCLUDE_HPP_LFSRCORRECTOR_
#define INCLUDE_HPP_LFSRCORRECTOR_


class LfsrCorrector
{
public:
    LfsrCorrector();

public:
    char Next(char inBit);

private:
    unsigned __int64 lfsr_;
};


#endif // INCLUDE_HPP_LFSRCORRECTOR_
