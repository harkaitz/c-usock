#ifndef USOCK_H
#define USOCK_H

#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>

#ifndef USOCK_FUNC
#  define USOCK_FUNC static __attribute__((unused))
#endif


typedef struct usock_srv_s usock_srv_t;
typedef struct usock_cli_s usock_cli_t;
typedef void (*usock_f) (FILE *,void *);

struct usock_cli_s {
	bool		locked;
	bool		joinable;
	FILE		*fp;
	pthread_t	thread;
	usock_f		handler;
	void		*udata;
};

struct usock_srv_s {
	pthread_t thread;
	int	  sfd;
	usock_f	  handler;
	void	 *udata;
	struct	  usock_cli_s clients[20];
};


USOCK_FUNC void *
__usock_client_thread(void *_cli_v)
{
	struct usock_cli_s *_cli = (struct usock_cli_s *)_cli_v;
	_cli->handler(_cli->fp, _cli->udata);
	_cli->joinable = true;
	return 0;
}

USOCK_FUNC void *
__usock_server_thread(void *_srv_v)
{
	int	cli_fd, slot, e;
	struct	usock_cli_s *cli;
	struct	usock_srv_s *_srv = (struct usock_srv_s *) _srv_v;
	
	while (1) {
		cli_fd = accept(_srv->sfd, NULL, NULL);
		if (cli_fd == -1/*err*/) {
			syslog(LOG_WARNING, "Accept returned -1.");
			break;
		}
		
		for (slot=0; slot<20; slot++) {
			if (_srv->clients[slot].joinable) {
				pthread_join(_srv->clients[slot].thread, NULL);
				_srv->clients[slot].joinable = false;
				_srv->clients[slot].locked = false;
				
			}
		}
		
		cli = NULL;
		for (slot=0; slot<20; slot++) {
			if (!_srv->clients[slot].locked) {
				cli = &_srv->clients[slot];
				break;
			}
		}
		
		if(!cli/*err*/) {
			syslog(LOG_WARNING, "Too much clients, refusing to serve %i", cli_fd);
			close(cli_fd);
			continue;
		}
		
		cli->locked  = true;
		cli->handler = _srv->handler;
		cli->udata   = _srv->udata;
		cli->fp      = fdopen(cli_fd, "r+");
		
		e = pthread_create(&cli->thread, NULL, __usock_client_thread, cli);
		if (e/*err*/) {
			syslog(LOG_ERR, "Can't create thread: %s", strerror(errno));
			continue;
		}
	}
	for(slot=0; slot<20; slot++) {
		if(_srv->clients[slot].locked) {
			pthread_join(_srv->clients[slot].thread, NULL);
			_srv->clients[slot].joinable = false;
		}
	}
	return NULL;
}

/* -------------------------------------------------------------------------- */

USOCK_FUNC struct usock_srv_s *
usock_srv_new(int _fd, usock_f _handler, void *_udata)
{
	struct	usock_srv_s *self;
	
	signal(SIGPIPE, SIG_IGN);
	
	self = (usock_srv_t *) malloc(sizeof(*self));
	if(!self) {
		syslog(LOG_ERR, "socket_ipc_server_new: Can't allocate memory.");
		exit(1);
	}
	self->sfd = _fd;
	self->handler = _handler;
	self->udata = _udata;
	memset(self->clients, 0, sizeof(self->clients));
	
	pthread_create(
	    &self->thread,
	    NULL,
	    __usock_server_thread,
	    self
	);
	return self;
}

USOCK_FUNC void
usock_srv_free(usock_srv_t *_self)
{
	close(_self->sfd);
	_self->sfd = -1;
	pthread_join(_self->thread, NULL);
	free(_self);
}

USOCK_FUNC void
usock_srv_wait(usock_srv_t *_self)
{
	pthread_join(_self->thread, NULL);
}

/* -------------------------------------------------------------------------- */

USOCK_FUNC struct usock_srv_s *
usock_srv_new_unix(const char *name, usock_f _handler, void *_udata)
{
	struct sockaddr_un addr;
	int fd=-1;
	
	if((fd=socket(AF_UNIX,SOCK_STREAM,0)) == -1) {
		syslog(LOG_ERR, "Can't open server UNIX socket `%s`", name);
		close(fd);
		return NULL;
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (name[0] == '@') {
		*addr.sun_path = '\0';
		strncpy(addr.sun_path+1, name+1, sizeof(addr.sun_path)-2);
	} else {
		strncpy(addr.sun_path, name, sizeof(addr.sun_path)-1);
		unlink(name);
	}
	
	if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		syslog(LOG_ERR, "Can't bind UNIX socket `%s`", name);
		close(fd);
		return NULL;
	}
	if (listen(fd, 5) == -1) {
		syslog(LOG_ERR, "Can't listen to UNIX socket `%s`", name);
		close(fd);
		return NULL;
	}
	
	return usock_srv_new(fd, _handler, _udata);
}

USOCK_FUNC FILE *
usock_cli_new_unix(const char *_name)
{
	struct sockaddr_un addr;
	int fd = -1;
	
	if((fd=socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		syslog(LOG_ERR, "Can't open server UNIX socket `%s`", _name);
		goto cleanup;
	}
	
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (_name[0] == '@') {
		*addr.sun_path = '\0';
		strncpy(addr.sun_path+1, _name+1, sizeof(addr.sun_path)-2);
	} else {
		strncpy(addr.sun_path, _name, sizeof(addr.sun_path)-1);
	}
	
	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		syslog(LOG_ERR, "Can't connect to UNIX socket `%s`: %s", _name, strerror(errno));;
		goto cleanup;
	}
	
	return fdopen(fd, "r+");
	cleanup:
	if(fd != -1) {
		close(fd);
	}
	return NULL;
}

USOCK_FUNC void
fclose_write(FILE *fp)
{
	fflush(fp);
	shutdown(fileno(fp), SHUT_WR);
}

USOCK_FUNC void
fclose_read(FILE *fp)
{
	fflush(fp);
	shutdown(fileno(fp), SHUT_RD);
}



#endif
