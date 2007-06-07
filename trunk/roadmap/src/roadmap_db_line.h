/* roadmap_db_line.h - the format of the address line table used by RoadMap.
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
 *   The RoadMap lines are described by the following table:
 *
 *   line/data       The ID of the line and its from and to points.
 *                   The lines are sorted by square.
 *   line/bysquare1  An index of layers per square (points to line/bylayer1).
 *   line/bylayer1   An indirect index, from layers to lines.
 *   line/bysquare2  A given line may have one end in a different square:
 *                   this 2nd index covers this (points to line/bylayer2).
 *   line/bylayer2   An indirect index, from layers to lines.
 *   line/index2     This index table points to line/data and is pointed
 *                   to from line/layer2. This is used to build lists.
 *
 *   The logic here is that line/bysquare1 points to a sublist of layers in
 *   line/bylayer1. each item in this sublist points to a sublist of lines.
 *
 *   The line/bysquare2 logic is slightly different, as there is one
 *   additional indirection: items in line/bylayer2 point to sublists in
 *   line/index2, and an item in line/index2 point to a line. This is done
 *   so because the lines are not sorted according to this 2nd index, only
 *   according to the first one.
 */

#ifndef INCLUDED__ROADMAP_DB_LINE__H
#define INCLUDED__ROADMAP_DB_LINE__H

#include "roadmap_types.h"

typedef struct {  /* table line/data */

   int from;
   int to;

} RoadMapLine;

typedef struct { /* tables line/bysquare1 and line/bysquare2 */

   int first; /* First layer item in line/layer. */
   int count; /* Number of layers + 1, the additional layer defines the end. */

} RoadMapLineBySquare;

typedef struct {

   int line;
   unsigned char layer;

   RoadMapArea area;
} RoadMapLongLine;

/* Table line/bylayer1 is an array of int. */

/* Table line/bylayer2 is an array of int. */

/* Table line/index2 is an array of int. */

#endif // INCLUDED__ROADMAP_DB_LINE__H

