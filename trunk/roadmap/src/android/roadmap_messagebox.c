/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright 2010 Danny Backx
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
 */

/**
 * @brief
 * @file manage the roadmap dialogs used for user info.
 */

#include <stdlib.h>
#include "roadmap_jni.h"

#include "roadmap.h"
#include "roadmap_start.h"

#define __ROADMAP_MESSAGEBOX_NO_LANG
#include "roadmap_messagebox.h"

#define	MYCLS2	"net/sourceforge/projects/roadmap/RoadMap"
static jclass	myRmClassCache = (jclass) 0;

static jclass TheRoadMapClass()
{
	if (myRmClassCache == 0) {
		myRmClassCache = (*RoadMapJniEnv)->FindClass(RoadMapJniEnv, MYCLS2);
		myRmClassCache = (*RoadMapJniEnv)->NewGlobalRef(RoadMapJniEnv, myRmClassCache);
	}
	if (myRmClassCache == 0) {
		__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "Class not found");
		// throw
		(*RoadMapJniEnv)->ThrowNew(RoadMapJniEnv,
			(*RoadMapJniEnv)->FindClass(RoadMapJniEnv, "java/io/IOException"),
			"A JNI Exception occurred");
	}

	return myRmClassCache;
}

static jmethodID TheMethod(const jclass cls, const char *name, const char *signature)
{
	jmethodID	mid;

	mid = (*RoadMapJniEnv)->GetMethodID(RoadMapJniEnv, cls, name, signature);
	if (mid == 0) {
		(*RoadMapJniEnv)->ThrowNew(RoadMapJniEnv,
			(*RoadMapJniEnv)->FindClass(RoadMapJniEnv, "java/io/IOException"),
			"A JNI Exception occurred");
	}
	return mid;
}
void roadmap_messagebox_hide (void *handle)
{
}

void *roadmap_messagebox (const char *title, const char *message)
{
	jclass          cls = TheRoadMapClass();
	jmethodID       mid = TheMethod(cls, "MessageBox", "(Ljava/lang/String;Ljava/lang/String;)V");
	jstring         jstitle, jsmessage;

	jstitle = title ? (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, title) : NULL;
	jsmessage = message ? (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, message) : NULL;

	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid, jstitle, jsmessage);
}

void *roadmap_messagebox_wait (const char *title, const char *message)
{
}

/**
 * @brief show a message, then terminate
 * All of the real action is in Java, except the hard exit, which is in roadmap_main.c
 */
void roadmap_messagebox_die (const char *title, const char *message)
{
	jclass          cls = TheRoadMapClass();
	jmethodID       mid = TheMethod(cls, "MessageBoxWait", "(Ljava/lang/String;Ljava/lang/String;)V");
	jstring         jstitle, jsmessage;

	jstitle = title ? (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, title) : NULL;
	jsmessage = message ? (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, message) : NULL;

	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid, jstitle, jsmessage);
}
