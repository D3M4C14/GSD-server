#ifndef EPOLL_H
#define EPOLL_H

#include <netdb.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

// static bool ep_invalid( int efd ) 
// {
// 	return efd == -1;
// }

static int ep_create() 
{
	return epoll_create( 1024 );
}

// static void ep_release( int efd ) 
// {
// 	close( efd );
// }

static int ep_add( int efd, int fd ) 
{
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = fd;
	return epoll_ctl( efd, EPOLL_CTL_ADD, fd, &ev );
}

static void ep_del( int efd, int fd ) 
{
	epoll_ctl( efd, EPOLL_CTL_DEL, fd , nullptr );
}

// static void ep_write( int efd, int fd, void *ud, bool enable )
// {
// 	struct epoll_event ev;
// 	ev.events = EPOLLIN | (enable ? EPOLLOUT : 0);
// 	ev.data.ptr = ud;
// 	epoll_ctl(efd, EPOLL_CTL_MOD, fd, &ev);
// }

// 先不考虑平台移植问题 直接用原生的epoll_event结构
static int ep_wait( int efd, struct epoll_event *e, int en, int t = -1 )
{

	int n = epoll_wait( efd, e, en, t );
	
	return n;
}

// static void ep_nonblocking( int fd )
// {
// 	int flag = fcntl( fd, F_GETFL, 0 );
// 	if ( -1 == flag )return;

// 	fcntl(fd, F_SETFL, flag | O_NONBLOCK);
// }

#endif
