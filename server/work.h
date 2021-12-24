#ifndef WORK_H
#define WORK_H

#include "parse_cfg.h"

int work_init( launch_cfg_t * const cfg, pthread_t * pids, int * const sock, pthread_barrier_t * const barr );

int append_work( int num );

void clear_work();

#endif
