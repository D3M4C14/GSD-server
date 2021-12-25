#include <sys/socket.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

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

    int sock = socket( PF_INET, SOCK_STREAM, 0 );
    assert( sock >= 0 );
    
    // 设置在线接收带外数据
    int on = 1;
    setsockopt( sock, SOL_SOCKET, SO_OOBINLINE, &on, sizeof(on) );

    // 设置写缓冲区大小
    int sbsz = 32;
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
    
    const int bsz = 4096;
    char buffer[ bsz ];
    while( true )
    {

        printf( "-input msg...\n" );
        std::cin >> buffer;
        printf( "\n" );

        int len = strlen( buffer );
        if(len>=1 && buffer[0]=='O' )
        {
            send( sock, buffer, len, MSG_OOB );
        }
        else if( len>=1 && buffer[0]=='D' )
        {
            // 第二个紧急数据会覆盖第一个(一个TCP包只有一个紧急指针)
            send( sock, buffer, len, MSG_OOB );
            char obd[] = "OOB";
            send( sock, obd, 3, MSG_OOB );
        }
        else if(len>=1 && buffer[0]=='B' )
        {
            send( sock, buffer, len, MSG_OOB );
            char obd[] = "OOB";
            send( sock, obd, 3, 0 );
        }
        else
        {
            send( sock, buffer, len, 0);
        }

        if(strcmp(buffer,"quit")==0)break;

        usleep(100000);

        for (int i = 0; i < 10; ++i)
        {
            int ret = sockatmark(sock);
            if(ret==1)
            {
                printf( "at OOB mark\n" );
            }
            else if(ret==-1)
            {
                perror( "sockatmark" );
            }

            memset( buffer, '\0', bsz );
            len = recv( sock, buffer, bsz, MSG_DONTWAIT );
            if(len>0)
            {
                printf( "from server msg: '%s'\n", buffer );
            }
            else
            {
                break;
            }
            usleep(10000);
        }

    }

    close(sock);

    return 0;
}
