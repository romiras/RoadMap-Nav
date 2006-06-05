/* navigate_bar.c - implement navigation bar
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
 * SYNOPSYS:
 *
 *   See navigate_bar.h
 */

#include <stdlib.h>
#include <string.h>
#include "roadmap.h"
#include "roadmap_canvas.h"
#include "roadmap_path.h"
#include "roadmap_file.h"
#include "roadmap_math.h"

#include "navigate_main.h"
#include "navigate_bar.h"

typedef struct {
   const char     *image_file;
   int             min_screen_width;
   RoadMapGuiPoint instruction_pos;
   RoadMapGuiRect  distance_rect;
   RoadMapGuiPoint distance_value_pos;
   RoadMapGuiPoint distance_unit_pos;
   int             street_start;
   int             street_width;
} NavigateBarPanel;

static NavigateBarPanel NavigateBarDefaultPanels[] = {
   {"nav_panel_wide.bmp", 320, {0, 0}, {0, 65, 48, 110}, {3, 70}, {3, 95}, 75, 230},
   {"nav_panel.bmp", 240, {0, 0}, {55, 0, 95, 50}, {55, 5}, {58, 30}, 98, 133}
};


static NavigateBarPanel *NavigatePanel = NULL;

const char NAVIGATE_DIR_IMG[][40] = {
   "nav_turn_left.bmp",
   "nav_turn_right.bmp",
   "nav_keep_left.bmp",
   "nav_keep_right.bmp",
   "nav_continue.bmp",
   "nav_approaching.bmp"
};

static RoadMapImage NavigateBarImage;
static RoadMapImage NavigateBarBG;
static RoadMapImage NavigateDirections[LAST_DIRECTION];
static int NavigateBarInitialized = 0;
static RoadMapGuiPoint NavigateBarLocation;

static enum NavigateInstr NavigateBarCurrentInstr = LAST_DIRECTION;
static int  NavigateBarCurrentDistance = -1;

static RoadMapImage navigate_bar_load_image (const char *name) {

   RoadMapImage image = NULL;
   const char *cursor;
   char *file = roadmap_path_join ("icons", name);

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

   return image;
}


void navigate_bar_initialize (void) {

   int i;
   int width;

   if (NavigateBarInitialized) return;
      
   width = roadmap_canvas_width ();

   for (i=0;
       (unsigned)i<
        sizeof(NavigateBarDefaultPanels)/sizeof(NavigateBarDefaultPanels[0]);
       i++) {

      if (width >= NavigateBarDefaultPanels[i].min_screen_width) {
         NavigatePanel = NavigateBarDefaultPanels + i;
         break;
      }
   }

   if (!NavigatePanel) {
      roadmap_log (ROADMAP_ERROR, "Can't find nav panel for screen width: %d",
            width);
      NavigateBarInitialized = -1;
   }

   NavigateBarBG    = navigate_bar_load_image (NavigatePanel->image_file);
   NavigateBarImage = navigate_bar_load_image (NavigatePanel->image_file);
   if (!NavigateBarBG || !NavigateBarImage) goto error;

   for (i=0; i<LAST_DIRECTION; i++) {
      NavigateDirections[i] = navigate_bar_load_image (NAVIGATE_DIR_IMG[i]);
      if (!NavigateDirections[i]) goto error;
   }
      

   NavigateBarLocation.x = 0;
   NavigateBarLocation.y = 0;

   NavigateBarInitialized = 1;
   return;

error:
   NavigateBarInitialized = -1;
}


void navigate_bar_set_instruction (enum NavigateInstr instr) {

   RoadMapGuiPoint pos = NavigatePanel->instruction_pos;

   if (NavigateBarInitialized != 1) return;
   
   roadmap_canvas_copy_image (NavigateBarImage, &pos, NULL, NavigateBarBG,
                              CANVAS_COPY_NORMAL);

   roadmap_canvas_copy_image (NavigateBarImage, &pos, NULL,
                              NavigateDirections[(int)instr],
                              CANVAS_COPY_BLEND);

   NavigateBarCurrentInstr = instr;

}


void navigate_bar_set_distance (int distance) {

   char str[100];
   char unit_str[20];
   int  distance_far;
   RoadMapGuiPoint position = {0, 0};

   if (NavigateBarInitialized != 1) return;
   if (NavigateBarCurrentDistance == distance) return;

   /* erase the old distance */
   roadmap_canvas_copy_image (NavigateBarImage, &position,
                              &NavigatePanel->distance_rect,
                              NavigateBarBG,
                              CANVAS_COPY_NORMAL);

   distance_far =
      roadmap_math_to_trip_distance(distance);

   if (distance_far > 0) {

      snprintf (str, sizeof(str), "%d", distance_far);
      snprintf (unit_str, sizeof(unit_str), "%s", roadmap_math_trip_unit());
   } else {

      snprintf (str, sizeof(str), "%d", distance);
      snprintf (unit_str, sizeof(unit_str), "%s",
                roadmap_math_distance_unit());
   };

   position = NavigatePanel->distance_value_pos;
   roadmap_canvas_draw_image_text
      (NavigateBarImage, &NavigatePanel->distance_value_pos, 22, str);


   position = NavigatePanel->distance_unit_pos;
   roadmap_canvas_draw_image_text
      (NavigateBarImage, &NavigatePanel->distance_unit_pos, 18, unit_str);
}


static int navigate_bar_align_text (char *text, char **line1, char **line2,
                                    int size) {

   int width, ascent, descent;

   roadmap_canvas_get_text_extents
      (text, size, &width, &ascent, &descent);

   if (width >= 2 * NavigatePanel->street_width) return -1;

   if (width < NavigatePanel->street_width) {

      *line1 = text;
      return 1;

   } else {

      /* Check if we can place the text in two lines */

      char *text_line = text;
      char *text_end = text_line + strlen(text_line);
      char *p1 = text_line + (strlen(text_line) / 2);
      char *p2 = p1;

      while (p1 > text_line) {
         if (*p1 == ' ') {
            break;
         }
         p1 -= 1;
      }
      while (p2 < text_end) {
         if (*p2 == ' ') {
            break;
         }
         p2 += 1;
      }
      if (text_end - p1 > p2 - text_line) {
         p1 = p2;
      }
      if (p1 > text_line) {

         char saved = *p1;
         *p1 = 0;

         roadmap_canvas_get_text_extents
            (text_line, size, &width, &ascent, &descent);

         if (width < NavigatePanel->street_width) {

            roadmap_canvas_get_text_extents
               (text_line, size, &width, &ascent, &descent);

            if (width < NavigatePanel->street_width) {

               *line1 = text_line;
               *line2 = p1 + 1;
               return 2;
            }
         }

         *p1 = saved;
      }
   }

   return -1;
}


void navigate_bar_set_street (const char *street) {

#define NORMAL_SIZE 20
#define SMALL_SIZE  16

   int width, ascent, descent;
   int size;
   char *line1;
   char *line2;
   char *text;
   int num_lines;
   int i;

   //street = "רחוב המלך הגדולי מלקט פרחים 18";
   if (NavigateBarInitialized != 1) return;

   text = strdup(street);

   size = NORMAL_SIZE;

   num_lines = navigate_bar_align_text (text, &line1, &line2, size);

   if (num_lines < 0) {
      /* Try again with a smaller font size */

      size = SMALL_SIZE;
      num_lines = navigate_bar_align_text (text, &line1, &line2, size);
   }

   /* Cut some text until it fits */
   while (num_lines < 0) {

      char *end = text + strlen(text) - 1;

      while ((end > text) && (*end != ' ')) end--;

      if (end == text) {

         roadmap_log (ROADMAP_ERROR, "Can't align street in nav bar: %s",
                      street);

         free(text);
         return;
      }

      *end = '\0';
      num_lines = navigate_bar_align_text (text, &line1, &line2, size);
   }

   for (i=0; i < num_lines; i++) {

      char *line;
      RoadMapGuiPoint position = {NavigatePanel->street_start, 5};

      if (i ==0 ) {
         line = line1;
      } else {
         line = line2;
         position.y = 20;
      }

      roadmap_canvas_get_text_extents
         (line, size, &width, &ascent, &descent);
      
      position.x = NavigatePanel->street_width - width + NavigatePanel->street_start;

      roadmap_canvas_draw_image_text (NavigateBarImage, &position, size, line);
   }
}


void navigate_bar_draw (void) {

   if (NavigateBarInitialized != 1) return;

   roadmap_canvas_draw_image (NavigateBarImage, &NavigateBarLocation, 0,
         IMAGE_NORAML);
}
