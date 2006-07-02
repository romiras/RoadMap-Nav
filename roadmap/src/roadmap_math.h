/* roadmap_math.h - Manage the little math required to place points on a map.
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
 */

#ifndef INCLUDED__ROADMAP_MATH__H
#define INCLUDED__ROADMAP_MATH__H


#include "roadmap_types.h"
#include "roadmap_gui.h"

enum { MATH_ZOOM_RESET = -1,
       MATH_ZOOM_NO_RESET = 0
};

enum { MATH_DIST_ACTUAL = 0,
       MATH_DIST_SQUARED = 1
};

void roadmap_math_initialize   (void);

void roadmap_math_use_metric   (void);
void roadmap_math_use_imperial (void);

void roadmap_math_restore_zoom (void);
void roadmap_math_zoom_in      (void);
void roadmap_math_zoom_out     (void);
void roadmap_math_zoom_reset   (void);

void roadmap_math_set_center      (RoadMapPosition *position);
void roadmap_math_set_size        (int width, int height);
int  roadmap_math_set_orientation (int direction);
int  roadmap_math_get_orientation (void);
void roadmap_math_set_horizon     (int horizon);

void roadmap_math_set_focus     (const RoadMapArea *focus);
void roadmap_math_release_focus (void);

int  roadmap_math_declutter (int level);
int  roadmap_math_thickness (int base, int declutter, int use_multiple_pens);

/* These 2 functions return: 0 (not visible), 1 (fully visible) or
 * -1 (partially visible).
 */
int  roadmap_math_is_visible       (const RoadMapArea *area);
int  roadmap_math_line_is_visible  (const RoadMapPosition *point1,
                                    const RoadMapPosition *point2);
int  roadmap_math_point_is_visible (const RoadMapPosition *point);

int roadmap_math_get_visible_coordinates (const RoadMapPosition *from,
                                          const RoadMapPosition *to,
                                          RoadMapGuiPoint *point0,
                                          RoadMapGuiPoint *point1);


void roadmap_math_coordinate  (const RoadMapPosition *position,
                               RoadMapGuiPoint *point);
void roadmap_math_to_position (const RoadMapGuiPoint *point,
                               RoadMapPosition *position,
                               int projected);
void roadmap_math_unproject   (RoadMapGuiPoint *point);

void roadmap_math_rotate_coordinates (int count, RoadMapGuiPoint *points);

void roadmap_math_rotate_point (RoadMapGuiPoint *points,
                                RoadMapGuiPoint *center, int angle);

void roadmap_math_rotate_object
         (int count, RoadMapGuiPoint *points,
          RoadMapGuiPoint *center, int orientation);

int  roadmap_math_azymuth
        (const RoadMapPosition *point1, const RoadMapPosition *point2);
int roadmap_math_angle
       (const RoadMapGuiPoint *point1, const RoadMapGuiPoint *point2);
long roadmap_math_screen_distance
       (const RoadMapGuiPoint *point1, const RoadMapGuiPoint *point2,
       int squared);

char *roadmap_math_distance_unit (void);
char *roadmap_math_trip_unit     (void);
char *roadmap_math_speed_unit    (void);

int  roadmap_math_distance
        (const RoadMapPosition *position1, const RoadMapPosition *position2);

int  roadmap_math_distance_convert (const char *string, int *was_explicit);
int  roadmap_math_to_trip_distance (int distance);
int  roadmap_math_to_trip_distance_tenths (int distance);

int  roadmap_math_to_speed_unit (int knots);


int  roadmap_math_to_current_unit (int value, const char *unit);
int  roadmap_math_to_cm (int value);

int  roadmap_math_get_distance_from_segment
        (const RoadMapPosition *position,
         const RoadMapPosition *position1,
         const RoadMapPosition *position2,
               RoadMapPosition *intersection,
                           int *which);

int  roadmap_math_intersection (RoadMapPosition *from1,
                                RoadMapPosition *to1,
                                RoadMapPosition *from2,
                                RoadMapPosition *to2,
                                RoadMapPosition *intersection);

int roadmap_math_screen_intersect (RoadMapGuiPoint *f1, RoadMapGuiPoint *t1,
			   RoadMapGuiPoint *f2, RoadMapGuiPoint *t2,
			   RoadMapGuiPoint *isect);

void roadmap_math_screen_edges (RoadMapArea *area);

unsigned int roadmap_math_street_address (const char *image, int length);

int  roadmap_math_compare_points (const RoadMapPosition *p1,
                                  const RoadMapPosition *p2);

int  roadmap_math_delta_direction (int direction1, int direction2);

void roadmap_math_set_context (RoadMapPosition *position, int zoom);

void roadmap_math_get_context (RoadMapPosition *position, int *zoom);

#endif // INCLUDED__ROADMAP_MATH__H

