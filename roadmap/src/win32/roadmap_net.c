/* roadmap_net.c - Network interface for the RoadMap application.
 *
 * LICENSE:
 *
 *   Copyright 2005 Ehud Shabtai
 *
 *   Based on an implementation by Pascal F. Martin.
 *   This file is part of RoadMap.
 *
 *   RoadMap is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   RoadMap is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with RoadMap; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * SYNOPSYS:
 *
 *   See roadmap_net.h
 */

#include <windows.h>
#include <winsock.h>
#include <ctype.h>

#include "../roadmap.h"
#include "../roadmap_net.h"


RoadMapSocket roadmap_net_connect (const char *protocol,
						 const char *name, int default_port)
{
	SOCKET fd;
	char *hostname;
	char *separator = strchr (name, ':');

	struct hostent *host;
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;

	hostname = strdup(name);
	roadmap_check_allocated(hostname);

	if (separator != NULL) {
#if 0
		struct servent *service;
		service = getservbyname(separator+1, "tcp");
#else
		void *service = NULL;
#endif
		if (service == NULL) {

			if (isdigit(separator[1])) {

				addr.sin_port = htons((unsigned short)atoi(separator+1));

				if (addr.sin_port == 0) {
					roadmap_log (ROADMAP_ERROR, "invalid port in '%s'", name);
					goto connection_failure;
				}

			} else {
				roadmap_log (ROADMAP_ERROR, "invalid service in '%s'", name);
				goto connection_failure;
			}

		} else {
#if 0
			addr.sin_port = service->s_port;
#endif
		}

		*(strchr(hostname, ':')) = 0;


	} else {
		addr.sin_port = htons((unsigned short)default_port);
	}


	host = gethostbyname(hostname);

	if (host == NULL) {
		if (isdigit(hostname[0])) {
			addr.sin_addr.s_addr = inet_addr(hostname);
			if (addr.sin_addr.s_addr == INADDR_NONE) {
				roadmap_log (ROADMAP_ERROR, "invalid IP address '%s'",
					hostname);
				goto connection_failure;
			}
		} else {
			roadmap_log (ROADMAP_ERROR, "invalid host name '%s'", hostname);
			goto connection_failure;
		}
	} else {
		memcpy (&addr.sin_addr, host->h_addr, host->h_length);
	}


	if (strcmp (protocol, "udp") == 0) {
		fd = socket (PF_INET, SOCK_DGRAM, 0);
	} else if (strcmp (protocol, "tcp") == 0) {
		fd = socket (PF_INET, SOCK_STREAM, 0);
	} else {
		roadmap_log (ROADMAP_ERROR, "unknown protocol %s", protocol);
		goto connection_failure;
	}

	if (fd == INVALID_SOCKET) {
		roadmap_log (ROADMAP_ERROR, "cannot create socket, errno = %d", WSAGetLastError());
		goto connection_failure;
	}

	/* FIXME: this way of establishing the connection is kind of dangerous
	* if the server process is not local: we might fail only after a long
	* delay that will disable RoadMap for a while.
	*/
	if (connect (fd, (struct sockaddr *) &addr, sizeof(addr)) == SOCKET_ERROR) {

		/* This is not a local error: the remote application might be
		* unavailable. This is not our fault, don't cry.
		*/
		closesocket(fd);
		goto connection_failure;
	}

	return fd;


connection_failure:

	free(hostname);
	return -1;
}


int roadmap_net_send (RoadMapSocket socket, const void *data, int length)
{
	return send(socket, data, length, 0);
}


int roadmap_net_receive (RoadMapSocket socket, void *data, int size)
{
	int res;
	res = recv(socket, data, size, 0);

	if (res == 0) return -1;
	else return res;
}


void roadmap_net_close (RoadMapSocket socket)
{
	closesocket(socket);
}


RoadMapSocket roadmap_net_listen(int port)
{
	struct sockaddr_in addr;
	SOCKET fd;
	int res;

	memset(&addr, 0, sizeof(addr));

	addr.sin_family = AF_INET;
	addr.sin_port = htons((short)port);

	fd = socket (PF_INET, SOCK_STREAM, 0);
	if (fd == INVALID_SOCKET) return fd;

	res = bind(fd, (struct sockaddr*)&addr, sizeof(addr));

	if (res == SOCKET_ERROR) {
		closesocket(fd);
		return INVALID_SOCKET;
	}

	res = listen(fd, 10);

	if (res == SOCKET_ERROR) {
		closesocket(fd);
		return INVALID_SOCKET;
	}
	
	return fd;
}


RoadMapSocket roadmap_net_accept(RoadMapSocket server_socket)
{
	return accept(server_socket, NULL, NULL);
}

