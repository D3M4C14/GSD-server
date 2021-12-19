#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "parse_cfg.h"
#include "work.h"

launch_cfg_t server_cfg;


int log_init();
int signal_init();
int listen_init();


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
    if( ret < 0 )
    {
        printf( "load launch_config_file:'%s' error\n", file_name );
        return 1;
    }

    print_launch_cfg_info( &server_cfg );

    ret = log_init();
    if( ret == -1 )
    {
        printf( "log init error\n" );
        return 1;
    }

    ret = signal_init();
    if( ret == -1 )
    {
        perror( "signal init error" );
        return 1;
    }

    ret = work_init( &server_cfg );
    if( ret == -1 )
    {
        perror( "listen init error" );
        return 1;
    }

    // 作为守护进程后台运行
    if( server_cfg.daemon )
    {
        ret = daemon( 1, 0 );
        if( ret == -1 )
        {
            perror( "daemon" );
            return 1;
        }
    }

    ret = listen_init( &server_cfg );
    if( ret == -1 )
    {
        perror( "listen init error" );
        return 1;
    }

    return 0;
}


int log_init(){
    
    // dup(0);

    return 0;
}


static void * signal_thread( void *arg )
{
    sigset_t *set = (sigset_t *) arg;
    int ret,sig;

    while( true )
    {
        ret = sigwait( set, &sig );
        if( ret == 0 )
        {
            switch( sig )
            {
                case SIGTERM:
                case SIGINT:
                {
                    break;
                }
                default:
                {
                    break;
                }
            }
        }

    }

}

int signal_init()
{
    sigset_t set;
    pthread_t pid;
    int ret;

    //忽略 一些信号
    signal( SIGPIPE, SIG_IGN );

    sigemptyset( &set );
    sigaddset( &set, SIGINT );
    sigaddset( &set, SIGTERM );

    // 主进程屏蔽信号
    ret = pthread_sigmask(SIG_BLOCK, &set, nullptr);
    if( ret != 0 )
    {
        perror( "pthread_sigmask" );
        return -1;
    }

    // 专门开一个线程来处理进程的所有信号
    ret = pthread_create( &pid, nullptr, &signal_thread, (void *) &set );
    if( ret != 0 )
    {
        perror( "signal pthread create" );
        return -1;
    }

    return 0;
}


int listen_init( launch_cfg_t* cfg )
{

    int backlog = atoi( argv[3] );
    
    int sock = socket( PF_INET, SOCK_STREAM, 0 );


    // 地址重用 端口状态位于 TIME_WAIT 可以重用端口
    int one = 1;
    setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one) );

    // 设置写缓冲区大小
    int sbsz = 4096;
    int slen = sizeof( sbsz );
    setsockopt( sock, SOL_SOCKET, SO_SNDBUF, &sbsz, sizeof( sbsz ) );
    getsockopt( sock, SOL_SOCKET, SO_SNDBUF, &sbsz, ( socklen_t* )&slen );
    printf( "* the send buffer size after setting is %d\n", sbsz );

    // 设置读缓冲区大小
    int rbsz = 4096;
    int rlen = sizeof( rbsz );
    setsockopt( sock, SOL_SOCKET, SO_RCVBUF, &rbsz, sizeof( rbsz ) );
    getsockopt( sock, SOL_SOCKET, SO_RCVBUF, &rbsz, ( socklen_t* )&rlen );
    printf( "* the receive buffer size after settting is %d\n", rbsz );

    // 设置地址和端口
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, "127.0.0.1", &address.sin_addr );
    address.sin_port = htons( cfg->prot );

    // 绑定
    int ret = bind( sock, (struct sockaddr*)&address, sizeof( address ) );
    assert( ret != -1 );

    // 监听
    ret = listen( sock, backlog );

    assert( ret != -1 );
    

    epoll_event events[ MAX_EVENT_NUMBER ];
    epollfd = epoll_create( 5 );
    assert( epollfd != -1 );
    addfd( epollfd, sock );


    while( !stop_server )
    {
        int number = epoll_wait( epollfd, events, MAX_EVENT_NUMBER, -1 );
        if ( ( number < 0 ) && ( errno != EINTR ) )
        {
            printf( "epoll failure\n" );
            break;
        }
        append_work(number);
        sched_yield();
    }

    close( sock );

    return 0;
}
