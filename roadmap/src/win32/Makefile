
# WinCE Makefile, based around the arm-wince-mingw32ce cross toolchain,
# which comes from http://cegcc.sourceforge.net/
# Code built this way is known to work.
#
# The code may work if built with microsoft tools, in which case this
# makefile isn't used, and the project files under wince_env/ may
# need updating. Not tested recently.
#

#
# Produce a build timestamp
#
BUILD_DATETIME = `date +%Y%m%d.%H%M`


TOP = ..
include $(TOP)/options.mk

RMLIBSRC= \
	roadmap_messagebox.c \
	roadmap_dialog.c \
	roadmap_file.c \
	roadmap_library.c \
	roadmap_main.c \
	roadmap_fileselection.c \
	roadmap_net.c \
	roadmap_path.c \
	roadmap_serial.c \
	roadmap_spawn.c \
	roadmap_time.c \
	roadmap_win32.c \
	time.c \
	wince_input_mon.c \
	roadmap_progress.c \
	CEDevice.c \
	CEException.c \
	roadmap_gps_detect.c \
	roadmap_colors.c

#
# No support for navigate plugin without trip plugin
#
ifeq ($(USE_TRIP_PLUGIN),yes)
RMLIBSRC += roadmap_sound.c
endif
ifeq ($(USE_NAVIGATE_PLUGIN),yes)
RMLIBSRC += roadmap_sound.c
endif

#	roadmap_keyboard.c \
#	roadmap_signals.c \

RMLIBOBJ = $(CANVAS_OBJS) $(RMLIBSRC:.c=.o)

HEADERS = \
	resource.h \
	newres.h \
	colors.h \
	time.h \
	roadmap_win32.h \
	roadmap_wincecanvas.h \
	wince_input_mon.h

TARGETS = wroadmap.exe
TARGETS += wroadmap.cab demomaps.cab
# TARGETS += testpower.exe

# --- The real targets --------------------------------------------

all: $(TARGETS)

strip:
	$(STRIP) $(TARGETS)

clean:
	rm -f *.o *.a *.da $(addsuffix .exe,$(TARGETS)) .depends.mk *.rsc

libwroadmap.a: $(RMLIBOBJ)
	$(AR) $(ARFLAGS) libwroadmap.a $(RMLIBOBJ)
	$(RANLIB) libwroadmap.a

WINCE_LIBS=	-lcommctrl -laygshell -lws2
# WINCE_LIBS=	-lcommctrl -lws2

wroadmap.exe: roadmap_main.o libwroadmap.a roadmap.rsc \
		roadmap_build.c \
		$(TOP)/libguiroadmap.a $(filter %.a, $(LIBS))
	$(CC) -DROADMAP_BUILD_DATE_TIME='"'$(BUILD_DATETIME)'"' -c roadmap_build.c
	$(CC) $(LDFLAGS) -o wroadmap.exe roadmap_main.o roadmap.rsc \
		roadmap_build.o \
		$(TOP)/libguiroadmap.a libwroadmap.a $(LIBS) \
		libwroadmap.a ${WINCE_LIBS}

#
# Unneeded, functionality is now in "roadmap"
#
# wroadgps: roadmap_main.o libwroadmap.a roadmap.rsc \
# 		$(TOP)/libguiroadgps.a  $(filter %.a, $(LIBS))
# 	$(CC) $(LDFLAGS) -o wroadgps roadmap_main.o roadmap.rsc \
# 		$(TOP)/libguiroadgps.a libwroadmap.a $(LIBS) \
# 		libwroadmap.a -lcommctrl -laygshell -lws2

# for win32 we aren't as rigorous about producing
# this list of source as we are for the other RoadMap
# directories.  (basically, pick up everything.)
sourcelist:
	@find . -name 'CVS' -prune -o -print

WINDRES=$(CROSS)windres
roadmap.rsc: roadmap.rc roadmap.ico
	$(WINDRES) -D_WIN32_WCE=0x0400 -D_WIN32_IE=0x0400 roadmap.rc roadmap.rsc

roadmap_colors.o:	colors.h

depends:
	$(MAKEDEPS) -Y -f - $(RMLIBSRC) > .depends.mk 2>/dev/null

-include .depends.mk

CABFILES=	wroadmap.exe \
		distribution/libexpat.dll \
		distribution/font.ttf \
		distribution/roadmap.screenobjects \
		distribution/roadmap.toolbar \
		distribution/sprites \
		distribution/All \
		distribution/session.txt \
		distribution/preferences.txt

wroadmap.cab:	wroadmap.exe
	CreateCab.pl

demomaps.cab:
