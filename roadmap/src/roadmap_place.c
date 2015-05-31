/*
 * LICENSE:
 *
 *   Copyright 2004 Stephen Woodbridge
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
 * @brief Manage placename points.
 *
 * SYNOPSYS:
 *
 *   int  roadmap_place_in_square
 *           (int square, int layer, int *first, int *last);
 *   void roadmap_place_point (int place, RoadMapPosition *position);
 *   int  roadmap_place_count (void);
 *
 * These functions are used to retrieve the points that make the places.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "roadmap.h"
#include "roadmap_dbread.h"
#include "roadmap_db_place.h"

#include "roadmap_point.h"
#include "roadmap_place.h"
#include "roadmap_square.h"

#include "roadmap_dictionary.h"

static char *RoadMapPlaceType = "RoadMapPlaceContext";

/**
 * @brief
 */
typedef struct {

   char *type;

   int  *Place;
   RoadMapString *NameByPlace;
   int   PlaceCount;

   int  *PlaceByLayer;
   int   PlaceByLayerCount;

   RoadMapPlaceBySquare *PlaceBySquare;
   int                   PlaceBySquareCount;

   RoadMapDictionary PlaceNames;

} RoadMapPlaceContext;

static RoadMapPlaceContext *RoadMapPlaceActive = NULL;

/**
 * @brief
 * @param root
 * @return
 */
static void *roadmap_place_map (roadmap_db *root) {

   RoadMapPlaceContext *context;

   roadmap_db *place_table;
   roadmap_db *name_table;
   roadmap_db *layer_table;
   roadmap_db *square_table;


   context = (RoadMapPlaceContext *) malloc (sizeof(RoadMapPlaceContext));
   if (context == NULL) {
      roadmap_log (ROADMAP_ERROR, "no more memory");
      return NULL;
   }
   context->type = RoadMapPlaceType;

   place_table   = roadmap_db_get_subsection (root, "data");
   name_table   = roadmap_db_get_subsection (root, "name");
   layer_table   = roadmap_db_get_subsection (root, "bylayer");
   square_table  = roadmap_db_get_subsection (root, "bysquare");

   context->Place = (int *) roadmap_db_get_data (place_table);
   context->PlaceCount = roadmap_db_get_count (place_table);
   if (roadmap_db_get_size (place_table) != context->PlaceCount * sizeof(int)) {
      roadmap_log (ROADMAP_ERROR, "invalid place/size values");
      goto roadmap_place_map_abort;
   }

   context->NameByPlace = (RoadMapString *) roadmap_db_get_data (name_table);
   if (context->PlaceCount != roadmap_db_get_count (name_table)) {
      roadmap_log (ROADMAP_ERROR, "invalid name/count values");
      goto roadmap_place_map_abort;
   }
   if (roadmap_db_get_size (name_table) != context->PlaceCount * sizeof(RoadMapString)) {
      roadmap_log (ROADMAP_ERROR, "invalid name/size values");
      goto roadmap_place_map_abort;
   }

   context->PlaceByLayer = (int *) roadmap_db_get_data (layer_table);
   context->PlaceByLayerCount = roadmap_db_get_count (layer_table);

   if (roadmap_db_get_size (layer_table) != context->PlaceByLayerCount * sizeof(int)) {
      roadmap_log (ROADMAP_ERROR, "invalid layer/size values");
      goto roadmap_place_map_abort;
   }

   context->PlaceBySquare =
      (RoadMapPlaceBySquare *) roadmap_db_get_data (square_table);
   context->PlaceBySquareCount = roadmap_db_get_count (square_table);

   if (roadmap_db_get_size (square_table) !=
       context->PlaceBySquareCount * sizeof(RoadMapPlaceBySquare)) {
      roadmap_log (ROADMAP_ERROR, "invalid square/size values");
      goto roadmap_place_map_abort;
   }

   context->PlaceNames    = NULL;

   return context;

roadmap_place_map_abort:

   free(context);
   return NULL;
}

/**
 * @brief
 * @param context
 */
static void roadmap_place_activate (void *context) {

   RoadMapPlaceContext *place_context = (RoadMapPlaceContext *) context;

   if (place_context && place_context->type != RoadMapPlaceType) {
      roadmap_log (ROADMAP_FATAL, "invalid place context activated");
   }
   if (place_context->PlaceNames == NULL) {
      place_context->PlaceNames = roadmap_dictionary_open ("city");
   }
   RoadMapPlaceActive = place_context;
}

/**
 * @brief
 * @param context
 */
static void roadmap_place_unmap (void *context) {

   RoadMapPlaceContext *place_context = (RoadMapPlaceContext *) context;

   if (place_context->type != RoadMapPlaceType) {
      roadmap_log (ROADMAP_FATAL, "unmapping invalid place context");
   }
   if (RoadMapPlaceActive == place_context) {
      RoadMapPlaceActive = NULL;
   }
   free (place_context);
}

/**
 * @brief
 */
roadmap_db_handler RoadMapPlaceHandler = {
   "place",
   roadmap_place_map,
   roadmap_place_activate,
   roadmap_place_unmap
};

/**
 * @brief
 * @param square
 * @param layer
 * @param first
 * @param last
 * @return
 */
int roadmap_place_in_square (int square, int layer, int *first, int *last) {

   int *index;

   if (RoadMapPlaceActive == NULL) return 0; /* No place. */

   square = roadmap_square_index(square);
   if (square < 0) {
      return 0;   /* This square is empty. */
   }

   index = RoadMapPlaceActive->PlaceByLayer
              + RoadMapPlaceActive->PlaceBySquare[square].first;

   *first = index[layer-1];
   *last  = index[layer] - 1;

   return (*first <= *last);
}

/**
 * @brief
 * @param place
 * @param position
 */
void roadmap_place_point   (int place, RoadMapPosition *position) {

#ifdef ROADMAP_INDEX_DEBUG
   if (place < 0 || place >= RoadMapPlaceActive->PlaceCount) {
      roadmap_log (ROADMAP_FATAL, "illegal place index %d", place);
   }
#endif
   roadmap_point_position (RoadMapPlaceActive->Place[place], position);
}

/**
 * @brief
 * @return
 */
int  roadmap_place_count (void) {

   return RoadMapPlaceActive->PlaceCount;
}

const char *roadmap_place_get_name(int place_id)
{
    RoadMapString stringid;

    stringid = RoadMapPlaceActive->NameByPlace[place_id];

    return roadmap_dictionary_get(RoadMapPlaceActive->PlaceNames, stringid);
}


