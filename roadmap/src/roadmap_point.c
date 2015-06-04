/*
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

/**
 * @file
 * @brief Manage the points that define lines.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "roadmap.h"
#include "roadmap_dbread.h"
#include "roadmap_db_point.h"

#include "roadmap_square.h"
#include "roadmap_point.h"

/**
 * @brief
 */
typedef struct {

   char *type;	/**< */

   RoadMapPoint *Point;	/**< */
   int           PointCount;	/**< */

   RoadMapPointBySquare *BySquare;	/**< */
   int                   BySquareCount;	/**< */

   int            *PointToSquare;	/**< */

} RoadMapPointContext;

static RoadMapPointContext *RoadMapPointActive = NULL;
static int RoadMapPointPositionLastSquare = -2;
static RoadMapPosition RoadMapPointPositionLastMin;

/**
 * @brief
 * @param root
 * @return
 */
static void *roadmap_point_map (roadmap_db *root) {

   roadmap_db *point_table;
   roadmap_db *bysquare_table;

   RoadMapPointContext *context;

   context = malloc (sizeof(RoadMapPointContext));
   roadmap_check_allocated(context);
   context->type = "RoadMapPointContext";

   bysquare_table  = roadmap_db_get_subsection (root, "bysquare");
   point_table = roadmap_db_get_subsection (root, "data");

   context->BySquare =
      (RoadMapPointBySquare *) roadmap_db_get_data (bysquare_table);
   context->Point = (RoadMapPoint *) roadmap_db_get_data (point_table);

   context->BySquareCount = roadmap_db_get_count (bysquare_table);
   context->PointCount    = roadmap_db_get_count (point_table);

   if (roadmap_db_get_size(bysquare_table) !=
          context->BySquareCount * sizeof(RoadMapPointBySquare)) {
      roadmap_log (ROADMAP_FATAL, "invalid point/bysquare structure");
   }
   if (roadmap_db_get_size(point_table) !=
          context->PointCount * sizeof(RoadMapPoint)) {
      roadmap_log (ROADMAP_FATAL, "invalid point/data structure");
   }

   context->PointToSquare = NULL;

   return context;
}

/**
 * @brief
 * @param context
 * @return
 */
static void roadmap_point_activate (void *context) {

   RoadMapPointContext *point_context = (RoadMapPointContext *) context;

   if ((point_context != NULL) &&
       (strcmp (point_context->type, "RoadMapPointContext") != 0)) {
      roadmap_log (ROADMAP_FATAL, "cannot activate (invalid context type)");
   }
   RoadMapPointActive = point_context;
   RoadMapPointPositionLastSquare = -2;
}

/**
 * @brief
 * @param context
 * @return
 */
static void roadmap_point_unmap (void *context) {

   RoadMapPointContext *point_context = (RoadMapPointContext *) context;

   if (point_context == RoadMapPointActive) {
      RoadMapPointActive = NULL;
   }

   if (point_context->PointToSquare != NULL) {
      free (point_context->PointToSquare);
   }
   free (point_context);
}

/**
 * @brief
 */
roadmap_db_handler RoadMapPointHandler = {
   "point",
   roadmap_point_map,
   roadmap_point_activate,
   roadmap_point_unmap
};

/**
 * @brief
 */
static void roadmap_point_retrieve_square (void) {

   int i;
   int j;
   int            *point2square;

   point2square = RoadMapPointActive->PointToSquare;
   if (point2square == NULL) {

       point2square = calloc (RoadMapPointActive->PointCount, sizeof(int));
       roadmap_check_allocated(point2square);

       RoadMapPointActive->PointToSquare = point2square;
   }

   for (i = 0; i < RoadMapPointActive->BySquareCount; i++) {

      int square;
      int end = RoadMapPointActive->BySquare[i].first
                   + RoadMapPointActive->BySquare[i].count;

      square = roadmap_square_from_index (i);

      for (j = RoadMapPointActive->BySquare[i].first; j < end; j++) {
         point2square[j] = square;
      }
   }
}

/**
 * @brief query the number of points (and the first and last ones) in a square
 * @param square
 * @param first
 * @param last
 * @return
 */
int roadmap_point_in_square (int square, int *first, int *last) {

   if (RoadMapPointActive == NULL) return 0;

   if (square < 0 || square >= RoadMapPointActive->BySquareCount) {
      return 0;
   }

   *first = RoadMapPointActive->BySquare[square].first;
   *last  = RoadMapPointActive->BySquare[square].first
               + RoadMapPointActive->BySquare[square].count - 1;

   return RoadMapPointActive->BySquare[square].count;
}

/**
 * @brief query the position of a point
 * @param point
 * @param position
 */
void roadmap_point_position  (int point, RoadMapPosition *position) {

   int point_square;
   RoadMapPoint *Point;


#ifdef ROADMAP_INDEX_DEBUG
   if (point < 0 || point >= RoadMapPointActive->PointCount) {
      roadmap_log (ROADMAP_FATAL, "invalid point index %d", point);
   }
#endif

   if (RoadMapPointActive->PointToSquare != NULL) {
      point_square = RoadMapPointActive->PointToSquare[point];
   } else {
      point_square = -1;
   }

   if (RoadMapPointPositionLastSquare != point_square) {

      if (point_square < 0) {

         roadmap_point_retrieve_square ();

         if (RoadMapPointActive->PointToSquare != NULL) {
            point_square = RoadMapPointActive->PointToSquare[point];
         } else {
            roadmap_log (ROADMAP_FATAL, "bad PointToSquare pointers");
         }
      }
      RoadMapPointPositionLastSquare = point_square;
      roadmap_square_min (point_square, &RoadMapPointPositionLastMin);
   }

   Point = RoadMapPointActive->Point + point;
   position->longitude =
	RoadMapPointPositionLastMin.longitude + Point->longitude;
   position->latitude =
	RoadMapPointPositionLastMin.latitude  + Point->latitude;
}

#ifdef HAVE_NAVIGATE_PLUGIN
/**
 * @brief queries the total number of points
 * @return the total number of points
 */
int roadmap_point_count(void)
{
	if (RoadMapPointActive == NULL)
		return 0;	/* No line */

	return RoadMapPointActive->PointCount;
}
#endif
