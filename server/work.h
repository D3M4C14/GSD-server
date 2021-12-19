#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "parse_cfg.h"


int work_init( launch_cfg_t* cfg );

int append_work( int num);


#endif
