#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "epoll.h"
#include "parse_cfg.h"
#include "work.h"

#define MAX_EVENT_NUMBER 1024

// 服务器状态 0-正常运行 1-结束
static volatile int server_state = 0;

launch_cfg_t server_cfg;

// 所有线程的id列表
static pthread_t * tids;

// 监听套接字
static int listen_sock;

// 工作线程开始障碍 主线程开始监听后,工作线程才开始运行
static pthread_barrier_t work_barrier;

int log_init( launch_cfg_t * const cfg );
int signal_init( pthread_t * pid );
int listen_init( launch_cfg_t * const cfg );
void clear_threads( const int thread_num );

int main( int argc, char* argv[] )
{
    if( argc < 2 )
    {
        printf( "usage: %s launch_config_file\n", basename( argv[0] ) );
        return 1;
    }

    int ret;

    const char* file_name = argv[1];
    ret = get_launch_cfg( file_name, &server_cfg );
    if( ret != 0 )
    {
        fprintf( stderr, "load launch_config_file:'%s' error\n", file_name );
        return 1;
    }

    print_launch_cfg_info( &server_cfg );

    // 作为守护进程后台运行 (注意,这里会切换进程)
    if( server_cfg.daemon )
    {
        ret = daemon( 1, 0 );
        if( ret == -1 )
        {
            perror( "daemon" );
            return 1;
        }
    }

    ret = log_init( &server_cfg );
    if( ret != 0 )
    {
        fprintf( stderr, "log init error\n" );
        return 1;
    }

    const int thread_num = server_cfg.work_thread_num + 1;
    tids = new pthread_t[ thread_num ];
    bzero( tids, sizeof(tids) );

    pthread_barrier_init( &work_barrier, nullptr, thread_num );

    ret = signal_init( &tids[ thread_num-1 ] );
    if( ret != 0 )
    {
        fprintf( stderr, "signal init error" );
        clear_threads( thread_num );
        return 1;
    }

    ret = work_init( &server_cfg, tids, &listen_sock, &work_barrier );
    if( ret != 0 )
    {
        fprintf( stderr, "listen init error" );
        clear_threads( thread_num );
        return 1;
    }

    ret = listen_init( &server_cfg );
    if( ret != 0 )
    {
        perror( "listen init error" );
        pthread_barrier_wait( &work_barrier );
        clear_threads( thread_num );
        return 1;
    }

    clear_threads( thread_num );

    return 0;
}

void clear_threads( const int thread_num )
{
    for (int i = 0; i < thread_num; ++i)
    {
        if( tids[i] > 0 )
        {
            pthread_cancel( tids[i] );
            pthread_join( tids[i], nullptr );
        }
    }

    delete [] tids;

    pthread_barrier_destroy( &work_barrier );

    clear_work();

    printf("server stop");
}

int log_init( launch_cfg_t * const cfg )
{

    if ( cfg->daemon && cfg->log_file[0] != '\0' )
    {
        int log_fd = open( cfg->log_file, O_CREAT | O_WRONLY | O_TRUNC, 0666 );
        if ( log_fd < 0 )
        {
            fprintf( stderr, "open log file : '%s'\n", cfg->log_file );
            perror( "open log file" );
            return -1;
        }

        dup2(log_fd, STDOUT_FILENO);
        dup2(log_fd, STDERR_FILENO);

        // pid
        printf( "server pid : %d\n", getpid() );

        // 后台时将启动配置信息再次输出到日志文件开头
        print_launch_cfg_info( cfg );
    }

    return 0;
}


static void * signal_thread( void * arg )
{
    sigset_t *sigset = (sigset_t *) arg;
    int sig;

    while( server_state == 0 )
    {
        timespec t = {0,1000};
        siginfo_t sinf;
        sig = sigtimedwait( sigset, &sinf, &t);
        if( sig < 0 )
        {
            if( errno != EAGAIN )
            {
                perror( "sigtimedwait" );
            }
            
            // 查看被挂起的信号 不知为何有时候就是无法捕获被挂起的信号,但是查看挂起列表中又有
            sigset_t pset;
            sigpending( &pset );
            if( sigismember( &pset, SIGTERM ) > 0 )
            {
                sig = SIGTERM;
            }
            else
            {
                sleep(1);
                continue;
            }

        }

        switch( sig )
        {
            case SIGTERM:
            case SIGINT:
            {
                server_state = 1;
                printf( "got exit sig server exit.\n" );
                break;
            }
            default:
            {
                break;
            }
        }
    }

    return nullptr;
}

int signal_init( pthread_t * pid )
{
    sigset_t sigset;
    int ret;

    //忽略 一些信号
    signal( SIGPIPE, SIG_IGN );

    sigemptyset( &sigset );
    sigaddset( &sigset, SIGINT );
    sigaddset( &sigset, SIGTERM );

    // 主进程屏蔽信号
    ret = pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
    if( ret != 0 )
    {
        perror( "pthread_sigmask" );
        return -1;
    }

    // 专门开一个线程来处理进程的所有信号
    ret = pthread_create( pid, nullptr, &signal_thread, (void *) &sigset );
    if( ret != 0 )
    {
        perror( "signal pthread create" );
        return -2;
    }

    return 0;
}


int listen_init( launch_cfg_t * const cfg )
{
    int ret;

    listen_sock = socket( PF_INET, SOCK_STREAM, 0 );

    // 地址重用 端口状态位于 TIME_WAIT 可以重用端口
    int one = 1;
    setsockopt( listen_sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one) );

    // 设置写缓冲区大小
    int sbsz = cfg->send_buffer_size;
    setsockopt( listen_sock, SOL_SOCKET, SO_SNDBUF, &sbsz, sizeof( sbsz ) );

    // 设置读缓冲区大小
    int rbsz = cfg->recv_buffer_size;
    setsockopt( listen_sock, SOL_SOCKET, SO_RCVBUF, &rbsz, sizeof( rbsz ) );

    // 设置地址和端口
    char ip[] = "127.0.0.1";
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( cfg->prot );

    // 绑定
    ret = bind( listen_sock, (struct sockaddr*)&address, sizeof( address ) );
    if( ret != 0 )
    {
        fprintf( stderr, "bind %s:%d error\n", ip, cfg->prot );
        perror( "bind" );
        return -1;
    }

    // 监听
    ret = listen( listen_sock, cfg->backlog );
    if( ret != 0 )
    {
        fprintf( stderr, "listen %s:%d error\n", ip, cfg->prot );
        perror( "listen" );
        return -1;
    }

    int efd = ep_create();
    if ( efd < 0 )
    {
        perror( "ep_create" );
        return -1;
    }

    ret = ep_add( efd, listen_sock, nullptr );
    if ( ret != 0 )
    {
        perror( "ep_add" );
        return -1;
    }

    struct event evs[ MAX_EVENT_NUMBER ];
    
    pthread_barrier_wait( &work_barrier );

    int evn;
    while( server_state == 0 )
    {
        evn = ep_wait( efd, evs, MAX_EVENT_NUMBER, 1000 );
        if ( evn > 0 )
        {
            append_work( evn );
        }
        sched_yield();
    }

    close( listen_sock );
    close( efd );

    return 0;
}
