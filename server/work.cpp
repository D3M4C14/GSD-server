#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <set>
#include <algorithm>
#include "work.h"
#include "parse_cfg.h"
#include "epoll.h"

using namespace std;

typedef struct
{
    int idx;
    int size;
}set_st;

// 循环消息队列
typedef struct
{
    char* data;
    int size;
    int begin_pos;
    int end_pos;
}cmq_t;

#define MAX_EVENT_NUMBER 1024
static bool work_inited = false;
static launch_cfg_t* server_cfg;
static set<int>* work_fd_sets;
static pthread_t* thread_ids;
static int** pip_fds;
static set_st* set_sortls;
static int* epfds;


static cmq_t* mqs;

static pthread_barrier_t * work_barrier;

static int* listen_sock;

static void* work_func( void* no );


int work_init( launch_cfg_t * const cfg, pthread_t * pids, int * const sock, pthread_barrier_t * const barr )
{
    work_inited = true;
    server_cfg = cfg;
    thread_ids = pids;
    work_barrier = barr;
    listen_sock = sock;

    const int thread_num = cfg->work_thread_num;
    int ret;


    // 分配内存

    epfds = new (std::nothrow) int[ thread_num ];
    if ( epfds == nullptr )
    {
        printf( "new memory error epfds int[%d]\n", thread_num );
        return -1;
    }
    bzero( epfds, sizeof(epfds) );

    set_sortls = new (std::nothrow) set_st[ thread_num ];
    if ( set_sortls == nullptr )
    {
        printf( "new memory error set_sortls set_st[%d]\n", thread_num );
        return -1;
    }

    pip_fds = new (std::nothrow) int*[ thread_num ];
    if ( pip_fds == nullptr )
    {
        printf( "new memory error pip_fds int[%d]\n", thread_num );
        return -1;
    }

    work_fd_sets = new (std::nothrow) set<int>[ thread_num ];
    if ( work_fd_sets == nullptr )
    {
        printf( "new memory error work_fd_sets set[%d]\n", thread_num );
        return -1;
    }

    mqs = new (std::nothrow) cmq_t[thread_num];
    if ( mqs == nullptr )
    {
        printf( "new memory error mqs cmq_t[%d]\n", thread_num );
        clear_work();
        return -1;
    }
    bzero( mqs, sizeof(mqs) );

    for (int i = 0; i < thread_num; ++i)
    {
        mqs[i].data = new (std::nothrow) char[ cfg->work_buffer_size ];
        if ( mqs[i].data == nullptr )
        {
            printf( "new memory error mqs.data[%d]\n", cfg->work_buffer_size );
            clear_work();
            return -1;
        }
        mqs[i].size = cfg->work_buffer_size;
        bzero( mqs[i].data, sizeof(mqs[i].data) );

        pip_fds[i] = new (std::nothrow) int[ 2 ];
        if ( pip_fds[i] == nullptr )
        {
            printf( "new memory error pip_fds int[%d]\n", 2 );
            clear_work();
            return -1;
        }
        bzero( pip_fds[i], sizeof(pip_fds[i]) );

        if( socketpair( PF_UNIX, SOCK_STREAM, 0, pip_fds[i] ) == -1 )
        {
            printf( "socketpair error\n" );
            clear_work();
            return -1;
        }

        epfds[i] = ep_create();
        if ( epfds[i] < 0 )
        {
            printf( "epfds ep_create error\n" );
            clear_work();
            return -1;
        }

    }


    // 创建线程
    int nos[thread_num];
    for (int i = 0; i < thread_num; ++i)
    {
        nos[i]=i;
        printf( "create the %dth thread\n", i );
        ret = pthread_create( &thread_ids[i], nullptr, work_func, &nos[i] );
        if( ret != 0 )
        {
            printf( "very serious error need manual kill process\n" );
            perror( "create work thread\n" );
            break;
        }
    }

    if ( ret != 0 )
    {
        clear_work();
    }

    return 0;
}

int append_work( int num )
{
    if ( !work_inited )
    {
        return -1;
    }

    int fsz,tsz=server_cfg->work_thread_num;
    for (int i = 0; i < tsz; ++i)
    {
        fsz = work_fd_sets[i].size();
        set_sortls[i].idx = i;
        set_sortls[i].size = fsz;
    }

    std::sort( set_sortls, set_sortls+tsz-1, []( set_st a,set_st b ){return a.size < b.size;} );

    int smax = set_sortls[tsz-1].size,ds;
    set_st* s;
    for (int i = 0; i < tsz; ++i)
    {
        s = set_sortls+i;
        ds = smax - s->size;
        if( num >= ds )
        {
            s->size = ds;
            num -= ds;
        }
        else
            s->size = 0;
    }

    if ( num > 0 )
    {
        int n = num/tsz;
        int m = num%tsz;
        if ( n > 0 )
        {
            for (int i = 0; i < tsz; ++i)
            {
                s = set_sortls+i;
                s->size += n;
            }
        }
        if ( m > 0 )
        {
            set_sortls[0].size += m;
        }
    }

    for (int i = 0; i < tsz; ++i)
    {
        s = set_sortls+i;

        if( s->size > 0 )
        {
            send( pip_fds[ s->idx ][1], (char*)&s->size, sizeof(s->size), 0 );
        }
        
    }

    return 0;
}

void clear_work()
{
    const int thread_num = server_cfg->work_thread_num;

    if( work_fd_sets ) delete [] work_fd_sets;

    for (int i = 0; i < thread_num; ++i)
    {
        if( mqs[i].data )
        {
            delete [] mqs[i].data;
        }

        if( pip_fds[i] )
        {
            if( pip_fds[i][0] > 0 ) close( pip_fds[i][0] );
            if( pip_fds[i][1] > 0 ) close( pip_fds[i][1] );

            delete [] pip_fds[i];
        }

        if ( epfds[i] > 0 )
        {
            close( epfds[i] );
        }
    }

    if( mqs ) delete [] mqs;

    if( pip_fds ) delete [] pip_fds;
}

static void* work_func( void* no )
{
    int idx = *(int*) no;
    
    pthread_barrier_wait( work_barrier );

    cmq_t mq = mqs[idx];
    set<int> fd_set = work_fd_sets[idx];
    int sock = *listen_sock;
    int efd = epfds[idx];
    int pip_fd=pip_fds[idx][0];

    int ret = ep_add( efd, pip_fd );
    if ( ret != 0 )
    {
        perror( "pip_fd ep_add" );
        return (void*)-1;
    }

    struct sockaddr_in client;
    socklen_t clen = sizeof( client );

    struct epoll_event evs[ MAX_EVENT_NUMBER ];
    int work_num,work_fd,evn,msg_len;

    while( true )
    {
        evn = ep_wait( efd, evs, MAX_EVENT_NUMBER, 100 );
        if ( evn > 0 )
        {
            for (int i = 0; i < evn; ++i)
            {
                // 接受连接
                if( evs[i].data.fd == pip_fd )
                {
                    recv( pip_fd, (char*)&work_num, sizeof(work_num), 0);
                    for (int wi = 0; wi < work_num; ++wi)
                    {
                        work_fd = accept( sock, (struct sockaddr*)&client, &clen );
                        if ( work_fd > 0 )
                        {
                            fd_set.insert( work_fd );
                            int ret = ep_add( efd, work_fd );
                            if ( ret != 0 )
                            {
                                printf("work_fd(%d) ep_add error set size : %lu\n", idx, fd_set.size() );
                                perror( "work_fd ep_add" );
                                return (void*)-1;
                            }
                        }
                    }
                }
                else
                {
                    // 连接发送消息到达
                    work_fd = evs[i].data.fd;

                    while( true )
                    {
                        if( mq.end_pos == mq.size )
                        {
                            // 消息绕回头部
                            mq.end_pos = 0;
                        }

                        msg_len = recv( work_fd, mq.data+mq.end_pos, mq.size-mq.end_pos, 0 );
                        if( msg_len > 0 )
                        {
                            // 消息穿仓
                            if( mq.end_pos < mq.begin_pos && mq.end_pos + msg_len > mq.begin_pos)
                            {
                                printf( "no.%d circle msg queue overload!!!\n", idx );
                            }

                            mq.end_pos += msg_len;

                        }
                        else
                            break;

                    }

                    printf( "msg:%s\n", mq.data );
                }
            }
        }

        // 消息队列分发给所有连接
        // if( timeok )
        {
            if( mq.begin_pos != mq.end_pos )
            {
                for( auto it=fd_set.begin(); it!=fd_set.end(); it++ )
                {
                    if( mq.begin_pos < mq.end_pos )
                    {
                        send( *it, mq.data+mq.begin_pos, mq.end_pos-mq.begin_pos, 0);
                    }
                    else
                    {
                        send( *it, mq.data+mq.begin_pos, mq.size-mq.begin_pos, 0);
                        send( *it, mq.data, mq.end_pos, 0);
                    }
                }
                mq.begin_pos = mq.end_pos;
            }

        }

    }

    printf( "thread over %d\n", idx );
    return (void*)0;
}