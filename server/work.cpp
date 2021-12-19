
#include <set>
#include <iostream>
#include "work.h"
#include "parse_cfg.h"

using namespace std;

static bool work_inited = false;
static launch_cfg_t* server_cfg;
static set<int>* work_fd_sets;
static pthread_t* thread_ids;
static int* pip_fds;
static epoll* epolls;

static char** msg_datas;
static int* msg_begin_pos;
static int* msg_end_pos;

static void* work_func( void* no );


int work_init( launch_cfg_t* cfg )
{
    work_inited = true;
    server_cfg = cfg;
    
    msg_datas = new char*[cfg->work_thread_num];

    for (int i = 0; i < cfg->work_thread_num; ++i)
    {
        
        msg_datas[i] = new char[cfg->size];

        printf( "create the %dth thread\n", i );
        if( pthread_create( thread_ids + i, NULL, work_func, &i ) != 0 )
        {
            delete [] thread_ids;
            throw std::exception();
        }
        if( pthread_detach( thread_ids[i] ) )
        {
            delete [] thread_ids;
            throw std::exception();
        }
    }

    return 0;
}

int append_work( int num)
{
    if ( !work_inited )
    {
        return -1;
    }

    set<int> s;
    int idx;
    for (int i = 0; i < cfg->work_thread_num; ++i)
    {
        work_fd_sets[i].size()

        
    }

    
    send(pip_fds[idx],num);


    return 0;
}


static void* work_func( void* no ){

    int idx = *(int*) no;
    set<int> fset = work_fd_sets[idx];
    char* msg_data = msg_datas[idx];


    int begin_pos = msg_begin_pos[idx];
    int end_pos = msg_end_pos[idx];

    while( true )
    {

        time*0.5
        epoll_wait(time);


        ret = recv(pip_fds[idx])
        if( ret > 0 )
        {

            fd = accept()
            fset.insert (fd);
        }

        if(epoll_in)
        {
            ret = recv(fd)
            write(msg_data,end_pos,ret);

            if(ret = 0)
            {
                fset.erase(fd);
            }
            
        }


        if( timeok )
        {
            if (begin_pos!=end_pos)
            {
                for(it=fset.begin ();it!=fset.end ();it++)
                {
                    if( begin_pos < end_pos )
                    {
                        send(*it,msg_data,begin_pos,end_pos);
                    }
                    else
                    {
                        send(*it,msg_data,begin_pos,max);
                        send(*it,msg_data,0,end_pos);
                    }
                }
            }

        }

    }

}