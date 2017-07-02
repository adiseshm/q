
#ifndef __SPSC_T_H__
#define __SPSC_T_H__

#include <unistd.h>
#include <time.h>

inline static unsigned long long gt_ns(void)
{
   struct timespec ct;

   clock_gettime(CLOCK_REALTIME, &ct);
   return ct.tv_sec * 1000000000ULL + ct.tv_nsec;
}

inline static unsigned long long dt_ns(unsigned long long start)
{
   struct timespec ct;

   clock_gettime(CLOCK_REALTIME, &ct);
   return (ct.tv_sec * 1000000000ULL + ct.tv_nsec) - start;
}

#endif
