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

#ifndef _ROADMAP_MATH__H_
#define _ROADMAP_MATH__H_


#include "roadmap_types.h"
#include "roadmap_gui.h"


void roadmap_math_initialize   (void);

void roadmap_math_use_metric   (void);
void roadmap_math_use_imperial (void);
void roadmap_math_zoom_in      (void);
void roadmap_math_zoom_out     (void);
void roadmap_math_zoom_reset   (void);

void roadmap_math_set_center      (RoadMapPosition *position);
void roadmap_math_set_size        (int width, int height);
int  roadmap_math_set_orientation (int direction);

void roadmap_math_set_focus     (int west, int east, int north, int south);
void roadmap_math_release_focus ();

int  roadmap_math_declutter (int level);
int  roadmap_math_thickness (int base);

/* These 2 functions return: 0 (not visible), 1 (fully visible) or
 * -1 (partially visible).
 */
int  roadmap_math_is_visible       (int west, int east, int north, int south);
int  roadmap_math_line_is_visible  (const RoadMapPosition *point1,
                                    const RoadMapPosition *point2);
int  roadmap_math_point_is_visible (const RoadMapPosition *point);

void roadmap_math_coordinate  (const RoadMapPosition *position,
                               RoadMapGuiPoint *point);
void roadmap_math_to_position (const RoadMapGuiPoint *point,
                               RoadMapPosition *position);

void roadmap_math_rotate_coordinates (int count, RoadMapGuiPoint *points);

void roadmap_math_rotate_object
         (int count, RoadMapGuiPoint *points,
          RoadMapGuiPoint *center, int orientation);

int  roadmap_math_azymuth (RoadMapPosition *point1, RoadMapPosition *point2);

char *roadmap_math_distance_unit (void);
char *roadmap_math_trip_unit     (void);
char *roadmap_math_speed_unit    (void);

int  roadmap_math_distance
        (RoadMapPosition *position1, RoadMapPosition *position2);

int roadmap_math_to_trip_distance (int distance);

int roadmap_math_to_speed_unit (int knots);

int  roadmap_math_get_distance_from_segment
        (RoadMapPosition *position,
         RoadMapPosition *position1, RoadMapPosition *position2);

void roadmap_math_screen_edges
        (int *west, int *east, int *north, int *south);

int  roadmap_math_street_address (char *image, int length);

#endif // _ROADMAP_MATH__H_