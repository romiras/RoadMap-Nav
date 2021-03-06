#ifndef __WINCE_SERIAL_
#define __WINCE_SERIAL_

#include <windows.h>
#include "../roadmap_main.h"
#include "../roadmap_io.h"

typedef struct roadmap_main_io {
   RoadMapIO *io;
   RoadMapInput callback;
   int is_valid;
} roadmap_main_io;

DWORD WINAPI SerialMonThread(LPVOID lpParam);
DWORD WINAPI SocketMonThread(LPVOID lpParam);
DWORD WINAPI FileMonThread(LPVOID lpParam);

#endif //__WINCE_SERIAL_
