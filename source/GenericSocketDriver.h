/*
 GSport - an Apple //gs Emulator
 Copyright (C) 2010 by GSport contributors
 
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
#ifndef SOCKET_INFO
#define SOCKET_INFO

#include <ctype.h>

#ifdef _WIN32
# include <winsock2.h>
#else
# include <sys/socket.h>
# include <netinet/in.h>
# include <netdb.h>
# ifndef SOCKET
#  define SOCKET		word32		/* for non-windows */
# endif
#endif

#define	SOCKET_INBUF_SIZE		1024		/* must be a power of 2 */
#define	SOCKET_OUTBUF_SIZE		1024		/* must be a power of 2 */

#define MAX_HOSTNAME_SIZE		256

typedef struct SocketInfo {
	char*	device_name;
	void*	device_data;
	SOCKET	sockfd;
	int		listen_port;
	int		listen_tries; // -1 = infinite
	int		rdwrfd;
	void	*host_handle;
	int		host_addrlen;

	int		rx_queue_depth;
	byte	rx_queue[4];
	BOOL	(*rx_handler)(SocketInfo *socket_info_ptr, int c);

	int		in_rdptr;
	int		in_wrptr;
	byte	in_buf[SOCKET_INBUF_SIZE];

	int		out_rdptr;
	int		out_wrptr;
	byte	out_buf[SOCKET_OUTBUF_SIZE];

	byte	hostname[MAX_HOSTNAME_SIZE];
} SocketInfo;

/* generic_socket_driver.c */
void socket_init(SocketInfo *socket_info_ptr);
void socket_shutdown(SocketInfo *socket_info_ptr);
void socket_maybe_open_incoming(SocketInfo *socket_info_ptr, double dcycs);
void socket_open_outgoing(SocketInfo *socket_info_ptr, double dcycs);
void socket_make_nonblock(SocketInfo *socket_info_ptr, double dcycs);
void socket_close(SocketInfo *socket_info_ptr, double dcycs);
void socket_accept(SocketInfo *socket_info_ptr, double dcycs);
void socket_fill_readbuf(SocketInfo *socket_info_ptr, int space_left, double dcycs);
void socket_recvd_char(SocketInfo *socket_info_ptr, int c, double dcycs);
void socket_empty_writebuf(SocketInfo *socket_info_ptr, double dcycs);

#endif // SOCKET_INFO
