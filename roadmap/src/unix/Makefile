# --- Tool specific options ------------------------------------------------

TOP = ..
include $(TOP)/options.mk

# --- UNIX OS sources & targets --------------------------------------------

LIBSRCS=roadmap_file.c \
          roadmap_path.c \
          roadmap_library.c \
          roadmap_net.c \
          roadmap_serial.c \
          roadmap_spawn.c \
          roadmap_time.c

ifeq ($(USE_TRIP_PLUGIN),yes)
LIBSRCS += roadmap_sound.c
endif
ifeq ($(USE_NAVIGATE_PLUGIN),yes)
LIBSRCS += roadmap_sound.c
endif

LIBOBJS=$(LIBSRCS:.c=.o)


# --- Conventional targets ----------------------------------------

.PHONY: all clean install uninstall

all: libosroadmap.a

clean:
	rm -f *.o *.a *.da .depends.mk

install:

uninstall:

sourcelist:
	@echo Makefile $(LIBSRCS)


# --- The real targets --------------------------------------------

libosroadmap.a: $(LIBOBJS)
	$(AR) $(ARFLAGS) libosroadmap.a $(LIBOBJS)
	$(RANLIB) libosroadmap.a


depends:
	$(MAKEDEPS) -Y -f - $(LIBSRCS) > .depends.mk 2>/dev/null

-include .depends.mk
