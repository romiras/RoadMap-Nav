/* roadmap_keyboard.c - Provide a keyboard widget for RoadMap dialogs.
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
 *   See roadmap_keyboard.h.
 */

#include <math.h>
#include <stdlib.h>

#include "roadmap_keyboard.h"

#include "roadmap.h"


typedef struct {

   char character;

   GtkWidget       *button;
   RoadMapKeyboard  keyboard;

} RoadMapKey;


struct roadmap_keyboard_context {

   GtkWidget *frame;
   GtkWidget *focus;

   RoadMapKey keys[40];
};


static gint roadmap_keyboard_pressed (GtkWidget *w, gpointer data) {

   gint cursor;

   RoadMapKey *key = (RoadMapKey *) data;
   RoadMapKeyboard keyboard = (RoadMapKeyboard) key->keyboard;


   if (keyboard->focus != NULL) {

      cursor = gtk_editable_get_position (GTK_EDITABLE(keyboard->focus));

      if (key->character >= ' ') {

         char text[2];

         text[0] = key->character;
         text[1] = 0;
         gtk_editable_insert_text (GTK_EDITABLE(keyboard->focus),
                                   text, 1, &cursor);
         gtk_editable_set_position (GTK_EDITABLE(keyboard->focus), cursor);

      } else if (key->character == '\b') {

         cursor = gtk_editable_get_position (GTK_EDITABLE(keyboard->focus));

         gtk_editable_delete_text
            (GTK_EDITABLE(keyboard->focus), cursor-1, cursor);

      } else if (key->character == '\r') {

         gtk_editable_delete_text (GTK_EDITABLE(keyboard->focus), 0, -1);
      }
      gtk_widget_grab_focus (keyboard->focus);
   }

   return FALSE;
}



static GtkWidget *roadmap_keyboard_add_key (RoadMapKey *key, char character) {

   char label[2];

   key->character = character;

   if (character < ' ') {
      if (character == '\r') {
         character = 0xab;
      }
      if (character == '\b') {
         character = '<';
      }
   }
   label[0] = character;
   label[1] = 0;

   key->button = gtk_button_new_with_label (label);

   gtk_signal_connect (GTK_OBJECT(key->button),
                       "clicked",
                       GTK_SIGNAL_FUNC(roadmap_keyboard_pressed),
                       key);

   return key->button;
}


RoadMapKeyboard roadmap_keyboard_new (void) {

   int i;
   int j;
   int k;
   char keymap[40] = "1234567890qwertyuiopasdfghjkl\b zxcvbnm \r";

   RoadMapKeyboard keyboard;
   GtkWidget *button;

   keyboard = (RoadMapKeyboard) malloc (sizeof(*keyboard));
   if (keyboard == NULL) {
      roadmap_log (ROADMAP_FATAL, "no more memory");
   }
   keyboard->frame = gtk_table_new (4, 10, TRUE);

   for (i = 0; i < 4; i++) {

      for (j = 0; j < 10; j++) {

         k = (10 * i) + j;

         button = roadmap_keyboard_add_key (&keyboard->keys[k], keymap[k]);
         gtk_table_attach_defaults
            (GTK_TABLE(keyboard->frame), button, j, j+1, i, i+1);

         keyboard->keys[k].keyboard = keyboard;
      }
   }

   return keyboard;
}


GtkWidget *roadmap_keyboard_widget (RoadMapKeyboard keyboard) {

   return keyboard->frame;
}


void roadmap_keyboard_set_focus (RoadMapKeyboard keyboard, GtkWidget *w) {

   keyboard->focus = w;
}

