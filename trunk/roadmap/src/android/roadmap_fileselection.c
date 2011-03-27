/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright (c) 2010, 2011 Danny Backx
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
 * @file
 * @brief manage the Widget used in roadmap dialogs.
 * @ingroup android
 */

#include <string.h>
#include <stdlib.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_start.h"
#include "roadmap_jni.h"

#include "roadmap_fileselection.h"
#include "roadmap_file.h"


/* Some cross compiler environment don't define this ? */
#ifndef PATH_MAX
#define PATH_MAX 512
#endif


/* We maintain a list of dialogs that have been created. */

struct roadmap_fileselection_item;
typedef struct roadmap_fileselection_item RoadMapFileSelection;
    
/**
 * @brief
 */
struct roadmap_fileselection_item {

    RoadMapFileSelection *next;
    
    const char *title;
    const char *mode;

    // GtkWidget *dialog;
    RoadMapFileCallback callback;

};


static RoadMapFileSelection *RoadMapFileWindows = NULL;

/*
 * JNI
 */
#define MYCLS2  "net/sourceforge/projects/roadmap/RoadMap"
static jclass   myRmClassCache = (jclass) 0;

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
   jmethodID   mid;

   mid = (*RoadMapJniEnv)->GetMethodID(RoadMapJniEnv, cls, name, signature);
   if (mid == 0) {
      (*RoadMapJniEnv)->ThrowNew(RoadMapJniEnv,
         (*RoadMapJniEnv)->FindClass(RoadMapJniEnv, "java/io/IOException"),
         "A JNI Exception occurred");
   }
   return mid;
}


/**
 * @brief huh ? always returns NULL
 * @param title
 * @return
 */
static RoadMapFileSelection *roadmap_fileselection_search (const char *title) {
    
    RoadMapFileSelection *item;
    
    for (item = RoadMapFileWindows; item != NULL; item = item->next) {
        if (strcmp (title, item->title) == 0) {
            break;
        }
    }
    
    return item;
}

RoadMapFileCallback	global_cb;
char			*global_mode = NULL;

/**
 * @brief open a file selection dialog
 * @param title
 * @param filter
 * @param path
 * @param mode
 * @param callback
 */
void roadmap_fileselection_new (const char *title,
                                const char *filter,
                                const char *path,
                                const char *mode,
                                RoadMapFileCallback callback) {
   // roadmap_log(ROADMAP_WARNING, "roadmap_fileselection_new(%s,%s,%s,%s)", title, filter, path, mode);

   global_cb = callback;
   if (global_mode)
      free(global_mode);
   global_mode = strdup(mode);


   jclass       cls = TheRoadMapClass();
   jmethodID    mid = TheMethod(cls, "FileSelection",
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)I");

   jstring      jst = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, title);
   jstring      jsf = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, filter);
   jstring      jsp = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, path);
   int          r, imode;

   imode = 0;
   if (mode) {
      if (mode[0] == 'r') imode = 0;
      if (mode[0] == 'w') imode = 1;
   }
   r = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, RoadMapThiz, mid, jst, jsf, jsp, imode);

   if (r < 0) { /* No suitable method for selecting a file */
      callback("/sdcard/roadmap/logfile.txt", mode);
   }
}

/**
 * @brief function to return string data
 * @param env JNI standard environment pointer
 * @param thiz JNI standard object from which we're being called
 * @param js the string we're getting from Java
 */
void Java_net_sourceforge_projects_roadmap_RoadMap_FileSelectionResult(JNIEnv* env, jobject thiz, jstring js)
{
	char *s = (*env)->GetStringUTFChars(env, js, NULL);
	(*global_cb)(s, global_mode);
}
