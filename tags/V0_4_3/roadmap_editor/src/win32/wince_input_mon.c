/* wince_input_mon.c - monitor for inputs (serial / network / file).
 *
 * LICENSE:
 *
 *   Copyright 2005 Ehud Shabtai
 *
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
 */

#include <windows.h>
#include <assert.h>
#include "../roadmap.h"
#include "../roadmap_io.h"
#include "../roadmap_serial.h"
#include "wince_input_mon.h"
#include "win32_serial.h"

extern HWND RoadMapMainWindow;

static HANDLE serial_open(const char *name,
                          const char *mode,
		                    int baud_rate) {

	HANDLE hCommPort = INVALID_HANDLE_VALUE;
	LPWSTR url_unicode = ConvertToWideChar(name, CP_UTF8);
	DCB dcb;
   COMMTIMEOUTS ct;

	hCommPort = CreateFile (url_unicode,
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_WRITE_THROUGH,
		NULL);

	free(url_unicode);

	if(hCommPort == INVALID_HANDLE_VALUE) {
		return INVALID_HANDLE_VALUE;
	}

	dcb.DCBlength = sizeof(DCB);
	if(!GetCommState(hCommPort, &dcb)) {
		roadmap_serial_close(hCommPort);
		return INVALID_HANDLE_VALUE;
	}

	dcb.BaudRate	       = baud_rate;
	dcb.fRtsControl        = RTS_CONTROL_DISABLE;
	dcb.fDtrControl        = DTR_CONTROL_DISABLE;
	dcb.ByteSize           = 8;
	dcb.fParity             = FALSE;
	dcb.StopBits           = ONESTOPBIT;

	if(!SetCommState(hCommPort, &dcb)) {
		roadmap_serial_close(hCommPort);
		return INVALID_HANDLE_VALUE;
	}

	ct.ReadIntervalTimeout = MAXDWORD;
	ct.ReadTotalTimeoutMultiplier = MAXDWORD;
	ct.ReadTotalTimeoutConstant = 10000;
	ct.WriteTotalTimeoutMultiplier = 10;
	ct.WriteTotalTimeoutConstant = 100;

	if(!SetCommTimeouts(hCommPort, &ct)) {
		roadmap_serial_close(hCommPort);
		return INVALID_HANDLE_VALUE;
	}

	return hCommPort;
}


static int serial_read(HANDLE handle, void *data, int size)
{
   DWORD dwBytesRead;

   if(!ReadFile(handle,
                data,
                size,
                &dwBytesRead,
                NULL)) {
      return -1;
   }

   return dwBytesRead;
}


DWORD WINAPI SerialMonThread(LPVOID lpParam) {

	roadmap_main_io *data = (roadmap_main_io*)lpParam;
   Win32SerialConn *conn = data->io->os.serial;

   conn->handle = serial_open (conn->name, conn->mode, conn->baud_rate);

   if (conn->handle == INVALID_HANDLE_VALUE) {

      /* Send a message to main window. A read attempt will fail
       * and this input will be removed.
       */

      /* Sleep to avoid busy loop, as RoadMap will try to create
       * a new connection as soon as the message is sent
       */

      Sleep(2000);

      conn->data_count = -1;
      SendMessage(RoadMapMainWindow, WM_USER_READ, (WPARAM)data, (LPARAM)conn);
   }

   while(conn->handle != INVALID_HANDLE_VALUE) {

      if (conn->data_count == 0) {

         do {
            conn->data_count =
               serial_read (conn->handle, conn->data, sizeof(conn->data));

         } while (conn->data_count == 0);
      }

   	/* Check if this input was unregistered while we were
		 * sleeping.
		 */
		if (!conn->valid) {
			break;
		}

   
      if (conn->data_count < 0) {
         
         int error_code = GetLastError();

			if( error_code == ERROR_INVALID_HANDLE) {
				roadmap_log (ROADMAP_INFO,
						"Com port is closed.");
			} else {
				roadmap_log (ROADMAP_ERROR,
						"Error in serial_read: %d", error_code);
			}
		
			/* Ok, we got some error. We continue to the same path
			 * as if input is available. The read attempt should
			 * fail, and result in removing this input.
			 */

		}

		/* Send a message to main window so it can read. */
		SendMessage(RoadMapMainWindow, WM_USER_READ, (WPARAM)data, (LPARAM)conn);
	}

   if (!--conn->ref_count) {
   	free(conn);
   } else {

      roadmap_log
         (ROADMAP_ERROR, "conn ref_count is not zero: %d",
          conn->ref_count);
      assert (0);
   }

	return 0;
}


DWORD WINAPI SocketMonThread(LPVOID lpParam)
{
	roadmap_main_io *data = (roadmap_main_io*)lpParam;
	RoadMapIO *io = data->io;
	SOCKET fd = data->io->os.socket;
	fd_set set;

	FD_ZERO(&set);
	while(io->subsystem != ROADMAP_IO_INVALID)
	{
		FD_SET(fd, &set);
		if(select(fd+1, &set, NULL, NULL, NULL) == SOCKET_ERROR) {
			roadmap_log (ROADMAP_ERROR,
					"Error in select.");
		}

		/* Check if this input was unregistered while we were
		 * sleeping.
		 */
		if (io->subsystem == ROADMAP_IO_INVALID) {
			break;
		}

		/* Send a message to main window so it can read. */
		SendMessage(RoadMapMainWindow, WM_USER_READ, (WPARAM)data, 0);
	}

	free(io);

	return 0;
}


/* This is not a real monitor thread. As this is a file we assume that input
 * is always ready and so we loop until a request to remove this input is
 * received.
 */
DWORD WINAPI FileMonThread(LPVOID lpParam)
{
	roadmap_main_io *data = (roadmap_main_io*)lpParam;
	RoadMapIO *io = data->io;

	while(io->subsystem != ROADMAP_IO_INVALID)
	{
		/* Send a message to main window so it can read. */
		SendMessage(RoadMapMainWindow, WM_USER_READ, (WPARAM)data, 0);
	}

	free(io);

	return 0;
}
