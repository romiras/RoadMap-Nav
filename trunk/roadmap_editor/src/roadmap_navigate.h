/* roadmap_navigate.h - basic navigation engine.
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

#ifndef INCLUDE__ROADMAP_NAVIGATE__H
#define INCLUDE__ROADMAP_NAVIGATE__H

#include "roadmap_gps.h"
#include "roadmap_fuzzy.h"
#include "roadmap_plugin.h"

typedef struct {
    RoadMapFuzzy direction;
    RoadMapFuzzy distance;
    RoadMapFuzzy connected;
} RoadMapDebug;

typedef struct {

    int valid;
    PluginStreet street;

    int azymuth;
    int line_direction;
    int opposite_street_direction;

    RoadMapFuzzy fuzzyfied;

    PluginLine intersection;

    RoadMapPosition entry;

    RoadMapDebug debug;

} RoadMapTracking;

#define ROADMAP_TRACKING_NULL  {0, PLUGIN_STREET_NULL, 0, 0, 0, 0, PLUGIN_LINE_NULL, {0, 0}, {0, 0, 0}};

typedef struct {
   void (*update) (RoadMapPosition *position, PluginLine *current);
   void (*get_next_line)
          (PluginLine *current, int direction, PluginLine *next);

} RoadMapNavigateRouteCB;

void roadmap_navigate_disable (void);
void roadmap_navigate_enable  (void);

int roadmap_navigate_retrieve_line
        (const RoadMapPosition *position, int accuracy, PluginLine *line,
         int *distance, int type);

void roadmap_navigate_locate (const RoadMapGpsPosition *gps_position);

void roadmap_navigate_initialize (void);

int roadmap_navigate_fuzzify
                (RoadMapTracking *tracked,
                 RoadMapTracking *previous_street,
                 RoadMapNeighbour *previous_line,
                 RoadMapNeighbour *line, int direction);

int roadmap_navigate_get_current (RoadMapPosition *position,
                                   PluginLine *line,
                                   int *direction);

void roadmap_navigate_route (RoadMapNavigateRouteCB callbacks);
void roadmap_navigate_end_route (RoadMapNavigateRouteCB callbacks);

#endif // INCLUDE__ROADMAP_NAVIGATE__H
