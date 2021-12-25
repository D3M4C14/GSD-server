#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int sock;
bool run = true;

void* thread_func( void* arg )
{
    const int bsz = 4096;
    char buffer[ bsz ];
    int len;
    while( run )
    {
        memset( buffer, '\0', bsz );
        len = recv( sock, buffer, bsz, MSG_DONTWAIT );
        if ( len > 0 )
        {
            printf( "reccv msg : %s\n",buffer );
        }
        else
        {
            usleep(100000);
        }
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

    const char* ip = argv[1];
    int port = atoi( argv[2] );
    
    printf( "* runing %s : %s:%d \n", basename(argv[0]), ip, port );

    sock = socket( PF_INET, SOCK_STREAM, 0 );
    assert( sock >= 0 );
    
    // 设置在线接收带外数据
    int on = 1;
    setsockopt( sock, SOL_SOCKET, SO_OOBINLINE, &on, sizeof(on) );

    // 设置写缓冲区大小
    int sbsz = 64;
    int slen = sizeof(sbsz);
    setsockopt( sock, SOL_SOCKET, SO_SNDBUF, &sbsz, sizeof(sbsz) );
    getsockopt( sock, SOL_SOCKET, SO_SNDBUF, &sbsz, (socklen_t*)&slen );
    printf( "* the send buffer size after setting is %d\n", sbsz );

    // 设置读缓冲区大小
    int rbsz = 4096;
    int rlen = sizeof(rbsz);
    setsockopt( sock, SOL_SOCKET, SO_RCVBUF, &rbsz, sizeof(rbsz) );
    getsockopt( sock, SOL_SOCKET, SO_RCVBUF, &rbsz, (socklen_t*)&rlen );
    printf( "* the receive buffer size after settting is %d\n", rbsz );

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
    
    pthread_t tid;
    int ret = pthread_create( &tid, nullptr, thread_func, nullptr );
    if( ret != 0 )
    {
        perror( "pthread_create" );
        return 1;
    }

    const int bsz = 4096;
    char buffer[ bsz ];
    while( run )
    {
        printf( "-input msg...\n" );
        std::cin >> buffer;
        printf( "\n" );

        int len = strlen( buffer );

        if( len >= 0 )
        {
            send( sock, buffer, len, 0);
        }

        if( strcmp(buffer,"quit") == 0 ) run = false;

    }

    close(sock);

    pthread_join( tid, nullptr );

    return 0;
}
