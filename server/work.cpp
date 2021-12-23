#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <set>
#include "work.h"
#include "parse_cfg.h"
#include "epoll.h"

using namespace std;

static bool work_inited = false;
static launch_cfg_t* server_cfg;
// static set<int>* work_fd_sets;
static pthread_t* thread_ids;
// static int* pip_fds;
// static epoll* epolls;

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
    msg_datas = new (std::nothrow) char*[thread_num];
    if ( msg_datas == nullptr )
    {
        printf( "new memory error char*[%d]\n", thread_num );
        return -1;
    }
    bzero( msg_datas, sizeof(msg_datas) );

    for (int i = 0; i < thread_num; ++i)
    {
        msg_datas[i] = new (std::nothrow) char[ cfg->work_buffer_size ];
        if ( msg_datas[i] == nullptr )
        {
            printf( "new memory error char[%d]\n", cfg->work_buffer_size );
            return -1;
        }
        bzero( msg_datas[i], sizeof(msg_datas[i]) );
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
        for (int i = 0; i < thread_num; ++i)
        {
            if( msg_datas[i] )
            {
                delete [] msg_datas[i];
            }
            else
                break;
        }
        delete [] msg_datas;

        return -1;
    }

    return 0;
}

int append_work( int num )
{
    if ( !work_inited )
    {
        return -1;
    }

    // set<int> s;
    // int idx;
    // for (int i = 0; i < cfg->work_thread_num; ++i)
    // {
    //     work_fd_sets[i].size()

        
    // }

    
    // send(pip_fds[idx],num);


    return 0;
}



static void* work_func( void* no )
{
    int idx = *(int*) no;
    
    pthread_barrier_wait( work_barrier );

    int sock = *listen_sock;

    while( true )
    {
        printf( "%d\n", idx );
        sleep(1);
        pthread_testcancel();
    }
    // set<int> fset = work_fd_sets[idx];
    // char* msg_data = msg_datas[idx];


    // int begin_pos = msg_begin_pos[idx];
    // int end_pos = msg_end_pos[idx];

    // while( true )
    // {

    //     time*0.5
    //     epoll_wait(time);


    //     ret = recv(pip_fds[idx])
    //     if( ret > 0 )
    //     {

    //         fd = accept()
    //         fset.insert (fd);
    //     }

    //     if(epoll_in)
    //     {
    //         ret = recv(fd)
    //         write(msg_data,end_pos,ret);

    //         if(ret = 0)
    //         {
    //             fset.erase(fd);
    //         }
            
    //     }


    //     if( timeok )
    //     {
    //         if (begin_pos!=end_pos)
    //         {
    //             for(it=fset.begin ();it!=fset.end ();it++)
    //             {
    //                 if( begin_pos < end_pos )
    //                 {
    //                     send(*it,msg_data,begin_pos,end_pos);
    //                 }
    //                 else
    //                 {
    //                     send(*it,msg_data,begin_pos,max);
    //                     send(*it,msg_data,0,end_pos);
    //                 }
    //             }
    //         }

    //     }

    // }

}