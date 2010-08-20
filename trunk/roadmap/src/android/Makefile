#
# Android Makefile
#
# Requires the NDK
#

ADB=	adb -e
#
# Produce a build timestamp
#
BUILD_DATETIME = `date +%Y%m%d.%H%M`


TOP = ..
include $(TOP)/options.mk

#
# No support for navigate plugin without trip plugin
#
ifeq ($(USE_TRIP_PLUGIN),yes)
RMLIBSRC += roadmap_sound.c
endif
ifeq ($(USE_NAVIGATE_PLUGIN),yes)
RMLIBSRC += roadmap_sound.c
endif

RMLIBOBJ = $(CANVAS_OBJS) $(RMLIBSRC:.c=.o)

HEADERS =

TARGETS = roadmap.apk
# TARGETS += demomaps.apk

# --- The real targets --------------------------------------------

all:: $(TARGETS)

depends:
	$(MAKEDEPS) -Y -f - $(RMLIBSRC) > .depends.mk 2>/dev/null

-include .depends.mk

roadmap.apk:	libs/armeabi/librmnative.so AndroidManifest.xml classes.dex \
	libs/armeabi/libexpat-1.so 
	rm -f bin/${SRCNAME}.ap_
	${SDKPLAT}/tools/aapt package -m -J gen -M AndroidManifest.xml -S res -I ${SDKPLAT}/android.jar -F bin/${SRCNAME}.ap_
	apkbuilder roadmap.apk -z bin/${SRCNAME}.ap_ -f classes.dex -nf libs
#	-f demomaps/usc06075.rdm -f demomaps/usc81002.rdm -f demomaps/usdir.rdm
# apkbuilder roadmap.apk -nf . -rj . -f librmnative.so -f classes.dex -f AndroidManifest.xml
# apkbuilder roadmap.apk -z bin/${SRCNAME}.ap_ -f librmnative.so -f classes.dex
#

libs/armeabi/libexpat-1.so:	/opt/android/lib/libexpat-1.so
	cp /opt/android/lib/libexpat-1.so libs/armeabi/libexpat-1.so

#
# I used this with Android 2.1
#

NDKHOME=	/home/danny/src/android/android-ndk-r4
SDKHOME=	/home/danny/src/android/android-sdk-linux_86
SDKPLAT=	${SDKHOME}/platforms/android-8
NDK_CFLAGS=	-I$(NDKHOME)/build/platforms/android-5/arch-arm/usr/include \
		-DANDROID -fno-short-enums -g
NDK_LDFLAGS=	-nostdlib -Wl,-shared,-Bsymbolic -Wl,--no-undefined -Wl,-z,noexecstack
JAVA=		/usr/share/java
JAVAC_FLAGS=	-cp ${JAVA}/jaxp_parser_impl.jar:${JAVA}/xml-commons-apis.jar:${JAVA}/antlr.jar:${JAVA}/ant/ant-antlr.jar:${JAVA}/regexp.jar:${JAVA}/ant/ant-apache-regexp.jar:${JAVA}/jakarta-commons-logging.jar:${JAVA}/ant/ant-commons-logging.jar:${JAVA}/javamail.jar:${JAVA}/jaf.jar:${JAVA}/ant/ant-javamail.jar:${JAVA}/jsch.jar:${JAVA}/ant/ant-jsch.jar:/usr/lib/jvm/java/lib/tools.jar:${SDKPLAT}/android.jar -sourcepath gen:src -bootclasspath ${SDKPLAT}/android.jar
SRCNAME=	RoadMap
CLSES=		${DIRHIER}/RoadMap.class ${DIRHIER}/Panel.class ${DIRHIER}/Pen.class

DIRHIER=	net/sourceforge/projects/roadmap

classes.dex:	${CLSES}
	# ${SDKPLAT}/tools/dx --dex --output=$@ ${DIRHIER}
	${SDKPLAT}/tools/dx --dex --output=$@ ${DIRHIER}/*.class
# ${SDKPLAT}/tools/dx --dex --output=$@ ${DIRHIER}/${SRCNAME}.class

gen/net/sourceforge/projects/roadmap/R.java: AndroidManifest.xml Makefile
	aapt package -m -J gen -M AndroidManifest.xml -S res -I ${SDKPLAT}/android.jar

${DIRHIER}/RoadMap.class:	src/${DIRHIER}/RoadMap.java gen/net/sourceforge/projects/roadmap/R.java
	javac ${JAVAC_FLAGS} -d . src/${DIRHIER}/RoadMap.java

${DIRHIER}/RoadMapCanvas.class:	src/${DIRHIER}/RoadMapCanvas.java
	javac ${JAVAC_FLAGS} -d . src/${DIRHIER}/RoadMapCanvas.java

${DIRHIER}/Panel.class:	src/${DIRHIER}/Panel.java
	javac ${JAVAC_FLAGS} -d . src/${DIRHIER}/Panel.java

${DIRHIER}/Pen.class:	src/${DIRHIER}/Pen.java
	javac ${JAVAC_FLAGS} -d . src/${DIRHIER}/Pen.java

${DIRHIER}/TutorialThread.class:	src/${DIRHIER}/TutorialThread.java
	javac ${JAVAC_FLAGS} -d . src/${DIRHIER}/TutorialThread.java

RMLIBS=	../libguiroadmap.a ../libroadmap.a ../gpx/libgpx.a ../unix/libosroadmap.a \
	-L/opt/android/lib -lexpat-1
RMDLIBS=	../libguiroadmap.a ../libroadmap.a ../gpx/libgpx.a ../unix/libosroadmap.a

RMASRC=		roadmap_canvas.c roadmap_dialog.c roadmap_fileselection.c \
		roadmap_keyboard.c roadmap_main.c roadmap_messagebox.c \
		roadmap_progress.c roadmap_androidgps.c \
		../win32/roadmap_colors.c

libs/armeabi/gdbserver:	${NDKHOME}/build/prebuilt/linux-x86/arm-eabi-4.4.0/bin/gdbserver
	cp ${NDKHOME}/build/prebuilt/linux-x86/arm-eabi-4.4.0/bin/gdbserver libs/armeabi/gdbserver

RMANDROID=	${RMASRC:.c=.o}

libs/armeabi/librmnative.so:	${RMANDROID} ${RMDLIBS}
	arm-eabi-gcc --Wl,-soname,librmnative.so -o libs/armeabi/librmnative.so \
		${NDK_CFLAGS} ${NDK_LDFLAGS} \
		${RMANDROID} \
		${RMLIBS} \
		-lgcc \
		-L${NDKHOME}/build/platforms/android-5/arch-arm/usr/lib -llog -lc -lm
#		${NDKHOME}/build/platforms/android-8/arch-arm/usr/lib/crtbegin_dynamic.o \
#		${NDKHOME}/build/platforms/android-8/arch-arm/usr/lib/crtend_android.o

clean:
	rm -f *.o *.a *.so .depends.mk *.class *.apk
	rm -f *~ jni/*~ src/${DIRHIER}/*~
	rm -f classes.dex
	rm -f gen/${DIRHIER}/R.java
	rm -f bin/${SRCNAME}.ap_
	rm -f libs/armeabi/librmnative.so libs/armeabi/libexpat-1.so
	rm -f ${SRCNAME}.class
	rm -f ${DIRHIER}/*.class
	rm -f libs/armeabi/gdbserver

install: roadmap.apk libs/armeabi/gdbserver
	${ADB} install -r roadmap.apk
	${ADB} push libs/armeabi/gdbserver /data/data/net.sourceforge.projects.roadmap/lib/gdbserver

# copied from win32
# we aren't as rigorous about producing
# this list of source as we are for the other RoadMap
# directories.  (basically, pick up everything.)
sourcelist:
	@find . -name 'CVS' -prune -o -print

