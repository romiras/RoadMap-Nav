/* roadmap_street.h - RoadMap streets operations and attributes.
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

#ifndef _ROADMAP_STREET__H_
#define _ROADMAP_STREET__H_

#include "roadmap_types.h"
#include "roadmap_dbread.h"

typedef struct {

   int street;
   int first;
   int count;

} RoadMapBlocks;

typedef struct {

   int min;
   int max;

} RoadMapStreetRange;


/* The function roadmap_street_blocks either returns a positive count
 * of matching blocks on success, or else an error code (null or negative).
 * The values below are the possible error codes:
 */
#define ROADMAP_STREET_NOADDRESS    0
#define ROADMAP_STREET_NOCITY      -1
#define ROADMAP_STREET_NOSTREET    -2

int roadmap_street_blocks_by_city
       (char *street_name, char *city_name, RoadMapBlocks *blocks, int size);
int roadmap_street_blocks_by_zip
       (char *street_name, int zip, RoadMapBlocks *blocks, int size);

int roadmap_street_get_ranges (RoadMapBlocks *blocks,
                               int count,
                               RoadMapStreetRange *ranges);

int roadmap_street_get_position (RoadMapBlocks *blocks,
                                 int number,
                                 RoadMapPosition *position);

int roadmap_street_get_distance (RoadMapPosition *position, int line);

int roadmap_street_get_closest
       (RoadMapPosition *position, int count, int *categories, int *distance);

char *roadmap_street_get_name_from_line (int line);

extern roadmap_db_handler RoadMapStreetHandler;
extern roadmap_db_handler RoadMapZipHandler;
extern roadmap_db_handler RoadMapRangeHandler;

#endif // _ROADMAP_STREET__H_
