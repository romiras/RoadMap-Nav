/* editor_line.c - line databse layer
 *
 * LICENSE:
 *
 *   Copyright 2005 Ehud Shabtai
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
 *   See editor_line.h
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../roadmap.h"
#include "../roadmap_file.h"
#include "../roadmap_path.h"
#include "../roadmap_locator.h"
#include "../roadmap_point.h"
#include "../roadmap_line.h"
#include "../roadmap_square.h"
#include "../roadmap_shape.h"
#include "../roadmap_street.h"
#include "../roadmap_math.h"
#include "../roadmap_navigate.h"

#include "editor_db.h"
#include "editor_log.h"
#include "editor_point.h"
#include "editor_line.h"
#include "editor_trkseg.h"
#include "editor_shape.h"
#include "editor_square.h"
#include "editor_street.h"
#include "editor_route.h"
#include "editor_override.h"
#include "editor_main.h"

static editor_db_section *ActiveLinesDB;
static editor_db_section *ActiveLinesDelDB;

static void editor_lines_activate (void *context) {
   ActiveLinesDB = (editor_db_section *) context;
}

static void editor_lines_del_activate (void *context) {
   ActiveLinesDelDB = (editor_db_section *) context;
}

roadmap_db_handler EditorLinesHandler = {
   "lines",
   editor_map,
   editor_lines_activate,
   editor_unmap
};

roadmap_db_handler EditorLinesDelHandler = {
   "lines_del",
   editor_map,
   editor_lines_del_activate,
   editor_unmap
};

#if 0
int editor_line_del_add (int line_id, int cfcc) {

   int id = editor_db_add_item (ActiveLinesDelDB, &line_id);

   if (id == -1) {
      editor_db_grow ();
      id = editor_db_add_item (ActiveLinesDelDB, &line_id);
   }

   if (id != -1) {
      editor_db_mark_cfcc (cfcc);
   }

   return id;
}


void editor_line_del_remove (int line_id) {

   int *line_db =
         (int *) editor_db_get_item
              (ActiveLinesDelDB, line_id, 0, NULL);

   assert (line_db != NULL);
   *line_db = -1;
}


int editor_line_del_find (int line_id, int cfcc) {
   
   int count;
   int i;
   
   if (cfcc > 0 && !editor_db_is_cfcc_marked (cfcc)) return 0;

   count = editor_db_get_item_count (ActiveLinesDelDB);
   
   for (i=0; i<count; i++) {

      if (line_id == *(int *)editor_db_get_item
                              (ActiveLinesDelDB, i, 0, NULL)) {
         return 1;
      }
   }

   return 0;
}
#endif
 
int editor_line_add
         (int p_from,
          int p_to,
          int trkseg,
          int cfcc,
          int flags) {

   editor_db_line line;
   int id;

   line.point_from = p_from;
   line.point_to = p_to;

   line.first_trkseg = line.last_trkseg = trkseg;

   line.cfcc = cfcc;
   line.flags = flags;

   line.street = -1;
   line.range = -1;
   line.route = -1;

   id = editor_db_add_item (ActiveLinesDB, &line);

   if (id == -1) {
      editor_db_grow ();
      id = editor_db_add_item (ActiveLinesDB, &line);
   }

   if (id == -1) return -1;
   assert (editor_line_length (id) > 0);

   editor_square_add_line (id, p_from, p_to, trkseg, cfcc);

   editor_log
      (ROADMAP_INFO,
       "Adding new line - from:%d, to:%d, trkseg:%d, cfcc:%d, flags:%d",
       p_from, p_to, trkseg, cfcc, flags);

   if (id == -1) {
      editor_log (ROADMAP_ERROR, "Can't add new line.");
   }

   return id;
}


void editor_line_get (int line,
                      RoadMapPosition *from,
                      RoadMapPosition *to,
                      int *trkseg,
                      int *cfcc,
                      int *flags) {
   
   editor_db_line *line_db;

   line_db = (editor_db_line *) editor_db_get_item
                           (ActiveLinesDB, line, 0, NULL);

   if (from) editor_point_position (line_db->point_from, from);
   if (to) editor_point_position (line_db->point_to, to);

   if (trkseg) *trkseg = line_db->first_trkseg;
   if (cfcc) *cfcc = line_db->cfcc;
   if (flags) *flags = line_db->flags;
}


void editor_line_get_points (int line, int *from, int *to) {

   editor_db_line *line_db;

   line_db = (editor_db_line *) editor_db_get_item
                           (ActiveLinesDB, line, 0, NULL);

   *from = line_db->point_from;
   *to = line_db->point_to;
}


void editor_line_modify_properties (int line,
                                    int cfcc,
                                    int flags) {
   
   editor_db_line *line_db;

   line_db = (editor_db_line *) editor_db_get_item
                           (ActiveLinesDB, line, 0, NULL);

   assert (line_db != NULL);

   if (line_db == NULL) return;

   line_db->cfcc = cfcc;
   line_db->flags = flags;
}


int editor_line_copy (int line, int cfcc, int fips) {

   int p_from, p_to;
   int trkseg_from, trkseg_to;
   RoadMapPosition pos;
   int j;
   int square;
   int first_shape_line;
   int last_shape_line;
   int first;
   int last;
   int editor_first_shape = -1;
   int editor_last_shape = -2;
   int line_id;
   editor_db_line *line_db;

   editor_log_push ("editor_line_copy");

   roadmap_locator_activate (fips);

   if (editor_override_line_set_flags (line,
            editor_override_line_get_flags (line)|EDITOR_LINE_DELETED)) {
      editor_log (ROADMAP_ERROR, "Can't override roadmap line.");
      editor_log_pop ();
      return -1;
   }

   roadmap_line_points (line, &p_from, &p_to);
   p_from = editor_point_roadmap_to_editor (p_from);
   p_to = editor_point_roadmap_to_editor (p_to);

   if ((p_from == -1) || (p_to == -1)) {
      editor_log (ROADMAP_ERROR, "Can't copy points.");
      editor_log_pop ();
      return -1;
   }

   roadmap_line_to (line, &pos);
   trkseg_to = editor_point_add (&pos, 0, 0);
   roadmap_line_from (line, &pos);
   trkseg_from = editor_point_add (&pos, 0, 0);

   if ((trkseg_from == -1) || (trkseg_to == -1)) {
      editor_log (ROADMAP_ERROR, "Can't create points.");
      editor_log_pop ();
      return -1;
   }
         
   square = roadmap_square_search (&pos);

   //TODO get avg speed and compute time for this trkseg
   if ((roadmap_shape_in_square (square, &first_shape_line,
                                 &last_shape_line) > 0) &&
       (roadmap_shape_of_line (line, first_shape_line,
                                 last_shape_line,
                                 &first, &last) > 0)) {

      RoadMapPosition shape_pos;

      shape_pos = pos;

      for (j = first; j <= last; j++) {
               
         roadmap_shape_get_position (j, &shape_pos);
         editor_last_shape =
               editor_shape_add
                        (shape_pos.longitude - pos.longitude,
                         shape_pos.latitude - pos.latitude,
                         -1);

         if (editor_last_shape == -1) {
            editor_log (ROADMAP_ERROR, "Can't create new shape. Line creation is aborted.");
            editor_log_pop ();
            return -1;
         }

         if (editor_first_shape == -1) {
            editor_first_shape = editor_last_shape;
         }

         pos = shape_pos;
      }
   }
         
   j = editor_trkseg_add
            (trkseg_from,
             trkseg_to,
             editor_first_shape,
             editor_last_shape,
             -1,
             -1,
             ED_TRKSEG_FAKE);

   if (j == -1) {
      editor_log_pop ();
      return -1;
   }

   line_id = editor_line_add
            (p_from, p_to, j, cfcc, 0);

   if (line_id == -1) {
      editor_log_pop ();
      return -1;
   }

   
   line_db = (editor_db_line *) editor_db_get_item
                           (ActiveLinesDB, line_id, 0, NULL);

   /* Add roadmap line trksegs to copied line */
   editor_override_line_get_trksegs (line, &first, &last);

   if (first != -1) {
      line_db->last_trkseg = last;
      editor_trkseg_connect (line_db->first_trkseg, first);
   }
   
   editor_route_segment_copy (line, 0, line_id);
   editor_street_copy_street (line, 0, line_id);
   editor_street_copy_range (line, 0, line_id);

   //TODO: copy street

   editor_log (ROADMAP_INFO, "Copied roadmap line:%d as editor line:%d",
         line, line_id);

   editor_log_pop ();
   return line_id;
}


int editor_line_split (RoadMapNeighbour *line,
                       RoadMapPosition *previous_point, 
                       RoadMapPosition *split_position,
                       int *split_point) {

   RoadMapPosition from;
   RoadMapPosition to;
   EditorStreetProperties properties;
   int i;
   int new_point;
   int new_line_id;
   editor_db_line *line_db;
   int azymuth;
   int lines[2];

   int trkseg;
   int new_trkseg_first;
   int new_trkseg_curr;

   editor_log_push ("editor_line_split");
   if (line->plugin_id != EditorPluginID) {

      line->line = editor_line_copy (line->line, line->cfcc, line->fips);
      if (line->line == -1) {
         editor_log (ROADMAP_ERROR, "Can't split line. Line copy failed.");
         editor_log_pop ();
         return -1;
      }
      line->plugin_id = EditorPluginID;
   }

   line_db = (editor_db_line *) editor_db_get_item
                           (ActiveLinesDB, line->line, 0, NULL);

   editor_point_position (line_db->point_from, &from);
   editor_point_position (line_db->point_to, &to);

   if (*split_point != -1) {
      new_point = *split_point;
   } else {
      *split_point = new_point = editor_point_add (split_position, 0, 0);

      if (new_point == -1) {
         editor_log (ROADMAP_ERROR, "Can't split line. Add point failed.");
         editor_log_pop ();
         return -1;
      }
   }

   /* split trksegs */
   trkseg = line_db->first_trkseg;
   new_trkseg_first = new_trkseg_curr = -1;

   while (trkseg != -1) {
      int new_trkseg_prev = new_trkseg_curr;

      new_trkseg_curr = editor_trkseg_split (trkseg, split_position);

      if (new_trkseg_first == -1) {
         new_trkseg_first = new_trkseg_curr;
      } else {
         editor_trkseg_connect (new_trkseg_prev, new_trkseg_curr);
      }

      trkseg = editor_trkseg_next (trkseg);
   }
      
   new_line_id =
      editor_line_add
         (new_point, line_db->point_to, 
          new_trkseg_first,
          line_db->cfcc, line_db->flags);

   if (new_line_id == -1) {
      editor_log (ROADMAP_ERROR, "Can't create new line.");
      editor_log_pop ();
      return -1;
   }

   line_db->point_to = new_point;

   editor_route_segment_copy (line->line, 1, new_line_id);
   editor_street_copy_street (line->line, 1, new_line_id);

   editor_street_get_properties (line->line, &properties);

   if (properties.range_id != -1) {
      int l_from, l_to, r_from, r_to;

      editor_street_copy_range (line->line, 1, new_line_id);
      editor_street_get_street_range
         (&properties, ED_STREET_LEFT_SIDE, &l_from, &l_to);
      editor_street_get_street_range
         (&properties, ED_STREET_RIGHT_SIDE, &r_from, &r_to);

      lines[0] = line->line;
      lines[1] = new_line_id;
      editor_street_distribute_range (lines, 2, l_from, l_to, r_from, r_to);
   }

   editor_log (ROADMAP_INFO, "split %d, to %d & %d", line->line,
         line->line, new_line_id);

   azymuth = roadmap_math_azymuth (previous_point, split_position);

   if (roadmap_delta_direction
         (azymuth, roadmap_math_azymuth (&from, split_position)) >
       roadmap_delta_direction
         (azymuth, roadmap_math_azymuth (&to, split_position))) {

      i = line->line;
      line->line = new_line_id;
      new_line_id = 1;
   }

   editor_log (ROADMAP_INFO, "current line: %d", line->line);

   editor_log_pop ();
   return new_line_id;
}


int editor_line_length (int line) {

   RoadMapPosition p1;
   RoadMapPosition p2;
   editor_db_line *line_db;
   int length = 0;
   int trk_from;
   int first_shape;
   int last_shape;
   int i;

   line_db = (editor_db_line *) editor_db_get_item
                           (ActiveLinesDB, line, 0, NULL);

   editor_trkseg_get (line_db->first_trkseg,
                      &trk_from, NULL, &first_shape, &last_shape, NULL);

   editor_point_position (line_db->point_from, &p1);

   if (first_shape > -1) {
      editor_point_position (trk_from, &p2);

      for (i=first_shape; i<=last_shape; i++) {

         editor_shape_position (i, &p2);
         length += roadmap_math_distance (&p1, &p2);
         p1 = p2;
      }
   }

   editor_point_position (line_db->point_to, &p2);
   length += roadmap_math_distance (&p1, &p2);

   return length;
}


int editor_line_get_street (int line,
                            int *street,
                            int *range) {

   editor_db_line *line_db;

   line_db =
      (editor_db_line *) editor_db_get_item
         (ActiveLinesDB, line, 0, NULL);

   if (line_db == NULL) return -1;

   if (street != NULL) *street = line_db->street;
   if (range != NULL) *range = line_db->range;

   return 0;
}


int editor_line_set_street (int line,
                            int *street,
                            int *range) {

   editor_db_line *line_db;

   line_db =
      (editor_db_line *) editor_db_get_item
         (ActiveLinesDB, line, 0, NULL);

   if (line_db == NULL) return -1;

   if (street != NULL) line_db->street = *street;
   if (range != NULL) line_db->range = *range;

   return 0;
}


int editor_line_get_route (int line) {
   editor_db_line *line_db;

   line_db =
      (editor_db_line *) editor_db_get_item (ActiveLinesDB, line, 0, NULL);

   if (line_db == NULL) return -1;

   return line_db->route;
}


int editor_line_set_route (int line, int route) {
   editor_db_line *line_db;

   line_db =
      (editor_db_line *) editor_db_get_item (ActiveLinesDB, line, 0, NULL);

   if (line_db == NULL) return -1;

   line_db->route = route;

   return 0;
}


void editor_line_get_trksegs (int line, int *first, int *last) {
   editor_db_line *line_db;

   line_db =
      (editor_db_line *) editor_db_get_item (ActiveLinesDB, line, 0, NULL);

   if (line_db == NULL) {
      *first = *last = -1;
      return;
   }

   *first = line_db->first_trkseg;
   *last = line_db->last_trkseg;
}


void editor_line_set_trksegs (int line, int first, int last) {
   editor_db_line *line_db;

   line_db =
      (editor_db_line *) editor_db_get_item (ActiveLinesDB, line, 0, NULL);

   if (line_db == NULL) {
      return;
   }

   line_db->first_trkseg = first;
   line_db->last_trkseg = last;
}


double editor_line_get_avg_speed (int line, int direction) {

   editor_db_line *line_db;
   int length;
   int start_time;
   int end_time;

   line_db = (editor_db_line *) editor_db_get_item
                           (ActiveLinesDB, line, 0, NULL);


   length = editor_line_length (line);

   if (!length) return -1;

   editor_trkseg_get_time (line_db->first_trkseg, &start_time, &end_time);

   if (start_time == end_time) return -1;

   return  1.0 * length / (end_time - start_time);
}

