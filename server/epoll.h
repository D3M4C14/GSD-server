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

struct event {
	void * ud;
	bool read;
	bool write;
	bool error;
	bool eof;
};

static bool ep_invalid( int efd ) 
{
	return efd == -1;
}

static int ep_create() 
{
	return epoll_create( 1024 );
}

static void ep_release( int efd ) 
{
	close( efd );
}

static int ep_add( int efd, int fd, void *ud ) 
{
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.ptr = ud;
	return epoll_ctl( efd, EPOLL_CTL_ADD, fd, &ev );
}

static void ep_del( int efd, int fd ) 
{
	epoll_ctl( efd, EPOLL_CTL_DEL, fd , nullptr );
}

static void ep_write( int efd, int fd, void *ud, bool enable )
{
	struct epoll_event ev;
	ev.events = EPOLLIN | (enable ? EPOLLOUT : 0);
	ev.data.ptr = ud;
	epoll_ctl(efd, EPOLL_CTL_MOD, fd, &ev);
}

static int ep_wait( int efd, struct event *e, int en, int t = -1 )
{
	struct epoll_event ev[en];
	int n = epoll_wait( efd, ev, en, t );
	
	for (int i = 0; i < n; ++i)
	{
		e[i].ud = ev[i].data.ptr;
		unsigned flag = ev[i].events;
		e[i].write = (flag & EPOLLOUT) != 0;
		e[i].read = (flag & (EPOLLIN | EPOLLHUP)) != 0;
		e[i].error = (flag & EPOLLERR) != 0;
		e[i].eof = false;
	}

	return n;
}

static void ep_nonblocking( int fd )
{
	int flag = fcntl( fd, F_GETFL, 0 );
	if ( -1 == flag )return;

	fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

#endif
