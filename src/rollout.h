#ifndef _ROLLOUT_H_
#define _ROLLOUT_H_

#include "buckets.h"
#include "cards.h"

short *ComputeRollout(unsigned int st, double *percentiles,
                      unsigned int num_percentiles, double squashing,
                      bool wins);

float *OppoClusterComputeRollout(unsigned int st, bool wins,
                                 const Buckets *oppo_bucket, int nb_threads);
#endif
