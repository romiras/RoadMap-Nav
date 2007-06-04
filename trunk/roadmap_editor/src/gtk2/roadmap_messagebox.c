/* roadmap_messagebox.c - manage the roadmap dialogs used for user info.
 *
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
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
 *
 * SYNOPSYS:
 *
 *   See roadmap_messagebox.h
 */

#include <gtk/gtk.h>

#ifdef ROADMAP_USES_GPE
#include <libdisplaymigration/displaymigration.h>
#endif

#include "roadmap.h"
#include "roadmap_start.h"
#include "roadmap_lang.h"

#include "roadmap_messagebox.h"


static gint roadmap_messagebox_ok (GtkWidget *w, gpointer data) {

   GtkWidget *dialog = (GtkWidget *) data;

   gtk_widget_destroy (dialog);

   return FALSE;
}

void roadmap_messagebox_show (const char *title, const char *text, int modal) {

   GtkWidget *ok;
   GtkWidget *label;
   GtkWidget *dialog;

   dialog = gtk_dialog_new();
#ifdef ROADMAP_USES_GPE
   displaymigration_mark_window (dialog);
#endif

   title = roadmap_lang_get (title);
   text  = roadmap_lang_get (text);

   label = gtk_label_new(text);

   gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_CENTER);

   ok = gtk_button_new_with_label ("Ok");

   GTK_WIDGET_SET_FLAGS (ok, GTK_CAN_DEFAULT);

   gtk_window_set_title (GTK_WINDOW(dialog),  roadmap_start_get_title(title));

   gtk_window_set_modal (GTK_WINDOW(dialog), modal);

   gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->vbox),
                       label, TRUE, TRUE, 0);

   gtk_box_pack_start (GTK_BOX(GTK_DIALOG(dialog)->action_area),
                       ok, TRUE, TRUE, 0);

   gtk_container_set_border_width
      (GTK_CONTAINER(GTK_BOX(GTK_DIALOG(dialog)->vbox)), 4);

   g_signal_connect (GTK_OBJECT(ok),
                     "clicked",
                     GTK_SIGNAL_FUNC(roadmap_messagebox_ok),
                     dialog);

   gtk_widget_grab_default (ok);

   gtk_widget_show_all (GTK_WIDGET(dialog));
}


void roadmap_messagebox (const char *title, const char *message) {
   roadmap_messagebox_show (title, message, 0);
}

void roadmap_messagebox_wait (const char *title, const char *message) {
   roadmap_messagebox_show (title, message, 1);
}

