#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <assert.h>
#include <set>
#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

#define CLIENT_MAX_NUM 2

#define MSG_PER_SECOND 3


set<int> client_fd_set;

bool run = true;
const char* ip;
int port;



int add_client()
{
    int sock = socket( PF_INET, SOCK_STREAM, 0 );
    if( sock < 0 )
    {
        perror( "socket" );
        return 1;
    }
    
    // 设置写缓冲区大小
    int sbsz = 64;
    int slen = sizeof(sbsz);
    setsockopt( sock, SOL_SOCKET, SO_SNDBUF, &sbsz, sizeof(sbsz) );
    getsockopt( sock, SOL_SOCKET, SO_SNDBUF, &sbsz, (socklen_t*)&slen );
    // printf( "* the send buffer size after setting is %d\n", sbsz );

    // 设置读缓冲区大小
    int rbsz = 256;
    int rlen = sizeof(rbsz);
    setsockopt( sock, SOL_SOCKET, SO_RCVBUF, &rbsz, sizeof(rbsz) );
    getsockopt( sock, SOL_SOCKET, SO_RCVBUF, &rbsz, (socklen_t*)&rlen );
    // printf( "* the receive buffer size after settting is %d\n", rbsz );

    // 设置地址和端口
    struct sockaddr_in address;
    bzero( &address, sizeof(address) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );

    // 连接
    if( connect( sock, (struct sockaddr*)&address, sizeof(address) ) < 0)
    {
        perror( "connect" );
        return 1;
    }

    client_fd_set.insert( sock );

    return 0;
}

void* recv_thread_func( void* arg )
{
    const int bsz = 4096;
    char buffer[ bsz ];
    int len;

    while( run )
    {
        for( auto it=client_fd_set.begin(); it!=client_fd_set.end(); ++it )
        {
            memset( buffer, '\0', bsz );
            len = recv( *it, buffer, bsz-1, 0 );
            if ( len > 0 )
            {
                printf( "reccv msg(%d) : '%s'\n",len, buffer );
            }
        }
        usleep(1000);
    }

    return nullptr;
}

void* send_thread_func( void* arg )
{
    const int bsz = 4096;
    char buffer[ bsz ];
    int t,r;

    while( run )
    {
        for( auto it=client_fd_set.begin(); it!=client_fd_set.end(); ++it )
        {
            r = rand() % 10;
            if ( r > 5 )
            {
                if ( r > 6 )
                {
                    sprintf( buffer, "[msg:%c%d]",'a'+(char)(rand() % 26), r );
                }
                else if ( r > 7 )
                {
                    sprintf( buffer, "[msg:%c%c%d]",'a'+(char)(rand() % 26),'a'+(char)(rand() % 26), r );
                }
                else
                {
                    sprintf( buffer, "[msg:%d]", r );
                }
                printf( "send:'%s'\n", buffer );
                send( *it, buffer, bsz, 0 );
            }
        }

        t = MSG_PER_SECOND * 100000 * ( rand() % 10 );
        usleep( t );
    }

    return nullptr;
}

int main( int argc, char* argv[] )
{
    if( argc <= 2 )
    {
        printf( "arg: ip port \n" );
        return 1;
    }

    ip = argv[1];
    port = atoi( argv[2] );
    
    printf( "* runing %s : %s:%d \n", basename(argv[0]), ip, port );

    srand( (unsigned)time( nullptr ) );

    pthread_t rtid;
    int ret = pthread_create( &rtid, nullptr, recv_thread_func, nullptr );
    if( ret != 0 )
    {
        perror( "pthread_create recv_thread_func" );
        return 1;
    }

    pthread_t stid;
    ret = pthread_create( &stid, nullptr, send_thread_func, nullptr );
    if( ret != 0 )
    {
        perror( "pthread_create send_thread_func" );
        return 1;
    }

    while( run )
    {

        if ( client_fd_set.size() < CLIENT_MAX_NUM )
        {
            int r = 1 + rand() % 40;
            for (int i = 0; i < r; ++i)
            {
                if( add_client() != 0  )
                {
                    run = false;
                    break;
                }
            }
        }

        usleep( rand() % 100000 );
    }

    for( auto it=client_fd_set.begin(); it!=client_fd_set.end(); ++it )
    {
        close( *it );
    }

    pthread_join( rtid, nullptr );
    pthread_join( stid, nullptr );

    return 0;
}
