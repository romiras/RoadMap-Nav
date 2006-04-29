/* roadmap_screen_obj.c - manage screen objects.
 *
 * LICENSE:
 *
 *   Copyright 2006 Ehud Shabtai
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
 * DESCRIPTION:
 *
 *   This module manages a dynamic list of objects to be displayed on screen.
 *   An object can be pressed on which may trigger an action and/or it can
 *   display a state using an image or a sprite.
 *
 *   The implementation is not terribly optimized: there should not be too
 *   many objects.
 */

#include <stdlib.h>
#include <string.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_file.h"
#include "roadmap_path.h"
#include "roadmap_canvas.h"
#include "roadmap_math.h"
#include "roadmap_sprite.h"
#include "roadmap_start.h"
#include "roadmap_state.h"

#include "roadmap_screen_obj.h"

#define MAX_STATES 9

struct RoadMapScreenObjDescriptor {

   char                *name; /* Unique name of the object. */

   char                *sprites[MAX_STATES]; /* Icons for each state. */
   RoadMapImage         images[MAX_STATES]; /* Icons for each state. */

   int                  states_count;

   RoadMapGuiPoint      position; /* position on screen */

   int                  disable_rotate; /* rotate with screen? */

   int                  opacity;

   const RoadMapAction *action;

   RoadMapStateFn state_fn;

   RoadMapGuiRect       bbox;

   struct RoadMapScreenObjDescriptor *next;
};

typedef struct RoadMapScreenObjDescriptor *RoadMapScreenObj;

static RoadMapScreenObj RoadMapObjectList = NULL;

static char *roadmap_object_string (const char *data, int length) {

    char *p = malloc (length+1);

    roadmap_check_allocated(p);

    strncpy (p, data, length);
    p[length] = 0;

    return p;
}


static RoadMapScreenObj roadmap_screen_obj_new
          (int argc, const char **argv, int *argl) {

   RoadMapScreenObj object = malloc(sizeof(*object));

   roadmap_check_allocated(object);

   memset (object, 0, sizeof(*object));

   object->name = roadmap_object_string (argv[1], argl[1]);

   object->next = RoadMapObjectList;
   RoadMapObjectList = object;

   return object;
}


static void roadmap_screen_obj_decode_arg (char *output, int o_size,
                                           const char *input, int i_size) {

   int size;

   o_size -= 1;
      
   size = i_size < o_size ? i_size : o_size;

   strncpy (output, input, size);
   output[size] = '\0';
}

      
static void roadmap_screen_obj_decode_icon
                        (RoadMapScreenObj object,
                         int argc, const char **argv, int *argl) {

   int i;

   argc -= 1;

   if (object->states_count > MAX_STATES) {
      roadmap_log (ROADMAP_ERROR, "screen object:'%s' has too many states.",
                  object->name);
      return;
   }

   for (i = 1; i <= argc; ++i) {
      char arg[256];

      roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[i], argl[i]);

      if (!strchr(arg, '.')) {
         /* this is a sprite icon */
         object->sprites[object->states_count] =
            roadmap_object_string (arg, argl[i]);
      } else {
         RoadMapImage image = NULL;
         const char *cursor;
         char *file = roadmap_path_join ("icons", arg);

         for (cursor = roadmap_path_first ("config");
               cursor != NULL;
               cursor = roadmap_path_next ("config", cursor)) {

            if (roadmap_file_exists (cursor, file)) {
               image = roadmap_canvas_load_image (cursor, file);
               break;
            }
         }

         if (!image) {
            for (cursor = roadmap_path_first ("user");
                  cursor != NULL;
                  cursor = roadmap_path_next ("user", cursor)) {

               if (roadmap_file_exists (cursor, file)) {
                  image = roadmap_canvas_load_image (cursor, file);
                  break;
               }
            }
         }

         free (file);

         if (image == NULL) {
            roadmap_log (ROADMAP_ERROR,
                  "screen object:'%s' can't load image:%s.",
                  object->name,
                  arg);
         }
         object->images[object->states_count] = image;
      }
   }

   ++object->states_count;
}


static void roadmap_screen_obj_decode_integer (int *value, int argc,
                                               const char **argv, int *argl) {

   char arg[255];

   argc -= 1;

   if (argc != 1) {
      roadmap_log (ROADMAP_ERROR, "screen object: illegal integer value - %s",
                  argv[0]);
      return;
   }

   roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[1], argl[1]);
   *value = atoi(arg);
}


static void roadmap_screen_obj_decode_bbox
                        (RoadMapScreenObj object,
                         int argc, const char **argv, int *argl) {

   char arg[255];

   argc -= 1;
   if (argc != 4) {
      roadmap_log (ROADMAP_ERROR, "screen object:'%s' illegal bbox.",
                  object->name);
      return;
   }

   roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[1], argl[1]);
   object->bbox.minx = atoi(arg);
   roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[2], argl[2]);
   object->bbox.miny = atoi(arg);
   roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[3], argl[3]);
   object->bbox.maxx = atoi(arg);
   roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[4], argl[4]);
   object->bbox.maxy = atoi(arg);
}


static void roadmap_screen_obj_decode_position
                        (RoadMapScreenObj object,
                         int argc, const char **argv, int *argl) {

   char arg[255];

   argc -= 1;
   if (argc != 2) {
      roadmap_log (ROADMAP_ERROR, "screen object:'%s' illegal position.",
                  object->name);
      return;
   }

   roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[1], argl[1]);
   object->position.x = atoi(arg);

   roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[2], argl[2]);
   object->position.y = atoi(arg);
}


static void roadmap_screen_obj_decode_state
                        (RoadMapScreenObj object,
                         int argc, const char **argv, int *argl) {

   char arg[255];

   argc -= 1;
   if (argc != 1) {
      roadmap_log (ROADMAP_ERROR, "screen object:'%s' illegal state indicator.",
                  object->name);
      return;
   }

   roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[1], argl[1]);

   object->state_fn = roadmap_state_find (arg);

   if (!object->state_fn) {
      roadmap_log (ROADMAP_ERROR,
                  "screen object:'%s' can't find state indicator.",
                  object->name);
   }
}


static void roadmap_screen_obj_decode_action
                        (RoadMapScreenObj object,
                         int argc, const char **argv, int *argl) {

   char arg[255];

   argc -= 1;
   if (argc != 1) {
      roadmap_log (ROADMAP_ERROR, "screen object:'%s' illegal action.",
                  object->name);
      return;
   }

   roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[1], argl[1]);

   object->action = roadmap_start_find_action (arg);

   if (!object->action) {
      roadmap_log (ROADMAP_ERROR, "screen object:'%s' can't find action.",
                  object->name);
   }
}


static void roadmap_screen_obj_load (const char *data, int size) {

   int argc;
   int argl[256];
   const char *argv[256];

   const char *p;
   const char *end;

   RoadMapScreenObj object = NULL;


   end  = data + size;

   while (data < end) {

      while (data[0] == '#' || data[0] < ' ') {

         if (*(data++) == '#') {
            while ((data < end) && (data[0] >= ' ')) data += 1;
         }
         while (data < end && data[0] == '\n' && data[0] != '\r') data += 1;
      }

      argc = 1;
      argv[0] = data;

      for (p = data; p < end; ++p) {

         if (*p == ' ') {
            argl[argc-1] = p - argv[argc-1];
            argv[argc]   = p+1;
            argc += 1;
            if (argc >= 255) break;
         }

         if (p >= end || *p < ' ') break;
      }
      argl[argc-1] = p - argv[argc-1];

      while (p < end && *p < ' ' && *p > 0) ++p;
      data = p;

      if (object == NULL) {

         if (argv[0][0] != 'N') {
            roadmap_log (ROADMAP_ERROR, "object name is missing");
            break;
         }

      }

      switch (argv[0][0]) {

      case 'O':

         roadmap_screen_obj_decode_integer (&object->opacity, argc, argv,
                                             argl);
         break;

      case 'A':

         roadmap_screen_obj_decode_action (object, argc, argv, argl);
         break;

      case 'B':

         roadmap_screen_obj_decode_bbox (object, argc, argv, argl);
         break;

      case 'P':

         roadmap_screen_obj_decode_position (object, argc, argv, argl);
         break;

      case 'I':

         roadmap_screen_obj_decode_icon (object, argc, argv, argl);
         break;

      case 'S':

         roadmap_screen_obj_decode_state (object, argc, argv, argl);
         break;

      case 'R':

         object->disable_rotate = 1;
         break;

      case 'N':

         object = roadmap_screen_obj_new (argc, argv, argl);
         break;
      }

      while (p < end && *p < ' ') p++;
      data = p;
   }
}


RoadMapScreenObj roadmap_screen_obj_search (const char *name) {

   RoadMapScreenObj cursor;

   for (cursor = RoadMapObjectList; cursor != NULL; cursor = cursor->next) {
      if (!strcmp(cursor->name, name)) return cursor;
   }

   return NULL;
}


void roadmap_screen_obj_move (const char *name,
                          const RoadMapGuiPoint *position) {

   RoadMapScreenObj cursor = roadmap_screen_obj_search (name);

   if (cursor != NULL) {

      cursor->position.x = position->x;
      cursor->position.y = position->y;
   }
}


#if 0
void roadmap_object_iterate (RoadMapObjectAction action) {

   RoadMapScreenObj *cursor;

   for (cursor = RoadMapObjectList; cursor != NULL; cursor = cursor->next) {

      (*action) (roadmap_string_get(cursor->name),
                 roadmap_string_get(cursor->sprite),
                 &(cursor->position));
   }
}
#endif


void roadmap_screen_obj_initialize (void) {

   const char *cursor;
   RoadMapFileContext file;

   for (cursor = roadmap_file_map ("config", "objects", NULL, "r", &file);
        cursor != NULL;
        cursor = roadmap_file_map ("config", "objects", cursor, "r", &file)) {

      roadmap_screen_obj_load (roadmap_file_base(file), roadmap_file_size(file));

      roadmap_file_unmap (&file);
   }

   for (cursor = roadmap_file_map ("user", "objects", NULL, "r", &file);
        cursor != NULL;
        cursor = roadmap_file_map ("user", "objects", cursor, "r", &file)) {

      roadmap_screen_obj_load (roadmap_file_base(file), roadmap_file_size(file));

      roadmap_file_unmap (&file);
   }
}


void roadmap_screen_obj_draw (void) {

   RoadMapScreenObj cursor;

   for (cursor = RoadMapObjectList; cursor != NULL; cursor = cursor->next) {
      int state = 0;

      if (cursor->state_fn) {
         state = (*cursor->state_fn) ();
         if ((state < 0) || (state >= MAX_STATES)) continue;
      }

      if (cursor->images[state]) {
         roadmap_canvas_draw_image (cursor->images[state], &cursor->position,
                                    cursor->opacity);
      }

      if (cursor->sprites[state]) {
         
         if (cursor->disable_rotate) {
            roadmap_sprite_draw (cursor->sprites[state], &cursor->position,
                                 -roadmap_math_get_orientation());
         } else {
            roadmap_sprite_draw (cursor->sprites[state], &cursor->position, 0);
         }
      }
   }
}


int roadmap_screen_obj_click (RoadMapGuiPoint *point) {

   RoadMapScreenObj cursor;

   for (cursor = RoadMapObjectList; cursor != NULL; cursor = cursor->next) {

      if ((point->x >= (cursor->position.x + cursor->bbox.minx)) &&
          (point->x <= (cursor->position.x + cursor->bbox.maxx)) &&
          (point->y >= (cursor->position.y + cursor->bbox.miny)) &&
          (point->y <= (cursor->position.y + cursor->bbox.maxy))) {

         if (cursor->action) (*(cursor->action->callback)) ();
         return 1;
      }
   }

   return 0;
}

