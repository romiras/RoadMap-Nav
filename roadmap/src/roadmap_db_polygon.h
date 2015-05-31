/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright 2010 Danny Backx
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
 * @brief roadmap_polygon.h - the format of the polygon table used by RoadMap.
 *
 * The RoadMap polygons are described by the following tables:
 *
 * polygon/head     for each polygon, the category and list of lines.
 * polygon/points   the list of points defining the polygons border.
 */

#ifndef INCLUDED__ROADMAP_DB_POLYGON__H
#define INCLUDED__ROADMAP_DB_POLYGON__H

#include "roadmap_types.h"

typedef struct {  /* table polygon/head */

   unsigned short first;  /* low 16 bits */
   unsigned short count;  /* low 16 bits */

   RoadMapString name;
   unsigned char  cfcc;
   unsigned char hi_f_c;  /* high 4 bits of "first" and "count" values */

   /* TBD: replace these with a RoadMapArea (not compatible!). */
   int   north;
   int   west;
   int   east;
   int   south;

} RoadMapPolygon1;

typedef struct {  /* table polygon/head */

   unsigned int first;  /* 32 bits */
   unsigned int count;  /* 32 bits */

   RoadMapString name;
   unsigned char cfcc;
   unsigned char filler;

   RoadMapArea area;

} RoadMapPolygon;


/* Table polygons/lines is an array of int. */
typedef int RoadMapPolygonLine;

// this isn't quite the true maximum, but if we hit this
// limit, it's really likely something's wrong.
#define MAX_POLYGON_LINE_COUNT  0x3fffffff

#endif // INCLUDED__ROADMAP_DB_POLYGON__H

