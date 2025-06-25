#ifndef MERSENNETWISTER_INCLUDED_
#define MERSENNETWISTER_INCLUDED_


/* Period parameters */  
#define TWISTER_N 624
#define TWISTER_M 397
#define TWISTER_MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define TWISTER_UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define TWISTER_LOWER_MASK 0x7fffffffUL /* least significant r bits */


class MersenneTwister
{
public:
    MersenneTwister();

public:
    void init_genrand(unsigned long s);
    void init_by_array(unsigned long init_key[], int key_length);
    unsigned long genrand_int32(void);
    long genrand_int31(void);
    double genrand_real1(void);
    double genrand_real2(void);
    double genrand_real3(void);
    double genrand_res53(void);

private:
    unsigned long mt[TWISTER_N]; /* the array for the state vector  */
    int mti; /* mti==N+1 means mt[N] is not initialized */
};


#endif // MERSENNETWISTER_INCLUDED_
