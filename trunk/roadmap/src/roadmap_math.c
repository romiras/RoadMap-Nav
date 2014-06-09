/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright 2005,2006 Ehud Shabtai
 *   Copyright (c) 2008, 2009, Danny Backx.
 *
 *   3D perspective support was integrated from the RoadNav project
 *   Copyright (c) 2004 - 2006 Richard L. Lynch <rllynch@users.sourceforge.net>
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
 * @brief roadmap_math.c - Manage the little math required to place points on a map.
 *
 * These functions are used to compute the position of points on the map,
 * or the distance between two points, given their position.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>

#include "roadmap.h"
#include "roadmap_math.h"
#include "roadmap_square.h"
#include "roadmap_state.h"
#include "roadmap_config.h"
#include "roadmap_message.h"

#include "roadmap_trigonometry.h"

#define ROADMAP_BASE_IMPERIAL 0
#define ROADMAP_BASE_METRIC   1

#define MIN_ZOOM_IN     2
#define MAX_ZOOM_OUT    0x10000


static RoadMapConfigDescriptor RoadMapConfigGeneralDefaultZoom =
                        ROADMAP_CONFIG_ITEM("General", "Default Zoom");

static RoadMapConfigDescriptor RoadMapConfigGeneralZoom =
                        ROADMAP_CONFIG_ITEM("General", "Zoom");


#define ROADMAP_REFERENCE_ZOOM 20


typedef struct {
    
    double unit_per_latitude;
    double unit_per_longitude;
    double speed_per_knot;
    double cm_to_unit;
    int    to_trip_unit;
    
    char  *length;
    char  *trip_distance;
    char  *speed;
    
} RoadMapUnits;


static RoadMapUnits RoadMapMetricSystem = {

    0.11112, /* Meters per latitude. */
    0.0,     /* Meters per longitude (dynamic). */
    1.852,   /* Kmh per knot. */
    0.01,    /* centimeters to meters. */
    1000,    /* meters per kilometer. */
    "m",
    "Km",
    "Kmh"
};

static RoadMapUnits RoadMapImperialSystem = {

    0.36464, /* Feet per latitude. */
    0.0,     /* Feet per longitude (dynamic). */
    1.151,   /* Mph per knot. */
    0.03281, /* centimeters to feet. */
    5280,    /* Feet per mile. */
    "ft",
    "Mi",
    "Mph",
};
    
/**
 * @brief We maintain two copies of this  context struct, which is all
 * of the state information needed to position and scale the map.
 *
 * One copy is the working copy, and is used by all of the worker
 * routines that adjust that context -- the zoom and motion commands, etc.
 *
 * The other copy is _only_ used while we're repainting the
 * screen -- this copy is created from the "working" context when
 * we commence the repaint.
 *
 * Making a static copy for the duration of the repaint guarantees that the
 * entire repaint will be done using just one set of parameters, even if
 * motion/zoom/rotation actions happen in the middle.  (Many of those actions
 * will cause a new repaint, but sometimes we ignore them -- if we're almost
 * finished with the current repaint, for instance.
 */
static struct {
   unsigned short zoom;

   /* The current position shown on the map: */
   RoadMapPosition center;

   /* The center point (current position), in pixel: */
   int center_x;
   int center_y;

   /* The size of the area shown (pixels): */
   int width;
   int height;

   /* The conversion ratio from position to pixels: */
   int zoom_x;
   int zoom_y;


   RoadMapArea focus;
   RoadMapArea upright_screen;
   RoadMapArea current_screen;


   /* Map orientation (0: north, 90: east): */

   int orientation; /* angle in degrees. */

   int sin_orientation; /* Multiplied by 32768. */
   int cos_orientation; /* Multiplied by 32768. */

   RoadMapUnits *units;

   int _3D_horizon;

} RoadMapDisplayContext, RoadMapWorkingContext, *RoadMapContext;

/**
 * @brief
 * @param copy
 */
void roadmap_math_display_context(int copy) {

    if (copy) RoadMapDisplayContext = RoadMapWorkingContext;

    RoadMapContext = &RoadMapDisplayContext;
}

/**
 * @brief
 */
void roadmap_math_working_context(void) {

    RoadMapContext = &RoadMapWorkingContext;
}

/**
 * @brief
 * @param angle
 * @param sine_p
 * @param cosine_p
 */
static void roadmap_math_trigonometry (int angle, int *sine_p, int *cosine_p) {

   int i = angle % 90;
   int sine;
   int cosine;

   while (i < 0) i += 90;

   if (i <= 45) {
      sine   = RoadMapTrigonometricTable[i].x;
      cosine = RoadMapTrigonometricTable[i].y;
   } else {
      i = 90 - i;
      sine   = RoadMapTrigonometricTable[i].y;
      cosine = RoadMapTrigonometricTable[i].x;
   }

   while (angle < 0) angle += 360;

   i = (angle / 90) % 4;
   if (i < 0) {
      i += 4;
   }

   switch (i) {
   case 0:   *sine_p = sine;       *cosine_p = cosine;     break;
   case 1:   *sine_p = cosine;     *cosine_p = 0 - sine;   break;
   case 2:   *sine_p = 0 - sine;   *cosine_p = 0 - cosine; break;
   case 3:   *sine_p = 0 - cosine; *cosine_p = sine;       break;
   }
}

/**
 * @brief
 * @param cosine
 * @param sign
 * @return
 */
static int roadmap_math_arccosine (int cosine, int sign) {
    
    int i;
    int low;
    int high;
    int result;
    int cosine_negative = 0;
    
    if (cosine < 0) {
        cosine = 0 - cosine;
        cosine_negative = 1;
    }
    if (cosine >= 32768) {
        if (cosine > 32768) {
            roadmap_log (ROADMAP_ERROR, "invalid cosine value %d", cosine);
            return 0;
        }
        cosine = 32767;
    }
    
    high = 45;
    low  = 0;
    
    if (cosine >= RoadMapTrigonometricTable[45].y) {
        
        while (high > low + 1) {
            
            i = (high + low) / 2;
            
            if (cosine > RoadMapTrigonometricTable[i-1].y) {
                high = i - 1;
            } else if (cosine < RoadMapTrigonometricTable[i].y) {
                low = i;
            } else {
                high = i;
                break;
            }
        }
        
        result = high;
        
    } else {
        
        while (high > low + 1) {
            
            i = (high + low) / 2;
            
            if (cosine >= RoadMapTrigonometricTable[i].x) {
                low = i;
            } else if (cosine < RoadMapTrigonometricTable[i-1].y) {
                high = i - 1;
            } else {
                high = i;
                break;
            }
        }
        
        result = 90 - high;
    }
    
    result = sign * result;
    
    if (cosine_negative) {
        result = 180 - result;
        if (result > 180) {
            result = result - 360;
        }
    }
    return result;
}

/**
 * @brief
 */
void roadmap_math_compute_scale (void) {

   int orientation;

   int sine;
   int cosine;

   RoadMapGuiPoint point;
   RoadMapPosition position;


   if (RoadMapContext->zoom == 0) {
       RoadMapContext->zoom = ROADMAP_REFERENCE_ZOOM;
   }
   
   RoadMapContext->center_x = RoadMapContext->width / 2;
   RoadMapContext->center_y = RoadMapContext->height / 2;

   RoadMapContext->zoom_x = RoadMapContext->zoom;
   RoadMapContext->zoom_y = RoadMapContext->zoom;

   /* The horizontal ratio is derived from the vertical one,
    * with a scaling depending on the latitude. The goal is to
    * compute a map projection and avoid an horizontal distortion
    * when getting close to the poles.
    */

   roadmap_math_trigonometry (RoadMapContext->center.latitude / 1000000,
                                &sine,
                                &cosine);

   RoadMapMetricSystem.unit_per_longitude =
      (RoadMapMetricSystem.unit_per_latitude * cosine) / 32768;
      
   RoadMapImperialSystem.unit_per_longitude =
      (RoadMapImperialSystem.unit_per_latitude * cosine) / 32768;
      
   RoadMapContext->zoom_y =
      (int) ((RoadMapContext->zoom_y * cosine / 32768) + 0.5);

   RoadMapContext->upright_screen.west =
      RoadMapContext->center.longitude
         - (RoadMapContext->center_x * RoadMapContext->zoom_x);

   RoadMapContext->upright_screen.north =
      RoadMapContext->center.latitude
         + (RoadMapContext->center_y * RoadMapContext->zoom_y);

   orientation = RoadMapContext->orientation;
   roadmap_math_set_orientation (0);

   point.x = RoadMapContext->width;
   point.y = RoadMapContext->height;
   roadmap_math_to_position (&point, &position, 1);
   RoadMapContext->upright_screen.south = position.latitude;
   RoadMapContext->upright_screen.east  = position.longitude;

   roadmap_math_trip_set_distance ('x',
   	RoadMapContext->width * RoadMapContext->zoom_x *
                    RoadMapContext->units->unit_per_longitude);
   roadmap_math_trip_set_distance ('y',
	RoadMapContext->height * RoadMapContext->zoom_y *
                    RoadMapContext->units->unit_per_latitude);

   roadmap_math_set_orientation (orientation);
}

/**
 * @brief
 * @param from
 * @param to
 * @param point
 * @param count
 * @param intersections
 * @return
 */
static int roadmap_math_check_point_in_segment (const RoadMapPosition *from,
                                                const RoadMapPosition *to,
                                                const RoadMapPosition *point,
                                                int count,
                                                RoadMapPosition intersections[]) {

   if (!roadmap_math_point_is_visible (point)) return count;

   if ( (((from->longitude >= point->longitude) &&
           (point->longitude >= to->longitude))    ||
         ((from->longitude <= point->longitude) &&
          (point->longitude <= to->longitude)))       &&
        (((from->latitude >= point->latitude)   &&
          (point->latitude >= to->latitude))       ||
         ((from->latitude <= point->latitude)   &&
          (point->latitude <= to->latitude))) ) {

      intersections[count] = *point;
      count++;
   }

   return count;
}

/**
 * @brief
 * @param from
 * @param to
 * @param a
 * @param b
 * @param intersections
 * @param max_intersections
 * @return
 */
static int roadmap_math_find_screen_intersection (const RoadMapPosition *from,
                                                  const RoadMapPosition *to,
                                                  double a,
                                                  double b,
                                                  RoadMapPosition intersections[],
                                                  int max_intersections) {

   int count = 0;
   RoadMapPosition point;


   if ((from->longitude - to->longitude > -1.0) &&
         (from->longitude - to->longitude < 1.0)) {

      /* The two points are very close, or the line is quasi vertical:
       * approximating the line to a vertical one is good enough.
       */
      point.longitude = from->longitude;
      point.latitude = RoadMapContext->upright_screen.north;
      count =
         roadmap_math_check_point_in_segment
               (from, to, &point, count, intersections);

      if (count == max_intersections) return count;

      point.latitude = RoadMapContext->upright_screen.south;
      count =
         roadmap_math_check_point_in_segment
               (from, to, &point, count, intersections);

      return count;

   } else if ((from->latitude - to->latitude > -1.0) &&
               (from->latitude - to->latitude < 1.0)) {

      /* The two points are very close, or the line is quasi horizontal:
       * approximating the line to a horizontal one is good enough.
       */

      point.latitude = from->latitude;
      point.longitude = RoadMapContext->upright_screen.west;
      count =
         roadmap_math_check_point_in_segment
             (from, to, &point, count, intersections);

      if (count == max_intersections) return count;

      point.longitude = RoadMapContext->upright_screen.east;
      count =
         roadmap_math_check_point_in_segment
             (from, to, &point, count, intersections);

      return count;

   } else {
      
      point.latitude = RoadMapContext->upright_screen.north;
      point.longitude = (int) ((point.latitude - b) / a);
      count =
         roadmap_math_check_point_in_segment
               (from, to, &point, count, intersections);
      if (count == max_intersections) return count;

      point.latitude = RoadMapContext->upright_screen.south;
      point.longitude = (int) ((point.latitude - b) / a);
      count =
         roadmap_math_check_point_in_segment
               (from, to, &point, count, intersections);
      if (count == max_intersections) return count;

      point.longitude = RoadMapContext->upright_screen.west;
      point.latitude = (int) (b + a * point.longitude);
      count =
         roadmap_math_check_point_in_segment
               (from, to, &point, count, intersections);
      if (count == max_intersections) return count;

      point.longitude = RoadMapContext->upright_screen.east;
      point.latitude = (int) (b + a * point.longitude);
      count =
         roadmap_math_check_point_in_segment
               (from, to, &point, count, intersections);
      if (count == max_intersections) return count;

   }

   return count;
}

/**
 * @brief
 * @param point
 */
static void roadmap_math_counter_rotate_coordinate (RoadMapGuiPoint *point) {

   int x = point->x - RoadMapContext->center_x;
   int y = RoadMapContext->center_y - point->y;

   point->x =
       RoadMapContext->center_x +
           (((x * RoadMapContext->cos_orientation)
                - (y * RoadMapContext->sin_orientation) + 16383) / 32768);

   point->y =
       RoadMapContext->center_y -
           (((x * RoadMapContext->sin_orientation)
                + (y * RoadMapContext->cos_orientation) + 16383) / 32768);
}

/**
 * @brief
 * @param point
 */
#if USE_FLOAT  /* for reference, until we're sure integer version works */
static void roadmap_math_project (RoadMapGuiPoint *point) {

   /* how far away is this point along the Y axis */
   double fDistFromCenterY = RoadMapContext->height - point->y;

   /* how far from the bottom of the screen is the horizon */
   double fVisibleRange = RoadMapContext->height - RoadMapContext->_3D_horizon;

   double fDistFromCenterX;
   double fDistFromHorizon;

   /* make the Y coordinate converge on the horizon as the
    * distance from the center goes to infinity */
   point->y = (int) (RoadMapContext->height - fDistFromCenterY /
                ( fabs(fDistFromCenterY / fVisibleRange) + 1 ));

   /* X distance from center of the screen */
   fDistFromCenterX = point->x - RoadMapContext->width / 2;

   /* distance from the horizon after adjusting for perspective */
   fDistFromHorizon = point->y - RoadMapContext->_3D_horizon;

   /* squeeze the X axis, make it a point at the horizon and
    * normal sized at the bottom of the screen */
   point->x = (int) (fDistFromCenterX * ( fDistFromHorizon / fVisibleRange )
                        + RoadMapContext->width / 2);
}

/**
 * @brief
 * @param point
 */
void roadmap_math_unproject (RoadMapGuiPoint *point) {

   RoadMapGuiPoint point2;

   /* X distance from center of screen */
   double fDistFromCenterX = point->x - RoadMapContext->width / 2;

   /* Y distance from horizon */
   double fDistFromHorizon = point->y - RoadMapContext->_3D_horizon;

   /* distance from bottom of screen to horizon */
   double fVisibleRange = RoadMapContext->height - RoadMapContext->_3D_horizon;
   double fDistFromBottom;
   double fD;

   if (RoadMapContext->_3D_horizon == 0) {
      return;
   }

   /* unsqueeze the X axis */
   point2.x = (int) (fDistFromCenterX / 
         ( fDistFromHorizon / fVisibleRange ) + RoadMapContext->width / 2);

   /* distance from bottom of screen */
   fDistFromBottom = RoadMapContext->height - point->y;
   /* inverse Y squeezing formula */
   fD = fDistFromBottom / ( 1.0 - fDistFromBottom / fVisibleRange);

   /* center on screen */
   point2.y = (int) (RoadMapContext->height - fD);

   *point = point2;
}
#else
static void roadmap_math_project (RoadMapGuiPoint *point) {

   /* how far away is this point along the Y axis */
   long DistFromCenterY = RoadMapContext->height - point->y;

   /* how far from the bottom of the screen is the horizon */
   long VisibleRange = RoadMapContext->height - RoadMapContext->_3D_horizon;

   long DistFromCenterX;
   long DistFromHorizon;

   /* make the Y coordinate converge on the horizon as the
    * distance from the center goes to infinity */
   point->y = RoadMapContext->height - 
               (DistFromCenterY * VisibleRange) /
                        (abs(DistFromCenterY) + VisibleRange) ;

   /* X distance from center of the screen */
   DistFromCenterX = point->x - RoadMapContext->width / 2;

   /* distance from the horizon after adjusting for perspective */
   DistFromHorizon = point->y - RoadMapContext->_3D_horizon;

   /* squeeze the X axis, make it a point at the horizon and
    * normal sized at the bottom of the screen */
   point->x = (DistFromCenterX * DistFromHorizon) / VisibleRange
                        + (RoadMapContext->width / 2);
}

void roadmap_math_unproject (RoadMapGuiPoint *point) {

   RoadMapGuiPoint point2;

   /* X distance from center of screen */
   long DistFromCenterX = point->x - RoadMapContext->width / 2;

   /* Y distance from horizon */
   long DistFromHorizon = point->y - RoadMapContext->_3D_horizon;

   /* distance from bottom of screen to horizon */
   long VisibleRange = RoadMapContext->height - RoadMapContext->_3D_horizon;
   long DistFromBottom;
   long D;

   if (RoadMapContext->_3D_horizon == 0) {
      return;
   }

   /* unsqueeze the X axis */
   point2.x = DistFromCenterX * VisibleRange / DistFromHorizon +
                RoadMapContext->width / 2;

   /* distance from bottom of screen */
   DistFromBottom = RoadMapContext->height - point->y;

   /* inverse Y squeezing formula */
   D = (DistFromBottom * VisibleRange) / ( (VisibleRange - DistFromBottom) );

   /* center on screen */
   point2.y = RoadMapContext->height - D;

   *point = point2;
}
#endif


static int roadmap_math_zoom_state (void) {

   if (RoadMapContext->zoom == 
         roadmap_config_get_integer (&RoadMapConfigGeneralDefaultZoom)) {

      return MATH_ZOOM_RESET;
   } else {

      return MATH_ZOOM_NO_RESET;
   }
}

#if defined(HAVE_TRIP_PLUGIN)

#define	DOTS_PER_INCH 72
#define INCHES_PER_DEGREE 4374754

void roadmap_math_set_scale (int scale, int use_map_units)
{
	int res;
	if (use_map_units) {
		res = scale / (RoadMapContext->units->unit_per_latitude * use_map_units);
	} else {
		res = scale / (1.0 * DOTS_PER_INCH * INCHES_PER_DEGREE / 1000000);
	}
	roadmap_math_zoom_set (res);
}
#endif

/**
 * @brief Rotation of the screen:
 * rotate the coordinates of a point on the screen, the center of
 * the rotation being the center of the screen.
 *
 * @param count
 * @param points
 */
void roadmap_math_rotate_coordinates (int count, RoadMapGuiPoint *points) {

   int i;
   int x;
   int y;


   if (!RoadMapContext->orientation && !RoadMapContext->_3D_horizon) return;

   for (i = count; i > 0; --i) {

      if (RoadMapContext->orientation) {
         x = points->x - RoadMapContext->center_x;
         y = RoadMapContext->center_y - points->y;

         points->x =
            RoadMapContext->center_x +
            (((x * RoadMapContext->cos_orientation)
              + (y * RoadMapContext->sin_orientation) + 16383) / 32768);

         points->y =
            RoadMapContext->center_y -
            (((y * RoadMapContext->cos_orientation)
              - (x * RoadMapContext->sin_orientation) + 16383) / 32768);
      }

      if (RoadMapContext->_3D_horizon) {

         roadmap_math_project (points);
      }

      points += 1;
   }
}


/**
 * @brief rotate the coordinates of a point to an arbitrary angle
 * @param point
 * @param center
 * @param angle
 */
void roadmap_math_rotate_point (RoadMapGuiPoint *point,
                                RoadMapGuiPoint *center, int angle) {

   static int cached_angle = -99999;
   static int sin_orientation;
   static int cos_orientation;

   int x;
   int y;

   if (angle != cached_angle) {
      cached_angle = angle;
      roadmap_math_trigonometry (cached_angle,
                                 &sin_orientation,
                                 &cos_orientation);
   }

   x = point->x;
   y = point->y;

   point->x =
      center->x +
      (((x * cos_orientation)
        + (y * sin_orientation) + 16383) / 32768);

   point->y =
      center->y -
      (((y * cos_orientation)
        - (x * sin_orientation) + 16383) / 32768);

   if (angle == 0) return;

   if (RoadMapContext->_3D_horizon) {
      roadmap_math_project (point);
   }
}


/* Rotate a specific object:
 * rotate the coordinates of the object's points according to the provided
 * rotation.
 */
void roadmap_math_rotate_object
         (int count, RoadMapGuiPoint *points,
          RoadMapGuiPoint *center, int orientation) {

   int i;
   int x;
   int y;
   int sin_o;
   int cos_o;
   int total = (RoadMapContext->orientation + orientation) % 360;


   if ((total == 0) && !RoadMapContext->_3D_horizon) return;

   if (total) {
      roadmap_math_trigonometry (total, &sin_o, &cos_o);
   }

   for (i = count; i > 0; --i) {

      if (total) {
         x = points->x - center->x;
         y = center->y - points->y;

         points->x = center->x + (((x * cos_o) + (y * sin_o) + 16383) / 32768);
         points->y = center->y - (((y * cos_o) - (x * sin_o) + 16383) / 32768);
      }

      points += 1;
   }
}

/**
 * @brief
 */
void roadmap_math_initialize (void) {

    roadmap_config_declare ("session", &RoadMapConfigGeneralZoom, "0");
    roadmap_config_declare
        ("preferences", &RoadMapConfigGeneralDefaultZoom, "20");

    roadmap_state_add ("zoom_reset", &roadmap_math_zoom_state);

    roadmap_math_working_context();

    RoadMapContext->orientation = 0;
    RoadMapContext->_3D_horizon = 0;

    roadmap_math_use_imperial ();
    roadmap_math_compute_scale ();
}

/**
 * @brief
 */
void roadmap_math_use_metric (void) {

    RoadMapContext->units = &RoadMapMetricSystem;

}

/**
 * @brief
 */
void roadmap_math_use_imperial (void) {

    RoadMapContext->units = &RoadMapImperialSystem;
}

/**
 * @brief
 * @param focus
 * @param focused_point
 */
static void roadmap_math_adjust_focus
                (RoadMapArea *focus, const RoadMapGuiPoint *focused_point) {

    RoadMapPosition focus_position;

    roadmap_math_to_position (focused_point, &focus_position, 1);

    if (focus_position.longitude < focus->west) {
        focus->west = focus_position.longitude;
    }
    if (focus_position.longitude > focus->east) {
        focus->east = focus_position.longitude;
    }
    if (focus_position.latitude < focus->south) {
        focus->south = focus_position.latitude;
    }
    if (focus_position.latitude > focus->north) {
        focus->north = focus_position.latitude;
    }
}

/**
 * @brief
 * @param focus
 * @param position
 * @param accuracy
 */
void
roadmap_math_focus_area 
                (RoadMapArea *focus, const RoadMapPosition *position,
                 int accuracy) {

    RoadMapGuiPoint focus_point;
    RoadMapPosition focus_position;
    roadmap_math_coordinate (position, &focus_point);
    roadmap_math_rotate_coordinates (1, &focus_point);

    focus_point.x += accuracy;
    focus_point.y += accuracy;
    roadmap_math_to_position (&focus_point, &focus_position, 1);

    focus->west = focus_position.longitude;
    focus->east = focus_position.longitude;
    focus->north = focus_position.latitude;
    focus->south = focus_position.latitude;

    accuracy *= 2;
 
    focus_point.x -= accuracy;
    roadmap_math_adjust_focus (focus, &focus_point);

    focus_point.y -= accuracy;
    roadmap_math_adjust_focus (focus, &focus_point);

    focus_point.x += accuracy;
    roadmap_math_adjust_focus (focus, &focus_point);
}


void roadmap_math_set_focus (const RoadMapArea *focus) {

   RoadMapContext->focus = *focus;
}

void roadmap_math_get_focus (RoadMapArea *area) {

   *area = RoadMapContext->focus;
}


void roadmap_math_release_focus (void) {

   RoadMapContext->focus = RoadMapContext->current_screen;
}


int roadmap_math_areas_intersect
        (const RoadMapArea *area1, const RoadMapArea *area2) {

   if (area1->west > area2->east ||
       area1->east < area2->west ||
       area1->south > area2->north ||
       area1->north < area2->south)
   {
       return 0;
   }

   if (area1->west >= area2->west &&
       area1->east < area2->east &&
       area1->south > area2->south &&
       area1->north <= area2->north)
   {
       return 1;
   }

   return -1;
}

int roadmap_math_is_visible (const RoadMapArea *area) {
    return roadmap_math_areas_intersect(area, &RoadMapContext->focus);
}

/**
 * @brief
 * @param point1
 * @param point2
 * @return
 */
int roadmap_math_line_is_visible (const RoadMapPosition *point1,
                                  const RoadMapPosition *point2) {

   if ((point1->longitude > RoadMapContext->focus.east) &&
       (point2->longitude > RoadMapContext->focus.east)) {
      return 0;
   }

   if ((point1->longitude < RoadMapContext->focus.west) &&
       (point2->longitude < RoadMapContext->focus.west)) {
      return 0;
   }

   if ((point1->latitude > RoadMapContext->focus.north) &&
       (point2->latitude > RoadMapContext->focus.north)) {
      return 0;
   }

   if ((point1->latitude < RoadMapContext->focus.south) &&
       (point2->latitude < RoadMapContext->focus.south)) {
      return 0;
   }

   return 1; /* Do not bother checking for partial visibility yet. */
}

/**
 * @brief
 * @param point
 * @return
 */
int roadmap_math_point_is_visible (const RoadMapPosition *point) {

   if ((point->longitude > RoadMapContext->focus.east) ||
       (point->longitude < RoadMapContext->focus.west) ||
       (point->latitude  > RoadMapContext->focus.north) ||
       (point->latitude  < RoadMapContext->focus.south)) {
      return 0;
   }

   return 1;
}


int roadmap_math_get_visible_coordinates (const RoadMapPosition *from,
                                          const RoadMapPosition *to,
                                          RoadMapGuiPoint *point0,
                                          RoadMapGuiPoint *point1) {

   int from_visible = roadmap_math_point_is_visible (from);
   int to_visible = roadmap_math_point_is_visible (to);
   int count;
   RoadMapPosition intersections[2];
   int max_intersections = 0;

   if (!from_visible || !to_visible) {

      double a;
      double b;

      if (from_visible || to_visible) {
         max_intersections = 1;
      } else {
         max_intersections = 2;
      }


      /* Equation of the line: */

      if ((from->longitude - to->longitude) == 0) {
         a = b = 0;
      } else {
         a = 1.0 * (from->latitude - to->latitude) /
                   (from->longitude - to->longitude);
         b = from->latitude - 1.0 * a * from->longitude;
      }

      if (roadmap_math_find_screen_intersection
            (from, to, a, b, intersections, max_intersections) !=
             max_intersections) {
         return 0;
      }
   }

   if (max_intersections == 2) {
      /* Make sure we didn't swap the from/to points */
      if (roadmap_math_distance (from, &intersections[0]) >
          roadmap_math_distance (from, &intersections[1])) {

         RoadMapPosition tmp = intersections[1];
         intersections[1] = intersections[0];
         intersections[0] = tmp;
      }
   }

   count = 0;

   if (from_visible) {
      roadmap_math_coordinate (from, point0);
   } else {
      roadmap_math_coordinate (&intersections[count], point0);
      count++;
   }

   if (to_visible) {
      roadmap_math_coordinate (to, point1);
   } else {
      roadmap_math_coordinate (&intersections[count], point1);
   }

   return 1;
}


void roadmap_math_restore_zoom (void) {

    int zoomval;
    zoomval = roadmap_config_get_integer (&RoadMapConfigGeneralZoom);
    if (zoomval < MIN_ZOOM_IN || zoomval > MAX_ZOOM_OUT) {
         zoomval =
            roadmap_config_get_integer (&RoadMapConfigGeneralDefaultZoom);
        if (zoomval < MIN_ZOOM_IN || zoomval > MAX_ZOOM_OUT) {
            zoomval = ROADMAP_REFERENCE_ZOOM;
        }
    }
    RoadMapContext->zoom = zoomval;
    roadmap_math_compute_scale ();
}

int roadmap_math_zoom_out (void) {

   unsigned int zoom;

   /* using float keeps the zoom levels consistent (no roundoff
    * error), which makes declutter caching work better */
   zoom = ((double)RoadMapContext->zoom * 1.5) + 0.5;

   if (zoom < MAX_ZOOM_OUT) {
      RoadMapContext->zoom = (unsigned short) zoom;
      roadmap_config_set_integer (&RoadMapConfigGeneralZoom, RoadMapContext->zoom);
      return 1;
   }
   return 0;
}


int roadmap_math_zoom_in (void) {

   unsigned int zoom;

   zoom = ((double)RoadMapContext->zoom / 1.5) + 0.5;

   if (zoom > MIN_ZOOM_IN) {
      RoadMapContext->zoom = zoom;
      roadmap_config_set_integer (&RoadMapConfigGeneralZoom, RoadMapContext->zoom);
      return 1;
   }
   return 0;
}

void roadmap_math_zoom_set (int zoom) {

   if (zoom < MIN_ZOOM_IN) {
      zoom = MIN_ZOOM_IN;
   } else if (zoom >= MAX_ZOOM_OUT) {
      zoom = MAX_ZOOM_OUT - 1;
   }
   RoadMapContext->zoom = zoom;

   roadmap_config_set_integer (&RoadMapConfigGeneralZoom, RoadMapContext->zoom);
   roadmap_math_compute_scale ();
}

int roadmap_math_zoom_reset (void) {

   int zoomval;

   zoomval = roadmap_config_get_integer (&RoadMapConfigGeneralDefaultZoom);
   if (zoomval < MIN_ZOOM_IN || zoomval > MAX_ZOOM_OUT) {
      zoomval = ROADMAP_REFERENCE_ZOOM;
   }

   if (RoadMapContext->zoom != zoomval) {
      RoadMapContext->zoom = zoomval;
      roadmap_config_set_integer (&RoadMapConfigGeneralZoom, RoadMapContext->zoom);
      return 1;
   }
   return 0;
}


int roadmap_math_declutter (int level) {

   return (RoadMapContext->zoom < level);
}


int roadmap_math_thickness (int base, int declutter, int use_multiple_pens) {

   double ratio;

   ratio = ((2.5 * ROADMAP_REFERENCE_ZOOM) * base) / RoadMapContext->zoom;

   if (ratio < 0.1 / base) {
      return 1;
   }

   if (declutter > (ROADMAP_REFERENCE_ZOOM*100)) {
      declutter = ROADMAP_REFERENCE_ZOOM*100;
   }

   if (ratio < 1.0 * base) {

      /* Use the declutter value to decide how fast should a line shrink.
       * This way, a street shrinks faster than a freeway when we zoom out.
       */
      ratio += (base-ratio) * (0.30*declutter/RoadMapContext->zoom);
      if (ratio > base) {
         ratio = base;
      }
   }

   /* if this is a multi-pen object, try to force a minimum thickness of 3
    * so we'll get a pretty drawing. Otherwise set it to 1.
    */
   if (use_multiple_pens && (ratio < 3)) {
      if ((1.0 * RoadMapContext->zoom / declutter) < 0.50) {
         ratio = 3;
      } else {
         ratio = 1;
      }
   }

   return (int) ratio;
}


void roadmap_math_set_size (int width, int height) {

   RoadMapContext->width = width;
   RoadMapContext->height = height;

   roadmap_math_compute_scale ();
}


void roadmap_math_set_center (const RoadMapPosition *position) {

   RoadMapPosition c;
   c = *position;

   /* impose limits:  roadmap doesn't have the math smarts to wrap
    * around 180/-180 going west/east, and mercator-like projections
    * just don't work past about 85 degrees.  */
   if (c.longitude < -179000000) c.longitude = -179000000;
   else if (c.longitude > 179000000) c.longitude = 179000000;

   if (c.latitude < -86000000) c.latitude = -86000000;
   else if (c.latitude > 86000000) c.latitude = 86000000;

   RoadMapContext->center = c;

   roadmap_math_compute_scale ();
}

RoadMapPosition *roadmap_math_get_center (void) {

   return &RoadMapContext->center;
   
}

void roadmap_math_set_horizon (int horizon) {

   RoadMapContext->_3D_horizon = horizon;

   roadmap_math_compute_scale ();
}


void roadmap_math_set_orientation (int direction) {

   /* FIXME: this function, which primary purpose was to
    * compute the span of the visible map area when rotated,
    * has become THE way for setting the visible map area
    * (i.e. RoadMapContext->current_screen). Therefore, one
    * must execute it to the end.
    */
#if BEFORE
   // removed this code -- it causes us to skip a refresh after
   // already adjusting the Context.  this can disrupt a refresh
   // already in progress (since refresh is now interruptible, to
   // some extent).
   int status = 1; /* Force a redraw by default. */

   direction = direction % 360;
   while (direction < 0) direction += 360;

   if (direction == RoadMapContext->orientation) {

      status = 0; /* Not modified at all. */

   } else if ((direction != 0) &&
              (abs(direction - RoadMapContext->orientation) <= 5)) {

      /* We do not force a redraw for every small move, except
       * when it is a back-to-zero event, which might be a reset.
       */
      status = 0; /* Not modified enough. */

   } else {

      RoadMapContext->orientation = direction;
   }
#else
   RoadMapContext->orientation = direction;
#endif

   roadmap_math_trigonometry (direction,
                              &RoadMapContext->sin_orientation,
                              &RoadMapContext->cos_orientation);

   RoadMapContext->current_screen = RoadMapContext->upright_screen;

   if ((direction != 0) || RoadMapContext->_3D_horizon) {

      int i;
      RoadMapGuiPoint point;
      RoadMapPosition position[4];

      point.x = 0;
      point.y = 0;
      roadmap_math_to_position (&point, position, 1);

      point.x = RoadMapContext->width;
      roadmap_math_to_position (&point, position+1, 1);

      point.y = RoadMapContext->height;
      roadmap_math_to_position (&point, position+2, 1);

      point.x = 0;
      roadmap_math_to_position (&point, position+3, 1);

      for (i = 0; i < 4; ++i) {

         if (position[i].longitude > RoadMapContext->current_screen.east) {

            RoadMapContext->current_screen.east = position[i].longitude;
         }
         if (position[i].longitude < RoadMapContext->current_screen.west) {

            RoadMapContext->current_screen.west = position[i].longitude;
         }
         if (position[i].latitude < RoadMapContext->current_screen.south) {

            RoadMapContext->current_screen.south = position[i].latitude;
         }
         if (position[i].latitude > RoadMapContext->current_screen.north) {

            RoadMapContext->current_screen.north = position[i].latitude;
         }
      }
   }

   roadmap_math_release_focus ();
#if 0
roadmap_log (ROADMAP_DEBUG, "visibility: north=%d south=%d east=%d west=%d\n",
        RoadMapContext->current_screen.north,
        RoadMapContext->current_screen.south,
        RoadMapContext->current_screen.east,
        RoadMapContext->current_screen.west);
#endif
}


int  roadmap_math_get_orientation (void) {

   return RoadMapContext->orientation;
}

void roadmap_math_to_position (const RoadMapGuiPoint *point,
                               RoadMapPosition *position,
                               int projected) {

   RoadMapGuiPoint point2;

   if (projected && RoadMapContext->_3D_horizon) {

      point2 = *point;
      roadmap_math_unproject (&point2);

      point = &point2;
   }

   if (RoadMapContext->orientation) {

      if (!projected || !RoadMapContext->_3D_horizon) {
         point2 = *point;
      }

      roadmap_math_counter_rotate_coordinate (&point2);
      point = &point2;
   }

   position->longitude =
      RoadMapContext->upright_screen.west
         + (point->x * RoadMapContext->zoom_x);

   position->latitude =
      RoadMapContext->upright_screen.north
         - (point->y * RoadMapContext->zoom_y);
}

/**
 * @brief determine whether "point" is within the "bbox" relative to "ref"
 * @param point
 * @param ref
 * @param bbox
 * @return
 */
int roadmap_math_point_in_box
    (RoadMapGuiPoint *point, RoadMapGuiPoint *ref, RoadMapGuiRect *bbox)
{
      return (point->x >= (ref->x + bbox->minx)) &&
             (point->x <= (ref->x + bbox->maxx)) &&
             (point->y >= (ref->y + bbox->miny)) &&
             (point->y <= (ref->y + bbox->maxy));
}

/**
 * @brief
 * @param a
 * @param b
 * @return
 */
int roadmap_math_rectangle_overlap (RoadMapGuiRect *a, RoadMapGuiRect *b) {

   if (a->minx > b->maxx) return 0;
   if (a->maxx < b->minx) return 0;
   if (a->miny > b->maxy) return 0;
   if (a->maxy < b->miny) return 0;

   return 1;
}


void roadmap_math_coordinate (const RoadMapPosition *position,
                              RoadMapGuiPoint *point) {

   int scale = 1;

   point->x =
      ((position->longitude - RoadMapContext->upright_screen.west)
             / (RoadMapContext->zoom_x / scale));

   point->y =
      ((RoadMapContext->upright_screen.north - position->latitude)
             / (RoadMapContext->zoom_y / scale));
}


int roadmap_math_azymuth
       (const RoadMapPosition *point1, const RoadMapPosition *point2) {

    int result;
    double x;
    double y;
    double d;


    x = RoadMapContext->units->unit_per_longitude
            * (point2->longitude - point1->longitude);
    y = RoadMapContext->units->unit_per_latitude
            * (point2->latitude  - point1->latitude);

    d = sqrt ((x * x) + (y * y));
    
    if (d > 0.0001 || d < -0.0001) {
        result = roadmap_math_arccosine
                    ((int) ((32768 * y) / d), (x > 0)?1:-1);
    } else {
        result = 0;
    }
    
#if 0
    roadmap_log (ROADMAP_DEBUG,
                    "azymuth for (x=%f, y=%f): %d",
                    x, y, result);
#endif
    return result;
}


int roadmap_math_angle
       (const RoadMapGuiPoint *point1, const RoadMapGuiPoint *point2) {

    int result;
    int x;
    int y;
    double d;

    x = point2->x - point1->x;
    y = point2->y - point1->y;

    d = sqrt ((x * x) + (y * y));
    
    if (d > 0.0001 || d < -0.0001) {
        result = roadmap_math_arccosine
                    ((int) ((32768 * y) / d), (x > 0)?1:-1);
    } else {
        result = 0;
    }
    
    return result;
}

long roadmap_math_screen_distance
       (const RoadMapGuiPoint *pt1, const RoadMapGuiPoint *pt2, int squared) {

   long dx;
   long dy;
   long ret;


   dx = pt1->x - pt2->x;
   dy = pt1->y - pt2->y;


   ret = (dx * dx) + (dy * dy);

   if (squared == MATH_DIST_ACTUAL) {
        ret =  (long) sqrt ((double)ret);
   }

   return ret;
}

/**
 * @brief calculate the distance between two points
 * @param position1
 * @param position2
 * @return
 */
int roadmap_math_distance (const RoadMapPosition *position1, const RoadMapPosition *position2)
{
   double x;
   double y;

   x = RoadMapContext->units->unit_per_longitude * (position1->longitude - position2->longitude);
   y = RoadMapContext->units->unit_per_latitude * (position1->latitude  - position2->latitude);

   return (int) sqrt ((x * x) + (y * y));
}


/**
 * @brief returns a square bounding box, centered at 'from', roughly "distance" miles/km on a side
 * @param bbox
 * @param from
 * @param distance
 * @param unitstring
 */
void roadmap_math_bbox_around_point (RoadMapArea *bbox, const RoadMapPosition *from,
         double distance, char *unitstring) {

   RoadMapUnits *units;
   double unit_per_longitude;
   int sine, cosine;

   if (strcasecmp(unitstring, "km") == 0) {
      units = &RoadMapMetricSystem;
   } else { /* miles */
      units = &RoadMapImperialSystem;
   }

   roadmap_math_trigonometry (from->latitude / 1000000, &sine, &cosine);

   distance *= units->to_trip_unit;  /* to meters or feet */
   unit_per_longitude = (units->unit_per_latitude * cosine) / 32768;

   distance /= 2; /* want half the square's dimensions */

   bbox->west = from->longitude - distance / unit_per_longitude;
   bbox->east = from->longitude + distance / unit_per_longitude;
   bbox->south = from->latitude - distance / units->unit_per_latitude;
   bbox->north = from->latitude + distance / units->unit_per_latitude;
}

/*
 * @brief Take a number followed by ft/mi/m/km, and converts it to current units.
 * @param string
 * @param was_explicit
 * @return
 */
int roadmap_math_distance_convert(const char *string, int *was_explicit)
{
    char *suffix;
    double distance;
    RoadMapUnits *my_units, *other_units;
    int had_units = 1;

    my_units = RoadMapContext->units;
    if (my_units == &RoadMapMetricSystem) {
        other_units = &RoadMapImperialSystem;
    } else {
        other_units = &RoadMapMetricSystem;
    }

    distance = strtol (string, &suffix, 10);

    while (*suffix && isspace(*suffix)) suffix++;

    if (*suffix) {
        if (0 == strcasecmp(suffix, my_units->length)) {
            /* Nothing to do, hopefully this is the usual case. */
        } else if (0 == strcasecmp(suffix, my_units->trip_distance)) {
            distance *= my_units->to_trip_unit;
        } else if (0 == strcasecmp(suffix, other_units->length)) {
            distance /= other_units->cm_to_unit;
            distance *= my_units->cm_to_unit;
        } else if (0 == strcasecmp(suffix, other_units->trip_distance)) {
            distance *= other_units->to_trip_unit;
            distance /= other_units->cm_to_unit;
            distance *= my_units->cm_to_unit;
        } else {
            roadmap_log (ROADMAP_WARNING, 
                "dropping unknown units '%s' from '%s'", suffix, string);
            had_units = 0;
        }
    } else {
        had_units = 0;
    }

    if (was_explicit) *was_explicit = had_units;

    return (int)distance;
}


char *roadmap_math_distance_unit (void) {
    
    return RoadMapContext->units->length;
}


char *roadmap_math_trip_unit (void) {
    
    return RoadMapContext->units->trip_distance;
}


char *roadmap_math_speed_unit (void) {
    
    return RoadMapContext->units->speed;
}


int roadmap_math_to_trip_distance (int distance) {
    
    return distance / RoadMapContext->units->to_trip_unit;
}

int roadmap_math_to_trip_distance_tenths (int distance) {
    
    return (10 * distance) / RoadMapContext->units->to_trip_unit;
}

/**
 * @brief set some distance into the message API's buffers
 * @param which indicate which distance to set
 * @param distance the value to use
 */
void roadmap_math_trip_set_distance(char which, int distance)
{
    int distance_far;

    distance_far = roadmap_math_to_trip_distance_tenths (distance);

    if (distance_far > 0) {
        roadmap_message_set (which, "%d.%d %s",
                             distance_far/10,
                             distance_far%10,
                             roadmap_math_trip_unit ());
    } else {
        roadmap_message_set (which, "%d %s",
                             distance,
                             roadmap_math_distance_unit ());
    }
}


int  roadmap_math_get_distance_from_segment
        (const RoadMapPosition *position,
         const RoadMapPosition *position1,
         const RoadMapPosition *position2,
               RoadMapPosition *intersection,
                           int *which) {

   int distance;
   int minimum;

   double x1;
   double y1;
   double x2;
   double y2;
   double x3;
   double y3;


   /* Compute the coordinates relative to the "position" point. */

   x1 = RoadMapContext->units->unit_per_longitude
           * (position->longitude - position1->longitude);
   y1 = RoadMapContext->units->unit_per_latitude
           * (position->latitude  - position1->latitude);

   x2 = RoadMapContext->units->unit_per_longitude
           * (position->longitude - position2->longitude);
   y2 = RoadMapContext->units->unit_per_latitude
           * (position->latitude  - position2->latitude);


   /* Compute the coordinates of the intersection with the perpendicular. */

   if ((x1 - x2 > -1.0) && (x1 - x2 < 1.0)) {

      /* The two points are very close, or the line is quasi vertical:
       * approximating the line to a vertical one is good enough.
       */
      x3 = (x1 + x2) / 2;
      y3 = 0.0;

      if (intersection != NULL) {
         intersection->longitude =
            (position1->longitude + position2->longitude) / 2;
      }

   } else {
      
      /* Equation of the line: */

      double a = (y1 - y2) / (x1 - x2);
      double b = y1 - a * x1;

      /* The equation of the perpendicular is: y = - (x / a). */

      y3 = b / ((a * a) + 1.0);
      x3 = -a * y3;


      if (intersection != NULL) {
         intersection->longitude =
            position1->longitude + (int)(((x1 - x3)
                    * (position2->longitude - position1->longitude))
                       / (x1 - x2));
      }
   }


   if ((((x1 >= x3) && (x3 >= x2)) || ((x1 <= x3) && (x3 <= x2))) &&
       (((y1 >= y3) && (y3 >= y2)) || ((y1 <= y3) && (y3 <= y2)))) {

      /* The intersection point is in the segment. */

      if (intersection != NULL) {
         if ((y1 - y2 > -1.0) && (y1 - y2 < 1.0)) {

            intersection->latitude =
               (position1->latitude + position2->latitude) / 2;

         } else {

            intersection->latitude =
               position1->latitude + (int)(((y1 - y3)
                      * (position2->latitude - position1->latitude))
                         / (y1 - y2));
         }
      }

      if (which != NULL) *which = 0;  /* neither endpoint is closest */

      return (int) sqrt ((x3 * x3) + (y3 * y3));
   }

   /* The intersection point is not in the segment: use the distance to
    * the closest segment's endpoint.
    */
   minimum  = (int) sqrt ((x1 * x1) + (y1 * y1));
   distance = (int) sqrt ((x2 * x2) + (y2 * y2));

   if (distance < minimum) {

      if (intersection != NULL) *intersection = *position2;

      if (which != NULL) *which = 2;  /* endpoint 2 is closest */

      return distance;
   }

   if (intersection != NULL) *intersection = *position1;

   if (which != NULL) *which = 1;  /* endpoint 1 is closest */

   return minimum;
}


int roadmap_math_knots_to_speed_unit (int knots) {
    
    return (int) (knots * RoadMapContext->units->speed_per_knot);
}


int roadmap_math_to_current_unit (int value, const char *unit) {

    if (strcasecmp (unit, "cm") == 0) {

        static int PreviousValue = 0;
        static int PreviousResult = 0;

        if (value != PreviousValue) {
            PreviousResult = (int) (RoadMapContext->units->cm_to_unit * value);
            PreviousValue = value;
        }
        return PreviousResult;
    }

    roadmap_log (ROADMAP_ERROR, "unsupported unit '%s'", unit);
    return value;
}


int roadmap_math_to_cm (int value) {

    static int PreviousValue = 0;
    static int PreviousResult = 0;

    if (value != PreviousValue) {
        PreviousResult = (int) (value / RoadMapContext->units->cm_to_unit);
        PreviousValue = value;
    }

    return PreviousResult;
}


void roadmap_math_screen_edges (RoadMapArea *area) {

   *area = RoadMapContext->current_screen;
}


unsigned int roadmap_math_street_address (const char *image, int length) {

   int i;
   int digit;
   unsigned int result = 0;

   if ((length > 8) &&
       ((image[0] == 'W') || (image[0] == 'E') ||
        (image[0] == 'S') || (image[0] == 'N'))) {

       /* This is a very special case: the street number is organized
        * like a geographical position (longitude/latitude). Skip
        * The west/north flags and format the number accordingly.
        */
       int separator = 0;
       unsigned int multiplier = 10;
       unsigned int part1 = 0;
       unsigned int part2 = 0;


       for (i = length - 1; i > 0; --i) {

          if ((image[i] == 'W') || (image[i] == 'E') ||
              (image[i] == 'S') || (image[i] == 'N') || (image[i] == '-')) {

             separator = i;
          }
       }

       for (i = 1; i < separator; ++i) {
          if (image[i] < '0' || image[i] > '9') {
             roadmap_log
                (ROADMAP_WARNING, "bad numerical character %c", image[i]);
          }
          part1 = (part1 * 10) + (unsigned int)(image[i] - '0');
       }

       for (i = separator + 1; i < length; ++i) {
          if (image[i] < '0' || image[i] > '9') {
             roadmap_log
                (ROADMAP_WARNING, "bad numerical character %c", image[i]);
          }
          part2 = (part2 * 10) + (unsigned int)(image[i] - '0');
          multiplier *= 10;
       }

       return (part1 * multiplier) + part2;
   }

   for (i = 0; i < length; i++) {

      digit = image[i];

      if ((digit < '0') || (digit > '9')) {

         if (length >= 9) {
            continue;
         } else if (digit == '-' || digit == ' ') {
            continue;
         } else if (digit >= 'A' && digit <= 'Z') {
            digit = '1' + digit - 'A';
         } else if (digit >= 'a' && digit <= 'z') {
            digit = '1' + digit - 'a';
         } else  {
            roadmap_log (ROADMAP_WARNING, "bad numerical character %c", digit);
            continue;
         }
      }
      result = (result * 10) + (unsigned int)(digit - '0');
   }

   return result;
}


int roadmap_math_intersection (RoadMapPosition *from1,
                               RoadMapPosition *to1,
                               RoadMapPosition *from2,
                               RoadMapPosition *to2,
                               RoadMapPosition *intersection) {

   double a1,b1;
   double a2,b2;

   if (from1->longitude == to1->longitude) {

      a1 = 0;
      b1 = from1->latitude;
   } else {
      a1 = 1.0 * (from1->latitude - to1->latitude) /
         (from1->longitude - to1->longitude);
      b1 = from1->latitude - 1.0 * a1 * from1->longitude;
   }

   if ((from2->longitude - to2->longitude) == 0) {

      a2 = 0;
      b2 = from2->latitude;
   } else {
      a2 = 1.0 * (from2->latitude - to2->latitude) /
         (from2->longitude - to2->longitude);
      b2 = from2->latitude - 1.0 * a2 * from2->longitude;
   }

   if (a1 == a2) return 0;

   intersection->longitude = (int) ((b1 - b2) / (a2 - a1));
   intersection->latitude = (int) (b1 + intersection->longitude * a1);

   return 1;
}

/* this routine isn't accurate for segments that are either very flat
 * or very steep (i.e. within a few degrees of 0 or 90).
 */
int roadmap_math_screen_intersect (RoadMapGuiPoint *f1, RoadMapGuiPoint *t1,
                           RoadMapGuiPoint *f2, RoadMapGuiPoint *t2,
                           RoadMapGuiPoint *isect) {

#if USE_FLOAT  /* for reference, until we're sure integer version works */
   double a1,b1;
   double a2,b2;
   double x;

   if (f1->x == t1->x) {

      a1 = 0;
      b1 = f1->y;
   } else {
      a1 = 1.0 * (f1->y - t1->y) / (f1->x - t1->x);
      b1 = f1->y - 1.0 * a1 * f1->x;
   }

   if ((f2->x - t2->x) == 0) {
      a2 = 0;
      b2 = f2->y;
   } else {
      a2 = 1.0 * (f2->y - t2->y) / (f2->x - t2->x);
      b2 = f2->y - 1.0 * a2 * f2->x;
   }

   if (a1 == a2) return 0;

   x = (b1 - b2) / (a2 - a1);
   if (fabs(a1) < fabs(a2)) {
      isect->y = (int) (b1 + x * a1);
   } else {
      isect->y = (int) (b2 + x * a2);
   }
   isect->x = (int)x;
#else
   long a1,b1;
   long a2,b2;
   long x;

   if (f1->x == t1->x) {
      a1 = 0;
      b1 = 1024 * f1->y;
   } else {
      a1 = 1024 * (f1->y - t1->y) / (f1->x - t1->x);
      b1 = 1024 * f1->y - a1 * f1->x;
   }

   if (f2->x == t2->x) {
      a2 = 0;
      b2 = 1024 * f2->y;
   } else {
      a2 = 1024 * (f2->y - t2->y) / (f2->x - t2->x);
      b2 = 1024 * f2->y - a2 * f2->x;
   }

   if (a1 == a2) return 0;

   x = (b1 - b2) / (a2 - a1);
   if (abs(a1) < abs(a2)) {
      isect->y = (b1 + x * a1) / 1024;
   } else {
      isect->y = (b2 + x * a2) / 1024;
   }
   isect->x = x;
#endif

   return 1;
}



int roadmap_math_compare_points (const RoadMapPosition *p1,
                                 const RoadMapPosition *p2) {

   if ((p1->longitude == p2->longitude) &&
         (p1->latitude == p2->latitude))
      return 0;

   if (p1->longitude < p2->longitude) return -1;
   if (p2->longitude > p1->longitude) return 1;

   if (p1->latitude < p2->latitude) {
      return -1;
   } else {
      return 1;
   }
}


int roadmap_math_delta_direction (int direction1, int direction2) {

    int delta = direction2 - direction1;

    while (delta > 180)  delta -= 360;
    while (delta < -180) delta += 360;

    if (delta < 0) delta = 0 - delta;

    while (delta >= 360) delta -= 360;

    return delta;
}


#if NEEDED
void roadmap_math_set_context (RoadMapPosition *position, unsigned int zoom) {

   RoadMapContext->center = *position;

   if (zoom < MAX_ZOOM_OUT) {
      RoadMapContext->zoom = (unsigned short) zoom;
   }

   if (RoadMapContext->zoom < MIN_ZOOM_IN) {
      RoadMapContext->zoom = MIN_ZOOM_IN;
   }

   roadmap_math_compute_scale ();
}
#endif


void roadmap_math_get_context
        (RoadMapPosition *position,
         unsigned int *zoom, RoadMapGuiPoint *lowerright) {

   if (position) *position = RoadMapContext->center;
   if (zoom) *zoom = RoadMapContext->zoom;
   if (lowerright) {
      lowerright->x = RoadMapContext->width;
      lowerright->y = RoadMapContext->height;
   }
}

int  roadmap_math_get_zoom (void) {

   return (int) RoadMapContext->zoom;
}


static int powers_of_10[] = { 1, 10, 100, 1000, 10000, 100000, 1000000 };

/**
 * @brief convert
 *  from a string that looks like a floating point number
 *  to an integer, using just the specified number of fractional digits
 *  (e.g. 40.1 degrees --> 40100000 millionths of a degree)
 * @param f
 * @param fracdigits
 * @return
 */
int roadmap_math_from_floatstring(const char *f, int fracdigits)
{
    int sign = 1;
    int whole = 0;
    int frac = 0;
    int scale = powers_of_10[fracdigits];

    if (*f == '-') {
        sign = -1;
        f++;
    } else if (*f == '+') {
        f++;
    }

    while (isdigit(*f)) {
        whole *= 10;
        whole += *f++ - '0';
    }

    if (*f == '.') {
        f++;
        fracdigits += 1;  /* so we can do rounding */
        while (isdigit(*f) && fracdigits-- > 0) {
            frac *= 10;
            frac += *f++ - '0';
        }
    }

    while (fracdigits-- > 0) {
        frac *= 10;
    }

    /* rounding */
    frac = (frac + 5) / 10;

    return sign * (whole * scale + frac);
}

char *roadmap_math_to_floatstring(char *buf, int value, int fracdigits)
{
    static char result[32];

    int scale = powers_of_10[fracdigits];
    int sign = (value == 0) ? 1 : value/abs(value);
    char *hyphen = (sign < 0) ? "-":"";

    if (buf == NULL)  buf = result;  /* supply buffer if non provided */

    sprintf(buf, "%s%d.%0*d",
        hyphen, abs(value) / scale, fracdigits, abs(value) % scale);

    return buf;
}

#if defined(HAVE_TRIP_PLUGIN) || defined(HAVE_NAVIGATE_PLUGIN)
/**
 * @brief
 * @param position
 * @param from_pos
 * @param to_pos
 * @param first_shape
 * @param last_shape
 * @param shape_itr
 * @param total_length
 * @return
 */
int roadmap_math_calc_line_length (const RoadMapPosition *position,
                                   const RoadMapPosition *from_pos,
                                   const RoadMapPosition *to_pos,
                                   int                    first_shape,
                                   int                    last_shape,
                                   RoadMapShapeItr        shape_itr,
                                   int                   *total_length)
{
   RoadMapPosition from;
   RoadMapPosition to;
   RoadMapPosition intersection;
   int current_length = 0;
   int length_result = 0;
   int smallest_distance = 0x7fffffff;
   int distance;
   int i;

   if (first_shape <= -1) {
      
      from = *from_pos;
      to = *to_pos;
   } else {

      from = *from_pos;
      to   = *from_pos;

      for (i = first_shape; i <= last_shape; i++) {

         shape_itr (i, &to);

         distance =
            roadmap_math_get_distance_from_segment
            (position, &from, &to, &intersection, NULL);

         if (distance < smallest_distance) {
            smallest_distance = distance;
            length_result = current_length +
               roadmap_math_distance (&from, &intersection);
         }

         current_length += roadmap_math_distance (&from, &to);
         from = to;
      }

      to = *to_pos;
   }

   distance =
      roadmap_math_get_distance_from_segment
      (position, &from, &to, &intersection, NULL);

   if (distance < smallest_distance) {

      length_result = current_length +
                        roadmap_math_distance (&from, &intersection);
   }

   current_length += roadmap_math_distance (&from, &to);

   if (total_length) *total_length = current_length;

   return length_result;
}
#endif




#if 0

/* this code came from http://mappinghacks.com/code/PolyLineReduction/ .
 * i converted it from float to int, and it is basically untested.
 */
typedef struct STACK_RECORD {
    int nAnchorIndex, nFloaterIndex;
    struct STACK_RECORD *precPrev;
} STACK_RECORD;

STACK_RECORD *m_pStack = 0;

static void
StackPush(int nAnchorIndex, int nFloaterIndex)
{
    STACK_RECORD *precPrev = m_pStack;
    m_pStack = (STACK_RECORD *) malloc(sizeof(STACK_RECORD));
    m_pStack->nAnchorIndex = nAnchorIndex;
    m_pStack->nFloaterIndex = nFloaterIndex;
    m_pStack->precPrev = precPrev;
}

static int
StackPop(int *pnAnchorIndex, int *pnFloaterIndex)
{
    STACK_RECORD *precStack = m_pStack;
    if (precStack == 0)
	return 0;
    *pnAnchorIndex = precStack->nAnchorIndex;
    *pnFloaterIndex = precStack->nFloaterIndex;
    m_pStack = precStack->precPrev;
    free(precStack);
    return 1;
}


void
roadmap_math_reduce_points(int *pPointsX, int *pPointsY, int nPointsCount,
	     int *pnUseFlag, int dTolerance)
{
    int nVertexIndex, nAnchorIndex, nFloaterIndex;
    int dSegmentVecLength;
    int dAnchorVecX, dAnchorVecY;
    int dAnchorUnitVecX, dAnchorUnitVecY;
    int dVertexVecLength;
    int dVertexVecX, dVertexVecY;
    int dProjScalar;
    int dVertexDistanceToSegment;
    int dMaxDistThisSegment;
    int nVertexIndexMaxDistance;

    nAnchorIndex = 0;
    nFloaterIndex = nPointsCount - 1;
    StackPush(nAnchorIndex, nFloaterIndex);
    while (StackPop(&nAnchorIndex, &nFloaterIndex)) {

	// initialize line segment
	dAnchorVecX = pPointsX[nFloaterIndex] - pPointsX[nAnchorIndex];
	dAnchorVecY = pPointsY[nFloaterIndex] - pPointsY[nAnchorIndex];
	dSegmentVecLength = sqrt(dAnchorVecX * dAnchorVecX
				 + dAnchorVecY * dAnchorVecY);
	if (dSegmentVecLength == 0) {
	    pnUseFlag[nAnchorIndex] = 1;
	    pnUseFlag[nFloaterIndex] = 1;
	    continue;
	}
	dAnchorUnitVecX = dAnchorVecX / dSegmentVecLength;
	dAnchorUnitVecY = dAnchorVecY / dSegmentVecLength;

	// inner loop:
	dMaxDistThisSegment = 0.0;
	nVertexIndexMaxDistance = nAnchorIndex + 1;
	for (nVertexIndex = nAnchorIndex + 1; nVertexIndex < nFloaterIndex;
	     nVertexIndex++) {

	    // compare to anchor
	    dVertexVecX = pPointsX[nVertexIndex] - pPointsX[nAnchorIndex];
	    dVertexVecY = pPointsY[nVertexIndex] - pPointsY[nAnchorIndex];
	    dVertexVecLength = sqrt(dVertexVecX * dVertexVecX
				    + dVertexVecY * dVertexVecY);
	    // dot product:
	    dProjScalar =
		dVertexVecX * dAnchorUnitVecX + dVertexVecY * dAnchorUnitVecY;
	    if (dProjScalar < 0.0) {
		dVertexDistanceToSegment = dVertexVecLength;
	    } else {
		// compare to floater
		dVertexVecX = pPointsX[nVertexIndex] - pPointsX[nFloaterIndex];
		dVertexVecY = pPointsY[nVertexIndex] - pPointsY[nFloaterIndex];
		dVertexVecLength = sqrt(dVertexVecX * dVertexVecX
					+ dVertexVecY * dVertexVecY);
		// dot product:
		dProjScalar =
		    dVertexVecX * (-dAnchorUnitVecX) +
		    dVertexVecY * (-dAnchorUnitVecY);
		if (dProjScalar < 0.0) {
		    dVertexDistanceToSegment = dVertexVecLength;
		} else {
		    // perpendicular distance to line
		    dVertexDistanceToSegment =
			sqrt(
			    abs(dVertexVecLength * dVertexVecLength -
				    dProjScalar * dProjScalar)
			    );
		}
	    }
	    if (dMaxDistThisSegment < dVertexDistanceToSegment) {
		dMaxDistThisSegment = dVertexDistanceToSegment;
		nVertexIndexMaxDistance = nVertexIndex;
	    }
	}
	if (dMaxDistThisSegment <= dTolerance) {	//use line segment
	    pnUseFlag[nAnchorIndex] = 1;
	    pnUseFlag[nFloaterIndex] = 1;
	} else {
	    StackPush(nAnchorIndex, nVertexIndexMaxDistance);
	    StackPush(nVertexIndexMaxDistance, nFloaterIndex);
	}
    }
}
#endif
