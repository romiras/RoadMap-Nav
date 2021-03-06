/* buildmap_point.c - Build a table of all points referenced in lines.
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
 *   void buildmap_point_initialize (void);
 *   int  buildmap_point_add        (int longitude, int latitude);
 *
 *   void buildmap_point_sort (void);
 *   int  buildmap_point_get_square (int pointid);
 *   int  buildmap_point_get_longitude (int pointid);
 *   int  buildmap_point_get_latitude  (int pointid);
 *   int  buildmap_point_get_sorted (int pointid);
 *   int  buildmap_point_get_longitude_sorted (int point);
 *   int  buildmap_point_get_latitude_sorted  (int point);
 *   int  buildmap_point_get_square_sorted (int pointid);
 *   void buildmap_point_save    (void);
 *   void buildmap_point_summary (void);
 *   void buildmap_point_reset   (void);
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

#include "roadmap_db_point.h"

#include "roadmap_hash.h"

#include "buildmap.h"
#include "buildmap_square.h"


typedef struct {
   int longitude;
   int latitude;
   int db_id;
   int sorted;
   int squareid;           /* Before sorting. */
   unsigned short square;  /* After sorting. */
} BuildMapPoint;


static int PointCount = 0;
static BuildMapPoint *Point[BUILDMAP_BLOCK] = {NULL};

static RoadMapHash *PointByPosition = NULL;
static RoadMapHash *PointById = NULL;

static int *SortedPoint = NULL;

static int SortMaxLongitude = -0x7fffffff;
static int SortMinLongitude =  0x7fffffff;
static int SortMaxLatitude  = -0x7fffffff;
static int SortMinLatitude  =  0x7fffffff;


void buildmap_point_initialize (void) {

   PointById = roadmap_hash_new ("PointById", BUILDMAP_BLOCK);

   PointByPosition =
      roadmap_hash_new ("PointByPosition", BUILDMAP_BLOCK);

   Point[0] = calloc (BUILDMAP_BLOCK, sizeof(BuildMapPoint));
   if (Point[0] == NULL) {
      buildmap_fatal (0, "no more memory");
   }

   PointCount = 0;

   SortMaxLongitude = -0x7fffffff;
   SortMinLongitude =  0x7fffffff;
   SortMaxLatitude  = -0x7fffffff;
   SortMinLatitude  =  0x7fffffff;
}


int buildmap_point_add (int longitude, int latitude, int db_id) {

   int i;
   int block;
   int offset;
   BuildMapPoint *this_point;


   /* First check if the point is already known. */

   for (i = roadmap_hash_get_first (PointByPosition, longitude);
        i >= 0;
        i = roadmap_hash_get_next (PointByPosition, i)) {

      this_point = Point[i / BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);

      if ((this_point->latitude == latitude) &&
          (this_point->longitude == longitude)) {
          
         return i;
      }
   }


   /* This is a new point: create a new entry. */

   block = PointCount / BUILDMAP_BLOCK;
   offset = PointCount % BUILDMAP_BLOCK;

   if (Point[block] == NULL) {

      /* We need to add a new block to the table. */

      Point[block] = calloc (BUILDMAP_BLOCK, sizeof(BuildMapPoint));
      if (Point[block] == NULL) {
         buildmap_fatal (0, "no more memory");
      }

      roadmap_hash_resize (PointByPosition, (block+1) * BUILDMAP_BLOCK);
      roadmap_hash_resize (PointById, (block+1) * BUILDMAP_BLOCK);
   }

   roadmap_hash_add (PointByPosition, longitude, PointCount);
   roadmap_hash_add (PointById, db_id, PointCount);

   this_point = Point[block] + offset;

   this_point->longitude = longitude;
   this_point->latitude  = latitude;
   this_point->db_id     = db_id;
   this_point->sorted    = -1;
   this_point->square    = -1;


   /* Compute the geographic limits of the area. This will be used later
    * to compute the list of squares.
    */

   if (longitude < SortMinLongitude) {
      SortMinLongitude = longitude;
   }
   if (longitude > SortMaxLongitude) {
      SortMaxLongitude = longitude;
   }

   if (latitude < SortMinLatitude) {
      SortMinLatitude = latitude;
   }
   if (latitude > SortMaxLatitude) {
      SortMaxLatitude = latitude;
   }

   return PointCount++;
}


static BuildMapPoint *buildmap_point_get (int pointid) {

   BuildMapPoint *this_point;

   if ((pointid < 0) || (pointid > PointCount)) {
      buildmap_fatal (0, "invalid point index %d", pointid);
   }

   this_point = Point[pointid/BUILDMAP_BLOCK] + (pointid % BUILDMAP_BLOCK);

   return this_point;
}


int buildmap_point_get_square (int pointid) {

   return buildmap_point_get(pointid)->square;
}


int buildmap_point_get_longitude (int pointid) {

   return buildmap_point_get(pointid)->longitude;
}


int buildmap_point_get_latitude  (int pointid) {

   return buildmap_point_get(pointid)->latitude;
}


int buildmap_point_get_sorted (int pointid) {

   int p_id;
   int square;

   if (SortedPoint == NULL) {
      buildmap_fatal (0, "points have not been sorted yet");
   }

   p_id = buildmap_point_get(pointid)->sorted;
   square = buildmap_point_get(pointid)->square;
   p_id -= buildmap_square_first_point_sorted (square);
   if (p_id < 0) {
      buildmap_fatal (0, "invalid point index %d", pointid);
   }
   return (square << 16) | (p_id & 0xffff);
}


int buildmap_point_get_square_sorted (int point) {

   int square;
   int p_id;

   if (SortedPoint == NULL) {
      buildmap_fatal (0, "points have not been sorted yet");
   }

   square = (point >> 16) & 0xffff;
   p_id = point & 0xffff;
   p_id += buildmap_square_first_point_sorted (square);

   if ((p_id < 0) || (p_id > PointCount)) {
      buildmap_fatal (0, "invalid point index %x", point);
   }

   if (square != buildmap_point_get(SortedPoint[p_id])->square) {
      buildmap_fatal (0, "invalid point index %x", point);
   }

   return square;
}


int  buildmap_point_get_longitude_sorted (int point) {

   int square;

   if (SortedPoint == NULL) {
      buildmap_fatal (0, "points have not been sorted yet");
   }

   square = (point >> 16) & 0xffff;
   point = point & 0xffff;
   point += buildmap_square_first_point_sorted (square);

   if ((point < 0) || (point > PointCount)) {
      buildmap_fatal (0, "invalid point index %d", point);
   }

   return buildmap_point_get(SortedPoint[point])->longitude;
}


int  buildmap_point_get_latitude_sorted  (int point) {

   int square;

   if (SortedPoint == NULL) {
      buildmap_fatal (0, "points have not been sorted yet");
   }

   square = (point >> 16) & 0xffff;
   point = point & 0xffff;
   point += buildmap_square_first_point_sorted (square);

   if ((point < 0) || (point > PointCount)) {
      buildmap_fatal (0, "invalid point index %d", point);
   }

   return buildmap_point_get(SortedPoint[point])->latitude;
}


int  buildmap_point_find_sorted (int db_id) {

   int index;
   BuildMapPoint *this_point;

   if (SortedPoint == NULL) {
      buildmap_fatal (0, "points not sorted yet");
   }

   for (index = roadmap_hash_get_first (PointById, db_id);
        index >= 0;
        index = roadmap_hash_get_next (PointById, index)) {

      this_point = Point[index / BUILDMAP_BLOCK] + (index % BUILDMAP_BLOCK);

      if (this_point->db_id == db_id) {
         int p_id = this_point->sorted;
         int square = this_point->square;
         p_id -= buildmap_square_first_point_sorted (square);
         if (p_id < 0) {
            buildmap_fatal (0, "invalid point index %d", index);
         }
         return (square << 16) | (p_id & 0xffff);
      }
   }

   return -1;
}


static int buildmap_point_compare (const void *r1, const void *r2) {

   int result;
   int index1 = *((int *)r1);
   int index2 = *((int *)r2);

   BuildMapPoint *record1;
   BuildMapPoint *record2;

   record1 = Point[index1/BUILDMAP_BLOCK] + (index1 % BUILDMAP_BLOCK);
   record2 = Point[index2/BUILDMAP_BLOCK] + (index2 % BUILDMAP_BLOCK);


   /* group together the points that are in the same square. */

   if (record1->square != record2->square) {
      return record1->square - record2->square;
   }

   /* The two points are inside the same square: compare exact location. */

   result = record1->longitude - record2->longitude;

   if (result != 0) {
      return result;
   }

   return record1->latitude - record2->latitude;
}

void buildmap_point_sort (void) {

   int i;
   int j;
   int last_square = -1;
   BuildMapPoint *record;

   if (SortedPoint != NULL) return; /* Sort was already performed. */

   buildmap_info ("generating squares...");

   buildmap_square_initialize (SortMinLongitude, SortMaxLongitude,
                               SortMinLatitude,  SortMaxLatitude);

   for (i = PointCount - 1; i >= 0; i--) {
      record = Point[i / BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);
      record->squareid =
         buildmap_square_add (record->longitude, record->latitude);
   }

   buildmap_square_sort ();

   for (i = PointCount - 1; i >= 0; i--) {
      record = Point[i / BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);
      record->square = buildmap_square_get_sorted (record->squareid);
   }


   buildmap_info ("sorting points...");

   SortedPoint = malloc (PointCount * sizeof(int));
   if (SortedPoint == NULL) {
      buildmap_fatal (0, "no more memory");
   }

   for (i = 0; i < PointCount; i++) {
      SortedPoint[i] = i;
   }

   qsort (SortedPoint, PointCount, sizeof(int), buildmap_point_compare);

   /* Create by square list and sort translation */

   for (i = 0; i < PointCount; i++) {
      j = SortedPoint[i];
      record = Point[j / BUILDMAP_BLOCK] + (j % BUILDMAP_BLOCK);
      record->sorted = i;

      if (record->square != last_square) {
         if (record->square != last_square + 1) {
            buildmap_fatal (0, "decreasing square order in point table");
         }
         last_square = record->square;
	 buildmap_square_set_first_point_sorted (last_square, i);

      }
   }
}


void buildmap_point_save (void) {

   int i;
   int j;

   int last_square = -1;
   int reference_longitude;
   int reference_latitude;

   BuildMapPoint *one_point;
   RoadMapPoint  *db_points;
  
   buildmap_db *root;
   buildmap_db *table_data;
#ifndef J2MEMAP
   int         *db_ids;
   buildmap_db *table_id;
#endif
 
   buildmap_info ("saving points...");

   root = buildmap_db_add_section (NULL, "point");
   if (root == NULL) buildmap_fatal (0, "Can't add a new section");

   table_data = buildmap_db_add_child
                  (root, "data", PointCount, sizeof(RoadMapPoint));

#ifndef J2MEMAP
   table_id = buildmap_db_add_child
                  (root, "id", PointCount, sizeof(int));

   db_ids      = (int *) buildmap_db_get_data (table_id);
#endif

   db_points   = (RoadMapPoint *) buildmap_db_get_data (table_data);


   for (i = 0; i < PointCount; i++) {

      j = SortedPoint[i];

      one_point = Point[j/BUILDMAP_BLOCK] + (j % BUILDMAP_BLOCK);

      if (one_point->square != last_square) {
         if (one_point->square != last_square + 1) {
            buildmap_fatal (0, "decreasing square order in point table");
         }
         last_square = one_point->square;

         buildmap_square_get_reference_sorted
              (last_square, &reference_longitude, &reference_latitude);
      }

      db_points[i].longitude =
         (unsigned short) (one_point->longitude - reference_longitude);
      db_points[i].latitude =
         (unsigned short) (one_point->latitude - reference_latitude);

#ifndef J2MEMAP
      db_ids[i] = one_point->db_id;
#endif
   }

   if (switch_endian) {
      int i;

      for (i=0; i<PointCount; i++) {
         switch_endian_short(&db_points[i].longitude);
         switch_endian_short(&db_points[i].latitude);
#ifndef J2MEMAP
         switch_endian_int(db_ids + i);
#endif
      }
   }
}


void buildmap_point_summary (void) {

   fprintf (stderr, "-- point table statistics: %d points, %d bytes used\n",
                    PointCount, PointCount * sizeof(RoadMapPoint)
#ifndef J2MEMAP
		    		+ PointCount * sizeof(int));
#else
);
#endif

}


void buildmap_point_reset (void) {

   int i;

   for (i = 0; i < BUILDMAP_BLOCK; i++) {
      if (Point[i] != NULL) {
         free (Point[i]);
         Point[i] = NULL;
      }
   }

   free (SortedPoint);
   SortedPoint = NULL;

   PointCount = 0;

   PointByPosition = NULL;
   PointById = NULL;

   SortMaxLongitude = -0x7fffffff;
   SortMinLongitude =  0x7fffffff;
   SortMaxLatitude  = -0x7fffffff;
   SortMinLatitude  =  0x7fffffff;
}

