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

#define MAX_EVENT_NUMBER 1024
static bool work_inited = false;
static launch_cfg_t* server_cfg;
static set<int>* work_fd_sets;
static pthread_t* thread_ids;
static int** pip_fds;
static set_st* set_sortls;
static int* epfds;


static char** msg_datas;
// static int* msg_begin_pos;
// static int* msg_end_pos;

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

    msg_datas = new (std::nothrow) char*[thread_num];
    if ( msg_datas == nullptr )
    {
        printf( "new memory error msg_datas char*[%d]\n", thread_num );
        clear_work();
        return -1;
    }
    bzero( msg_datas, sizeof(msg_datas) );

    for (int i = 0; i < thread_num; ++i)
    {
        msg_datas[i] = new (std::nothrow) char[ cfg->work_buffer_size ];
        if ( msg_datas[i] == nullptr )
        {
            printf( "new memory error msg_datas char[%d]\n", cfg->work_buffer_size );
            clear_work();
            return -1;
        }
        bzero( msg_datas[i], sizeof(msg_datas[i]) );

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
        if( msg_datas[i] )
        {
            delete [] msg_datas[i];
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

    if( msg_datas ) delete [] msg_datas;

    if( pip_fds ) delete [] pip_fds;
}


static void* work_func( void* no )
{
    int idx = *(int*) no;
    
    pthread_barrier_wait( work_barrier );

    char* msg_data = msg_datas[idx];
    set<int> fd_set = work_fd_sets[idx];
    int sock = *listen_sock;
    int efd = epfds[idx];
    int pip_fd=pip_fds[idx][0];

    int ret = ep_add( efd, pip_fd, (void*)&pip_fd );
    if ( ret != 0 )
    {
        perror( "pip_fd ep_add" );
        return (void*)-1;
    }


    struct sockaddr_in client;
    socklen_t clen = sizeof( client );

    struct event evs[ MAX_EVENT_NUMBER ];
    int work_num,work_fd,evn;
    pair< set<int>::iterator, bool > set_ins;

    while( true )
    {
        evn = ep_wait( efd, evs, MAX_EVENT_NUMBER, 100 );
        if ( evn > 0 )
        {printf("zzzzzzzzzzaaa\n");
            for (int i = 0; i < evn; ++i)
            {
                // 接受连接
                if( evs[i].ud && *(int*)evs[i].ud == pip_fd )
                {
                    recv( pip_fd, (char*)&work_num, sizeof(work_num), 0);
                    for (int wi = 0; i < work_num; ++wi)
                    {
                        work_fd = accept( sock, (struct sockaddr*)&client, &clen );
                        set_ins = fd_set.insert( work_fd );
                        printf("aaaaaaa@ %d %d\n",work_fd,*set_ins.first);
                        int ret = ep_add( efd, work_fd, (void*)&*set_ins.first );
                        if ( ret != 0 )
                        {
                            printf("work_fd(%d) ep_add error set size : %lu\n", idx, fd_set.size() );
                            perror( "work_fd ep_add" );
                            return (void*)-1;
                        }
                    }
                }
                else
                {printf("zzzzzzzzzz\n");
                    // 连接发送消息到达
                    work_fd = *(int*)evs[i].ud;
                    recv( work_fd, msg_data, server_cfg->work_buffer_size, 0);
                    printf( "msg:%s\n", msg_data );
                }
            }
        }

        // 消息队列分发给所有连接

        // if( timeok )
        // {
        //     if (begin_pos!=end_pos)
        //     {
        //         for(it=fset.begin ();it!=fset.end ();it++)
        //         {
        //             if( begin_pos < end_pos )
        //             {
        //                 send(*it,msg_data,begin_pos,end_pos);
        //             }
        //             else
        //             {
        //                 send(*it,msg_data,begin_pos,max);
        //                 send(*it,msg_data,0,end_pos);
        //             }
        //         }
        //     }

        // }

    }

}