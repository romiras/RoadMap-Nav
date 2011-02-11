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

#
# I used this with Android 2.3
#

NDKHOME=	/home/danny/src/android/android-ndk-r5
NDK_LEVEL=	9
NDK_ARCH=	arm
NDK_NAME=	arm-eabi-4.4.0
NDK_PREFIX=	linux-x86
NDK_SYSROOT=	${NDKHOME}/platforms/android-${NDK_LEVEL}/arch-${NDK_ARCH}
NDK_CC=		${NDKHOME}/toolchains/${NDK_NAME}/prebuilt/${NDK_SYSTEM}/bin/${NDK_PREFIX}gcc --sysroot=${NDK_SYSROOT}
NDK_LDFLAGS=	'-Wl,--fix-cortex-a8'

SDKHOME=	/home/danny/src/android/android-sdk-linux_86
SDK_ANDV=	android-9
SDKPLAT=	${SDKHOME}/platforms/${SDK_ANDV}
NDK_CFLAGS=	-I$(NDKHOME)/platforms/android-${NDK_LEVEL}/arch-${NDK_ARCH}/usr/include \
		-DANDROID -fno-short-enums -g
NDK_LDFLAGS=	-nostdlib -Wl,-shared,-Bsymbolic -Wl,--no-undefined -Wl,-z,noexecstack
JAVA=		/usr/share/java
JAVAC_FLAGS=	-cp ${JAVA}/jaxp_parser_impl.jar:${JAVA}/xml-commons-apis.jar:${JAVA}/antlr.jar:${JAVA}/ant/ant-antlr.jar:${JAVA}/regexp.jar:${JAVA}/ant/ant-apache-regexp.jar:${JAVA}/jakarta-commons-logging.jar:${JAVA}/ant/ant-commons-logging.jar:${JAVA}/javamail.jar:${JAVA}/jaf.jar:${JAVA}/ant/ant-javamail.jar:${JAVA}/jsch.jar:${JAVA}/ant/ant-jsch.jar:/usr/lib/jvm/java/lib/tools.jar:${SDKPLAT}/android.jar -sourcepath gen:src -bootclasspath ${SDKPLAT}/android.jar
SRCNAME=	RoadMap
CLSES=		${DIRHIER}/RoadMap.class ${DIRHIER}/Panel.class ${DIRHIER}/Pen.class
DX=		${SDKHOME}/platform-tools/dx
AAPT=		${SDKHOME}/platform-tools/aapt
APKBUILDER=	${SDKHOME}/tools/apkbuilder
ZIPALIGN=	${SDKHOME}/tools/zipalign
ZIP_PARAM=	-f 4
#
# Note jarsigner is not an Android but a Java tool, so it's in the JDK,
# not in Android SDK or NDK.
#
JARSIGNER=	jarsigner

DIRHIER=	net/sourceforge/projects/roadmap

# --- The real targets --------------------------------------------

all:: $(TARGETS)

depends:
	$(MAKEDEPS) -Y -f - $(RMLIBSRC) > .depends.mk 2>/dev/null

-include .depends.mk

roadmap.apk:	roadmap-3.apk
	-rm -f $@
	${JARSIGNER} -verbose -keystore ${HOME}/.android/debug.keystore -storepass android -signedjar $@ roadmap-3.apk androiddebugkey

roadmap-3.apk:	roadmap-2.apk
	-rm -f $@
	${ZIPALIGN} ${ZIP_PARAM} roadmap-2.apk roadmap-3.apk

#
# Note Android 2.3 (SDK v9) apkbuilder says it's deprecated but it still works
# and reply to inquiry in forums says there's no alternative.
#
roadmap-2.apk:	roadmap-1.apk
	-rm -f $@
	${APKBUILDER} roadmap-2.apk -u -z roadmap-1.apk  -f bin/classes.dex -rf src -nf libs

roadmap-1.apk: libs/armeabi/librmnative.so AndroidManifest.xml bin/classes.dex \
	libs/armeabi/libexpat-1.so libs/armeabi/gdbserver libs/armeabi/gdb.setup \
	res/values/strings.xml res/values/style.xml res/layout/list_item.xml
	-rm -f $@
	${AAPT} package -f -M AndroidManifest.xml -S res -I ${SDKPLAT}/android.jar -F roadmap-1.apk

libs/armeabi/libexpat-1.so:	/opt/android/lib/libexpat-1.so
	cp /opt/android/lib/libexpat-1.so libs/armeabi/libexpat-1.so

bin/classes.dex:	${CLSES}
	-mkdir -p bin
	${DX} --dex --output=$@ ${DIRHIER}/*.class

gen/net/sourceforge/projects/roadmap/R.java: AndroidManifest.xml Makefile
	-mkdir -p gen
	${AAPT} package -m -J gen -M AndroidManifest.xml -S res -I ${SDKPLAT}/android.jar

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

RMASRC=		roadmap_canvas.c \
		roadmap_dialog.c \
		roadmap_fileselection.c \
		roadmap_keyboard.c \
		roadmap_main.c \
		roadmap_messagebox.c \
		roadmap_progress.c \
		roadmap_androidgps.c \
		../win32/roadmap_colors.c \
		roadmap_sound.c

libs/armeabi/gdbserver:	${NDKHOME}/toolchains/arm-eabi-4.4.0/prebuilt/gdbserver
	cp ${NDKHOME}/toolchains/arm-eabi-4.4.0/prebuilt/gdbserver libs/armeabi/gdbserver

libs/armeabi/gdb.setup:
	echo "set solib-search-path libs/armeabi" >$@
	echo "" >>$@

RMANDROID=	${RMASRC:.c=.o}

libs/armeabi/librmnative.so:	${RMANDROID} ${RMDLIBS}
	-mkdir -p libs/armeabi
	arm-eabi-gcc --Wl,-soname,librmnative.so -o libs/armeabi/librmnative.so \
		${NDK_CFLAGS} ${NDK_LDFLAGS} \
		${RMANDROID} \
		${RMLIBS} \
		-lgcc \
		-L${NDKHOME}/platforms/android-${NDK_LEVEL}/arch-${NDK_ARCH}/usr/lib -llog -lc -lm

clean:
	rm -f *.o *.a *.so .depends.mk *.class *.apk
	rm -f *~ jni/*~ src/${DIRHIER}/*~
	rm -f bin/classes.dex
	rm -f gen/${DIRHIER}/R.java
	rm -f bin/${SRCNAME}.ap_
	rm -f libs/armeabi/librmnative.so libs/armeabi/libexpat-1.so
	rm -f ${SRCNAME}.class
	rm -f ${DIRHIER}/*.class
	rm -f libs/armeabi/gdbserver
	rm -f roadmap-1.apk roadmap-2.apk roadmap-3.apk
	rm -f libs/armeabi/gdb.setup

install: roadmap.apk libs/armeabi/gdbserver
	${ADB} install -r roadmap.apk
	${ADB} push libs/armeabi/gdbserver /data/data/net.sourceforge.projects.roadmap/lib/gdbserver

# copied from win32
# we aren't as rigorous about producing
# this list of source as we are for the other RoadMap
# directories.  (basically, pick up everything.)
sourcelist:
	@find . -name 'CVS' -prune -o -print

