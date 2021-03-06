/* roadmap_start.h - The interface for the RoadMap main module.
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

#ifndef INCLUDE__ROADMAP_START__H
#define INCLUDE__ROADMAP_START__H

#include "roadmap_factory.h"

/* The two following functions are used to freeze all RoadMap function
 * in cases when the context does not allow for RoadMap to function in
 * a normal fashion. The single example is when downloading maps:
 * RoadMap should not try access the maps, as there is at least one
 * map file that is incomplete.
 * There ought to be a better way, such using a temporary file name...
 */
void roadmap_start_freeze   (void);
void roadmap_start_unfreeze (void);

void roadmap_start      (int argc, char **argv);
void roadmap_start_exit (void);
const RoadMapAction *roadmap_start_find_action (const char *name);

const char *roadmap_start_get_title (const char *name);

int roadmap_start_map_active(void);
int roadmap_start_return_to_map(void);
void roadmap_start_do_callback(RoadMapCallback cb);

enum {
    REPAINT_NOT_NEEDED = 0,
    REPAINT_MAYBE,
    REPAINT_NOW
};
int roadmap_start_repaint_scheduled(void);

void roadmap_start_request_repaint_map (int interrupt);
void roadmap_start_request_repaint (int screen, int priority);
#define ROADMAP_MAP 1
#define ROADMAP_GPS 2

#endif /* INCLUDE__ROADMAP_START__H */

