/* roadmap_screen.c - Draw the map on the screen.
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
 *   See roadmap_screen.h.
 */

#include <math.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_gui.h"
#include "roadmap_math.h"
#include "roadmap_config.h"
#include "roadmap_layer.h"
#include "roadmap_square.h"
#include "roadmap_line.h"
#include "roadmap_shape.h"
#include "roadmap_point.h"
#include "roadmap_polygon.h"
#include "roadmap_locator.h"
#include "roadmap_navigate.h"
#include "roadmap_lang.h"

#include "roadmap_sprite.h"
#include "roadmap_object.h"
#include "roadmap_trip.h"
#include "roadmap_message.h"
#include "roadmap_canvas.h"
#include "roadmap_screen_obj.h"
#include "roadmap_state.h"
#include "roadmap_pointer.h"
#include "roadmap_display.h"
#include "roadmap_label.h"
#include "roadmap_plugin.h"
#include "roadmap_skin.h"
#include "roadmap_main.h"

#include "roadmap_screen.h"

#ifdef SSD
#include "ssd/ssd_dialog.h"
#endif

static RoadMapConfigDescriptor RoadMapConfigAccuracyMouse =
                        ROADMAP_CONFIG_ITEM("Accuracy", "Mouse");

static RoadMapConfigDescriptor RoadMapConfigMapBackground =
                        ROADMAP_CONFIG_ITEM("Map", "Background");

static RoadMapConfigDescriptor RoadMapConfigMapSigns =
                        ROADMAP_CONFIG_ITEM("Map", "Signs");

static RoadMapConfigDescriptor RoadMapConfigMapRefresh =
                        ROADMAP_CONFIG_ITEM("Map", "Refresh");

static RoadMapConfigDescriptor RoadMapConfigStylePrettyDrag =
                  ROADMAP_CONFIG_ITEM("Style", "Pretty Lines when Dragging");

static RoadMapConfigDescriptor RoadMapConfigStyleObjects =
                  ROADMAP_CONFIG_ITEM("Style", "Show Objects when Dragging");

static RoadMapConfigDescriptor RoadMapConfigMapLabels =
                        ROADMAP_CONFIG_ITEM("Map", "Labels");

static int RoadMapScreenInitialized = 0;
static int RoadMapScreenFrozen = 0;

static RoadMapGuiPoint RoadMapScreenPointerLocation;
static RoadMapPosition RoadMapScreenCenter;

static int RoadMapScreenViewMode = VIEW_MODE_2D;
static int RoadMapScreenOrientationMode = ORIENTATION_DYNAMIC;
static int RoadMapScreen3dHorizon;
static int RoadMapScreenLabels;
static int RoadMapScreenRotation;
static int RoadMapScreenWidth;
static int RoadMapScreenHeight;

static int RoadMapScreenDeltaX;
static int RoadMapScreenDeltaY;
static int RoadMapScreenCenterDelta;
static RoadMapGuiPoint RoadMapScreenCenterPixel;

static char *SquareOnScreen;
static int   SquareOnScreenCount;

static RoadMapGuiPoint RoadMapScreenLowerEdge;
static int RoadMapScreenAreaDist[LAYER_PROJ_AREAS-1];

static RoadMapPen RoadMapScreenLastPen = NULL;

static void roadmap_screen_after_refresh (void) {}

static RoadMapScreenSubscriber RoadMapScreenAfterRefresh =
                                       roadmap_screen_after_refresh;


/* Define the buffers used to group all actual drawings. */

#define ROADMAP_SCREEN_BULK  4096

/* This is a default definition, because we might want to set this smaller
 * for some memory-starved targets.
 */
#ifndef ROADMAP_MAX_VISIBLE
#define ROADMAP_MAX_VISIBLE  20000
#endif

#define SQUARE_IN_VIEW 0x1
#define SQUARE_DRAWN   0x2

#ifdef J2ME
#define REFRESH_FLOW_CONTROL_TIMEOUT 75
#else
#define REFRESH_FLOW_CONTROL_TIMEOUT 50
#endif
#define SCREEN_FAST_DRAG  0x1
#define SCREEN_FAST_OTHER 0x2

static struct {

   int *cursor;
   int *end;
   int data[ROADMAP_SCREEN_BULK];

} RoadMapScreenObjects;

struct roadmap_screen_point_buffer {

   RoadMapGuiPoint *cursor;
   RoadMapGuiPoint *end;
   RoadMapGuiPoint data[ROADMAP_SCREEN_BULK];
};

static struct roadmap_screen_point_buffer RoadMapScreenLinePoints;
static RoadMapGuiPoint *RoadMapScreenLinePointsAccum = RoadMapScreenLinePoints.data;
static struct roadmap_screen_point_buffer RoadMapScreenPoints;

static int RoadMapPolygonGeoPoints[ROADMAP_SCREEN_BULK];


static RoadMapPen RoadMapBackground = NULL;
static RoadMapPen RoadMapPenEdges = NULL;

static int RoadMapScreenRefreshFlowControl = 0;
static int RoadMapScreenFastRefresh = 0;
static void roadmap_screen_repaint_now (void);

static void roadmap_screen_flush_points (void) {

   if (RoadMapScreenPoints.cursor == RoadMapScreenPoints.data) return;

   dbg_time_start(DBG_TIME_FLUSH_POINTS);
   roadmap_math_rotate_coordinates
       (RoadMapScreenPoints.cursor - RoadMapScreenPoints.data,
        RoadMapScreenPoints.data);

   roadmap_canvas_draw_multiple_points
       (RoadMapScreenPoints.cursor - RoadMapScreenPoints.data,
        RoadMapScreenPoints.data);

   RoadMapScreenPoints.cursor  = RoadMapScreenPoints.data;

   dbg_time_end(DBG_TIME_FLUSH_POINTS);
}


static void roadmap_screen_flush_lines (void) {

   if (RoadMapScreenObjects.cursor == RoadMapScreenObjects.data) return;

   dbg_time_start(DBG_TIME_FLUSH_LINES);

   roadmap_math_rotate_coordinates
       (RoadMapScreenLinePoints.cursor - RoadMapScreenLinePoints.data,
        RoadMapScreenLinePoints.data);

   dbg_time_end(DBG_TIME_FLUSH_LINES);
   roadmap_canvas_draw_multiple_lines
      (RoadMapScreenObjects.cursor - RoadMapScreenObjects.data,
       RoadMapScreenObjects.data,
       RoadMapScreenLinePoints.data, RoadMapScreenFastRefresh);

   dbg_time_start(DBG_TIME_FLUSH_LINES);
   if (RoadMapScreenLinePoints.cursor < RoadMapScreenLinePointsAccum) {
      int count = RoadMapScreenLinePointsAccum - RoadMapScreenLinePoints.cursor;
      memmove(RoadMapScreenLinePoints.data, RoadMapScreenLinePointsAccum,
          sizeof(*RoadMapScreenLinePointsAccum) * count);

      RoadMapScreenLinePointsAccum = RoadMapScreenLinePoints.data + count;
   } else {
      RoadMapScreenLinePointsAccum = RoadMapScreenLinePoints.data;
   }
   RoadMapScreenObjects.cursor = RoadMapScreenObjects.data;
   RoadMapScreenLinePoints.cursor  = RoadMapScreenLinePoints.data;
   dbg_time_end(DBG_TIME_FLUSH_LINES);
}


#define SEGMENT_START    0x1
#define SEGMENT_END      0x2
#define SEGMENT_AS_POINT 0x4

static void roadmap_screen_add_segment_point (RoadMapGuiPoint *point,
                                              RoadMapPen *pens,
                                              int num_pens,
                                              int flags) {

   int i;
   RoadMapPen pen;

   dbg_time_start(DBG_TIME_ADD_SEGMENT);

   num_pens--;

   if (point) {

      if (RoadMapScreenViewMode == VIEW_MODE_3D) {
         int edge_distance = roadmap_math_screen_distance
            (point, &RoadMapScreenLowerEdge, MATH_DIST_SQUARED);

         for (i=0; i<LAYER_PROJ_AREAS-1; i++) {
            if ((i == num_pens) || (edge_distance < RoadMapScreenAreaDist[i]))
                  break;
         }

         pen = pens[i];
      } else {
         pen = pens[0];
      }

      if (flags & SEGMENT_AS_POINT) {

         /* This is a speical handling of a point */

         if (!pen) {
            dbg_time_end(DBG_TIME_ADD_SEGMENT);
            return;
         }

         if (RoadMapScreenPoints.cursor >= RoadMapScreenPoints.end) {
            roadmap_screen_flush_points ();
         }

         RoadMapScreenPoints.cursor[0] = *point;
         RoadMapScreenPoints.cursor += 1;

      } else {
         /* This is the start of the segment */
         *(RoadMapScreenLinePointsAccum++) = *point;
      }

   } else {
      pen = RoadMapScreenLastPen;
   }

   if ((RoadMapScreenLastPen != pen) ||
       (flags & SEGMENT_END)) {

      if (RoadMapScreenLastPen &&
         (RoadMapScreenLinePointsAccum - RoadMapScreenLinePoints.cursor) > 1) {

         *RoadMapScreenObjects.cursor =
            RoadMapScreenLinePointsAccum - RoadMapScreenLinePoints.cursor;
         RoadMapScreenLinePoints.cursor = RoadMapScreenLinePointsAccum;
         RoadMapScreenObjects.cursor += 1;
      } else {
         RoadMapScreenLinePointsAccum = RoadMapScreenLinePoints.cursor;
      }


      if (RoadMapScreenLastPen != pen) {
         dbg_time_end(DBG_TIME_ADD_SEGMENT);
         roadmap_screen_flush_lines ();
         roadmap_screen_flush_points ();
         dbg_time_start(DBG_TIME_ADD_SEGMENT);
         if (pen) roadmap_canvas_select_pen (pen);
         RoadMapScreenLastPen = pen;
      }

      if (!(flags & SEGMENT_END) && !(flags & SEGMENT_AS_POINT)) {
         *(RoadMapScreenLinePointsAccum++) = *point; /* Show the start of this segment. */
      }
   }

   dbg_time_end(DBG_TIME_ADD_SEGMENT);
}


#ifndef J2ME
void roadmap_screen_draw_one_line (RoadMapPosition *from,
                                   RoadMapPosition *to,
                                   int fully_visible,
                                   RoadMapPosition *first_shape_pos,
                                   int first_shape,
                                   int last_shape,
                                   RoadMapShapeItr shape_itr,
                                   RoadMapPen *pens,
                                   int num_pens,
                                   int *total_length_ptr,
                                   RoadMapGuiPoint *middle,
                                   int *angle) {

   RoadMapGuiPoint point0;
   RoadMapGuiPoint point1;

   /* These are used when the line has a shape: */
   RoadMapPosition midposition;
   RoadMapPosition last_midposition;

   int i;

   int last_point_visible = 0;
   int longest = -1;

   dbg_time_start(DBG_TIME_DRAW_ONE_LINE);

   if (total_length_ptr) *total_length_ptr = 0;

   fully_visible = 0;

   /* if the pen has changed, we need to flush the previous lines and points
    */

   if (first_shape >= 0) {
      /* Draw a shaped line. */

      if (last_shape - first_shape + 3 >=
            RoadMapScreenLinePoints.end - RoadMapScreenLinePoints.cursor) {

         if (last_shape - first_shape + 3 >=
               (RoadMapScreenLinePoints.end - RoadMapScreenLinePoints.data)) {

            roadmap_log (ROADMAP_ERROR,
                  "cannot show all shape points (%d entries needed).",
                  last_shape - first_shape + 3);

            last_shape =
               first_shape
               + (RoadMapScreenLinePoints.data - RoadMapScreenLinePoints.cursor)
               - 3;
         }

         dbg_time_end(DBG_TIME_DRAW_ONE_LINE);
         roadmap_screen_flush_lines ();
         dbg_time_start(DBG_TIME_DRAW_ONE_LINE);
      }

      /* All the shape positions are relative: we need an absolute position
       * to start with.
       */
      last_midposition = *from;
      midposition = *first_shape_pos;

      if (fully_visible) {

         roadmap_math_coordinate (from, &point0);
         roadmap_screen_add_segment_point (&point0, pens, num_pens, 
                                           SEGMENT_START);

         for (i = first_shape; i <= last_shape; ++i) {

            (*shape_itr) (i, &midposition);

            roadmap_math_coordinate (&midposition, &point0);
            roadmap_screen_add_segment_point (&point0, pens, num_pens, 0);
         }

         roadmap_math_coordinate (to, &point0);
         roadmap_screen_add_segment_point (&point0, pens, num_pens,
                                           SEGMENT_END);

      } else {

         last_point_visible = 0; /* We have drawn nothing yet. */

         for (i = first_shape; i <= last_shape; ++i) {

            (*shape_itr) (i, &midposition);

            if (roadmap_math_line_is_visible (&last_midposition, &midposition) && 
                  roadmap_math_get_visible_coordinates
                  (&last_midposition, &midposition, &point0, &point1)) {

               if ((point0.x == point1.x) && (point0.y == point1.y)) {

                  if (last_point_visible) {

                     /* This segment is very short, we can skip it */
                     last_midposition = midposition; 

                     continue;
                  }

               } else {

                  if (total_length_ptr) {

                     int length_sq = roadmap_math_screen_distance
                        (&point1, &point0, MATH_DIST_SQUARED);

                     /* bad math, but it's for a labelling heuristic anyway */
                     *total_length_ptr += length_sq;

                     if (length_sq > longest) {
                        longest = length_sq;
                        if (angle) {
                           *angle = roadmap_math_azymuth(&last_midposition, &midposition);
                        }
                        middle->x = (point1.x + point0.x) / 2;
                        middle->y = (point1.y + point0.y) / 2;
                     }
                  }
               }

               /* Show this line: add 2 points if this is the start of a new
                * complete line (i.e. the first visible line), or just add
                * one more point to the current complete line.
                */
               if (!last_point_visible) {
                  roadmap_screen_add_segment_point (&point0, pens, num_pens,
                                                    SEGMENT_START);
               }

               last_point_visible = roadmap_math_point_is_visible (&midposition);
               if (last_point_visible) {
                  roadmap_screen_add_segment_point (&point1, pens, num_pens, 0);

               } else {

                  /* Show the previous segment as the end of a complete line.
                   * The remaining part of the shaped line, if any, will be
                   * drawn as a new complete line.
                   */
                  roadmap_screen_add_segment_point (&point1, pens, num_pens,
                                                    SEGMENT_END);
                  if (last_shape - i + 3 >=
                        RoadMapScreenLinePoints.end - RoadMapScreenLinePoints.cursor) {
                     dbg_time_end(DBG_TIME_DRAW_ONE_LINE);
                     roadmap_screen_flush_lines ();
                     dbg_time_start(DBG_TIME_DRAW_ONE_LINE);
                  }
               }
            }
            last_midposition = midposition; /* The latest position is our new start. */
         }

         if (roadmap_math_line_is_visible (&last_midposition, to) && 
               roadmap_math_get_visible_coordinates
               (&last_midposition, to, &point0, &point1)) {

            if (total_length_ptr) {

               int length_sq = roadmap_math_screen_distance
                  (&point1, &point0, MATH_DIST_SQUARED);
               if (length_sq) {
                  /* bad math, but it's for a labelling heuristic anyway */
                  *total_length_ptr += length_sq;

                  if (length_sq > longest) {
                     longest = length_sq;
                     if (angle) {
                        *angle = roadmap_math_azymuth(&last_midposition, to);
                     }
                     middle->x = (point1.x + point0.x) / 2;
                     middle->y = (point1.y + point0.y) / 2;
                  }
               }
            }

            if (!last_point_visible) {
               roadmap_screen_add_segment_point (&point0, pens, num_pens,
                                                 SEGMENT_START);
            }
            roadmap_screen_add_segment_point (&point1, pens, num_pens, 0);

            /* set last point as visible to force line completion at the next
             * statement.
             */
            last_point_visible = 1;
         }

         if (last_point_visible) {

            /* End the current complete line. */
            roadmap_screen_add_segment_point (NULL, pens, num_pens,
                                              SEGMENT_END);

         }
      }

   } else {
      /* Draw a line with no shape. */

      /* Optimization: do not draw a line that is obviously not visible. */
      if (! fully_visible) {
         if (! roadmap_math_line_is_visible (from, to)) {
            dbg_time_end(DBG_TIME_DRAW_ONE_LINE);
            return;
         }
      }

      /* Optimization: adjust the edges of the line so
       * they do not go out of the screen. */
      if (!roadmap_math_get_visible_coordinates (from, to, &point0, &point1)) {
         return;
      }

      if ((point0.x == point1.x) && (point0.y == point1.y)) {
         /* draw a point instead of a line */

         roadmap_screen_add_segment_point (&point0, pens, num_pens,
                                           SEGMENT_AS_POINT);

         dbg_time_end(DBG_TIME_DRAW_ONE_LINE);
         return;

      } else {
      
         if (total_length_ptr) {

            *total_length_ptr = roadmap_math_screen_distance
                                   (&point1, &point0, MATH_DIST_SQUARED);

            if (angle) {
               *angle = roadmap_math_azymuth(from, to);
            }
            middle->x = (point1.x + point0.x) / 2;
            middle->y = (point1.y + point0.y) / 2;
         }
      }
      
      if (RoadMapScreenLinePoints.cursor + 2 >= RoadMapScreenLinePoints.end) {
         dbg_time_end(DBG_TIME_DRAW_ONE_LINE);
         roadmap_screen_flush_lines ();
         dbg_time_start(DBG_TIME_DRAW_ONE_LINE);
      }

      roadmap_screen_add_segment_point (&point0, pens, num_pens, SEGMENT_START);
      roadmap_screen_add_segment_point (&point1, pens, num_pens, SEGMENT_END);
   }

   dbg_time_end(DBG_TIME_DRAW_ONE_LINE);
}

#else

void roadmap_screen_draw_one_line (RoadMapPosition *from,
                                   RoadMapPosition *to,
                                   int fully_visible,
                                   RoadMapPosition *first_shape_pos,
                                   int first_shape,
                                   int last_shape,
                                   RoadMapShapeItr shape_itr,
                                   RoadMapPen *pens,
                                   int num_pens,
                                   int *total_length_ptr,
                                   RoadMapGuiPoint *middle,
                                   int *angle) {

   RoadMapGuiPoint point0;
   RoadMapGuiPoint point1;

   /* These are used when the line has a shape: */
   RoadMapPosition midposition;
   RoadMapPosition last_midposition;

   int i;

   int longest = -1;
   int last_point_visible = 0;

   dbg_time_start(DBG_TIME_DRAW_ONE_LINE);

   if (total_length_ptr) *total_length_ptr = 0;

   fully_visible = 0;

   if (first_shape >= 0) {
      /* Draw a shaped line. */
      RoadMapPosition label_p1;
      RoadMapPosition label_p2;

      if (last_shape - first_shape + 3 >=
            RoadMapScreenLinePoints.end - RoadMapScreenLinePoints.cursor) {

         if (last_shape - first_shape + 3 >=
               (RoadMapScreenLinePoints.end - RoadMapScreenLinePoints.data)) {

            roadmap_log (ROADMAP_ERROR,
                  "cannot show all shape points (%d entries needed).",
                  last_shape - first_shape + 3);

            last_shape =
               first_shape
               + (RoadMapScreenLinePoints.data - RoadMapScreenLinePoints.cursor)
               - 3;
         }

         dbg_time_end(DBG_TIME_DRAW_ONE_LINE);
         roadmap_screen_flush_lines ();
         dbg_time_start(DBG_TIME_DRAW_ONE_LINE);
      }

      /* All the shape positions are relative: we need an absolute position
       * to start with.
       */
      last_midposition = *from;
      midposition = *first_shape_pos;

      for (i = first_shape; i <= (last_shape + 1); ++i) {

         if (i<= last_shape) (*shape_itr) (i, &midposition);
         else midposition = *to;

         if (!fully_visible && !roadmap_math_line_is_visible (&last_midposition, &midposition)) {
            last_midposition = midposition;

            if (last_point_visible) {
               roadmap_screen_add_segment_point (NULL, pens, num_pens,
                                                 SEGMENT_END);
               last_point_visible = 0;
            }
            continue;
         }

         if (!last_point_visible) {
            roadmap_math_coordinate (&last_midposition, &point0);
            roadmap_screen_add_segment_point (&point0, pens, num_pens, SEGMENT_START);
            last_point_visible = 1;
         }

         roadmap_math_coordinate (&midposition, &point1);
         roadmap_screen_add_segment_point (&point1, pens, num_pens, i<=last_shape ? 0 : SEGMENT_END);

         if (total_length_ptr) {

            int length_sq = roadmap_math_screen_distance
               (&point1, &point0, MATH_DIST_SQUARED);

            /* bad math, but it's for a labelling heuristic anyway */
            *total_length_ptr += length_sq;

            if (length_sq > longest) {
               longest = length_sq;
               label_p1 = last_midposition;
               label_p2 = midposition;
               middle->x = (point1.x + point0.x) / 2;
               middle->y = (point1.y + point0.y) / 2;
            }
         }

         last_midposition = midposition;
         point0 = point1;
      }

      if (angle && longest) {
         *angle = roadmap_math_azymuth(&last_midposition, &midposition);
      }

   } else {
      /* Draw a line with no shape. */

      /* Optimization: do not draw a line that is obviously not visible. */
      if (! fully_visible) {
         if (! roadmap_math_line_is_visible (from, to)) {
            dbg_time_end(DBG_TIME_DRAW_ONE_LINE);
            return;
         }
      }

      roadmap_math_coordinate (from, &point0);
      roadmap_math_coordinate (to, &point1);

      if ((point0.x == point1.x) && (point0.y == point1.y)) {
         /* draw a point instead of a line */

         roadmap_screen_add_segment_point (&point0, pens, num_pens,
                                           SEGMENT_AS_POINT);

         dbg_time_end(DBG_TIME_DRAW_ONE_LINE);
         return;

      } else {
      
         if (total_length_ptr) {

            *total_length_ptr = roadmap_math_screen_distance
                                   (&point1, &point0, MATH_DIST_SQUARED);

            if (angle) {
               *angle = roadmap_math_azymuth(from, to);
            }
            middle->x = (point1.x + point0.x) / 2;
            middle->y = (point1.y + point0.y) / 2;
         }
      }
      
      if (RoadMapScreenLinePoints.cursor + 2 >= RoadMapScreenLinePoints.end) {
         dbg_time_end(DBG_TIME_DRAW_ONE_LINE);
         roadmap_screen_flush_lines ();
         dbg_time_start(DBG_TIME_DRAW_ONE_LINE);
      }

      roadmap_screen_add_segment_point (&point0, pens, num_pens, SEGMENT_START);
      roadmap_screen_add_segment_point (&point1, pens, num_pens, SEGMENT_END);
   }

   dbg_time_end(DBG_TIME_DRAW_ONE_LINE);
}

#endif


static void roadmap_screen_flush_polygons (void) {

   int count = RoadMapScreenObjects.cursor - RoadMapScreenObjects.data;
    
   if (count == 0) {
       return;
   }
   
   roadmap_math_rotate_coordinates
       (RoadMapScreenLinePoints.cursor - RoadMapScreenLinePoints.data,
        RoadMapScreenLinePoints.data);

   roadmap_canvas_draw_multiple_polygons
      (count, RoadMapScreenObjects.data, RoadMapScreenLinePoints.data, 1,
       RoadMapScreenFastRefresh);

   RoadMapScreenObjects.cursor = RoadMapScreenObjects.data;
   RoadMapScreenLinePoints.cursor  = RoadMapScreenLinePoints.data;
}


static void roadmap_screen_draw_polygons (void) {

   static RoadMapGuiPoint null_point = {0, 0};

   int i;
   int j;
   int size;
   int category;
   int *geo_point;
   RoadMapPosition position;
   RoadMapGuiPoint *graphic_point;
   RoadMapGuiPoint *previous_point;

   RoadMapGuiPoint upper_left;
   RoadMapGuiPoint lower_right;

   RoadMapArea edges;
   RoadMapPen pen = NULL;

   RoadMapScreenLastPen = NULL;

   if (! roadmap_is_visible (ROADMAP_SHOW_AREA)) return;


   for (i = roadmap_polygon_count() - 1; i >= 0; --i) {

      category = roadmap_polygon_category (i);
      pen = roadmap_layer_get_pen (category, 0, 0);
      
      if (pen == NULL) continue;

      if (RoadMapScreenLastPen != pen) {
         roadmap_screen_flush_polygons ();
         roadmap_canvas_select_pen (pen);
         RoadMapScreenLastPen = pen;
      }

      roadmap_polygon_edges (i, &edges);

      if (! roadmap_math_is_visible (&edges)) continue;

      /* Declutter logic: do not show the polygon when it has been
       * reduced (by the zoom) to a quasi-point.
       */
      position.longitude = edges.west;
      position.latitude  = edges.north;
      roadmap_math_coordinate (&position, &upper_left);

      position.longitude = edges.east;
      position.latitude  = edges.south;
      roadmap_math_coordinate (&position, &lower_right);

      if (abs(upper_left.x - lower_right.x) < 5 &&
          abs(upper_left.y - lower_right.y) < 5) {
         continue;
      }

      size = roadmap_polygon_points
                (i,
                 RoadMapPolygonGeoPoints,
                 RoadMapScreenLinePoints.end
                    - RoadMapScreenLinePoints.cursor - 1);

      if (size <= 0) {

         roadmap_screen_flush_polygons ();

         size = roadmap_polygon_points
                   (i,
                    RoadMapPolygonGeoPoints,
                    RoadMapScreenLinePoints.end
                       - RoadMapScreenLinePoints.cursor - 1);
      }
      geo_point = RoadMapPolygonGeoPoints;
      graphic_point = RoadMapScreenLinePoints.cursor;
      previous_point = &null_point;

      for (j = size; j > 0; --j) {

         roadmap_point_position  (*geo_point, &position);
         roadmap_math_coordinate (&position, graphic_point);

         if ((graphic_point->x != previous_point->x) ||
             (graphic_point->y != previous_point->y)) {

            previous_point = graphic_point;
            graphic_point += 1;
         }
         geo_point += 1;
      }

      /* Do not show polygons that have been reduced to a single
       * graphical point because of the zoom factor (natural declutter).
       */
      if (graphic_point != RoadMapScreenLinePoints.cursor) {

         *(graphic_point++) = *RoadMapScreenLinePoints.cursor;

         *(RoadMapScreenObjects.cursor++) =
             graphic_point - RoadMapScreenLinePoints.cursor;

         RoadMapScreenLinePoints.cursor = graphic_point;
      }
   }

   roadmap_screen_flush_polygons ();
}


#ifndef J2ME
static void roadmap_screen_draw_square_edges (int square) {

   int count;
   RoadMapArea edges;
   RoadMapPosition topleft;
   RoadMapPosition bottomright;
   RoadMapPosition position;

   RoadMapGuiPoint points[6];


   if (! roadmap_is_visible (ROADMAP_SHOW_SQUARE)) return;

   roadmap_square_edges (square, &edges);
   
   topleft.longitude     = edges.west;
   topleft.latitude      = edges.north;
   bottomright.longitude = edges.east;
   bottomright.latitude  = edges.south;

   roadmap_math_coordinate (&topleft, points);
   points[4] = points[0];

   position.longitude = bottomright.longitude;
   position.latitude  = topleft.latitude;
   roadmap_math_coordinate (&position, points+1);
   points[5] = points[1];

   roadmap_math_coordinate (&bottomright, points+2);

   position.longitude = topleft.longitude;
   position.latitude  = bottomright.latitude;
   roadmap_math_coordinate (&position, points+3);

   roadmap_canvas_select_pen (RoadMapPenEdges);
   count = 6;
   roadmap_math_rotate_coordinates (count, points);
   roadmap_canvas_draw_multiple_lines (1, &count, points,
                                       RoadMapScreenFastRefresh);
   RoadMapScreenLastPen = NULL;
}

#endif
#if 0
struct square_cache_line {
   int cfcc;
   RoadMapPosition from;
   RoadMapPosition to;
   int first_shape;
   int last_shape;
   int contained;
};

struct square_cache {
   int count;
   struct square_cache_line lines[1];
};

static struct square_cache *SquareCache[9000];

static struct square_cache *roadmap_screen_cache_square
              (int square, int *layers, int layer_count) {

   int line;
   int first_line;
   int last_line;
   int first_shape;
   int last_shape;
   int fips = roadmap_locator_active ();

   int i;

   struct square_cache *cache = NULL;
   RoadMapArea square_edges;

   roadmap_square_edges (square, &square_edges);

   for (i = 0; i < layer_count; ++i) {

      int cfcc = layers[i];

      if ((cfcc < ROADMAP_ROAD_FIRST) || (cfcc > ROADMAP_ROAD_LAST)) {
         continue;
      }

      if (roadmap_line_in_square (square, cfcc, &first_line, &last_line) > 0) {
         int has_shapes;

         if (!cache) {
            cache = malloc(sizeof(*cache) + sizeof(struct square_cache_line) * 2000);
            cache->count = 0;
         }

         if (roadmap_square_has_shapes (square)) {
            has_shapes = 1;
         } else {
            has_shapes = 0;
            first_shape = last_shape = -1;
         }

         for (line = first_line; line <= last_line; ++line) {

            /* A plugin may override a line: it can change the pen or
             * decide not to draw the line.
             */

            struct square_cache_line *cache_entry = cache->lines + cache->count;

            if (!roadmap_plugin_override_line (line, cfcc, fips)) {


               if (has_shapes) {

                  roadmap_line_shapes (line, square, &first_shape, &last_shape);
               }

               cache_entry->contained = 1;

               roadmap_line_from (line, &cache_entry->from);
               roadmap_line_to (line, &cache_entry->to);

               if (first_shape != -1) {
                  int j;
                  RoadMapPosition shape_pos;
                  shape_pos = cache_entry->from;
                  for (j = first_shape; j <= last_shape; ++j) {

                     roadmap_shape_get_position (j, &shape_pos);

                     if ((shape_pos.longitude > square_edges.east) ||
                           (shape_pos.longitude < square_edges.west) ||
                           (shape_pos.latitude  > square_edges.north) ||
                           (shape_pos.latitude  < square_edges.south)) {

                        cache_entry->contained = 0;
                        break;
                     }
                  }
               }

               cache_entry->first_shape = first_shape;
               cache_entry->last_shape = last_shape;
               cache_entry->cfcc = cfcc;
               cache->count++;
            }
         }
      }
   }

   roadmap_log_pop ();
   if (!cache) cache = -1;
   return cache;
}
#endif

//#define DEBUG_TIME
#ifdef J2ME
#include <java/lang.h>
#define roadmap_plugin_override_line(x,y,z) 0
#define roadmap_plugin_override_pen(x, y, z, a, b) 0
#endif

static int roadmap_screen_draw_square
              (int square, int cfcc, int fully_visible, int pen_type) {

   int line;
   int first_line;
   int last_line;
   int first_shape;
   int last_shape;
   RoadMapPen layer_pens[LAYER_PROJ_AREAS];
   int fips = roadmap_locator_active ();
   int total_length;
   int *total_length_ptr = 0;
   int angle = 90;
   int *angle_ptr = 0;
   RoadMapGuiPoint seg_middle;
   RoadMapGuiPoint loweredge;
   int cutoff_dist = 0;

   int drawn = 0;
   int i;

   int start_time;
   int end_time;

   roadmap_log_push ("roadmap_screen_draw_square");

#ifdef DEBUG_TIME
    start_time = NOPH_System_currentTimeMillis();
#endif    

   for (i = 0; i < LAYER_PROJ_AREAS; i++) {
      layer_pens[i] = roadmap_layer_get_pen (cfcc, pen_type, i);
   }
   
#ifdef DEBUG_TIME
    end_time = NOPH_System_currentTimeMillis();
    printf ("roadmap_screen_square after projs %d ms\n", end_time - start_time);
    start_time = end_time;
#endif    

   if (layer_pens[0] == NULL) {
      roadmap_log_pop ();
      return 0;
   }
   
   if ((pen_type == 0) &&       /* we do labels only for the first pen */
         !RoadMapScreenFastRefresh &&
         RoadMapScreenLabels) {
      total_length_ptr = &total_length;
      if (RoadMapScreen3dHorizon != 0) {
         /* arrange to not do labels further than 3/4 up the screen */
         RoadMapGuiPoint label_cutoff;
         label_cutoff.y = roadmap_canvas_height() / 4;
         label_cutoff.x = roadmap_canvas_width() / 2;
         loweredge.x = roadmap_canvas_width() / 2;
         loweredge.y = roadmap_canvas_height();
         roadmap_math_unproject(&label_cutoff);
         roadmap_math_unproject(&loweredge);
         cutoff_dist = roadmap_math_screen_distance
                (&label_cutoff, &loweredge, MATH_DIST_SQUARED);
      } else {
#ifndef J2ME
         angle_ptr = &angle;
#endif
      }
   }

   /* Draw each line that belongs to this square. */

#if 0
   if (SquareCache[square]) {

      struct square_cache *cache = SquareCache[square];
      int i;

      for (i = 0; i < cache->count; ++i) {

         struct square_cache_line *entry = cache->lines + i;

         RoadMapPen *pens = layer_pens;

         if (entry->cfcc != cfcc) continue;

         roadmap_screen_draw_one_line
            (&entry->from, &entry->to, entry->contained,
             &entry->from, entry->first_shape, entry->last_shape,
             roadmap_shape_get_position, pens, total_length_ptr,
             &seg_middle, angle_ptr);

         if (total_length_ptr && total_length && (cutoff_dist == 0 ||
                  cutoff_dist > roadmap_math_screen_distance
                  (&seg_middle, &loweredge, MATH_DIST_SQUARED)) ) {
            PluginLine l = {ROADMAP_PLUGIN_ID, line, cfcc, fips};
            //roadmap_label_add (&seg_middle, angle, total_length, &l);
         }

         drawn += 1;
      }
   }

#endif

   /* Draw each line that belongs to this square. */

#ifdef DEBUG_TIME
    end_time = NOPH_System_currentTimeMillis();
    printf ("roadmap_screen_square b4 search lines projs %d ms\n", end_time - start_time);
    start_time = end_time;
#endif
   if (roadmap_line_in_square (square, cfcc, &first_line, &last_line) > 0) {
      int has_shapes;

      if (roadmap_square_has_shapes (square)) {
         has_shapes = 1;
      } else {
         has_shapes = 0;
         first_shape = last_shape = -1;
      }

#ifdef DEBUG_TIME
    end_time = NOPH_System_currentTimeMillis();
    printf ("roadmap_screen_square b4 itr of:%d %d ms\n", (last_line - first_line), end_time - start_time);
    start_time = end_time;
#endif
      for (line = first_line; line <= last_line; ++line) {

         /* A plugin may override a line: it can change the pen or
          * decide not to draw the line.
          */

         if (!roadmap_plugin_override_line (line, cfcc, fips)) {
            RoadMapPosition from;
            RoadMapPosition to;
            RoadMapPen override_pen;


            if (has_shapes) {

               roadmap_line_shapes (line, square,
                                   &first_shape, &last_shape);
            }

            roadmap_line_from (line, &from);
            roadmap_line_to (line, &to);

            /* Check if the plugin wants to override the pen. */
            if (roadmap_plugin_override_pen
                     (line, cfcc, fips, pen_type, &override_pen)) {

               if (override_pen == NULL) continue;
               roadmap_screen_draw_one_line
                  (&from, &to, fully_visible, &from, first_shape, last_shape,
                   roadmap_shape_get_position, &override_pen, 1,
                   total_length_ptr, &seg_middle, angle_ptr);
            } else {

               roadmap_screen_draw_one_line
                  (&from, &to, fully_visible, &from, first_shape, last_shape,
                   roadmap_shape_get_position, layer_pens, LAYER_PROJ_AREAS,
                   total_length_ptr, &seg_middle, angle_ptr);
            }

            if (total_length_ptr && total_length && (cutoff_dist == 0 ||
                    cutoff_dist > roadmap_math_screen_distance
                            (&seg_middle, &loweredge, MATH_DIST_SQUARED)) ) {
               PluginLine l = {ROADMAP_PLUGIN_ID, line, cfcc, fips};
               roadmap_label_add (&seg_middle, angle, total_length, &l);
            }

            drawn += 1;
         }
      }
#ifdef DEBUG_TIME
    end_time = NOPH_System_currentTimeMillis();
    printf ("roadmap_screen_square after itr %d ms\n", end_time - start_time);
    start_time = end_time;
#endif
   }

#ifdef DEBUG_TIME
    end_time = NOPH_System_currentTimeMillis();
    printf ("roadmap_screen_square before sq2 %d ms\n", end_time - start_time);
    start_time = end_time;
#endif
   /* Draw each line that intersects with this square (but belongs
    * to another square--the crossing lines).
    */
   if (roadmap_line_in_square2 (square, cfcc, &first_line, &last_line) > 0) {

      int last_real_square = -1;
      int real_square  = 0;
      int real_square_drawn  = 0;
      int has_shapes = 0;
      RoadMapArea edges = {0, 0, 0, 0};

      int real_line;
      int square_count = roadmap_square_count();

      for (line = first_line; line <= last_line; ++line) {

         RoadMapPosition position;


         real_line = roadmap_line_get_from_index2 (line);

         roadmap_line_from (real_line, &position);

         /* Optimization: search for the square only if the new line does not
          * belong to the same square as the line before.
          */
         if ((position.longitude <  edges.west) ||
             (position.longitude >= edges.east) ||
             (position.latitude <  edges.south) ||
             (position.latitude >= edges.north)) {
            real_square = roadmap_square_search (&position);
         }

         if (real_square != last_real_square) {

            if (real_square < 0 || real_square >= square_count) {
               roadmap_log (ROADMAP_ERROR,
                            "Invalid square index %d", real_square);
               continue;
            }

            roadmap_square_edges (real_square, &edges);

            last_real_square = real_square;

            if (roadmap_math_is_visible (&edges)) {

               real_square_drawn = 1;
               continue;
            } else {
               real_square_drawn = 0;
            }

            if (roadmap_square_has_shapes (real_square)) {
               has_shapes = 1;
            } else {
               has_shapes = 0;
          first_shape = last_shape = -1;
            }
         }

         if (real_square_drawn) {
            /* Either it has already been drawn, or it will be soon. */
            continue;
         }

         if (!roadmap_plugin_override_line (real_line, cfcc, fips)) {

            RoadMapPosition from;
            RoadMapPosition to;
            RoadMapPen override_pen;

            if (has_shapes) {

               roadmap_line_shapes (real_line, real_square,
                                   &first_shape, &last_shape);
            }

            roadmap_line_from (real_line, &from);
            roadmap_line_to (real_line, &to);

            /* Check if a plugin wants to override the pen */
            if (roadmap_plugin_override_pen
                  (real_line, cfcc, fips, pen_type, &override_pen)) {

               if (override_pen == NULL) continue;

               roadmap_screen_draw_one_line
                  (&from, &to, fully_visible, &from, first_shape, last_shape,
                   roadmap_shape_get_position, &override_pen, 1,
                   total_length_ptr, &seg_middle, angle_ptr);
            } else {
               roadmap_screen_draw_one_line
                  (&from, &to, fully_visible, &from, first_shape, last_shape,
                   roadmap_shape_get_position, layer_pens, LAYER_PROJ_AREAS,
                   total_length_ptr, &seg_middle, angle_ptr);
            }


            if (total_length_ptr && total_length && (cutoff_dist == 0 ||
                    cutoff_dist > roadmap_math_screen_distance
                            (&seg_middle, &loweredge, MATH_DIST_SQUARED)) ) {
               PluginLine l = {ROADMAP_PLUGIN_ID, real_line, cfcc, fips};
               roadmap_label_add (&seg_middle, angle, total_length, &l);
            }

            drawn += 1;
         }
      }
   }

#ifdef DEBUG_TIME
    end_time = NOPH_System_currentTimeMillis();
    printf ("roadmap_screen_square end %d ms\n", end_time - start_time);
    start_time = end_time;
#endif
   roadmap_log_pop ();
   return drawn;
}


static int roadmap_screen_draw_long_lines (int pen_type) {

   int last_cfcc = -1;
   int index = 0;
   int real_line;
   int real_square = -2;
   int cfcc;
   int first_shape;
   int last_shape;
   RoadMapPen layer_pens[LAYER_PROJ_AREAS] = {NULL};
   int fips = roadmap_locator_active ();
   int total_length;
   int *total_length_ptr;
   int angle = 90;
   int *angle_ptr = 0;
   RoadMapGuiPoint seg_middle;
   RoadMapArea area;
   int last_real_square = -1;
   RoadMapArea edges = {0, 0, 0, 0};
   int has_shapes = 0;

   int drawn = 0;

   dbg_time_start(DBG_TIME_DRAW_LONG_LINES);

   roadmap_log_push ("roadmap_screen_draw_long_lines");

   if (RoadMapScreen3dHorizon || (pen_type != 0) || RoadMapScreenFastRefresh) {
      /* we do labels only for the first pen */
      total_length_ptr = 0;
   } else {
      total_length_ptr = &total_length;
   }

#ifndef J2ME
   angle_ptr = &angle;
#endif

   while (roadmap_line_long (index++, &real_line, &area, &cfcc)) {
         RoadMapPosition position;
         RoadMapPosition to_position;

         if (!roadmap_math_is_visible (&area)) {
            continue;
         }

         if (last_cfcc != cfcc) {
            int i;
            for (i=0; i<LAYER_PROJ_AREAS; i++) {
               layer_pens[i] = roadmap_layer_get_pen (cfcc, pen_type, i);
            }
            last_cfcc = cfcc;
         }

         roadmap_line_to (real_line, &to_position);

         if (roadmap_math_point_is_visible (&to_position)) {
            continue;

         } else {
            int to_square;
            RoadMapArea to_edges;

            to_square = roadmap_square_search (&to_position);
            roadmap_square_edges (to_square, &to_edges);

            if (roadmap_math_is_visible (&to_edges)) {
               continue;
            }
         }

         roadmap_line_from (real_line, &position);

         /* Optimization: search for the square only if the new line does not
          * belong to the same square as the line before.
          */
         if ((position.longitude <  edges.west) ||
             (position.longitude >= edges.east) ||
             (position.latitude <  edges.south) ||
             (position.latitude >= edges.north)) {
            real_square = roadmap_square_search (&position);
         }

         if (real_square != last_real_square) {

            roadmap_square_edges (real_square, &edges);

            last_real_square = real_square;

            if (roadmap_math_is_visible (&edges)) {

               continue;
            }

            if (roadmap_square_has_shapes (real_square)) {
               has_shapes = 1;
            } else {
               has_shapes = 0;
               first_shape = last_shape = -1;
            }
         }


         if (roadmap_math_is_visible (&edges)) {
            continue;
         }

         if (!roadmap_plugin_override_line (real_line, cfcc, fips)) {

            RoadMapPosition from;
            RoadMapPosition to;
            RoadMapPen override_pen;

            if (has_shapes) {

               roadmap_line_shapes (real_line, real_square,
                                   &first_shape, &last_shape);
            }

            roadmap_line_from (real_line, &from);
            roadmap_line_to (real_line, &to);

            /* Check if a plugin wants to override the pen */
            if (roadmap_plugin_override_pen
                  (real_line, cfcc, fips, pen_type, &override_pen)) {

               if (override_pen == NULL) continue;

               roadmap_screen_draw_one_line
                  (&from, &to, 0, &from, first_shape, last_shape,
                   roadmap_shape_get_position, &override_pen, 1,
                   total_length_ptr, &seg_middle, angle_ptr);
            } else {

               dbg_time_end(DBG_TIME_DRAW_LONG_LINES);
               roadmap_screen_draw_one_line
                  (&from, &to, 0, &from, first_shape, last_shape,
                   roadmap_shape_get_position, layer_pens, LAYER_PROJ_AREAS,
                   total_length_ptr, &seg_middle, angle_ptr);
               dbg_time_start(DBG_TIME_DRAW_LONG_LINES);
            }

            if (total_length_ptr) {

               PluginLine l = {ROADMAP_PLUGIN_ID, real_line, cfcc, fips};
               roadmap_label_add (&seg_middle, angle, total_length, &l);
            }

            drawn += 1;
         }
   }

   roadmap_log_pop ();
   dbg_time_end(DBG_TIME_DRAW_LONG_LINES);
   return drawn;
}


static void roadmap_screen_draw_object
               (const char *name,
                const char *sprite,
                const RoadMapGpsPosition *gps_position) {

   RoadMapPosition position;
   RoadMapGuiPoint screen_point;


   if (sprite == NULL) return; /* Not a visible object. */

   position.latitude = gps_position->latitude;
   position.longitude = gps_position->longitude;

   if (roadmap_math_point_is_visible(&position)) {

      roadmap_math_coordinate (&position, &screen_point);
      roadmap_math_rotate_coordinates (1, &screen_point);
      roadmap_sprite_draw (sprite, &screen_point, gps_position->steering);
   }
}


static void roadmap_screen_alloc_square_mask (void) {

   int count = roadmap_square_count();

   if (count != SquareOnScreenCount) {
      if (SquareOnScreen != NULL) {
         free(SquareOnScreen);
      }
      SquareOnScreenCount = count;
      SquareOnScreen = malloc (SquareOnScreenCount * sizeof(char));
      roadmap_check_allocated(SquareOnScreen);
   }
}


static int roadmap_screen_repaint_square (int square, int pen_type, 
                                          int layer_count, int *layers) {

   int i;

   RoadMapArea edges;

   int drawn = 0;
   int category;
   int fully_visible;


   if (SquareOnScreen[square]) {
      return 0;
   }

   dbg_time_start(DBG_TIME_DRAW_SQUARE);

   SquareOnScreen[square] = 1;

#if 0
   if (!SquareCache[square]) {
      SquareCache[square] = roadmap_screen_cache_square (square, layers, layer_count);
   }

   if (SquareCache[square] == -1) return 0;
#endif

   roadmap_log_push ("roadmap_screen_repaint_square");

   roadmap_square_edges (square, &edges);

   switch (roadmap_math_is_visible (&edges)) {
   case 0:
      roadmap_log_pop ();
      return 0;
   case 1:
      fully_visible = 1;
      break;
   default:
      fully_visible = 0;
   }

#ifndef J2ME
   if (pen_type == 0) roadmap_screen_draw_square_edges (square);
#endif
   
   RoadMapScreenLastPen = NULL;

   for (i = 0; i < layer_count; ++i) {

        category = layers[i];

        drawn += roadmap_screen_draw_square
                    (square, category, fully_visible, pen_type);

   }

   dbg_time_end(DBG_TIME_DRAW_SQUARE);

   roadmap_screen_flush_lines();
   roadmap_screen_flush_points();

   roadmap_log_pop ();
   
   return drawn;
}


static void roadmap_screen_repaint_now (void) {

    static int *fips = NULL;
    static int *in_view = NULL;

    int i;
    int j;
    int k;
    int count;
    int max_pen = roadmap_layer_max_pen();
    static int nomap;
    int use_only_main_pen = 0;
    RoadMapGuiPoint area;

#ifdef DEBUG_TIME
    int start_time;
    int end_time;
#endif
        
    if (!RoadMapScreenInitialized) return;

#ifdef SSD    
    if (RoadMapScreenFrozen) {
       ssd_dialog_draw ();
       return;
    }
#endif

#ifdef DEBUG_TIME
    start_time = NOPH_System_currentTimeMillis();
    printf ("In roadmap_screen_repaint...\n");
#endif
    dbg_time_start(DBG_TIME_FULL);
    dbg_time_start(DBG_TIME_T1);

    if (RoadMapScreenViewMode == VIEW_MODE_3D) {
       RoadMapScreenLowerEdge.x = roadmap_canvas_width() / 2;
       RoadMapScreenLowerEdge.y = roadmap_canvas_height();
       roadmap_math_unproject(&RoadMapScreenLowerEdge);
       roadmap_math_counter_rotate_coordinate (&RoadMapScreenLowerEdge);

       for (i=0; i<LAYER_PROJ_AREAS-1; i++) {
          area.y = roadmap_canvas_height() / LAYER_PROJ_AREAS *
                     (LAYER_PROJ_AREAS - 1 - i);
          area.x = roadmap_canvas_width() / 2;

          roadmap_math_unproject(&area);

          roadmap_math_counter_rotate_coordinate (&area);

          RoadMapScreenAreaDist[i] = roadmap_math_screen_distance
             (&area, &RoadMapScreenLowerEdge, MATH_DIST_SQUARED);
       }
    }

    if (RoadMapScreenFastRefresh &&
        (! roadmap_config_match(&RoadMapConfigStylePrettyDrag, "yes"))) {
       max_pen = 1;
       use_only_main_pen = 1;
    }

    if (in_view == NULL) {
       in_view = calloc (ROADMAP_MAX_VISIBLE, sizeof(int));
       roadmap_check_allocated(in_view);
    }

    roadmap_log_push ("roadmap_screen_repaint");

    /* Repaint the drawing buffer. */
    
    /* - Identifies the candidate counties. */

    count = roadmap_locator_by_position (&RoadMapScreenCenter, &fips);

    /* Activate the first fips before erasing the canvas. This is useful
     * for small devices which it may take some time to load the fips
     * data.
     */

    if (count) roadmap_locator_activate (fips[0]);

    /* Clean the drawing buffer. */

    roadmap_canvas_select_pen (RoadMapBackground);
    roadmap_canvas_erase ();
    RoadMapScreenLastPen = NULL;

    if (count == 0) {
       roadmap_display_text("Info", roadmap_lang_get ("No map available"));
       nomap = 1;
    } else if (nomap) {
       roadmap_display_hide("Info");
       nomap = 0;
    }

    roadmap_label_start();

#ifdef DEBUG_TIME
    end_time = NOPH_System_currentTimeMillis();
    printf ("roadmap_screen_repaint start drawing squares %d ms\n", end_time - start_time);
    start_time = end_time;
#endif    

    /* - For each candidate county: */

    dbg_time_end(DBG_TIME_T1);
    for (i = count-1; i >= 0; --i) {

        dbg_time_start(DBG_TIME_T2);
        /* -- Access the county's database. */

        if (roadmap_locator_activate (fips[i]) != ROADMAP_US_OK) {
           dbg_time_end(DBG_TIME_T2);
           continue;
        }
        roadmap_screen_alloc_square_mask();

        /* -- Look for the square that are currently visible. */

        count = roadmap_square_view (in_view, ROADMAP_MAX_VISIBLE);

        roadmap_screen_draw_polygons ();

        dbg_time_end(DBG_TIME_T2);
        for (k = 0; k < max_pen; ++k) {

           int layer_count;
           int layers[256];
           int pen_type = k;

           dbg_time_start(DBG_TIME_T3);
           layer_count = roadmap_layer_visible_lines (layers, 256, k);
           if (!layer_count) {
              dbg_time_end(DBG_TIME_T3);
              continue;
           }

           /* Reset square mask */
           for (j = 0; j < count; j++) {
              SquareOnScreen[in_view[j]] = 0;
           }

           if (use_only_main_pen) {
              pen_type = -1;
           }

#ifdef DEBUG_TIME
    end_time = NOPH_System_currentTimeMillis();
    printf ("roadmap_screen_repaint before squares %d ms\n", end_time - start_time);
    start_time = end_time;
#endif    
           dbg_time_end(DBG_TIME_T3);
           for (j = count - 1; j >= 0; --j) {
              roadmap_screen_repaint_square (in_view[j], pen_type, layer_count, layers);
           }

#ifdef DEBUG_TIME
    end_time = NOPH_System_currentTimeMillis();
    printf ("roadmap_screen_repaint after squares %d ms\n", end_time - start_time);
    start_time = end_time;
#endif    
           roadmap_screen_draw_long_lines (k);
        }

        dbg_time_end(DBG_TIME_FULL);
        roadmap_screen_flush_lines ();
        roadmap_screen_flush_points ();
        dbg_time_start(DBG_TIME_FULL);

#ifdef DEBUG_TIME
    end_time = NOPH_System_currentTimeMillis();
    printf ("roadmap_screen_repaint end drawing squares %d ms\n", end_time - start_time);
    start_time = end_time;
#endif    

        roadmap_plugin_screen_repaint (max_pen);
        roadmap_screen_flush_lines ();
        roadmap_screen_flush_points ();

        dbg_time_start(DBG_TIME_T4);
        if (!RoadMapScreenFastRefresh) {
            roadmap_label_draw_cache (RoadMapScreen3dHorizon == 0);
#ifdef DEBUG_TIME
    end_time = NOPH_System_currentTimeMillis();
    printf ("roadmap_screen_repaint end drawing labels %d ms\n", end_time - start_time);
    start_time = end_time;
#endif    
        }

        dbg_time_end(DBG_TIME_FULL);
        dbg_time_end(DBG_TIME_T4);
        roadmap_screen_flush_lines ();
        roadmap_screen_flush_points ();
        dbg_time_start(DBG_TIME_FULL);
        dbg_time_start(DBG_TIME_T4);
    }

#ifdef DEBUG_TIME
    end_time = NOPH_System_currentTimeMillis();
    printf ("roadmap_screen_repaint end drawing map %d ms\n", end_time - start_time);
    start_time = end_time;
#endif    
    if (!RoadMapScreenFastRefresh ||
        roadmap_config_match(&RoadMapConfigStyleObjects, "yes")) {

       roadmap_object_iterate (roadmap_screen_draw_object);

       roadmap_message_update ();
    
       roadmap_screen_obj_draw ();
    }

    if (roadmap_config_match (&RoadMapConfigMapSigns, "yes")) {

       roadmap_trip_display ();
    }

#ifdef DEBUG_TIME
    end_time = NOPH_System_currentTimeMillis();
    printf ("roadmap_screen_repaint b4 after_refresh callback %d ms\n", end_time - start_time);
    start_time = end_time;
#endif    
    RoadMapScreenAfterRefresh();

    roadmap_display_signs ();

#ifdef SSD
    ssd_dialog_draw ();
#endif

#ifdef DEBUG_TIME
    end_time = NOPH_System_currentTimeMillis();
    printf ("roadmap_screen_repaint b4 canvas refresh %d ms\n", end_time - start_time);
    start_time = end_time;
#endif    
    dbg_time_end(DBG_TIME_T4);
    roadmap_canvas_refresh ();

    roadmap_log_pop ();
    dbg_time_end(DBG_TIME_FULL);
    dbg_time_print();
#ifdef DEBUG_TIME
    printf ("Finished roadmap_screen_repaint in %d ms\n", (int)NOPH_System_currentTimeMillis() - start_time);
#endif    
}


/* Instead of repainting the screen with every event,
 * we use this timer as a flow control. It may take time for the
 * application to finish the task of drawing the screen and we don't
 * want to lag.
 */
static void roadmap_screen_refresh_flow_control(void) {

   roadmap_main_remove_periodic(roadmap_screen_refresh_flow_control);

   if (RoadMapScreenRefreshFlowControl > 1) {
      roadmap_screen_repaint_now ();

   } else if (!RoadMapScreenFastRefresh) {
      /* If we had no requests to redraw the screen we don't need
       * to reset the timer.
       */

      RoadMapScreenRefreshFlowControl = 0;
      return;
   }

   if (RoadMapScreenFastRefresh) {
      RoadMapScreenRefreshFlowControl = 2;
      RoadMapScreenFastRefresh &= ~SCREEN_FAST_OTHER;
   } else {
      RoadMapScreenRefreshFlowControl = 1;
   }
   roadmap_main_set_periodic
      (REFRESH_FLOW_CONTROL_TIMEOUT, roadmap_screen_refresh_flow_control);
}


static void roadmap_screen_repaint (void) {

   if (!RoadMapScreenRefreshFlowControl) {
      roadmap_screen_repaint_now ();
      RoadMapScreenRefreshFlowControl = 1;
      roadmap_main_set_periodic
         (REFRESH_FLOW_CONTROL_TIMEOUT, roadmap_screen_refresh_flow_control);
   } else {
      RoadMapScreenRefreshFlowControl++;
   }
}


static void roadmap_screen_repaint_fast (void) {

   RoadMapScreenFastRefresh |= SCREEN_FAST_OTHER;
   roadmap_screen_repaint ();
}


static void roadmap_screen_configure (void) {

   RoadMapScreenWidth = roadmap_canvas_width();
   RoadMapScreenHeight = roadmap_canvas_height();
   roadmap_log (ROADMAP_DEBUG, "In roadmap_screen_configure. width:%d, height:%d\n", RoadMapScreenWidth, RoadMapScreenHeight);

   RoadMapScreenLabels = ! roadmap_config_match(&RoadMapConfigMapLabels, "off");

   roadmap_math_set_size (RoadMapScreenWidth, RoadMapScreenHeight);
   roadmap_log (ROADMAP_DEBUG, "B4 RoadMapScreenInitialized:%d\n", RoadMapScreenInitialized);

   if (RoadMapScreenInitialized) {
      roadmap_screen_repaint_now ();
   }
}



static int roadmap_screen_short_click (RoadMapGuiPoint *point) {
    
   PluginLine line;
   int distance;
   RoadMapPosition position;

   roadmap_math_to_position (point, &position, 1);
   
    if (roadmap_navigate_retrieve_line
             (&position,
              roadmap_config_get_integer (&RoadMapConfigAccuracyMouse),
              &line,
              &distance,
              LAYER_ALL_ROADS) != -1) {

       PluginStreet street;

       roadmap_trip_set_point ("Selection", &position);
       roadmap_display_activate ("Selected Street", &line, &position, &street);
       roadmap_screen_repaint ();
   }

   return 1;
}


static void roadmap_screen_reset_delta (void) {

   RoadMapScreenDeltaX = 0;
   RoadMapScreenDeltaY = 0;
   RoadMapScreenRotation = 0;
}


static void roadmap_screen_record_move (int dx, int dy) {

   RoadMapGuiPoint center;

   RoadMapScreenDeltaX += dx;
   RoadMapScreenDeltaY += dy;

   center.x = (RoadMapScreenWidth / 2) + dx;
   center.y = (RoadMapScreenHeight / 2) + dy;

   roadmap_math_to_position (&center, &RoadMapScreenCenter, 0);
   roadmap_math_set_center (&RoadMapScreenCenter);
}


static int roadmap_screen_drag_start (RoadMapGuiPoint *point) {

   RoadMapScreenFastRefresh |= SCREEN_FAST_DRAG;
   RoadMapScreenPointerLocation = *point;
   roadmap_screen_hold (); /* We don't want to move with the GPS position. */
   roadmap_screen_refresh ();

   return 1;
}

static int roadmap_screen_drag_end (RoadMapGuiPoint *point) {

   roadmap_screen_record_move
      (RoadMapScreenPointerLocation.x - point->x,
       RoadMapScreenPointerLocation.y - point->y);

   RoadMapScreenFastRefresh &= ~SCREEN_FAST_DRAG;
   RoadMapScreenPointerLocation = *point;
   roadmap_screen_repaint ();

   return 1;
}

static int roadmap_screen_drag_motion (RoadMapGuiPoint *point) {

   if (RoadMapScreenViewMode == VIEW_MODE_3D) {

      RoadMapGuiPoint p = *point;
      RoadMapGuiPoint p2 = RoadMapScreenPointerLocation;

      roadmap_math_unproject (&p);
      roadmap_math_unproject (&p2);

      roadmap_screen_record_move (p2.x - p.x, p2.y - p.y);
      
   } else {

      roadmap_screen_record_move
         (RoadMapScreenPointerLocation.x - point->x,
          RoadMapScreenPointerLocation.y - point->y);
   }

   roadmap_screen_repaint ();
   RoadMapScreenPointerLocation = *point;

   return 1;
}

static int roadmap_screen_get_view_mode (void) {
   
   return RoadMapScreenViewMode;
}

static int roadmap_screen_get_orientation_mode (void) {
   
   return RoadMapScreenOrientationMode;
}

static void roadmap_screen_reset_pens (void) {

    RoadMapBackground = roadmap_canvas_create_pen ("Map.Background");
    roadmap_canvas_set_foreground
        (roadmap_config_get (&RoadMapConfigMapBackground));
}


static void roadmap_screen_update_center (const RoadMapPosition *pos) {
    RoadMapPosition view_center;

    RoadMapScreenCenter = *pos;
    roadmap_math_set_center (&RoadMapScreenCenter);

    if ((RoadMapScreenViewMode != VIEW_MODE_3D) &&
        !RoadMapScreenCenterDelta) return;

    RoadMapScreenCenterPixel.x = (RoadMapScreenWidth / 2);

    if (RoadMapScreenViewMode != VIEW_MODE_3D) {
       RoadMapScreenCenterPixel.y = (RoadMapScreenHeight / 2);
    } else {
       RoadMapScreenCenterPixel.y = (RoadMapScreenHeight / 3);
    }

    RoadMapScreenCenterPixel.y += RoadMapScreenCenterDelta;

    roadmap_math_to_position (&RoadMapScreenCenterPixel, &view_center, 0);
    roadmap_math_set_center (&view_center);
}


void roadmap_screen_move_center (int dy) {
   RoadMapScreenCenterDelta += dy;
}


int roadmap_screen_height (void) {
   int height = RoadMapScreenHeight + RoadMapScreenCenterDelta;
   if (RoadMapScreenViewMode == VIEW_MODE_3D) {
      height = height * 3;
   }
   return height;
}


void roadmap_screen_refresh (void) {

    int refresh = 0;

    roadmap_log_push ("roadmap_screen_refresh");

    if (roadmap_trip_is_focus_changed()) {

        roadmap_screen_reset_delta ();
        roadmap_screen_update_center (roadmap_trip_get_focus_position ());
        refresh = 1;
        
        if (RoadMapScreenOrientationMode != ORIENTATION_FIXED) {
           roadmap_math_set_orientation (roadmap_trip_get_orientation());
        }

    } else if (roadmap_trip_is_focus_moved()) {

        roadmap_screen_update_center (roadmap_trip_get_focus_position ());

        if (RoadMapScreenOrientationMode != ORIENTATION_FIXED) {
           refresh |=
              roadmap_math_set_orientation
                 (roadmap_trip_get_orientation() + RoadMapScreenRotation);
        } else {
           refresh |=
              roadmap_math_set_orientation (RoadMapScreenRotation);
        }

        if (RoadMapScreenDeltaX || RoadMapScreenDeltaY) {

           /* Force recomputation. */

           int dx = RoadMapScreenDeltaX;
           int dy = RoadMapScreenDeltaY;

           RoadMapScreenDeltaX = RoadMapScreenDeltaY = 0;
           roadmap_screen_record_move (dx, dy);
        }
    }

    if (roadmap_config_match (&RoadMapConfigMapRefresh, "forced")) {

        roadmap_screen_repaint ();

    } else if (refresh || roadmap_trip_is_refresh_needed()) {

       roadmap_screen_repaint ();
    }

    roadmap_log_pop ();
}


void roadmap_screen_redraw (void) {

    roadmap_screen_repaint ();
}


void roadmap_screen_freeze (void) {

   RoadMapScreenFrozen = 1;
}

void roadmap_screen_unfreeze (void) {

   if (!RoadMapScreenFrozen) return;

   RoadMapScreenFrozen = 0;
   roadmap_screen_repaint ();
}


void roadmap_screen_hold (void) {

   roadmap_trip_copy_focus ("Hold");
   roadmap_trip_set_focus ("Hold");
}


void roadmap_screen_rotate (int delta) {

   int rotation = RoadMapScreenRotation + delta;
   int calculated_rotation;

   if (RoadMapScreenRefreshFlowControl > 2) return;

   while (rotation >= 360) {
      rotation -= 360;
   }
   while (rotation < 0) {
      rotation += 360;
   }

   if (RoadMapScreenOrientationMode == ORIENTATION_DYNAMIC) {
      calculated_rotation = roadmap_trip_get_orientation() + rotation;
   } else {
      calculated_rotation = rotation;
   }

   if (roadmap_math_set_orientation (calculated_rotation)) {
      RoadMapScreenRotation = rotation;
      roadmap_screen_update_center (&RoadMapScreenCenter);
      roadmap_screen_repaint_fast ();
   }
}


void roadmap_screen_toggle_view_mode (void) {
   
   if (RoadMapScreenViewMode == VIEW_MODE_2D) {
      RoadMapScreenViewMode = VIEW_MODE_3D;
      RoadMapScreen3dHorizon = -100;
   } else {
      RoadMapScreenViewMode = VIEW_MODE_2D;
      RoadMapScreen3dHorizon = 0;
   }

   roadmap_math_set_horizon (RoadMapScreen3dHorizon);
   roadmap_screen_update_center (&RoadMapScreenCenter);
   roadmap_screen_repaint ();
}

void roadmap_screen_toggle_labels (void) {
   
   RoadMapScreenLabels = ! RoadMapScreenLabels;
   roadmap_screen_repaint();
}

void roadmap_screen_toggle_orientation_mode (void) {
   
   if (RoadMapScreenOrientationMode == ORIENTATION_DYNAMIC) {

      RoadMapScreenOrientationMode = ORIENTATION_FIXED;
      
   } else {
      RoadMapScreenOrientationMode = ORIENTATION_DYNAMIC;
   }

   RoadMapScreenRotation = 0;
   roadmap_state_refresh ();
   roadmap_screen_rotate (0);
}


void roadmap_screen_increase_horizon (void) {

   if (RoadMapScreenViewMode != VIEW_MODE_3D) return;

   if (RoadMapScreen3dHorizon >= -100) return;

   RoadMapScreen3dHorizon += 100;
   roadmap_math_set_horizon (RoadMapScreen3dHorizon);
   roadmap_screen_repaint ();
}


void roadmap_screen_decrease_horizon (void) {

   if (RoadMapScreenViewMode != VIEW_MODE_3D) return;

   RoadMapScreen3dHorizon -= 100;
   roadmap_math_set_horizon (RoadMapScreen3dHorizon);
   roadmap_screen_repaint ();
}

#define FRACMOVE 4

void roadmap_screen_move_up (void) {

   if (RoadMapScreenRefreshFlowControl < 3) {
      roadmap_screen_record_move (0, 0 - (RoadMapScreenHeight / FRACMOVE));
      roadmap_screen_repaint_fast ();
   }
}


void roadmap_screen_move_down (void) {

   if (RoadMapScreenRefreshFlowControl < 3) {
      roadmap_screen_record_move (0, RoadMapScreenHeight / FRACMOVE);
      roadmap_screen_repaint_fast ();
  }
}


void roadmap_screen_move_right (void) {

   if (RoadMapScreenRefreshFlowControl < 3) {
      roadmap_screen_record_move (RoadMapScreenHeight / FRACMOVE, 0);
      roadmap_screen_repaint_fast ();
   }
}


void roadmap_screen_move_left (void) {

   if (RoadMapScreenRefreshFlowControl < 3) {
      roadmap_screen_record_move (0 - (RoadMapScreenHeight / FRACMOVE), 0);
      roadmap_screen_repaint_fast ();
   }
}


void roadmap_screen_zoom_in  (void) {

    if (RoadMapScreenRefreshFlowControl > 2) return;
    roadmap_math_zoom_in ();

    roadmap_layer_adjust ();
    roadmap_screen_update_center (&RoadMapScreenCenter);
    roadmap_screen_repaint_fast ();
}


void roadmap_screen_zoom_out (void) {

    if (RoadMapScreenRefreshFlowControl > 2) return;
    roadmap_math_zoom_out ();

    roadmap_layer_adjust ();
    roadmap_screen_update_center (&RoadMapScreenCenter);
    roadmap_screen_repaint_fast ();
}


void roadmap_screen_zoom_reset (void) {

   roadmap_math_zoom_reset ();

   roadmap_layer_adjust ();
   roadmap_screen_update_center (&RoadMapScreenCenter);
   roadmap_screen_repaint ();
}


void roadmap_screen_initialize (void) {

   roadmap_config_declare
       ("preferences", &RoadMapConfigAccuracyMouse,  "20", NULL);

   roadmap_config_declare
       ("schema", &RoadMapConfigMapBackground, "#ffffe0", NULL);

   roadmap_config_declare_enumeration
       ("preferences", &RoadMapConfigMapSigns, NULL, "yes", "no", NULL);

   roadmap_config_declare_enumeration
       ("preferences", &RoadMapConfigMapRefresh, NULL, "normal", "forced", NULL);

   roadmap_config_declare_enumeration
       ("preferences", &RoadMapConfigStylePrettyDrag, NULL, "yes", "no", NULL);

   roadmap_config_declare_enumeration
       ("preferences", &RoadMapConfigStyleObjects, NULL, "yes", "no", NULL);

   roadmap_config_declare_enumeration
        ("preferences", &RoadMapConfigMapLabels, NULL, "on", "off", NULL);

   roadmap_pointer_register_short_click
      (&roadmap_screen_short_click, POINTER_DEFAULT);
   roadmap_pointer_register_drag_start
      (&roadmap_screen_drag_start, POINTER_DEFAULT);
   roadmap_pointer_register_drag_end
      (&roadmap_screen_drag_end, POINTER_DEFAULT);
   roadmap_pointer_register_drag_motion
      (&roadmap_screen_drag_motion, POINTER_DEFAULT);

   roadmap_canvas_register_configure_handler (&roadmap_screen_configure);

   RoadMapScreenObjects.cursor = RoadMapScreenObjects.data;
   RoadMapScreenObjects.end = RoadMapScreenObjects.data + ROADMAP_SCREEN_BULK;

   RoadMapScreenLinePoints.cursor = RoadMapScreenLinePoints.data;
   RoadMapScreenLinePoints.end = RoadMapScreenLinePoints.data + ROADMAP_SCREEN_BULK;

   RoadMapScreenPoints.cursor = RoadMapScreenPoints.data;
   RoadMapScreenPoints.end = RoadMapScreenPoints.data + ROADMAP_SCREEN_BULK;
   
   RoadMapScreen3dHorizon = 0;

   roadmap_state_add ("view_mode", &roadmap_screen_get_view_mode);
   roadmap_state_add ("orientation_mode",
            &roadmap_screen_get_orientation_mode);
   roadmap_skin_register (roadmap_screen_reset_pens);

   roadmap_state_monitor (&roadmap_screen_repaint);
}


void roadmap_screen_shutdown (void) {
   RoadMapGpsPosition point;
   point.longitude = RoadMapScreenCenter.longitude;
   point.latitude  = RoadMapScreenCenter.latitude;
   point.steering  = RoadMapScreenRotation;

   roadmap_trip_set_mobile ("Hold", &point);
}


void roadmap_screen_set_initial_position (void) {

    RoadMapScreenInitialized = 1;
    
    roadmap_layer_initialize();

    RoadMapBackground = roadmap_canvas_create_pen ("Map.Background");
    roadmap_canvas_set_foreground
        (roadmap_config_get (&RoadMapConfigMapBackground));

    RoadMapPenEdges = roadmap_canvas_create_pen ("Map.Edges");
    roadmap_canvas_set_thickness (4);
    roadmap_canvas_set_foreground ("#bebebe");

    roadmap_layer_adjust ();
}


void roadmap_screen_get_center (RoadMapPosition *center) {

   if (center != NULL) {
      *center = RoadMapScreenCenter;
   }
}


RoadMapScreenSubscriber roadmap_screen_subscribe_after_refresh
                                 (RoadMapScreenSubscriber handler) {
                                    
   RoadMapScreenSubscriber previous = RoadMapScreenAfterRefresh;

   if (handler == NULL) {
      RoadMapScreenAfterRefresh = roadmap_screen_after_refresh;
   } else {
      RoadMapScreenAfterRefresh = handler;
   }

   return previous;
}


#ifndef J2ME
/* TODO: ugly hack (both the hack and the drawing itself are ugly!).
 * This should be rewritten to allow specifying a sprite for
 * the direction mark.
 */
static void
roadmap_screen_draw_direction (RoadMapGuiPoint *point0,
                               RoadMapGuiPoint *point1,
                               int width,
                               int direction) {
   RoadMapGuiPoint from;
   RoadMapGuiPoint to;

   double delta_x = point1->x - point0->x;
   double delta_y = point1->y - point0->y;
   double ll = (1.0 * (abs((int)(delta_x)) + abs((int)(delta_y))) / 15);

   if (ll >= 1) {

      static int i=0;
      
      double step_x = delta_x / ll;
      double step_y = delta_y / ll;
      double x = point0->x + step_x;
      double y = point0->y + step_y;

      while (abs((int)(x + step_x - point0->x)) < abs((int)(delta_x))) {
         i++;

         from.x = (int)x;
         from.y = (int)y;
         to.x = (int) (x + step_x);
         to.y = (int) (y + step_y);

         if (RoadMapScreenLinePoints.cursor + 5 >=
               RoadMapScreenLinePoints.end) {
            roadmap_screen_flush_lines ();
         }

         /* main line */
         RoadMapScreenLinePoints.cursor[0] = from;
         RoadMapScreenLinePoints.cursor[1] = to;
         *RoadMapScreenObjects.cursor = 2;
         RoadMapScreenLinePoints.cursor  += 2;
         RoadMapScreenObjects.cursor += 1;

         /* head */
         if ((direction == 1) ||
               ((direction == 3) && (i % 2))) {
            RoadMapGuiPoint head;
            double dir=atan2(from.x-to.x, from.y-to.y);
            int i1=9;
            head.x = (short)(to.x + i1*sin(dir+0.5));
            head.y = (short)(to.y + i1*cos(dir+0.5));
            RoadMapScreenLinePoints.cursor[0] = head;
            head.x = (short)(to.x + i1*sin(dir-0.5));
            head.y = (short)(to.y + i1*cos(dir-0.5));
            RoadMapScreenLinePoints.cursor[2] = head;
            RoadMapScreenLinePoints.cursor[1] = to;
            *RoadMapScreenObjects.cursor = 3;
            RoadMapScreenLinePoints.cursor  += 3;
            RoadMapScreenObjects.cursor += 1;
         }

         if ((direction == 2) ||
               ((direction == 3) && !(i % 2))) {
            /* second head */
            RoadMapGuiPoint head;
            double dir=atan2(to.x-from.x, to.y-from.y);
            int i1=9;
            head.x = (short)(from.x + i1*sin(dir+0.5));
            head.y = (short)(from.y + i1*cos(dir+0.5));
            RoadMapScreenLinePoints.cursor[0] = head;
            head.x = (short)(from.x + i1*sin(dir-0.5));
            head.y = (short)(from.y + i1*cos(dir-0.5));
            RoadMapScreenLinePoints.cursor[2] = head;
            RoadMapScreenLinePoints.cursor[1] = from;
            *RoadMapScreenObjects.cursor = 3;
            RoadMapScreenLinePoints.cursor  += 3;
            RoadMapScreenObjects.cursor += 1;
         }

         x += step_x*3;
         y += step_y*3;
      }
   }
}


/* TODO: this function should either be implemented in
 * roadmap_screen_draw_one_line(), or common parts should be extracted.
 */
void roadmap_screen_draw_line_direction (RoadMapPosition *from,
                                         RoadMapPosition *to,
                                         RoadMapPosition *first_shape_pos,
                                         int first_shape,
                                         int last_shape,
                                         RoadMapShapeItr shape_itr,
                                         int width,
                                         int direction) {

   static RoadMapPen direction_pen = NULL;

   RoadMapGuiPoint point0;
   RoadMapGuiPoint point1;

   /* These are used when the line has a shape: */
   RoadMapPosition midposition;
   RoadMapPosition last_midposition;

   int i;

   roadmap_screen_flush_lines ();
   roadmap_screen_flush_points ();

   if (direction_pen == NULL) {
      direction_pen = roadmap_canvas_create_pen ("direction_mark");
      roadmap_canvas_set_thickness (1);
      roadmap_canvas_set_foreground ("#000000");
   } else {
     roadmap_canvas_select_pen (direction_pen);
   }

   if (first_shape >= 0) {

      last_midposition = *from;
      midposition = *first_shape_pos;

      for (i = first_shape; i <= last_shape; ++i) {

         (*shape_itr) (i, &midposition);

         if (roadmap_math_line_is_visible (&last_midposition, &midposition) && 
             roadmap_math_get_visible_coordinates
                        (&last_midposition, &midposition, &point0, &point1)) {

            roadmap_screen_draw_direction (&point0, &point1, width, direction);

         }
         last_midposition = midposition;
      }

      if (roadmap_math_line_is_visible (&last_midposition, to) && 
             roadmap_math_get_visible_coordinates
                        (&last_midposition, to, &point0, &point1)) {

         roadmap_screen_draw_direction (&point0, &point1, width, direction);

      }

   } else {

      if (roadmap_math_get_visible_coordinates (from, to, &point0, &point1)) {
         roadmap_screen_draw_direction (&point0, &point1, width, direction);
      }
   }

   roadmap_screen_flush_lines ();
   roadmap_screen_flush_points ();
   RoadMapScreenLastPen = NULL;
}
#endif

int roadmap_screen_fast_refresh (void) {

   return RoadMapScreenFastRefresh;
}


#if 0
//#ifndef J2ME
//#ifdef _WIN32
static unsigned long dbg_time_rec[DBG_TIME_LAST_COUNTER];
static unsigned long dbg_time_tmp[DBG_TIME_LAST_COUNTER];

void dbg_time_start(int type) {
   dbg_time_tmp[type] = (long)NOPH_System_currentTimeMillis();
}

void dbg_time_end(int type) {
   dbg_time_rec[type] += (long)NOPH_System_currentTimeMillis() - dbg_time_tmp[type];
}

int dbg_time_print() {
    int i;
    for (i=0; i<DBG_TIME_LAST_COUNTER; i++) printf ("Timer %d: %ld\n", i, dbg_time_rec[i]);
}

#else
void dbg_time_start(int type) {
}

void dbg_time_end(int type) {
}

void dbg_time_print() {}
#endif
