// Tasofro Mersenne Twister implementation
// Stolen from Brightmoon sources...

#ifndef MT_HPP
#define MT_HPP

typedef long int32_t;
typedef unsigned short uint16_t;
typedef unsigned char   uint8_t;

class RNG_MT
{
private:
  enum {
    N = 624,
    M = 397,
    MATRIX_A = 0x9908b0dfUL,
    UPPER_MASK = 0x80000000UL,
    LOWER_MASK = 0x7FFFFFFFUL
  };

private:
  ulong mt[N];
  long mti;

public:
  explicit RNG_MT(ulong s)
  {
    init(s);
  }

  void init(ulong s)
  {
    mt[0] = s;
    for(mti = 1; mti < N; ++mti) {
      mt[mti] =
        (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti);
    }
  }

  ulong next_int32()
  {
    ulong y;
    static const ulong mag01[2]={0x0UL, MATRIX_A};
    if (mti >= N) {
      int kk;
      if (mti == N+1)
          init(5489UL);

      for (kk=0;kk<N-M;kk++) {
        y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
        mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1UL];
      }
      for (;kk<N-1;kk++) {
        y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
        mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
      }
      y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
      mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];

      mti = 0;
    }
  
    y = mt[mti++];

    /* Tempering */
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
  }
};

#endif
