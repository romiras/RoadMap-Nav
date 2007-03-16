/* navigate_graph.h - generic navigate functions
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
 */

#ifndef _NAVIGATE_GRAPH_H_
#define _NAVIGATE_GRAPH_H_

#include "navigate_main.h"

#define GRAPH_IGNORE_TURNS 1

#define NEW_ROUTE 1
#define RECALC_ROUTE 2

int navigate_reload_data (void);
int navigate_load_data   (void);

int navigate_get_route_segments (PluginLine *from_line,
                                 int from_point,
                                 PluginLine *to_line,
                                 int to_point,
                                 NavigateSegment *segments,
                                 int *size,
                                 int *result);

#endif /* _NAVIGATE_GRAPH_H_ */

