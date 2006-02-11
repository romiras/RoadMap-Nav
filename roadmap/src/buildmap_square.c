/* buildmap_square.c - Divide the area in more manageable squares.
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
 *   void  buildmap_square_initialize
 *            (int minlongitude, int maxlongitude,
 *             int minlatitude,  int maxlatitude);
 *
 *   int   buildmap_square_add  (int longitude, int latitude);
 *
 *   short buildmap_square_get_sorted (int squareid);
 *   int   buildmap_square_get_count (void);
 *   void  buildmap_square_get_reference_sorted
 *            (int square, int *longitude, int *latitude);
 *
 * These functions are used to build a table of lines from
 * the Tiger maps. The objective is double: (1) reduce the size of
 * the Tiger data by sharing all duplicated information and
 * (2) produce the index data to serve as the basis for a fast
 * search mechanism for streets in roadmap.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "roadmap_db_square.h"

#include "buildmap.h"
#include "buildmap_line.h"
#include "buildmap_square.h"


typedef struct {
   int minlongitude;
   int maxlongitude;
   int minlatitude;
   int maxlatitude;
   int sorted;
   int count;
} BuildMapSquare;


static int SquareCount = 0;
static int SquareTableSize = 0;
static BuildMapSquare *Square = NULL;

static int *SortedSquare = NULL;

static int SortMaxLongitude;
static int SortMinLongitude;
static int SortMaxLatitude;
static int SortMinLatitude;

static int SortStepLongitude;
static int SortStepLatitude;

static int SortCountLongitude;
static int SortCountLatitude;


static void buildmap_square_register (void);


static int buildmap_square_search (int longitude, int latitude) {

   int x = (longitude - SortMinLongitude) / SortStepLongitude;
   int y = (latitude  - SortMinLatitude)  / SortStepLatitude;

   int index;


   if (x >= SortCountLongitude) {
      x = SortCountLongitude - 1;
   }
   if (y >= SortCountLatitude) {
      y = SortCountLatitude - 1;
   }

   index = (x * SortCountLatitude) + y;

   /* The computation above may have rouding errors: adjust. */

   while (Square[index].maxlongitude < longitude) {
      if (index == SquareTableSize - 1) break;
      index++;
   }
   while (Square[index].minlongitude > longitude) {
      if (index == 0) break;
      index--;
   }
   while (Square[index].maxlatitude < latitude) {
      if (index == SquareTableSize - 1) break;
      index++;
   }
   while (Square[index].minlatitude > latitude) {
      if (index == 0) break;
      index--;
   }

   if ((Square[index].minlongitude > longitude) ||
       (Square[index].maxlongitude < longitude) ||
       (Square[index].minlatitude > latitude) ||
       (Square[index].maxlatitude < latitude)) {

      buildmap_fatal (0, "point %d (%dx%d) does not fit in any square",
                         index,
                         longitude,
                         latitude);
   }

   return index;
}


void buildmap_square_initialize
        (int minlongitude, int maxlongitude,
         int minlatitude,  int maxlatitude) {

   int i;
   int k;
   int latitude;
   int size_latitude;
   int size_longitude;
   int count_latitude;
   int count_longitude;

   SquareCount = 0;

   count_longitude = (maxlongitude - minlongitude + 0x7fef) / 0x7ff0;
   count_latitude  = (maxlatitude - minlatitude + 0x7fef) / 0x7ff0;

   SquareTableSize = count_longitude * count_latitude;
   SquareCount = SquareTableSize;

   Square = calloc (SquareTableSize, sizeof(BuildMapSquare));

   if (Square == NULL) {
      buildmap_fatal (0, "no more memory");
   }

   size_longitude = (maxlongitude - minlongitude) / count_longitude;
   size_latitude = (maxlatitude - minlatitude) / count_latitude;

   SortMaxLongitude = maxlongitude;
   SortMinLongitude = minlongitude;
   SortMaxLatitude  = maxlatitude;
   SortMinLatitude  = minlatitude;

   SortStepLongitude = size_longitude;
   SortStepLatitude  = size_latitude;

   SortCountLongitude = count_longitude;
   SortCountLatitude  = count_latitude;

   /* "Format" the square table as a sorted list. */

   for (i = 0; i < count_longitude; i++) {

      int start = i * count_latitude;
      int end   = (i+1) * count_latitude;

      latitude = minlatitude;

      for (k = start; k < end; k++) {

         Square[k].minlongitude = minlongitude;
         Square[k].minlatitude  = latitude;

         latitude  += size_latitude;

         Square[k].maxlongitude = minlongitude + size_longitude;
         Square[k].maxlatitude  = latitude;

         Square[k].count = 0;
      }

      Square[k-1].maxlatitude = maxlatitude;

      minlongitude += size_longitude;
   }

   /* There might be some rounding error when calculating the last
    * maximum longitude: let use the real value instead.
    */
   for (i = SquareTableSize - count_latitude; i < SquareTableSize; i++) {
      Square[i].maxlongitude = maxlongitude;
   }

   buildmap_square_register();
}


int buildmap_square_add (int longitude, int latitude) {

   int squareid;

   /* Check which square this point fits in. */

   squareid = buildmap_square_search (longitude, latitude);

   Square[squareid].count += 1;

   return squareid;
}



short buildmap_square_get_sorted (int squareid) {

   if (SortedSquare == NULL) {
      buildmap_fatal (0, "squares have not been sorted yet");
   }

   if ((squareid < 0) || (squareid > SquareTableSize)) {
      buildmap_fatal (0, "invalid square index %d", squareid);
   }

   return Square[squareid].sorted;
}


int   buildmap_square_get_count (void) {

   if (SortedSquare == NULL) {
      buildmap_fatal (0, "squares have not been sorted yet");
   }

   return SquareCount;
}


void  buildmap_square_get_reference_sorted
         (int square, int *longitude, int *latitude) {

   if (SortedSquare == NULL) {
      buildmap_fatal (0, "squares have not been sorted yet");
   }

   if ((square < 0) || (square > SquareCount)) {
      buildmap_fatal (0, "invalid square index %d", square);
   }

   square = SortedSquare[square];

   *longitude = Square[square].minlongitude;
   *latitude  = Square[square].minlatitude;
}


void buildmap_square_sort (void) {

   int i;
   int final_count;

   if (SortedSquare != NULL) return; /* Sort was already performed. */

   buildmap_info ("sorting squares...");

   SortedSquare = calloc (SquareCount, sizeof(int));
   if (SortedSquare == NULL) {
      buildmap_fatal (0, "no more memory");
   }

   final_count = 0;

   for (i = 0; i < SquareCount; i++) {
      if (Square[i].count != 0) {
         Square[i].sorted = final_count;
         SortedSquare[final_count] = i;
         final_count += 1;
      }
   }

   SquareCount = final_count;
}


static void buildmap_square_save (void) {

   int i;

   BuildMapSquare *one_square;
   RoadMapGlobal  *db_global;
   RoadMapSquare  *db_square;
   
   buildmap_db *root;
   buildmap_db *table_data;
   buildmap_db *table_global;


   buildmap_info ("saving squares...");

   root = buildmap_db_add_section (NULL, "square");
   if (root == NULL) buildmap_fatal (0, "Can't add a new section");

   table_global = buildmap_db_add_section (root, "global");
   if (table_global == NULL) buildmap_fatal (0, "Can't add a new section");
   buildmap_db_add_data (table_global, 1, sizeof(RoadMapGlobal));

   table_data = buildmap_db_add_section (root, "data");
   if (table_data == NULL) buildmap_fatal (0, "Can't add a new section");
   buildmap_db_add_data (table_data, SquareCount, sizeof(RoadMapSquare));

   db_global = (RoadMapGlobal *) buildmap_db_get_data (table_global);
   db_square = (RoadMapSquare *) buildmap_db_get_data (table_data);


   db_global->edges.east = SortMaxLongitude;
   db_global->edges.north  = SortMaxLatitude;

   db_global->edges.west = SortMinLongitude;
   db_global->edges.south  = SortMinLatitude;

   db_global->step_longitude = SortStepLongitude;
   db_global->step_latitude  = SortStepLatitude;

   db_global->count_longitude = SortCountLongitude;
   db_global->count_latitude  = SortCountLatitude;

   db_global->count_squares = SquareCount;

   for (i = 0; i < SquareCount; i++) {

      one_square = Square + SortedSquare[i];

      db_square[i].edges.east = one_square->maxlongitude;
      db_square[i].edges.north  = one_square->maxlatitude;

      db_square[i].edges.west = one_square->minlongitude;
      db_square[i].edges.south  = one_square->minlatitude;

      db_square[i].position      = SortedSquare[i];
      db_square[i].count_points  = one_square->count;
   }
}


static void buildmap_square_summary (void) {

   fprintf (stderr, "-- square table statistics: %d squares, %d bytes used\n",
                    SquareCount, (int)(SquareCount * sizeof(RoadMapSquare)));
}


static void buildmap_square_reset (void) {

   SquareCount = 0;
   SquareTableSize = 0;

   free (Square);
   Square = NULL;

   free (SortedSquare);
   SortedSquare = NULL;

   SortMaxLongitude = 0;
   SortMinLongitude = 0;
   SortMaxLatitude = 0;
   SortMinLatitude = 0;

   SortStepLongitude = 0;
   SortStepLatitude = 0;

   SortCountLongitude = 0;
   SortCountLatitude = 0;
}


static buildmap_db_module BuildMapSquareModule = {
   "square",
   buildmap_square_sort,
   buildmap_square_save,
   buildmap_square_summary,
   buildmap_square_reset
}; 
   

static void buildmap_square_register (void) {
   buildmap_db_register (&BuildMapSquareModule);
}

