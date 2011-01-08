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
 * @file
 * @brief Stuff to support the application startup on Android
 * @ingroup android
 */

#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>


#include "roadmap.h"
#include "roadmap_path.h"
#include "roadmap_start.h"
#include "roadmap_config.h"
#include "roadmap_history.h"
#include "roadmap_canvas.h"

#include "roadmap_main.h"
#include "roadmap_time.h"

#include <android/log.h>
#include "roadmap_jni.h"

struct roadmap_main_io {
   int id;
   RoadMapIO io;
   RoadMapInput callback;
};

#define ROADMAP_MAX_IO 16
static struct roadmap_main_io RoadMapMainIo[ROADMAP_MAX_IO];


struct roadmap_main_timer {
	int		id, interval;
	RoadMapCallback	callback;
};

#define ROADMAP_MAX_TIMER 16
static struct roadmap_main_timer RoadMapMainPeriodicTimer[ROADMAP_MAX_TIMER];


static char *RoadMapMainTitle = NULL;

static RoadMapKeyInput RoadMapMainInput = NULL;

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

void roadmap_main_toggle_full_screen (void)
{
}

void roadmap_main_new (const char *title, int width, int height)
{
//	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_new");
}

void roadmap_main_title(char *fmt, ...)
{
   char newtitle[200];
   va_list ap;
   int n;

   n = snprintf(newtitle, 200, "%s", RoadMapMainTitle);
   va_start(ap, fmt);
   vsnprintf(&newtitle[n], 200 - n, fmt, ap);
   va_end(ap);

//   __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_title(%s) FIX ME", newtitle);
}

void roadmap_main_set_keyboard (RoadMapKeyInput callback)
{
//	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_set_keyboard");
	RoadMapMainInput = callback;
}


/**
 * @brief Create a new menu.
 * On Android, this doesn't do much except assigning a unique number.
 *
 * @return returned value must be non-null for menus to work (see roadmap_factory.c)
 */
RoadMapMenu roadmap_main_new_menu (const char *title)
{
	jclass		cls = TheRoadMapClass();
	jmethodID	mid = TheMethod(cls, "CreateMenu", "(Ljava/lang/String;)I");
	jstring		js;

	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_new_menu(%s)", title);

	js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, title);
	int i = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, RoadMapThiz, mid, js);

	return (RoadMapMenu)i;
}


void roadmap_main_free_menu (RoadMapMenu menu)
{
//	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_free_menu FIX ME");
}


void roadmap_main_add_menu (RoadMapMenu menu, const char *label)
{
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_add_menu(%d,%s)", (int)menu, label);
	/*
	int		m = (int)menu;
	jclass		cls = TheRoadMapClass();
	jmethodID	mid = TheMethod(cls, "AddSubMenu", "(ILjava/lang/String;)I");
	jstring		js;

	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_add_menu(%d,%s)", m, label);

	js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, label);
	int i = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, RoadMapThiz, mid, m, js);
	*/
}


void roadmap_main_popup_menu (RoadMapMenu menu, const RoadMapGuiPoint *position)
{
	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_popup_menu FIX ME");
}


struct callback {
	RoadMapCallback	callback;
	char	*label;			// only for debug
} callbacks[150] /* FIX ME make dynamic */;

void roadmap_main_add_menu_item (RoadMapMenu menu,
                                 const char *label,
                                 const char *tip,
                                 RoadMapCallback callback)
{
	if (label == NULL) {
		// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_add_menu_item(%d,NULL,cb %p)", menu, callback);
		return;
	}

	int		m = (int)menu;
	jclass		cls = TheRoadMapClass();
	jmethodID	mid = TheMethod(cls, "AddMenuItem", "(ILjava/lang/String;)I");
	jstring		js;

	js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, label);
	int i = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, RoadMapThiz, mid, m, js);

	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_add_menu_item(%d,%s,cb %p) -> %d", m, label, callback, i);

	callbacks[i].callback = callback;
	callbacks[i].label = strdup(label);	// only for debug
}

/**
 * @brief call a callback from the menu system
 * @param item the index that this menu entry was given, to be used in our table
 *  (Note: Android doesn't seem to provide a simple way to pass some info so we
 *   need to add yet another table.)
 * @return feedback for the onOptionsMenuSelected -> 1 for true, 0 for false.
 */
int roadmap_main_callback(int item)
{
	if (callbacks[item].callback) {
//		__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_callback(%d,%s) %p", item, callbacks[item].label, callbacks[item].callback);
		(*(callbacks[item].callback))();
		return 1;
	}
//	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_callback(%d,%s) %p -> 0", item, callbacks[item].label, callbacks[item].callback);
	return 0;
}

/**
 * @brief
 * @return 1 if ok, 0 if not found
 */
int
Java_net_sourceforge_projects_roadmap_RoadMap_MenuCallback(JNIEnv* env, jobject thiz, int id)
{
	return roadmap_main_callback(id);
}

void roadmap_main_add_separator (RoadMapMenu menu)
{
	roadmap_main_add_menu_item (menu, NULL, NULL, NULL);
}


void roadmap_main_add_toolbar (const char *orientation)
{
	jclass          cls = TheRoadMapClass();
	jmethodID       mid = TheMethod(cls, "AddToolbar", "(Ljava/lang/String;)V");
	jstring         js;

	js = orientation ? (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, orientation) : NULL;
	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid, js);
}

#define	MAXTOOLCALLBACKS	20
static RoadMapCallback	ToolCallbacks[MAXTOOLCALLBACKS];
static int nToolCallbacks = 0;

void roadmap_main_add_tool (const char *label,
                            const char *icon,
                            const char *tip,
                            RoadMapCallback callback)
{
	jclass          cls;
	jmethodID       mid;
	jstring         jslabel, jsicon, jstip;

	if ((label == 0 || strlen(label) == 0) && (icon == 0 || strlen(icon) == 0))
		return;
	if (callback == 0)
		return;

	cls = TheRoadMapClass();
	mid = TheMethod(cls, "AddTool",
		"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V");
 
	/*
	 * Locate the icon bitmap here, or pass NULL.
	 * The Java code doesn't handle this because we do it here.
	 */
	const char *fnicon = roadmap_path_search_icon(icon);
	jsicon = fnicon ? (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, fnicon) : NULL;

	jslabel = label ? (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, label) : NULL;
	jstip = tip ? (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, tip) : NULL;

	if (nToolCallbacks == MAXTOOLCALLBACKS) {
		// FIX ME should throw an exception
		return;
	}
	ToolCallbacks[nToolCallbacks] = callback;

	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid,
		jslabel, jsicon, jstip, nToolCallbacks);

	nToolCallbacks++;
}

void
Java_net_sourceforge_projects_roadmap_RoadMap_ToolbarCallback(JNIEnv* env, jobject thiz, int ix)
{
	/* No need to check, in theory. */
	(*ToolCallbacks[ix])();
}

void roadmap_main_add_tool_space (void)
{
//	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_add_tool_space FIX ME");
}

void roadmap_main_set_cursor (RoadMapCursor newcursor)
{
//	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_set_cursor FIX ME");
}

/**
 * @brief add a drawing canvas. On Android: already done in Java (Panel).
 */
void roadmap_main_add_canvas (void)
{
//	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_add_canvas");
}


void roadmap_main_add_status (void)
{
//	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_add_status FIX ME");
}

/**
 * @brief show the ui. On Android, this is in Java - the Activity.
 */
void roadmap_main_show (void)
{
//	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_main_show");
}

/**
 * @brief Organize a system dependent way to handle input from this source,
 * and call the callback when something happens on it.
 * On Android, just make a registry in the table.
 * The only call we're currently getting is from the GPS, handled in Java code.
 *
 * @param io description of the input
 * @param callback call this function when awakened
 */
void roadmap_main_set_input (RoadMapIO *io, RoadMapInput callback)
{
   int i;

   for (i = 0; i < ROADMAP_MAX_IO; ++i) {
      if (RoadMapMainIo[i].io.subsystem == ROADMAP_IO_INVALID) {
         RoadMapMainIo[i].io = *io;
         RoadMapMainIo[i].callback = callback;
         RoadMapMainIo[i].id = 0;
         break;
      }
   }
}

/**
 * @brief Remove an input handler
 * @param io description of this input handler
 */
void roadmap_main_remove_input (RoadMapIO *io)
{
   int i;
   int fd = io->os.file;

   for (i = 0; i < ROADMAP_MAX_IO; ++i) {
      if (RoadMapMainIo[i].io.os.file == fd) {
         RoadMapMainIo[i].io.os.file = -1;
         RoadMapMainIo[i].io.subsystem = ROADMAP_IO_INVALID;
         break;
      }
   }
}

/**
 * @brief Call the callback periodically from now on
 * @param interval
 * @param callback
 */
void roadmap_main_set_periodic (int interval, RoadMapCallback callback)
{
   int index;
   struct roadmap_main_timer *timer = NULL;

   for (index = 0; index < ROADMAP_MAX_TIMER; ++index) {
      if (RoadMapMainPeriodicTimer[index].callback == callback) {
	 /* this should not happen */
	 roadmap_log(ROADMAP_ERROR, "roadmap_main_set_periodic - duplicate %d %p",
			 interval, callback);
         return;
      }
      if (RoadMapMainPeriodicTimer[index].callback == NULL) {
         timer = RoadMapMainPeriodicTimer + index;
	 break;
      }
   }

   if (timer == NULL) {
      roadmap_log (ROADMAP_FATAL, "Timer table saturated");
   } else {
      jclass	cls = TheRoadMapClass();
      jmethodID	mid = TheMethod(cls, "SetPeriodic", "(II)V");

      (*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid, index, interval);

      timer->callback = callback;
      timer->interval = interval;
   }
}

/**
 * @brief helper function to call the callback function specified by roadmap_main_set_periodic
 * @param env JNI environment
 * @param thiz JNI object
 * @param index index into the RoadMapMainPeriodicTimer array
 */
void
Java_net_sourceforge_projects_roadmap_RoadMap_CallPeriodic(JNIEnv* env, jobject thiz, int index)
{
	if (RoadMapMainPeriodicTimer[index].callback)
		(*RoadMapMainPeriodicTimer[index].callback)();
}

/**
 * @brief Remove a periodically called callback
 * @param callback
 */
void roadmap_main_remove_periodic (RoadMapCallback callback)
{
   int index;

   for (index = 0; index < ROADMAP_MAX_TIMER; ++index) {
      if (RoadMapMainPeriodicTimer[index].callback == callback) {
	      jclass	cls = TheRoadMapClass();
	      jmethodID	mid = TheMethod(cls, "RemovePeriodic", "(I)V");

	      (*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid, index);

	      RoadMapMainPeriodicTimer[index].callback = NULL;
      }
   }
}

/**
 * @brief store the function to be called periodically when "idle"
 */
static RoadMapCallback idle_callback;

/**
 * @brief function to call the idle function callback if it is set
 * @param data ignored
 * @return always 0
 */
int roadmap_main_idle_function_helper (void *data)
{
	if (idle_callback) {
		idle_callback();
	} else {
		roadmap_log(ROADMAP_WARNING, "There is no idle callback");
	}
	return 0;
}

/**
 * @brief register a handler to be called when idle (note : triggered only once), and trigger it.
 * @param callback the function to be called
 */
void roadmap_main_set_idle_function (RoadMapCallback callback)
{
	idle_callback = callback;

	jclass	cls = TheRoadMapClass();
	jmethodID	mid = TheMethod(cls, "TriggerIdleFunction", "()V");
	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid);
}

/**
 * @brief stop the "idle" callback
 */
void roadmap_main_remove_idle_function (void)
{
	idle_callback = NULL;
}


/**
 * @brief set a title text FIX ME
 * @param text
 */
void roadmap_main_set_status (const char *text)
{
}

/**
 * @brief Flush the event queue.
 * Empty on Android
 */
int roadmap_main_flush (void)
{
	return 0;
}

/**
 * @brief Flush the event queue.
 * @param deadline
 * Empty on Android
 */
int roadmap_main_flush_synchronous (int deadline)
{
	return 0;
}

jstring
Java_net_sourceforge_projects_roadmap_RoadMap_roadmapStart(JNIEnv* env, jobject thiz)
{
	char	*argv[] = { "roadmap", NULL };
	int	argc = 1;

roadmap_log(ROADMAP_ERROR, "RoadMap_roadmapStart");
	roadmap_option(argc, argv, 0, NULL);
	roadmap_start(argc, argv);
roadmap_log(ROADMAP_ERROR, "RoadMap_roadmapStart (end)");

	return (*env)->NewStringUTF(env, "Hello");
}

jstring
Java_net_sourceforge_projects_roadmap_RoadMap_roadmapStartExit(JNIEnv* env, jobject thiz)
{
	roadmap_start_exit();
	return 0;
}

jint
JNI_OnLoad(JavaVM *vm, void *reserved)
{
	return JNI_VERSION_1_6;
}

void
JNI_OnUnload(JavaVM *vm, void *reserved)
{
}

jclass		RoadMapJniClass = 0;
JNIEnv		*RoadMapJniEnv = 0;
jobject		RoadMapThiz = 0;
jobject		PanelThiz = 0;

void
Java_net_sourceforge_projects_roadmap_RoadMap_JniStart(JNIEnv* env, jobject thiz)
{
	RoadMapJniEnv = env;

	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "RoadMap->JniStart");

	RoadMapJniClass = (*env)->NewGlobalRef(env, 
		(*env)->FindClass(env, "net/sourceforge/projects/roadmap/RoadMap"));
	RoadMapThiz = (jobject)(*env)->NewGlobalRef(env, thiz);
}

/**
 * @brief
 * run this function only when both the Panel and the Menu are active
 */
static void RoadMapStart(void)
{
	int	argc = 1;
	char	*argv[] = { "RoadMap", 0 };
	extern void roadmap_canvas_configure(void);

	roadmap_option (argc, argv, 0, NULL);
	roadmap_start (argc, argv);

	/*
	 * FIX ME
	 * Do we have signals on Android ?
	 *
	 * roadmap_signals_init ();
	 */
	roadmap_canvas_configure();
}

void
Java_net_sourceforge_projects_roadmap_Panel_RoadMapStart(JNIEnv* env, jobject thiz)
{
	RoadMapStart();
}

void
Java_net_sourceforge_projects_roadmap_Panel_PanelJniStart(JNIEnv* env, jobject thiz)
{
	PanelThiz = (*env)->NewGlobalRef(env, thiz);
}

void
Java_net_sourceforge_projects_roadmap_RoadMap_CallIdleFunction(JNIEnv* env, jobject thiz)
{
	extern int roadmap_main_idle_function_helper(void *data);

	(void)roadmap_main_idle_function_helper(NULL);
}

#if 0
void
Java_net_sourceforge_projects_roadmap_RoadMap_KickRedraw(JNIEnv* env, jobject thiz)
{
	roadmap_start_request_repaint_map (REPAINT_NOW);
}
#endif

/**
 * @brief Hard exit
 * @return 1 if ok, 0 if not found
 */
void
Java_net_sourceforge_projects_roadmap_RoadMap_HardExit(JNIEnv* env, jobject thiz, int rc)
{
	exit(rc);
}

/**
 * @brief Terminate RoadMap
 */
void roadmap_main_exit (void)
{
	jclass		cls;
	jmethodID	mid;

	roadmap_start_exit ();

	myRmClassCache = (jclass) 0;
	idle_callback = NULL;
	nToolCallbacks = 0;

	roadmap_canvas_shutdown();

	/* Terminate the Java code. */
	cls = TheRoadMapClass();
	mid = TheMethod(cls, "Finish", "()V");
	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid);
}

void roadmap_android_test(void)
{
	char *p = NULL;
	*p = 'a';
}
