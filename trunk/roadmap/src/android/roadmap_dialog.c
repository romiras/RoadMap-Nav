/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright (c) 2008, 2010, 2011, Danny Backx.
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

#define	MAX_CALLBACKS	3

struct roadmap_dialog_item {

   char *typeid;

   struct roadmap_dialog_item *next;
   struct roadmap_dialog_item *parent;

   char *name;

   void *context;  /* References a caller-specific context. */

   int widget_type;
   int w;		/**< the dialog id */

   short rank;
   short count;
   RoadMapDialogItem children;

   RoadMapDialogCallback callback;

   int nbutton;				/**< number of buttons already seen */
   RoadMapDialogCallback callbacks[MAX_CALLBACKS];	/**< callbacks for buttons on some dialogs, Android supports up to three */

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

/**
 * @brief find the id of a dialog by name, create it if it doesn't exist yet
 * @param parent the dialog looked for should be a child of this parent (not recursive !)
 * @param name what are we looking fore
 * @return pointer to the dialog structure
 */
static RoadMapDialogItem roadmap_dialog_get (RoadMapDialogItem parent,
                                             const char *name) {
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_get(%s)", name);
   RoadMapDialogItem child;
   int i;

   if (parent == NULL) {
      child = RoadMapDialogWindows;
   } else {
      child = parent->children;
   }

   while (child != NULL) {
      if (strcmp (child->name, name) == 0) {
// 	 roadmap_log(ROADMAP_WARNING, "roadmap_dialog_get(%p,%s) -> %p", parent, name, child);
         return child;
      }
      child = child->next;
   }

   /* We did not find this child: create a new one. */

   child = (RoadMapDialogItem) malloc (sizeof (*child));

   roadmap_check_allocated(child);
   roadmap_log (ROADMAP_WARNING, "roadmap_dialog_get(%s) -> new %p, parent %p (%s)", name, child, parent, parent ? parent->name : "");

   child->typeid = "RoadMapDialogItem";

   child->widget_type = ROADMAP_WIDGET_CONTAINER; /* Safe default. */
   child->w        = 0;
   child->count    = 0;
   child->name     = strdup(name);
   child->context  = NULL;
   child->parent   = parent;
   child->children = NULL;
   child->callback = NULL;
   child->value    = "";
   child->choice   = NULL;

   child->nbutton = 0;
   for (i=0; i<MAX_CALLBACKS; i++)
      child->callbacks[i] = NULL;

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
//       roadmap_log(ROADMAP_WARNING, "roadmap_dialog_get(%s) -> current", name);
   }

//    roadmap_log(ROADMAP_WARNING, "roadmap_dialog_get(%p,%s) -> %p", parent, name, child);
   return child;
}

/**
 * @brief lookup by number, don't create a new one if not found
 *   Note this searches recursively, unlike its sister function(s).
 * @param parent look in the hierarchy under this
 * @param id look for this dialog
 * @return the dialog pointer
 */
static RoadMapDialogItem roadmap_dialog_get_nr (RoadMapDialogItem parent, const int id) {
   // __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_get_nr(%d)", id);
   RoadMapDialogItem child;

   if (parent == NULL) {
      child = RoadMapDialogWindows;
   } else {
      child = parent->children;
   }

   /* breadth-first lookup */
   while (child != NULL) {
      if (child->w == id) {
         // __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_get_nr -> %p", child);
         return child;
      }
      // __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_get_nr found %d", child->w);
      child = child->next;
   }

   /* lookup in depth now */
   if (parent == NULL) {
      child = RoadMapDialogWindows;
   } else {
      child = parent->children;
   }
   while (child != NULL) {
      if (child->children != NULL) {
         // __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_get_nr -> %p", child);
         RoadMapDialogItem p = child->children;
         while (p) {
            RoadMapDialogItem r = roadmap_dialog_get_nr(p, id);
            if (r)
               return r;
            p = p->next;
         }
      }
      // __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_get_nr found %d", child->w);
      child = child->next;
   }

   return NULL;
}

static void roadmap_dialog_hide_window (RoadMapDialogItem dialog) {
#if 0
   if (dialog->w != NULL) {
      gtk_widget_hide (dialog->w);
   }
#endif
}

/**
 * @brief
 * @param frame
 * @param name
 * @return
 */
static RoadMapDialogItem roadmap_dialog_new_item (const char *frame, const char *name) {
   RoadMapDialogItem parent;
   RoadMapDialogItem child;


   parent = roadmap_dialog_get (RoadMapDialogCurrent, frame);
   child  = roadmap_dialog_get (parent, name);
   child->widget_type = ROADMAP_WIDGET_CONTAINER; /* Safe default. */

// roadmap_log(ROADMAP_WARNING, "roadmap_dialog_new_item(%s,%s) parent %d child %d", frame, name, parent->w, child ? child->w : -1);

   if (parent->w == 0) {
	/*
	 * Create a heading, don't do structural changes so no additional level of hierarchy.
	 * Because of "no additional level", the widget is copied from the parent
	 */
	parent->w = parent->parent->w;

	jclass		cls = TheRoadMapClass();
	jmethodID	mid = TheMethod(cls, "CreateHeading", "(Ljava/lang/String;I)I");
	jstring		js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, frame);

	(*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, RoadMapThiz, mid, js,
			parent->w);
	// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "CreateDialog -> parent->w %d", parent->w);
   }

   /*
    * Create a widget to show the name of this object.
    * A separate widget will be the data entry field (e.g. created by roadmap_dialog_new_entry).
    */
   if (name[0] != '.') {
	jclass		cls = TheRoadMapClass();
	jmethodID	mid = TheMethod(cls, "DialogAddLabel", "(ILjava/lang/String;)I");
	jstring		js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, name);

	// child->w = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, RoadMapThiz, mid, RoadMapDialogCurrent->w, js);
	child->w = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, RoadMapThiz, mid, parent->w, js);
   } else {
	   /* ?? FIX ME Not sure what to do with this */
   }

//   roadmap_log(ROADMAP_WARNING, "roadmap_dialog_new_item -> %d", child->w);
   return child;
}

/**
 * @brief This function activates a dialog:
 * If the dialog did not exist yet, it will create an empty dialog
 * and roadmap_dialog_activate() returns 1; the application must then
 * enumerate all the dialog's items.
 * If the dialog did exist already, it will be shown on top and
 * roadmap_dialog_activate() returns 0.
 * This function never fails. The given dialog becomes the current dialog.
 *
 * @param name
 * @param context
 * @return returns 1 if this is a new, undefined, dialog; 0 otherwise.
 */
int roadmap_dialog_activate (const char *name, void *context)
{
roadmap_log(ROADMAP_WARNING, "roadmap_dialog_activate(%s)", name);
	RoadMapDialogItem dialog = roadmap_dialog_get (NULL, name);

	dialog->context = context;

	if (dialog->w != 0) {
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
// 	roadmap_log(ROADMAP_WARNING, "CreateDialog(%s) -> %d", name, dialog->w);

	return 1; /* Tell the caller this is a new, undefined, dialog. */
}

/**
 * @brief Hide the given dialog, if it exists.
 * @param name the dialog name
 */
void roadmap_dialog_hide (const char *name)
{
//   __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_hide(%s)", name);
//   roadmap_dialog_hide_window (roadmap_dialog_get (NULL, name));
}

/**
 * @brief Add one text entry item to the current dialog
 * @param frame
 * @param name
 */
void roadmap_dialog_new_entry (const char *frame, const char *name) {

// roadmap_log(ROADMAP_WARNING, "roadmap_dialog_new_entry(%s,%s)", frame, name);
   jclass	cls = TheRoadMapClass();
   jmethodID	mid = TheMethod(cls, "DialogAddTextEntry", "(ILjava/lang/String;)I");
   jstring	js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, name);

   RoadMapDialogItem parent, child;

   child = roadmap_dialog_new_item (frame, name);
   parent = roadmap_dialog_get (RoadMapDialogCurrent, frame);
   child  = roadmap_dialog_get (parent, name);
   int w = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, RoadMapThiz, mid, parent->w, js);
   /* Return value is the index of the text field in the container widget */
   child->w = w;

   child->widget_type = ROADMAP_WIDGET_ENTRY;
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

/*
 * This used to be a local variable in roadmap_dialog_show_list but then roadmap_dialog_chosen
 * can't use it, and the Android NDK/SDK pair aren't good at passing this type of info along.
 */
RoadMapDialogSelection *choice;

/**
 * @brief Add one choice item (a selection box or menu).
 * The optional callback is called each time a new selection is being made,
 * not when the OK button is called--that is the job of the OK button callback.
 *
 * Note there's a rather strange difference in types between values (as passed here as a parameter)
 * and the thing we need inside the function. Casting this via the "vals" local variable is
 * copied from the gtk2 implementation because it works.
 *
 * @param frame
 * @param name
 * @param count
 * @param current specifies the initial value to be shown in the UI
 * @param labels
 * @param values note strange type, probably requires FIX ME in all platform dependent sources
 * @param callback
 */
void roadmap_dialog_new_choice (const char *frame,
                                const char *name,
                                int count,
                                int current,
                                char **labels,
                                void *values,
                                RoadMapDialogCallback callback) {

   jclass	cls = TheRoadMapClass();
   jmethodID	mid = TheMethod(cls, "DialogCreateDropDown", "(ILjava/lang/String;[Ljava/lang/String;)I");
   jstring	js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, name);
   jobjectArray	joa;
   int		i;
   RoadMapDialogItem parent, child;
   char **vals = (char **)values;

   // roadmap_log (ROADMAP_WARNING, "roadmap_dialog_new_choice(%s,%s,%d,%d)", frame, name, count, current);

   child = roadmap_dialog_new_item (frame, name);
   parent = roadmap_dialog_get (RoadMapDialogCurrent, frame);
   child  = roadmap_dialog_get (parent, name);

   choice = (RoadMapDialogSelection *) calloc (count, sizeof(*choice));
   roadmap_check_allocated(choice);

   /*
    * Create list
    */
   joa = (*RoadMapJniEnv)->NewObjectArray(RoadMapJniEnv, count, cls, NULL);

   for (i = 0; i < count; ++i) {
      choice[i].typeid = "RoadMapDialogSelection";
      choice[i].item = child;
      choice[i].callback = callback;
      choice[i].value = vals[i];

      js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, labels[i]);
      (*RoadMapJniEnv)->SetObjectArrayElement(RoadMapJniEnv, joa, i, js);
   }
   child->choice = choice;
   child->value  = choice[0].value;
   /** End list creation */

   /* Return value is the index of the text field in the container widget */
   child->w = (*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, RoadMapThiz, mid, parent->w, js, joa);
   child->widget_type = ROADMAP_WIDGET_CHOICE;

   // roadmap_log (ROADMAP_WARNING, "roadmap_dialog_new_choice(%s,%s,%d,%d) -> w %d, child %p, choice %p", frame, name, count, current, child->w, child, choice);
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
// roadmap_log(ROADMAP_WARNING, "roadmap_dialog_new_list(%s,%s)", frame, name);

   RoadMapDialogItem	parent, child;
   if (name[0] == '.') name += 1;
   child = roadmap_dialog_new_item (frame, name);

   jclass	cls = TheRoadMapClass();
   jmethodID	mid = TheMethod(cls, "DialogCreateList", "(I)V");

   parent = roadmap_dialog_get (RoadMapDialogCurrent, frame);
   (*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid, RoadMapDialogCurrent->w);

//   child->widget_type = ROADMAP_WIDGET_LIST;

}

/**
 * @brief Find the "choice" for more than one case (list, drop down menu).
 * @param id the dialog wiget
 * @return choice
 */
static RoadMapDialogSelection *GetChoice(int id)
{
	RoadMapDialogItem dlgp = roadmap_dialog_get_nr(NULL, id);
	return dlgp->choice;
}

void roadmap_dialog_chosen (int dlg, int position, long id) {

   // RoadMapDialogSelection *selection = &choice[position];

   RoadMapDialogSelection *choice = GetChoice(dlg);
   RoadMapDialogSelection *selection = &choice[position];

   roadmap_log (ROADMAP_WARNING, "roadmap_dialog_chosen --> value old %s new %s, callback %p", selection->item->value, selection->value, selection->callback);

   if (selection != NULL) {

      RoadMapDialogItem	item = selection->item;
      if (item->w == 0)
         return;

      selection->item->value = selection->value;

      if (selection->callback != NULL) {

         RoadMapDialogItem item = selection->item;

         while (item->parent != NULL) {
            item = item->parent;
         }
         RoadMapDialogCurrent = item;

         (*selection->callback) (item->name, item->context);
      }
   }
}

void Java_net_sourceforge_projects_roadmap_RoadMap_RoadMapDialogChosen(JNIEnv* env, jobject thiz, int dlg, int position, long id)
{
	roadmap_log(ROADMAP_WARNING, "RoadMapDialogChosen(%d,%d,%d)", dlg, position, id);
	roadmap_dialog_chosen(dlg, position, id);
}

/**
 * @brief show a list of items.
 *   When the user selects an item, the callback must be called with the name of that item and
 *   its context (used?).
 * @param frame determine the dialog
 * @param name determine the dialog
 * @param count number of list items
 * @param labels list item strings
 * @param values
 * @param callback to be called when user selects
 */
void roadmap_dialog_show_list (const char  *frame,
                               const char  *name,
                               int    count,
                               char **labels,
                               void **values,
                               RoadMapDialogCallback callback) {
roadmap_log(ROADMAP_WARNING, "roadmap_dialog_show_list(%s,%s)", frame, name);
   int i;
   RoadMapDialogItem parent;
   RoadMapDialogItem child;

   parent = roadmap_dialog_get (RoadMapDialogCurrent, frame);
   child  = roadmap_dialog_get (parent, name);

// __android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_show_list(%s,%s) -> count %d, cur %d, par %d, child %d", frame, name, count, RoadMapDialogCurrent->w, parent->w, child->w);

   // model = gtk_tree_view_get_model (GTK_TREE_VIEW(child->w));
   if (child->choice != NULL) {
      // gtk_list_store_clear (GTK_LIST_STORE(model));
      free (child->choice);
      child->choice = NULL;
   }

   choice = (RoadMapDialogSelection *) calloc (count, sizeof(*choice));
   roadmap_check_allocated(choice);

   // gtk_tree_selection_set_select_function
   //     (gtk_tree_view_get_selection (GTK_TREE_VIEW (child->w)),
   //      roadmap_dialog_list_selected,
   //      (gpointer)choice,
   //      NULL);

   jstring	js;
   jobjectArray	joa;
   jclass	cls = TheRoadMapClass();
   jmethodID	mid = TheMethod(cls, "DialogSetListContents", "(I[Ljava/lang/String;)V");

   joa = (*RoadMapJniEnv)->NewObjectArray(RoadMapJniEnv, count, cls, NULL);

   for (i = 0; i < count; ++i) {

      choice[i].typeid = "RoadMapDialogSelection";
      choice[i].item = child;
      choice[i].value = values[i];
      choice[i].callback = callback;

      // gtk_list_store_append (GTK_LIST_STORE(model), &iterator);
      // gtk_list_store_set (GTK_LIST_STORE(model), &iterator,
      //                     RM_LIST_WAYPOINT_NAME, labels[i],
      //                     -1);

      js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, labels[i]);
      (*RoadMapJniEnv)->SetObjectArrayElement(RoadMapJniEnv, joa, i, js);
   }
   child->choice = choice;
   child->value  = choice[0].value;

   // gtk_widget_show (parent->w);

   (*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid, RoadMapDialogCurrent->w, joa);
   // (*RoadMapJniEnv)->ReleaseObjectArrayElements(RoadMapJniEnv, NULL, joa, JNI_ABORT);
}

/**
 * @brief Add one button to the bottom of the dialog
 * @param label
 * @param callback
 */
void roadmap_dialog_add_button (const char *label, RoadMapDialogCallback callback)
{
// 	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_add_button(%s,%p)", label, callback);
// 	__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "  Parent dialog %d {%s}", RoadMapDialogCurrent->w, RoadMapDialogCurrent->name);

	jclass		cls = TheRoadMapClass();
	jmethodID	mid = TheMethod(cls, "DialogAddSpecialButton", "(ILjava/lang/String;I)I");
	jstring		js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, label);

	if (RoadMapDialogCurrent->nbutton < 0 || RoadMapDialogCurrent->nbutton > MAX_CALLBACKS) {
// 		__android_log_print(ANDROID_LOG_ERROR, "RoadMap", "roadmap_dialog_add_button error nbutton %d", RoadMapDialogCurrent->nbutton);
		return;
	}

	(*RoadMapJniEnv)->CallIntMethod(RoadMapJniEnv, RoadMapThiz, mid,
		RoadMapDialogCurrent->w,	/* which dialog */
		js,
		RoadMapDialogCurrent->nbutton);	/* which button */

	RoadMapDialogCurrent->callbacks[RoadMapDialogCurrent->nbutton] = callback;
	RoadMapDialogCurrent->nbutton++;
}

/*
 * @brief callback from one of the "special" buttons in a dialog
 */
void
Java_net_sourceforge_projects_roadmap_RoadMap_DialogSpecialCallback(JNIEnv* env, jobject thiz, int dlg, int btn)
{
	RoadMapDialogItem	dlgp;

	roadmap_log(ROADMAP_WARNING, "DialogSpecialCallback(%d,%d)", dlg, btn);

	dlgp = roadmap_dialog_get_nr(NULL, dlg);

	if (dlgp && dlgp->callbacks && dlgp->callbacks[btn])
		dlgp->callbacks[btn](dlgp->name, dlgp->context);
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


/**< Not sure how to return a string as return value from a function in JNI, so
 * using this workaround (call the other way, pass string as an argument) to bypass.
 */
static char *ReturnStringDataHack = NULL;

/**
 * @brief function to return string data
 * @param env JNI standard environment pointer
 * @param thiz JNI standard object from which we're being called
 * @param js the string we're getting from Java
 */
void Java_net_sourceforge_projects_roadmap_RoadMap_ReturnStringDataHack(JNIEnv* env, jobject thiz, jstring js)
{
	ReturnStringDataHack = (*env)->GetStringUTFChars(env, js, NULL);
}

/**
 * @brief Look up which selection has been made in this dialog, or retrieve the contents
 * of a text entry field, or get the data we stored here.
 *
 * @param frame used to determine the dialog
 * @param name used to determine the dialog
 * @return pointer to the data passed back
 *
 * Note the data is retrieved from the user interface only if the widget is ROADMAP_WIDGET_ENTRY.
 * Also in that case we know it's a string.
 *
 * In other cases, we just pass pointers along from memory, and we're not sure about the data
 * type (they're sometimes pointers to structures).
 */
void *roadmap_dialog_get_data (const char *frame, const char *name)
{
   RoadMapDialogItem this_frame, this_item;

   // roadmap_log(ROADMAP_WARNING, "roadmap_dialog_get_data(%s,%s)", frame, name);
   this_frame  = roadmap_dialog_get (RoadMapDialogCurrent, frame);
   this_item   = roadmap_dialog_get (this_frame, name);

   if (this_item->widget_type == ROADMAP_WIDGET_ENTRY) {
      if (RoadMapDialogCurrent->w == 0 && this_item->w == 0) {
         // roadmap_log(ROADMAP_WARNING, "roadmap_dialog_get_data(%s,%s) null", frame, name);
         return NULL;
      }

      jclass	cls = TheRoadMapClass();
      jmethodID	mid = TheMethod(cls, "DialogGetData", "(II)V");

      (*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid,
         RoadMapDialogCurrent->w,
         this_item->w);

      this_item->value = ReturnStringDataHack;
   }

   // roadmap_log(ROADMAP_WARNING, "roadmap_dialog_get_data(%s,%s) -> {%p,%s}", frame, name, this_item->value, this_item->value);

   return (void *)this_item->value;
}


/**
 * @brief Store some data. See notes below.
 * @param frame used to determine the dialog
 * @param name used to determine the dialog
 * @param data pointer to the data passed back
 *
 * Note the data is sent to the user interface only if the widget is ROADMAP_WIDGET_ENTRY or
 * ROADMAP_WIDGET_LABEL. In those cases we know it's string data.
 *
 * In other cases, we just pass pointers along from memory, and we're not sure about the data
 * type (they're sometimes pointers to structures).
 *
 * Note that some initialisation happens in roadmap_dialog_new_choice, through the "current"
 * parameter.
 */
void roadmap_dialog_set_data (const char *frame, const char *name,
                               const void *data) {
   RoadMapDialogItem this_frame, this_item;

   this_frame  = roadmap_dialog_get (RoadMapDialogCurrent, frame);
   this_item   = roadmap_dialog_get (this_frame, name);

   roadmap_log(ROADMAP_WARNING, "roadmap_dialog_set_data(%s,%s) cur %d fr %d it %d", frame, name, RoadMapDialogCurrent->w, this_frame->w, this_item->w);

   switch (this_item->widget_type) {

   case ROADMAP_WIDGET_ENTRY:
   case ROADMAP_WIDGET_LABEL:
   {
      jclass		cls = TheRoadMapClass();
      jmethodID		mid = TheMethod(cls, "DialogSetData", "(IILjava/lang/String;)V");
      jstring		js = (*RoadMapJniEnv)->NewStringUTF(RoadMapJniEnv, (const char *)data);

      (*RoadMapJniEnv)->CallVoidMethod(RoadMapJniEnv, RoadMapThiz, mid,
	   /* dialog id */ RoadMapDialogCurrent->w,
	   /* row id in the dialog */ this_item->w,
	   js);
      break;

   }
   }

   this_item->value = (char *)data;
}

/**
 * @brief
 * @param frame
 * @param name
 *
 * Note not implemented in gtk2/ either, only used from navigate plugin
 */
void roadmap_dialog_new_progress (const char *frame, const char *name)
{
#warning implement roadmap_dialog_new_progress
}

/**
 * @brief
 * @param frame
 * @param name
 * @param progress
 *
 * Note not implemented in gtk2/ either, only used from navigate plugin
 */
void  roadmap_dialog_set_progress (const char *frame, const char *name, int progress)
{
#warning implement roadmap_dialog_set_progress
}

/**
 * @brief reinitialize this module
 */
void roadmap_dialog_shutdown (void)
{
	/* FIX ME memory leak */
	RoadMapDialogWindows = NULL;
	RoadMapDialogCurrent = NULL;
}

#if 0
/**
 * @brief this is a debug function, recursive, prints the hierarchy of the Dialog table
 * @param parent
 * @param level how many levels deep are we already
 */
void roadmap_dialog_test_r(RoadMapDialogItem parent, int level)
{
	RoadMapDialogItem p;
	RoadMapDialogItem child;
	char prefix[8];

	strcpy(prefix, "\t\t\t\t\t\t\t");
	prefix[level] = '\0';

	for (p = parent; p; p = p->next) {
		roadmap_log(ROADMAP_WARNING, "%sDialog %p {%s} parent %p next %p widget %d",
			prefix,
			p, p->name, p->parent, p->next, p->w);
      	
		child = p->children;
		while (child != NULL) {
			if (child->parent == parent) {
				roadmap_log(ROADMAP_WARNING, "%s  Child %p {%s}",
					prefix, child, child->name);
				roadmap_dialog_test_r(child, level + 1);
			}
			child = child->next;
		}
	}
}

/**
 * @brief Kickstart the recursive debug function to print the dialog table
 */
void roadmap_dialog_test(void)
{
	roadmap_log(ROADMAP_WARNING, "\n OVERVIEW OF DIALOGS \n\tCurrent %p\n\n", RoadMapDialogCurrent);
	roadmap_dialog_test_r(RoadMapDialogWindows, 0);
}
#endif
