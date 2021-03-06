# iPhone makefile

TOP = ..
include $(TOP)/options.mk

# --- iPhone-specific options ---------------------------------------

ROADMAP_BUNDLE=RoadMap.app
ROADGPS_BUNDLE=RoadGps.app
LIBS += -lobjc -lstdc++.6
LDFLAGS += -fobjc-abi-version=2 \
	-L/home/ditlemor/dat/sys/usr/lib \
	-F/home/ditlemor/dat/sys/System/Library/Frameworks \
        -framework CoreFoundation -framework CoreGraphics \
        -framework Foundation \
	-framework UIKit -bind_at_load \
#	-framework GraphicsServices
#        -F/System/Library/Frameworks \
#
CFLAGS += -fobjc-abi-version=2

# --- iPhone sources & targets -----------------------------------

# note that source is Objective C, hence .m suffix

SOURCE = roadmap_main.m roadmap_canvas.m

RMLIBSRCS=roadmap_messagebox.m \
          roadmap_dialog.m \
          roadmap_fileselection.m \
	  roadmap_progress.m

RMLIBOBJS = $(CANVAS_OBJS) $(RMLIBSRCS:.m=.o)

HEADERS = \
	roadmap_iphonecanvas.h \
	roadmap_iphonemain.h \
	roadmap_iphonemessagebox.h \
	colors.h

TARGETS = iphoneroadgps iphoneroadmap

# --- Conventional targets ----------------------------------------

.PHONY: all strip clean install uninstall

all: $(TARGETS)

strip:
	$(STRIP) $(TARGETS)

clean:
	rm -f *.o *.a *.da $(TARGETS) .depends.mk
	rm -rf $(ROADMAP_BUNDLE) $(ROADGPS_BUNDLE)

install:
	mkdir -p $(ROADMAP_BUNDLE)/resources/default
	cp iphoneroadmap $(INSTALLDIR)/RoadMap
	cp Info.plist.roadmap $(INSTALLDIR)/Info.plist
	cp ../icons/iphone_default.png $(INSTALLDIR)/icon.png
	cp -r ../icons $(INSTALLDIR)/resources/pixmaps

uninstall:
	cd $(bindir) && rm -f $(TARGETS) iphoneroadmap iphoneroadgps

sourcelist:
	@echo Makefile $(SOURCE)
	    $(RMLIBSRCS) $(HEADERS)

# --- The real targets --------------------------------------------

libiphoneroadmap.a: $(RMLIBOBJS)
	$(AR) $(ARFLAGS) libiphoneroadmap.a $(RMLIBOBJS)
	$(RANLIB) libiphoneroadmap.a

%.o: %.m
	$(CC) $(CFLAGS) -c $< -o $@

iphoneroadmap: roadmap_main.o libiphoneroadmap.a \
		$(TOP)/libguiroadmap.a $(filter %.a, $(LIBS))
	$(CXX) $(LDFLAGS) -o iphoneroadmap roadmap_main.o \
		$(TOP)/libguiroadmap.a libiphoneroadmap.a $(LIBS)

iphoneroadgps: roadmap_main.o libiphoneroadmap.a \
		$(TOP)/libguiroadgps.a  $(filter %.a, $(LIBS))
	$(CXX) $(LDFLAGS) -o iphoneroadgps roadmap_main.o \
		$(TOP)/libguiroadgps.a libiphoneroadmap.a $(LIBS)


depends:
	$(MAKEDEPS) -Y -f - $(SOURCE) $(RMLIBSRCS) > .depends.mk

-include .depends.mk
