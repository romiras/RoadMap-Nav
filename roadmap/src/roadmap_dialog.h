/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright (c) 2008, 2011, Danny Backx.
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
 * @brief roadmap_dialog.h - manage the roadmap dialogs is used for user input.
 *
 *   This module define an API to create and manipulate dialog windows.
 *
 *   A dialog window is made of two area:
 *   The 1st area (usually at the top) is used to show framed items.
 *   The 2nd area is used to show the buttons ("OK", "Cancel", etc..).
 *
 *   Every item in a dialog has a title (usually a label to place on
 *   the left side of the widget), a type (text entry, color, choice, etc..)
 *   and a parent frame.
 *
 *   When a dialog contains several parent frames, all frames should be
 *   visible as separate entities (for example using a notebook widget).
 *
 *   Here is an example of a simple dialog asking for a name and an email
 *   address (with no keyboard attached):
 *
 *   if (roadmap_dialog_activate ("Email Address", context)) {
 *     roadmap_dialog_new_label  ("Address", "Please enter a valid address");
 *     roadmap_dialog_new_entry  ("Address", "Name");
 *     roadmap_dialog_new_entry  ("Address", "email");
 *     roadmap_dialog_add_button ("OK", add_this_address);
 *     roadmap_dialog_add_button ("Cancel", roadmap_dialog_hide);
 *     roadmap_dialog_complete   (NULL);
 *   }
 *
 * The application can retrieve the current values of each item using
 * the roadmap_dialog_select() and roadmap_dialog_get_data() functions.
 */

#ifndef INCLUDE__ROADMAP_DIALOG__H
#define INCLUDE__ROADMAP_DIALOG__H

typedef void (*RoadMapDialogCallback) (const char *name, void *context);


int roadmap_dialog_activate (const char *name, void *context);
void roadmap_dialog_shutdown (void);
void roadmap_dialog_hide (const char *name);
void roadmap_dialog_new_label (const char *frame, const char *name);
void roadmap_dialog_new_entry (const char *frame, const char *name);
void roadmap_dialog_new_color (const char *frame, const char *name);
void roadmap_dialog_new_choice (const char *frame,
                                const char *name,
                                int count,
                                int current,
                                char **labels,
                                void *values,
                                RoadMapDialogCallback callback);
void roadmap_dialog_new_list (const char  *frame, const char  *name);
void roadmap_dialog_show_list (const char  *frame,
                               const char  *name,
                               int    count,
                               char **labels,
                               void **values,
                               RoadMapDialogCallback callback);
void roadmap_dialog_add_button (const char *label, RoadMapDialogCallback callback);
void roadmap_dialog_new_hidden (const char *frame, const char *name);
void roadmap_dialog_complete (int use_keyboard);
void  roadmap_dialog_select   (const char *dialog);
void *roadmap_dialog_get_data (const char *frame, const char *name);
void  roadmap_dialog_set_data (const char *frame, const char *name,
                               const void *data);
void  roadmap_dialog_protect  (const char *frame, const char *name);
void roadmap_dialog_shutdown (void);

#ifdef LANG_SUPPORT
#include "roadmap_lang.h"

#ifndef __ROADMAP_DIALOG_NO_LANG
static __inline int roadmap_dialog_activate_i (const char *name, void *context) {
   return roadmap_dialog_activate (roadmap_lang_get (name), context);
}

static __inline void roadmap_dialog_hide_i (const char *name) {
   roadmap_dialog_hide (roadmap_lang_get (name));
}

static __inline void roadmap_dialog_new_label_i
                     (const char *frame, const char *name) {

   roadmap_dialog_new_label
      (roadmap_lang_get (frame), roadmap_lang_get (name));
}

static __inline void roadmap_dialog_new_entry_i
       (const char *frame, const char *name) {

   roadmap_dialog_new_entry
      (roadmap_lang_get (frame), roadmap_lang_get (name));
}

static __inline void roadmap_dialog_new_color_i
                     (const char *frame, const char *name) {

   roadmap_dialog_new_color
      (roadmap_lang_get (frame), roadmap_lang_get (name));
}

static __inline void roadmap_dialog_new_choice_i
                                          (const char *frame,
                                           const char *name,
                                           int count,
                                	   int current,
                                           char **labels,
                                           void *values,
                                           RoadMapDialogCallback callback) {
/* the labels should be translated */
   roadmap_dialog_new_choice (roadmap_lang_get (frame),
                              roadmap_lang_get (name),
                              count, current, labels, values, callback);
}

static __inline void roadmap_dialog_new_list_i
                     (const char  *frame, const char  *name) {

   roadmap_dialog_new_list (roadmap_lang_get (frame), roadmap_lang_get (name));
}

static __inline void roadmap_dialog_show_list_i
                                             (const char  *frame,
                                              const char  *name,
                                              int    count,
                                              char **labels,
                                              void **values,
                                              RoadMapDialogCallback callback) {

   roadmap_dialog_show_list (roadmap_lang_get (frame),
                             roadmap_lang_get (name),
                             count, labels, values, callback);
}

static __inline void roadmap_dialog_add_button_i
                     (const char *label, RoadMapDialogCallback callback) {

   roadmap_dialog_add_button (roadmap_lang_get (label), callback);
}

static __inline void *roadmap_dialog_get_data_i 
                        (const char *frame, const char *name) {

   return roadmap_dialog_get_data
               (roadmap_lang_get (frame), roadmap_lang_get (name));
}

static __inline void roadmap_dialog_set_data_i
                      (const char *frame, const char *name, const void *data) {

   roadmap_dialog_set_data
      (roadmap_lang_get (frame), roadmap_lang_get (name), data);
}

static __inline void roadmap_dialog_protect_i
                     (const char *frame, const char *name) {

   roadmap_dialog_protect (roadmap_lang_get (frame), roadmap_lang_get (name));
}
         
#define roadmap_dialog_activate      roadmap_dialog_activate_i
#define roadmap_dialog_hide          roadmap_dialog_hide_i
#define roadmap_dialog_new_label     roadmap_dialog_new_label_i
#define roadmap_dialog_new_entry     roadmap_dialog_new_entry_i
#define roadmap_dialog_new_color     roadmap_dialog_new_color_i
#define roadmap_dialog_new_choice    roadmap_dialog_new_choice_i
#define roadmap_dialog_new_list      roadmap_dialog_new_list_i
#define roadmap_dialog_show_list     roadmap_dialog_show_list_i
#define roadmap_dialog_add_button    roadmap_dialog_add_button_i
#define roadmap_dialog_select        roadmap_dialog_select_i
#define roadmap_dialog_get_data      roadmap_dialog_get_data_i
#define roadmap_dialog_set_data      roadmap_dialog_set_data_i
#define roadmap_dialog_protect       roadmap_dialog_protect_i

#endif /* __ROADMAP_DIALOG_NO_LANG */
#endif /* LANG_SUPPORT */

#ifdef	_WIN32_WCE
void roadmap_dialog_set_resolution(const int height, const int width);
#endif

void roadmap_dialog_new_progress (const char *frame, const char *name);
void  roadmap_dialog_set_progress (const char *frame, const char *name, int progress);
#endif // INCLUDE__ROADMAP_DIALOG__H
