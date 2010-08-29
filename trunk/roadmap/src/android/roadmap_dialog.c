/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright (c) 2008, 2010, Danny Backx.
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
 * @brief roadmap_dialog.c - manage the Widget used in roadmap dialogs.
 * @ingroup android
 */

#include <string.h>
#include <stdlib.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_start.h"
#include "roadmap_jni.h"

#define __ROADMAP_DIALOG_NO_LANG
#include "roadmap_dialog.h"


#define ROADMAP_WIDGET_CONTAINER 0
#define ROADMAP_WIDGET_ENTRY     1
#define ROADMAP_WIDGET_CHOICE    2
#define ROADMAP_WIDGET_BUTTON    3
#define ROADMAP_WIDGET_LIST      4
#define ROADMAP_WIDGET_LABEL     5
#define ROADMAP_WIDGET_HIDDEN    6

enum {
    RM_LIST_WAYPOINT_NAME,
    RM_LIST_WAYPOINT_COLUMNS
};

/* We maintain a three-level tree of lists:
 * level 1: list of dialogs.
 * level 2: for each dialog, list of frames.
 * level 3: for each frame, list of "input" items.
 * In addition, "choice" items have a list of values.
 */
typedef struct {

   char *typeid;

   struct roadmap_dialog_item *item;
   RoadMapDialogCallback callback;
   char *value;

} RoadMapDialogSelection;


struct roadmap_dialog_item;
typedef struct roadmap_dialog_item *RoadMapDialogItem;

struct roadmap_dialog_item {

   char *typeid;

   struct roadmap_dialog_item *next;
   struct roadmap_dialog_item *parent;

   char *name;

   void *context;  /* References a caller-specific context. */

   int widget_type;
   int w;

   short rank;
   short count;
   RoadMapDialogItem children;

   RoadMapDialogCallback callback;

   char *value;
   RoadMapDialogSelection *choice;
};

static RoadMapDialogItem RoadMapDialogWindows = NULL;
static RoadMapDialogItem RoadMapDialogCurrent = NULL;

/*
 * JNI
 */
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

/*
 * roadmap_dialog.c
 */
static RoadMapDialogItem roadmap_dialog_get (RoadMapDialogItem parent,
                                             const char *name) {
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_get(%s)", name);
   RoadMapDialogItem child;

   if (parent == NULL) {
      child = RoadMapDialogWindows;
   } else {
      child = parent->children;
   }

   while (child != NULL) {
      if (strcmp (child->name, name) == 0) {
         return child;
      }
      child = child->next;
   }

   /* We did not find this child: create a new one. */

   child = (RoadMapDialogItem) malloc (sizeof (*child));

   roadmap_check_allocated(child);

   child->typeid = "RoadMapDialogItem";

   child->widget_type = ROADMAP_WIDGET_CONTAINER; /* Safe default. */
   child->w        = NULL;
   child->count    = 0;
   child->name     = strdup(name);
   child->context  = NULL;
   child->parent   = parent;
   child->children = NULL;
   child->callback = NULL;
   child->value    = "";
   child->choice   = NULL;

   if (parent != NULL) {

      child->rank = parent->count;
      child->next = parent->children;
      parent->children = child;
      parent->count += 1;

   } else {

      /* This is a top-level list element (dialog window). */
      if (RoadMapDialogWindows == NULL) {
         child->rank = 0;
      } else {
         child->rank = RoadMapDialogWindows->rank + 1;
      }
      child->next = RoadMapDialogWindows;
      RoadMapDialogWindows = child;
      RoadMapDialogCurrent = child;
   }

   return child;
}


static void roadmap_dialog_hide_window (RoadMapDialogItem dialog) {
#if 0
   if (dialog->w != NULL) {
      gtk_widget_hide (dialog->w);
   }
#endif
}

static RoadMapDialogItem roadmap_dialog_new_item (const char *frame, const char *name) {
   RoadMapDialogItem parent;
   RoadMapDialogItem child;

   // __android_log_print (ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_new_item(%s,%s)", frame, name);

   parent = roadmap_dialog_get (RoadMapDialogCurrent, frame);
   child  = roadmap_dialog_get (parent, name);
   // __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_new_item -> parent->w %d", parent->w);
   // if (child) __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_new_item -> child->w %d", child->w);

   if (parent->w == NULL) {
	/*
	 * New sub-dialog : create a separate Android dialog
	 */
	jclass		cls = TheRoadMapClass();
	jmethodID	mid = TheMethod(cls, "CreateDialog", "(Ljava/lang/String;I)I");
	jstring		js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, frame);

	parent->w = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, RoadMapThiz, mid, js,
			parent->parent->w);
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "CreateDialog -> parent->w %d", parent->w);
   }

   /*
    * Create a widget to show the name of this object.
    * A separate widget will be the data entry field (e.g. created by roadmap_dialog_new_entry).
    */
   if (name[0] != '.') {
	jclass		cls = TheRoadMapClass();
	jmethodID	mid = TheMethod(cls, "DialogAddButton", "(ILjava/lang/String;)I");
	jstring		js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, name);

	child->w = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, RoadMapThiz, mid, parent->w, js);
   } else {
	   /* ?? FIX ME Not sure what to do with this */
   }

   return child;
}

/**
 * @brief This function activates a dialog:
 * If the dialog did not exist yet, it will create an empty dialog
 * and roadmap_dialog_activate() returns 1; the application must then
 * enumerate all the dialog's items.
 * If the dialog did exist already, it will be shown on top and
 * roadmap_dialog_activate() returns 0.
 * This function never fails. The given dialog becomes the curent dialog.
 * @param name
 * @param context
 * @return returns 1 if this is a new, undefined, dialog; 0 otherwise.
 */
int roadmap_dialog_activate (const char *name, void *context)
{
	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_activate(%s)", name);
	RoadMapDialogItem dialog = roadmap_dialog_get (NULL, name);

	dialog->context = context;

	if (dialog->w != NULL) {
		/* The dialog exists already: show it on top. */
		RoadMapDialogCurrent = dialog;

		// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_activate(%s) -> id %d -> ShowDialog", name, dialog->w);

		jclass		cls = TheRoadMapClass();
		jmethodID	mid = TheMethod(cls, "ShowDialog", "(I)V");

		(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid, dialog->w);

		return 0; /* Tell the caller the dialog already exists. */
	}

	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_activate(%s) -> CreateDialog", name);

	/* Create the dialog's window. */
	jclass		cls = TheRoadMapClass();
	jmethodID	mid = TheMethod(cls, "CreateDialog", "(Ljava/lang/String;I)I");
	jstring		js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, name);

	dialog->w = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, RoadMapThiz, mid, js, 0);
// 	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "CreateDialog -> dialog->w %d, int name %s", dialog->w, dialog->name);

	return 1; /* Tell the caller this is a new, undefined, dialog. */
}

/**
 * @brief Hide the given dialog, if it exists.
 * @param name the dialog name
 */
void roadmap_dialog_hide (const char *name)
{
	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_hide(%s)", name);
   roadmap_dialog_hide_window (roadmap_dialog_get (NULL, name));
}

/**
 * @brief Add one text entry item to the current dialog
 * @param frame
 * @param name
 */
void roadmap_dialog_new_entry (const char *frame, const char *name) {
 	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_new_entry(%s,%s)", frame, name);
	jclass		cls = TheRoadMapClass();
	jmethodID	mid = TheMethod(cls, "DialogAddTextEntry", "(ILjava/lang/String;)I");
	jstring		js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, name);

	/* Side effect of .._new_item() : creates a widget with this name */
	RoadMapDialogItem child = roadmap_dialog_new_item (frame, name);
	// RoadMapDialogItem parent = roadmap_dialog_get (NULL, frame);
	// RoadMapDialogItem child  = roadmap_dialog_get (parent, name);
	RoadMapDialogItem parent = roadmap_dialog_get (RoadMapDialogCurrent, frame);

	/* Now also create the real "edit" widget */
	int w = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, RoadMapThiz, mid, parent->w, js);
}

/**
 * @brief Add one text label item to the current dialog
 * @param frame
 * @param name
 */
void roadmap_dialog_new_label (const char *frame, const char *name) {
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_new_label(%s,%s)", frame, name);
#if 0
   GtkWidget *w = gtk_label_new (name);
   RoadMapDialogItem child = roadmap_dialog_new_item (frame, name);

   child->widget_type = ROADMAP_WIDGET_LABEL;
#endif
}

/**
 * @brief Add one color selection item to to the current dialog
 * @param frame
 * @param name
 */
void roadmap_dialog_new_color (const char *frame, const char *name)
{
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_new_color(%s,%s)", frame, name);
   roadmap_dialog_new_entry (frame, name);
}

/**
 * @brief Add one hidden data item to the current dialog
 * @param frame
 * @param name
 */
void roadmap_dialog_new_hidden (const char *frame, const char *name)
{
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_new_hidden(%s,%s)", frame, name);
#if 0
   RoadMapDialogItem child = roadmap_dialog_new_item (frame, name);

   child->widget_type = ROADMAP_WIDGET_HIDDEN;
#endif
}

/**
 * @brief Add one choice item (a selection box or menu).
 * The optional callback is called each time a new selection is being made,
 * not when the OK button is called--that is the job of the OK button callback.
 * @param frame
 * @param name
 * @param count
 * @param current
 * @param labels
 * @param values
 * @param callback
 */
void roadmap_dialog_new_choice (const char *frame,
                                const char *name,
                                int count,
                                int current,
                                char **labels,
                                void *values,
                                RoadMapDialogCallback callback) {
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_new_choice(%s,%s)", frame, name);
}

/**
 * @brief Add one list item.
 * This item is similar to the choice one, except for two things:
 * 1) it uses a scrollable list widget instead of a combo box.
 * 2) the list of items shown is dynamic and can be modified (it is initially empty).
 * @param frame
 * @param name
 */
void roadmap_dialog_new_list (const char  *frame, const char  *name) {
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_new_list(%s,%s)", frame, name);
}


void roadmap_dialog_show_list (const char  *frame,
                               const char  *name,
                               int    count,
                               char **labels,
                               void **values,
                               RoadMapDialogCallback callback) {
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_show_list(%s,%s)", frame, name);
}

/**
 * @brief Add one button to the bottom of the dialog
 * @param label
 * @param callback
 */
void roadmap_dialog_add_button (const char *label, RoadMapDialogCallback callback)
{
	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_add_button(%s)", label);
}

/**
 * @brief When all done with building the dialog, call this to finalize and show.
 * @param use_keyboard
 */
void roadmap_dialog_complete (int use_keyboard)
{
	RoadMapDialogItem	dialog = RoadMapDialogCurrent;
	jclass			cls = TheRoadMapClass();
	jmethodID		mid = TheMethod(cls, "ShowDialog", "(I)V");

	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_complete %d", dialog->w);
	(*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid, dialog->w);
}


void roadmap_dialog_select (const char *dialog)
{
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_select(%s)", dialog);
   RoadMapDialogCurrent = roadmap_dialog_get (NULL, dialog);
}


void *roadmap_dialog_get_data (const char *frame, const char *name)
{
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_get_data(%s,%s)", frame, name);
}


void  roadmap_dialog_set_data (const char *frame, const char *name,
                               const void *data) {
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_set_data(%s,%s)", frame, name);
   RoadMapDialogItem this_frame, this_item;

   this_frame  = roadmap_dialog_get (RoadMapDialogCurrent, frame);
   this_item   = roadmap_dialog_get (this_frame, name);

   switch (this_item->widget_type) {

   case ROADMAP_WIDGET_ENTRY:

//      gtk_entry_set_text (GTK_ENTRY(this_item->w), (const char *)data);
      break;

   case ROADMAP_WIDGET_LABEL:

//      gtk_label_set_text (GTK_LABEL(this_item->w), (const char *)data);
      break;
   }
   this_item->value = (char *)data;
}

void roadmap_dialog_new_progress (const char *frame, const char *name)
{
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_new_progress(%s,%s)", frame, name);
#warning implement roadmap_dialog_new_progress
}

void  roadmap_dialog_set_progress (const char *frame, const char *name, int progress)
{
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_set_progress(%s,%s,%d)", frame, name, progress);
#warning implement roadmap_dialog_set_progress
}
