/*
 GSport - an Apple //gs Emulator
 Copyright (C) 2010 - 2012 by GSport contributors
 
 Based on the KEGS emulator written by and Copyright (C) 2003 Kent Dickey

 This program is free software; you can redistribute it and/or modify it 
 under the terms of the GNU General Public License as published by the 
 Free Software Foundation; either version 2 of the License, or (at your 
 option) any later version.

 This program is distributed in the hope that it will be useful, but 
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License 
 for more details.

 You should have received a copy of the GNU General Public License along 
 with this program; if not, write to the Free Software Foundation, Inc., 
 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/

/* This file contains the socket calls */

#include "StdAfx.h"
#include "GenericSocketDriver.h"
#include "Log.h"

#ifndef MIN
#define  MIN(a,b)          (((a) < (b)) ? (a) : (b))
#endif

#define printf_ENABLED 1

#define fatal_printf(...)

#if !(defined _MSC_VER || defined __CYGWIN__)
	extern int h_errno;
	#if printf_ENABLED
	#else
		#define printf(...)
	#endif
#else
	#define socklen_t int
	#if printf_ENABLED
		#define printf LogOutput
	#else
		#define printf(...)
	#endif
#endif

int g_wsastartup_called;

void
socket_init_connection(SocketInfo *parent_socket_info_ptr, SocketInfo *socket_info_ptr)
{
	socket_info_ptr->host_handle = NULL;
	socket_info_ptr->sockfd = -1;		/* Indicate no socket open yet */
	socket_info_ptr->rdwrfd = -1;		/* Indicate no socket open yet */
	socket_info_ptr->host_addrlen = sizeof(struct sockaddr_in);
	socket_info_ptr->host_handle = malloc(socket_info_ptr->host_addrlen);   /* Used in accept, freed in shutdown */
	/* Real init will be done when bytes need to be read/write from skt */

	socket_info_ptr->root_connection = parent_socket_info_ptr->root_connection;
	socket_info_ptr->next_connection = NULL;
	socket_info_ptr->connection_number = parent_socket_info_ptr->connection_number + 1;

	socket_info_ptr->in_rdptr = 0;
	socket_info_ptr->in_wrptr = 0;
	socket_info_ptr->out_rdptr = 0;
	socket_info_ptr->out_wrptr = 0;
}

/* Usage: socket_init() called to init socket mode */
/*  At all times, we try to have a listen running on the incoming socket */
/* If we want to dial out, we close the incoming socket and create a new */
/*  outgoing socket.  Any hang-up causes the socket to be closed and it will */
/*  then re-open on a subsequent call to scc_socket_open */

void
socket_init(SocketInfo *socket_info_ptr)
{
#ifdef _WIN32
	WSADATA	wsadata;
	int	ret;

	if (g_wsastartup_called == 0) {
		ret = WSAStartup(MAKEWORD(2, 0), &wsadata);
		printf("WSAStartup ret: %d\n", ret);
		g_wsastartup_called = 1;
	}
#endif

	socket_info_ptr->root_connection = socket_info_ptr;
	socket_init_connection(socket_info_ptr, socket_info_ptr);
	socket_info_ptr->connection_count = 0;
	socket_info_ptr->connection_number = 0;
}

void
socket_shutdown(SocketInfo *socket_info_ptr)
{
	if (socket_info_ptr->next_connection) {
		socket_shutdown(socket_info_ptr->next_connection);  // recursively clean up
		free(socket_info_ptr->next_connection);
		socket_info_ptr->next_connection = NULL;
	}

	socket_close(socket_info_ptr, 0);
	free(socket_info_ptr->host_handle);
	socket_info_ptr->host_handle = NULL;
}

SocketInfo *
socket_create_connection(SocketInfo *parent_socket_info_ptr)
{
	SocketInfo *root_socket_info_ptr = parent_socket_info_ptr->root_connection;
	SocketInfo *new_socket_info_ptr = root_socket_info_ptr;

	if (root_socket_info_ptr->connection_count >= root_socket_info_ptr->max_connections)
		return NULL;

	new_socket_info_ptr = (SocketInfo *)malloc(sizeof(SocketInfo));
	socket_init_connection(parent_socket_info_ptr, new_socket_info_ptr);
	parent_socket_info_ptr->next_connection = new_socket_info_ptr;
	return new_socket_info_ptr;
}

void
socket_add_new_inbound_connection(SocketInfo *socket_info_ptr, double dcycs, int rdwrfd)
{
	SocketInfo *root_socket_info_ptr = socket_info_ptr->root_connection;
	SocketInfo *new_socket_info_ptr = root_socket_info_ptr;

	do {
		if (new_socket_info_ptr->rdwrfd == -1) {
			new_socket_info_ptr->rdwrfd = rdwrfd;
			root_socket_info_ptr->connection_count += 1;
			printf("%s #%d connected on rdwrfd=%d\n", root_socket_info_ptr->device_name, new_socket_info_ptr->connection_number, rdwrfd);
			socket_recvd_char(socket_info_ptr, -1, dcycs); // reset buffer
			return;
		}
		if (new_socket_info_ptr->next_connection)
			new_socket_info_ptr = new_socket_info_ptr->next_connection;
		else
			new_socket_info_ptr = socket_create_connection(new_socket_info_ptr);
	} while (new_socket_info_ptr);
}

static int
socket_close_handle(SOCKET sockfd)
{
	if (sockfd != -1)
	{
#if defined(_WIN32) || defined (__OS2__)
		return closesocket(sockfd); // NW: a Windows socket handle is not a file descriptor
#else
		return close(sockfd);
#endif
	}
	return 0;
}

void
socket_maybe_open_incoming(SocketInfo *socket_info_ptr, double dcycs)
{
	struct sockaddr_in sa_in;
	int	on;
	int	ret;
	SOCKET	sockfd;
	int	inc;

	inc = 0;

	if(socket_info_ptr->sockfd != -1) {
		/* it's already open, get out */
		return;
	}

	if (socket_info_ptr->	listen_tries == 0) {
		return; // too many retries
	}

	socket_close(socket_info_ptr, dcycs);
	memset(socket_info_ptr->host_handle, 0, socket_info_ptr->host_addrlen);

	while(1) {
		sockfd = socket(AF_INET, (socket_info_ptr->udp) ? SOCK_DGRAM : SOCK_STREAM, 0);
		if(sockfd == -1) {
			printf("socket ret: %d, errno: %d\n", sockfd, errno);
			socket_close(socket_info_ptr, dcycs);
			return;
		}
		printf("%s opened %s socket ret: %d\n", socket_info_ptr->device_name, (socket_info_ptr->udp) ? "UDP" : "TCP", sockfd);

		on = 1;
		ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
					(char *)&on, sizeof(on));
		if(ret < 0) {
			printf("setsockopt REUSEADDR ret: %d, err:%d\n",
				ret, errno);
			socket_close(socket_info_ptr, dcycs);
			return;
		}

		memset(&sa_in, 0, sizeof(sa_in));
		sa_in.sin_family = AF_INET;
		sa_in.sin_port = htons(socket_info_ptr->listen_port);
		sa_in.sin_addr.s_addr = htonl(INADDR_ANY);

		ret = bind(sockfd, (struct sockaddr *)&sa_in, sizeof(sa_in));

		if(ret >= 0) {
			if (!socket_info_ptr->udp) {
				ret = listen(sockfd, 1);
			}
			break;
		}
		/* else ret to bind was < 0 */
		printf("bind ret: %d, errno: %d\n", ret, errno);
		printf("%s failed to listen on TCP port %d\n", socket_info_ptr->device_name, socket_info_ptr->listen_port);
		//inc++;
		socket_close_handle(sockfd);
		// TODO: add port increment as an option?
		//printf("Trying next port: %d\n", SCC_LISTEN_PORT + port);
		//if(inc >= 10) {
			//printf("Too many retries, quitting\n");
		if (socket_info_ptr->listen_tries > 0)
			--socket_info_ptr->listen_tries;

			socket_close(socket_info_ptr, dcycs);
			return;
		//}
	}

	printf("%s listening on port %d\n", socket_info_ptr->device_name, socket_info_ptr->listen_port);
	socket_info_ptr->sockfd = sockfd;
	socket_make_nonblock(socket_info_ptr, dcycs);
}

void
socket_open_outgoing(SocketInfo *socket_info_ptr, double dcycs)
{
	struct sockaddr_in sa_in;
	struct hostent *hostentptr;
	int	on;
	int	ret;
	SOCKET	sockfd;

	//printf("socket_close being called from socket_open_outgoing\n");
	socket_close(socket_info_ptr, dcycs);

	memset(socket_info_ptr->host_handle, 0, socket_info_ptr->host_addrlen);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd == -1) {
		printf("%s failed to open outgoing socket ret: %d, errno: %d\n", socket_info_ptr->device_name, sockfd, errno);
		socket_close(socket_info_ptr, dcycs);
		return;
	}
	printf("%s opened outgoing sockfd ret: %d\n", socket_info_ptr->device_name, sockfd);

	on = 1;
	ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
					(char *)&on, sizeof(on));
	if(ret < 0) {
		printf("setsockopt REUSEADDR ret: %d, err:%d\n",
			ret, errno);
		socket_close(socket_info_ptr, dcycs);
		return;
	}

	memset(&sa_in, 0, sizeof(sa_in));
	sa_in.sin_family = AF_INET;
	sa_in.sin_port = htons(23);
	hostentptr = gethostbyname((const char*)&socket_info_ptr->hostname[0]);	// OG Added Cast
	if(hostentptr == 0) {
#if defined(_WIN32) || defined (__OS2__)
		fatal_printf("Lookup host %s failed\n",
			&socket_info_ptr->hostname[0]);
#else
		fatal_printf("Lookup host %s failed, herrno: %d\n",
			&socket_info_ptr->hostname[0], h_errno);
#endif
		socket_close_handle(sockfd);
		socket_close(socket_info_ptr, dcycs);
		//x_show_alert(0, 0);
		return;
	}
	memcpy(&sa_in.sin_addr.s_addr, hostentptr->h_addr,
							hostentptr->h_length);
	/* The above copies the 32-bit internet address into */
	/*   sin_addr.s_addr.  It's in correct network format */

	ret = connect(sockfd, (struct sockaddr *)&sa_in, sizeof(sa_in));
	if(ret < 0) {
		printf("connect ret: %d, errno: %d\n", ret, errno);
		socket_close_handle(sockfd);
		socket_close(socket_info_ptr, dcycs);
		return;
	}

	printf("%s socket is now outgoing to %s\n", socket_info_ptr->device_name, &socket_info_ptr->hostname[0]);
	socket_info_ptr->sockfd = sockfd;
	socket_make_nonblock(socket_info_ptr, dcycs);
	socket_info_ptr->rdwrfd = socket_info_ptr->sockfd;
}

void
socket_make_nonblock(SocketInfo *socket_info_ptr, double dcycs)
{
	SOCKET	sockfd;
	int	ret;
#if defined(_WIN32) || defined (__OS2__)
	u_long	flags;
#else
	int	flags;
#endif

	sockfd = socket_info_ptr->sockfd;

#if defined(_WIN32) || defined (__OS2__)
	flags = 1;
	ret = ioctlsocket(sockfd, FIONBIO, &flags);
	if(ret != 0) {
		printf("ioctlsocket ret: %d\n", ret);
	}
#else
	flags = fcntl(sockfd, F_GETFL, 0);
	if(flags == -1) {
		printf("fcntl GETFL ret: %d, errno: %d\n", flags, errno);
		socket_close(socket_info_ptr, dcycs);
		return;
	}
	ret = fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
	if(ret == -1) {
		printf("fcntl SETFL ret: %d, errno: %d\n", ret, errno);
		socket_close(socket_info_ptr, dcycs);
		return;
	}
#endif
}

void
socket_close(SocketInfo *socket_info_ptr, double dcycs)
{
	int	rdwrfd;
	SOCKET	sockfd;

	rdwrfd = socket_info_ptr->rdwrfd;
	if(rdwrfd >= 0) {
		printf("socket_close: rdwrfd=%d, closing\n", rdwrfd);
		socket_close_handle(rdwrfd);
		socket_recvd_char(socket_info_ptr, -1, dcycs); // reset buffer
		socket_info_ptr->root_connection->connection_count -= 1;
	}
	sockfd = socket_info_ptr->sockfd;
	if(sockfd != -1) {
		printf("socket_close: sockfd=%d, closing\n", sockfd);
		socket_close_handle(sockfd);
	}

	socket_info_ptr->rdwrfd = -1;
	socket_info_ptr->sockfd = -1;
}

void
socket_accept(SocketInfo *socket_info_ptr, double dcycs)
{
#ifdef SOCKET_INFO
	int	flags;
	int	rdwrfd;
	int	ret;

	if(socket_info_ptr->sockfd == -1) {
		socket_maybe_open_incoming(socket_info_ptr, dcycs);
	}
	if(socket_info_ptr->sockfd == -1 || socket_info_ptr->udp) {
		return;		/* just give up */
	}
	if((socket_info_ptr->rdwrfd == -1) || (socket_info_ptr->connection_count < socket_info_ptr->max_connections)) {
		rdwrfd = accept(socket_info_ptr->sockfd, (struct sockaddr*)socket_info_ptr->host_handle,
			(socklen_t*)&(socket_info_ptr->host_addrlen));
		if(rdwrfd < 0) {
			return;
		}

		flags = 0;
		ret = 0;
#if !defined(_WIN32) && !defined(__OS2__)
		/* For Linux, we need to set O_NONBLOCK on the rdwrfd */
		flags = fcntl(rdwrfd, F_GETFL, 0);
		if(flags == -1) {
			printf("fcntl GETFL ret: %d, errno: %d\n", flags,errno);
			return;
		}
		ret = fcntl(rdwrfd, F_SETFL, flags | O_NONBLOCK);
		if(ret == -1) {
			printf("fcntl SETFL ret: %d, errno: %d\n", ret, errno);
			return;
		}
#endif
		socket_add_new_inbound_connection(socket_info_ptr, dcycs, rdwrfd);
	}
#endif
}

static byte tmp_buf[256]; // used for receiving bytes in socket_fill_readbuf

void
socket_fill_readbuf(SocketInfo *socket_info_ptr, int space_left, double dcycs)
{
#ifdef SOCKET_INFO
	int	rdwrfd;
	int	ret;
	int	i;

	socket_accept(socket_info_ptr, dcycs);

	do
	{
		rdwrfd = (socket_info_ptr->udp) ? socket_info_ptr->sockfd : socket_info_ptr->rdwrfd;
		if (rdwrfd >= 0) {
			/* Try reading some bytes */
			space_left = MIN(space_left, sizeof(tmp_buf));
			if (socket_info_ptr->udp)
				ret = recvfrom(rdwrfd, (char*)tmp_buf, space_left, 0, NULL, NULL);
			else
				ret = recv(rdwrfd, (char*)tmp_buf, space_left, 0);	// OG Added cast
			if (ret > 0) {
				for (i = 0; i < ret; i++) {
					byte c = tmp_buf[i];
					socket_recvd_char(socket_info_ptr, c, dcycs);
				}
			}
			else if (ret == 0) {
				/* assume socket close */
				printf("%s disconnecting from rdwrfd=%d (recv got 0)\n", socket_info_ptr->root_connection->device_name, rdwrfd);
				socket_close(socket_info_ptr, dcycs);
			}
		}
		socket_info_ptr = socket_info_ptr->next_connection;
	} while (socket_info_ptr);
#endif
}

void
socket_recvd_char(SocketInfo *socket_info_ptr, int c, double dcycs)
{
	int handled_externally = 0; // TODO: would prefer bool or BOOL, but not sure about non-Windows builds

	// NOTE: c might be -1 as a signal to clear the buffers (eg from socket_close)
	// TODO: should we add 	if(socket_info_ptr->sockfd == -1) {
	//socket_maybe_open_incoming(socket_info_ptr, dcycs); // TODO: would prefer not to do this for every character!

	if (socket_info_ptr->root_connection->rx_handler != NULL) {
		handled_externally = socket_info_ptr->root_connection->rx_handler(socket_info_ptr, c);
	}
	if (!handled_externally) {
		// we handle this
		// TODO: implement the read buffer
		//scc_add_to_readbuf(port, c, dcycs);
	}
}

void
socket_empty_writebuf(SocketInfo *socket_info_ptr, double dcycs)
{
#ifdef SOCKET_INFO
# if !defined(_WIN32) && !defined(__OS2__)
	struct sigaction newact, oldact;
# endif
	int	rdptr;
	int	wrptr;
	int	rdwrfd;
	int	done;
	int	ret;
	int	len;

	/* Try writing some bytes */
	done = 0;
	while(!done) {
		rdptr = socket_info_ptr->out_rdptr;
		wrptr = socket_info_ptr->out_wrptr;
		if(rdptr == wrptr) {
			done = 1;
			break;
		}
		rdwrfd = socket_info_ptr->rdwrfd;
		len = wrptr - rdptr;
		if(len < 0) {
			len = SOCKET_OUTBUF_SIZE - rdptr;
		}
		if(len > 32) {
			len = 32;
		}
		if(len <= 0) {
			done = 1;
			break;
		}

		if(rdwrfd == -1) {
			socket_maybe_open_incoming(socket_info_ptr, dcycs);
			return;
		}

#if defined(_WIN32) || defined (__OS2__)
		ret = send(rdwrfd, (const char*)&(socket_info_ptr->out_buf[rdptr]), len, 0); // OG Added Cast
# else
		/* ignore SIGPIPE around writes to the socket, so we */
		/*  can catch a closed socket and prepare to accept */
		/*  a new connection.  Otherwise, SIGPIPE kills GSport */
		sigemptyset(&newact.sa_mask);
		newact.sa_handler = SIG_IGN;
		newact.sa_flags = 0;
		sigaction(SIGPIPE, &newact, &oldact);

		ret = send(rdwrfd, &(socket_info_ptr->out_buf[rdptr]), len, 0);

		sigaction(SIGPIPE, &oldact, 0);
		/* restore previous SIGPIPE behavior */
# endif	/* WIN32 */

#if 0
		printf("sock output: %02x\n", socket_info_ptr->out_buf[rdptr]);
#endif
		if(ret == 0) {
			done = 1;	/* give up for now */
			break;
		} else if(ret < 0) {
			/* assume socket is dead */
			printf("socket write failed on rdwrfd=%d, closing\n", rdwrfd);
			socket_close(socket_info_ptr, dcycs);
			done = 1;
			break;
		} else {
			rdptr = rdptr + ret;
			if(rdptr >= SOCKET_OUTBUF_SIZE) {
				rdptr = rdptr - SOCKET_OUTBUF_SIZE;
			}
			socket_info_ptr->out_rdptr = rdptr;
		}
	}
#endif
}
