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
#include <ctype.h>
#include <assert.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_file.h"
#include "roadmap_path.h"
#include "roadmap_canvas.h"
#include "roadmap_math.h"
#include "roadmap_sprite.h"
#include "roadmap_factory.h"
#include "roadmap_start.h"
#include "roadmap_state.h"
#include "roadmap_pointer.h"
#include "roadmap_scan.h"
#if LATER_SOUND_SUPPORT
#include "roadmap_res.h"
#include "roadmap_sound.h"
#endif
#include "roadmap_main.h"

#include "roadmap_screen_obj.h"

typedef struct {
   const char   *name;
   int           min_screen_height;
} ObjectFile;

static ObjectFile RoadMapObjFiles[] = {
   {"roadmap.screenobjects",      300},
   {"roadmap.screenobjects_wide", 200},
   {"roadmap.screenobjects_short",100}
};

#define MAX_STATES 9

#define OBJ_FLAG_NO_ROTATE        0x1
#define OBJ_FLAG_REPEAT           0x2
#define OBJ_FLAG_EXPLICIT_BBOX    0x4

#define OBJ_REPEAT_TIMEOUT 100

struct RoadMapScreenObjDescriptor {

   char                *name; /* Unique name of the object. */

   char                *sprites[MAX_STATES]; /* Sprites for each state. */
#if LATER_ICON_SUPPORT
   RoadMapImage         images[MAX_STATES]; /* Icons for each state. */
#endif

   int                  states_count;

   short                pos_x; /* position on screen */
   short                pos_y; /* position on screen */

   int                  flags;

   int                  opacity;

   RoadMapCallback      callback;
   RoadMapCallback      long_callback;

   RoadMapStateFn state_fn;

   RoadMapGuiRect       bbox;

   struct RoadMapScreenObjDescriptor *next;
};

static RoadMapScreenObj RoadMapObjectList = NULL;
static RoadMapScreenObj RoadMapScreenObjSelected = NULL;
static int OffsetX = 0;
static int OffsetY = 0;

static char *roadmap_object_string (const char *data, int length) {

    char *p = malloc (length+1);

    roadmap_check_allocated(p);

    strncpy (p, data, length);
    p[length] = 0;

    return p;
}


static RoadMapScreenObj roadmap_screen_obj_new
          (int argc, const char **argv, int *argl) {

   RoadMapScreenObj object = calloc(sizeof(*object), 1);

   roadmap_check_allocated(object);

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

      
#if LATER_ICON_SUPPORT
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
      RoadMapImage image = NULL;
      char arg[256];

      roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[i], argl[i]);

      if (!object->images[object->states_count]) {

         image = roadmap_res_get (RES_BITMAP, RES_SKIN, arg);

         if (image == NULL) {
            roadmap_log (ROADMAP_ERROR,
                  "screen object:'%s' can't load image:%s.",
                  object->name,
                  arg);
         }
         object->images[object->states_count] = image;
      } else {
         object->sprites[object->states_count] =
            roadmap_object_string (arg, argl[i]);
      }
   }

   ++object->states_count;
}
#endif


static void roadmap_screen_obj_decode_sprite
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

      object->sprites[object->states_count] =
         roadmap_object_string (arg, argl[i]);
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

   object->flags |= OBJ_FLAG_EXPLICIT_BBOX;
}

static void roadmap_screen_obj_square_bbox(RoadMapScreenObj object) {

    /* unless the bbox was set explicitly, if the object
     * will rotate, we want the bbox to be square, using
     * the max dimension of the actual bbox.  */

    RoadMapGuiRect *bbox = &object->bbox;
    int max = 0;

    if (object->flags & OBJ_FLAG_NO_ROTATE) return;

    if (object->flags & OBJ_FLAG_EXPLICIT_BBOX) return;

    if (max < abs(bbox->minx)) max = abs(bbox->minx);
    if (max < abs(bbox->maxx)) max = abs(bbox->maxx);
    if (max < abs(bbox->miny)) max = abs(bbox->miny);
    if (max < abs(bbox->maxy)) max = abs(bbox->maxy);

    bbox->minx = bbox->miny = -max;
    bbox->maxx = bbox->maxy = max;

}


static void roadmap_screen_obj_decode_position
                        (RoadMapScreenObj object,
                         int argc, const char **argv, int *argl) {

   char arg[255];
   int pos;

   argc -= 1;
   if (argc != 2) {
      roadmap_log (ROADMAP_ERROR, "screen object:'%s' illegal position.",
                  object->name);
      return;
   }

   roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[1], argl[1]);
   pos = atoi(arg);
   object->pos_x = pos;

   roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[2], argl[2]);
   pos = atoi(arg);
   object->pos_y = pos;
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
   const RoadMapAction *action;

   argc -= 1;
   if (argc < 1) {
      roadmap_log (ROADMAP_ERROR, "screen object:'%s' illegal action.",
                  object->name);
      return;
   }

   roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[1], argl[1]);

   action = roadmap_start_find_action (arg);
   if (action) {
      object->callback = action->callback;
   } else {
      roadmap_log (ROADMAP_ERROR, "screen object:'%s' can't find action.",
                  object->name);
   }

   if (argc == 1) return;

   roadmap_screen_obj_decode_arg (arg, sizeof(arg), argv[2], argl[2]);

   action = roadmap_start_find_action (arg);
   if (action) {
      object->long_callback = action->callback;
   } else {
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

   RoadMapGuiRect bbox[1];

   RoadMapScreenObj object = NULL;


   end  = data + size;

   while (data < end) {

      while (data < end && (data[0] == '#' || data[0] < ' ')) {

         if (*(data++) == '#') {
            while ((data < end) && (data[0] >= ' ')) data += 1;
         }
         while (data < end && data[0] == '\n' && data[0] != '\r') data += 1;
      }

      if (data >= end)  break;

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

         /* ignore any existing bbox */
         roadmap_screen_obj_decode_bbox (object, argc, argv, argl);
         break;

      case 'P':

         roadmap_screen_obj_decode_position (object, argc, argv, argl);
         break;

      case 'I':
#if LATER_ICON_SUPPORT
         roadmap_screen_obj_decode_icon (object, argc, argv, argl);
         break;
#else
         roadmap_log
            (ROADMAP_WARNING, "'screenobjects' file icons not yet supported");
#endif
         break;

      case 'E':

         roadmap_screen_obj_decode_sprite (object, argc, argv, argl);

         if (object->flags & OBJ_FLAG_EXPLICIT_BBOX) break;

         roadmap_sprite_bbox
                (object->sprites[object->states_count-1] , bbox);

         /* we want the bounds of the largest sprite referenced
          * by this object */
         if (bbox->minx < object->bbox.minx) object->bbox.minx = bbox->minx;
         if (bbox->maxx > object->bbox.maxx) object->bbox.maxx = bbox->maxx;
         if (bbox->miny < object->bbox.miny) object->bbox.miny = bbox->miny;
         if (bbox->maxy > object->bbox.maxy) object->bbox.maxy = bbox->maxy;

         break;

      case 'S':

         roadmap_screen_obj_decode_state (object, argc, argv, argl);
         break;

      case 'R':

         object->flags |= OBJ_FLAG_NO_ROTATE;
         break;

      case 'T':

         object->flags |= OBJ_FLAG_REPEAT;
         break;

      case 'N':

         if (object != NULL) {
            roadmap_screen_obj_square_bbox(object);
         }
         object = roadmap_screen_obj_new (argc, argv, argl);
         break;
      }

      while (p < end && *p < ' ') p++;
      data = p;
   }

   if (object != NULL) {
      roadmap_screen_obj_square_bbox(object);
   }

}


static RoadMapScreenObj roadmap_screen_obj_search (const char *name) {

   RoadMapScreenObj cursor;

   for (cursor = RoadMapObjectList; cursor != NULL; cursor = cursor->next) {
      if (!strcmp(cursor->name, name)) return cursor;
   }

   return NULL;
}


static void roadmap_screen_obj_pos (RoadMapScreenObj object,
                                    RoadMapGuiPoint *pos) {

   pos->x = object->pos_x;
   pos->y = object->pos_y;

   if (pos->x < 0) {
      pos->x += roadmap_canvas_width ();
   } else {
      pos->x += OffsetX;
   }

   if (pos->y < 0) {
      pos->y += roadmap_canvas_height ();
   } else {
      pos->y += OffsetY;
   }
}


static RoadMapScreenObj roadmap_screen_obj_by_pos (RoadMapGuiPoint *point) {

   RoadMapScreenObj cursor;

   for (cursor = RoadMapObjectList; cursor != NULL; cursor = cursor->next) {

      RoadMapGuiPoint pos;
      roadmap_screen_obj_pos (cursor, &pos);

      if ((point->x >= (pos.x + cursor->bbox.minx)) &&
          (point->x <= (pos.x + cursor->bbox.maxx)) &&
          (point->y >= (pos.y + cursor->bbox.miny)) &&
          (point->y <= (pos.y + cursor->bbox.maxy))) {

         return cursor;
      }
   }

   return NULL;
}


static void roadmap_screen_obj_repeat (void) {
   assert(RoadMapScreenObjSelected);

   if (RoadMapScreenObjSelected && RoadMapScreenObjSelected->callback) {
      (*(RoadMapScreenObjSelected->callback)) ();
   }
}


static int roadmap_screen_obj_pressed (RoadMapGuiPoint *point) {
   int state = 0;

   RoadMapScreenObjSelected = roadmap_screen_obj_by_pos (point);

   if (!RoadMapScreenObjSelected) return 0;

   if (RoadMapScreenObjSelected->state_fn) {
      state = (*RoadMapScreenObjSelected->state_fn) ();
      if ((state < 0) || (state >= MAX_STATES)) return 1;
   }

#if LATER_ICON_SUPPORT
   if (RoadMapScreenObjSelected->images[state]) {
      RoadMapGuiPoint pos;
      roadmap_screen_obj_pos (RoadMapScreenObjSelected, &pos);

      roadmap_canvas_draw_image (RoadMapScreenObjSelected->images[state], &pos,
                           RoadMapScreenObjSelected->opacity, IMAGE_SELECTED);
   }
#endif
   
   roadmap_canvas_refresh ();

   if (RoadMapScreenObjSelected->flags & OBJ_FLAG_REPEAT) {
      if (RoadMapScreenObjSelected->callback) {
         (*(RoadMapScreenObjSelected->callback)) ();
      }

      roadmap_main_set_periodic (OBJ_REPEAT_TIMEOUT,
            roadmap_screen_obj_repeat);
   }

   return 1;
}


static int roadmap_screen_obj_released (RoadMapGuiPoint *point) {
   RoadMapScreenObj object = RoadMapScreenObjSelected;

   if (!RoadMapScreenObjSelected) {
      return 0;
   }

   if (object->flags & OBJ_FLAG_REPEAT) {
      roadmap_main_remove_periodic (roadmap_screen_obj_repeat);
      RoadMapScreenObjSelected = 0;
   }

   return 1;
}


static int roadmap_screen_obj_short_click (RoadMapGuiPoint *point) {

   RoadMapScreenObj object = RoadMapScreenObjSelected;

   if (!RoadMapScreenObjSelected) {
      return 0;
   }

   if (object->flags & OBJ_FLAG_REPEAT) return 1;

   RoadMapScreenObjSelected = NULL;

   if (object->callback) {
#if LATER_SOUND_SUPPORT
      static RoadMapSoundList list;
   
      if (!list) {
         list = roadmap_sound_list_create (SOUND_LIST_NO_FREE);
         roadmap_sound_list_add (list, "click.wav");
         roadmap_res_get (RES_SOUND, 0, "click.wav");
      }

      roadmap_sound_play_list (list);
#endif

      (*(object->callback)) ();
   }

   return 1;
}


static int roadmap_screen_obj_long_click (RoadMapGuiPoint *point) {

#if LATER_SOUND_SUPPORT
   static RoadMapSoundList list;
#endif
   RoadMapScreenObj object = RoadMapScreenObjSelected;

   if (!RoadMapScreenObjSelected) {
      return 0;
   }

   if (RoadMapScreenObjSelected->flags & OBJ_FLAG_REPEAT) return 1;

   RoadMapScreenObjSelected = NULL;

#if LATER_SOUND_SUPPORT
   if (!list) {
      list = roadmap_sound_list_create (SOUND_LIST_NO_FREE);
      roadmap_sound_list_add (list, "click_long.wav");
      roadmap_res_get (RES_SOUND, 0, "click_long.wav");
   }

   if (object->long_callback) {
      roadmap_sound_play_list (list);
      (*(object->long_callback)) ();

   } else if (object->callback) {
      roadmap_sound_play_list (list);
      (*(object->callback)) ();
   }
#else
   if (object->long_callback) {
      (*(object->long_callback)) ();

   } else if (object->callback) {
      (*(object->callback)) ();
   }
#endif

   return 1;
}


static void roadmap_screen_obj_reload (void) {

   const char *cursor;
   RoadMapFileContext file;
   unsigned int i;
   int height = roadmap_canvas_height ();
   const char *object_name = NULL;

   for (i=0; i<sizeof(RoadMapObjFiles)/sizeof(RoadMapObjFiles[0]); i++) {

      if (height >= RoadMapObjFiles[i].min_screen_height) {
         object_name = RoadMapObjFiles[i].name;
         break;
      }
   }

   if (!object_name) {
      roadmap_log
         (ROADMAP_ERROR, "Can't find object file for screen height: %d",
          height);
      return;
   }


#if LATER_SKIN_SUPPORT
   for (cursor = roadmap_file_map ("skin", object_name, NULL, "r", &file);
        cursor != NULL;
        cursor = roadmap_file_map ("skin", object_name, cursor, "r", &file)) {

      roadmap_screen_obj_load (roadmap_file_base(file), roadmap_file_size(file));

      roadmap_file_unmap (&file);
      return;
   }
#else
   {
   static char *configs[] = {"config", "user", NULL};
   char **config;
   for (config = configs; *config != NULL; config++) {
       for (cursor = roadmap_scan (*config, object_name);
            cursor != NULL;
            cursor = roadmap_scan_next (*config, object_name, cursor)) {

           if (roadmap_file_map (cursor, object_name, "r", &file) == NULL) {
              roadmap_log (ROADMAP_ERROR,
                "cannot map file %s in %s", object_name, cursor);
           } else {
              roadmap_screen_obj_load
                 (roadmap_file_base(file), roadmap_file_size(file));

              roadmap_file_unmap (&file);
           }
       }
   }
   }
#endif
}


void roadmap_screen_obj_move (const char *name,
                              const RoadMapGuiPoint *position) {

   RoadMapScreenObj cursor = roadmap_screen_obj_search (name);

   if (cursor != NULL) {

      cursor->pos_x = position->x;
      cursor->pos_y = position->y;
   }
}


void roadmap_screen_obj_offset (int x, int y) {

   OffsetX += x;
   OffsetY += y;
}


void roadmap_screen_obj_initialize (void) {

   roadmap_pointer_register_pressed
      (roadmap_screen_obj_pressed, POINTER_HIGH);
   roadmap_pointer_register_released
      (roadmap_screen_obj_released, POINTER_HIGH);
   roadmap_pointer_register_short_click
      (roadmap_screen_obj_short_click, POINTER_HIGH);
   roadmap_pointer_register_long_click
      (roadmap_screen_obj_long_click, POINTER_HIGH);

   roadmap_screen_obj_reload ();
}


void roadmap_screen_obj_draw (void) {

   RoadMapScreenObj cursor;

   for (cursor = RoadMapObjectList; cursor != NULL; cursor = cursor->next) {
      int state = 0;
      int angle = 0;
      int image_mode = IMAGE_NORMAL;
      RoadMapGuiPoint pos;

      if (cursor->state_fn) {
         int rawstate = (*cursor->state_fn) ();

         angle = ROADMAP_STATE_DECODE_ANGLE(rawstate);
         state = ROADMAP_STATE_DECODE_STATE(rawstate);
         if (state < 0 || state >= MAX_STATES) continue;
      }

      if (cursor == RoadMapScreenObjSelected) {
         image_mode = IMAGE_SELECTED;
      }

      roadmap_screen_obj_pos (cursor, &pos);

#if LATER_ICON_SUPPORT
      if (cursor->images[state]) {

         roadmap_canvas_draw_image (cursor->images[state], &pos,
                                    cursor->opacity, image_mode);
      }
#endif

      if (cursor->sprites[state]) {
         
         if (cursor->flags & OBJ_FLAG_NO_ROTATE) {
            roadmap_sprite_draw (cursor->sprites[state], &pos,
                                 -roadmap_math_get_orientation());
         } else {
            roadmap_sprite_draw (cursor->sprites[state], &pos, angle);
         }
      }
   }
}



