/* roadmap_street.c - Handle streets operations and attributes.
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
 *   see roadmap_street.h.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "roadmap_db_street.h"
#include "roadmap_db_range.h"

#include "roadmap.h"
#include "roadmap_dbread.h"

#include "roadmap_locator.h"

#include "roadmap_math.h"
#include "roadmap_square.h"
#include "roadmap_street.h"
#include "roadmap_shape.h"
#include "roadmap_line.h"
#include "roadmap_dictionary.h"

static char *RoadMapStreetType = "RoadMapStreetContext";
static char *RoadMapZipType    = "RoadMapZipContext";
static char *RoadMapRangeType  = "RoadMapRangeContext";

typedef struct {

   RoadMapString prefix;
   RoadMapString city;

} StreetSearchContext;

typedef struct {

   char *type;

   RoadMapStreet        *RoadMapStreets;
   char                 *RoadMapStreetType;
   int                   RoadMapStreetsCount;

} RoadMapStreetContext;

typedef struct {

   char *type;

   int *RoadMapZipCode;
   int  RoadMapZipCodeCount;

} RoadMapZipContext;

typedef struct {

   char *type;

   RoadMapRangeByStreet *RoadMapByStreet;
   int                   RoadMapByStreetCount;

   RoadMapRangeByCity *RoadMapByCity;
   int                 RoadMapByCityCount;

   RoadMapRangePlace *RoadMapPlace;
   int                RoadMapPlaceCount;

   RoadMapRangeByZip *RoadMapByZip;
   int                RoadMapByZipCount;

   RoadMapRangeBySquare *bysquare;
   int                   square_count;

   RoadMapRange *RoadMapAddr;
   int           RoadMapAddrCount;

   RoadMapRangeNoAddress *noaddr;
   int                    noaddr_count;

   RoadMapDictionary RoadMapCityNames;
   RoadMapDictionary RoadMapStreetPrefix;
   RoadMapDictionary RoadMapStreetNames;
   RoadMapDictionary RoadMapText2Speech;
   RoadMapDictionary RoadMapStreetType;
   RoadMapDictionary RoadMapStreetSuffix;

} RoadMapRangeContext;


static RoadMapStreetContext *RoadMapStreetActive = NULL;
static RoadMapZipContext    *RoadMapZipActive    = NULL;
static RoadMapRangeContext  *RoadMapRangeActive  = NULL;

#define MAX_SEARCH_NAMES 100
static int RoadMapStreetSearchCount;
static char *RoadMapStreetSearchNames[MAX_SEARCH_NAMES];

static void *roadmap_street_map (roadmap_db *root) {

   RoadMapStreetContext *context;

   roadmap_db *table;


   context = malloc (sizeof(RoadMapStreetContext));
   roadmap_check_allocated(context);

   context->type = RoadMapStreetType;

   table = roadmap_db_get_subsection (root, "name");
   context->RoadMapStreets = (RoadMapStreet *) roadmap_db_get_data (table);
   context->RoadMapStreetsCount = roadmap_db_get_count(table);

   if (roadmap_db_get_size (table) !=
       context->RoadMapStreetsCount * sizeof(RoadMapStreet)) {
      roadmap_log (ROADMAP_FATAL,
                   "invalid street structure (%d != %d / %d)",
                   context->RoadMapStreetsCount,
                   roadmap_db_get_size (table),
                   sizeof(RoadMapStreet));
   }

   table = roadmap_db_get_subsection (root, "type");
   context->RoadMapStreetType = (char *) roadmap_db_get_data (table);

   if (roadmap_db_get_count(table) != context->RoadMapStreetsCount) {
      roadmap_log (ROADMAP_FATAL, "inconsistent count of street");
   }

   return context;
}

static void roadmap_street_activate (void *context) {

   RoadMapStreetContext *this = (RoadMapStreetContext *) context;

   if ((this != NULL) && (this->type != RoadMapStreetType)) {
      roadmap_log (ROADMAP_FATAL, "cannot activate (bad context type)");
   }
   RoadMapStreetActive = this;
}

static void roadmap_street_unmap (void *context) {

   RoadMapStreetContext *this = (RoadMapStreetContext *) context;

   if (this->type != RoadMapStreetType) {
      roadmap_log (ROADMAP_FATAL, "cannot unmap (bad context type)");
   }
   if (RoadMapStreetActive == this) {
      RoadMapStreetActive = NULL;
   }
   free (this);
}

roadmap_db_handler RoadMapStreetHandler = {
   "street",
   roadmap_street_map,
   roadmap_street_activate,
   roadmap_street_unmap
};


static void *roadmap_street_zip_map (roadmap_db *root) {

   RoadMapZipContext *context;


   context = malloc (sizeof(RoadMapZipContext));
   roadmap_check_allocated(context);

   context->type = RoadMapZipType;

   context->RoadMapZipCode = (int *) roadmap_db_get_data (root);
   context->RoadMapZipCodeCount = roadmap_db_get_count(root);

   if (roadmap_db_get_size (root) !=
       context->RoadMapZipCodeCount * sizeof(int)) {
      roadmap_log (ROADMAP_FATAL, "invalid zip structure");
   }

   return context;
}

static void roadmap_street_zip_activate (void *context) {

   RoadMapZipContext *this = (RoadMapZipContext *) context;

   if ((this != NULL) && (this->type != RoadMapZipType)) {
      roadmap_log (ROADMAP_FATAL, "cannot activate (bad context type)");
   }
   RoadMapZipActive = this;
}

static void roadmap_street_zip_unmap (void *context) {

   RoadMapZipContext *this = (RoadMapZipContext *) context;

   if (this->type != RoadMapZipType) {
      roadmap_log (ROADMAP_FATAL, "cannot unmap (bad context type)");
   }
   if (RoadMapZipActive == this) {
      RoadMapZipActive = NULL;
   }
   free (this);
}

roadmap_db_handler RoadMapZipHandler = {
   "zip",
   roadmap_street_zip_map,
   roadmap_street_zip_activate,
   roadmap_street_zip_unmap
};


static void *roadmap_street_range_map (roadmap_db *root) {

   RoadMapRangeContext *context;
   roadmap_db *table;


   context = malloc (sizeof(RoadMapRangeContext));
   roadmap_check_allocated(context);

   context->type = RoadMapRangeType;
   context->RoadMapStreetPrefix = NULL;
   context->RoadMapStreetNames  = NULL;
   context->RoadMapText2Speech  = NULL;
   context->RoadMapStreetType   = NULL;
   context->RoadMapStreetSuffix = NULL;
   context->RoadMapCityNames    = NULL;

   table = roadmap_db_get_subsection (root, "bystreet");
   context->RoadMapByStreet =
       (RoadMapRangeByStreet *) roadmap_db_get_data (table);
   context->RoadMapByStreetCount = roadmap_db_get_count (table);

   if (roadmap_db_get_size (table) !=
       context->RoadMapByStreetCount * sizeof(RoadMapRangeByStreet)) {
      roadmap_log (ROADMAP_FATAL, "invalid range/bystreet structure");
   }

   table = roadmap_db_get_subsection (root, "bycity");
   context->RoadMapByCity = (RoadMapRangeByCity *) roadmap_db_get_data (table);
   context->RoadMapByCityCount = roadmap_db_get_count (table);

   if (roadmap_db_get_size (table) !=
       context->RoadMapByCityCount * sizeof(RoadMapRangeByCity)) {
      roadmap_log (ROADMAP_FATAL, "invalid range/bycity structure");
   }

   table = roadmap_db_get_subsection (root, "place");
   context->RoadMapPlace = (RoadMapRangePlace *) roadmap_db_get_data (table);
   context->RoadMapPlaceCount = roadmap_db_get_count (table);

   if (roadmap_db_get_size (table) !=
       context->RoadMapPlaceCount * sizeof(RoadMapRangePlace)) {
      roadmap_log (ROADMAP_FATAL, "invalid range/place structure");
   }

   table = roadmap_db_get_subsection (root, "byzip");
   context->RoadMapByZip = (RoadMapRangeByZip *) roadmap_db_get_data (table);
   context->RoadMapByZipCount = roadmap_db_get_count (table);

   if (roadmap_db_get_size (table) !=
       context->RoadMapByZipCount * sizeof(RoadMapRangeByZip)) {
      roadmap_log (ROADMAP_FATAL, "invalid range/byzip structure");
   }

   table = roadmap_db_get_subsection (root, "bysquare");
   context->bysquare = (RoadMapRangeBySquare *) roadmap_db_get_data (table);
   context->square_count = roadmap_db_get_count (table);

   if (roadmap_db_get_size (table) !=
       context->square_count * sizeof(RoadMapRangeBySquare)) {
      roadmap_log (ROADMAP_FATAL, "invalid range/bysquare structure");
   }

   table = roadmap_db_get_subsection (root, "addr");
   context->RoadMapAddr = (RoadMapRange *) roadmap_db_get_data (table);
   context->RoadMapAddrCount = roadmap_db_get_count (table);

   if (roadmap_db_get_size (table) !=
       context->RoadMapAddrCount * sizeof(RoadMapRange)) {
      roadmap_log (ROADMAP_FATAL, "invalid range/addr structure");
   }

   table = roadmap_db_get_subsection (root, "noaddr");
   context->noaddr = (RoadMapRangeNoAddress *) roadmap_db_get_data (table);
   context->noaddr_count = roadmap_db_get_count (table);

   if (roadmap_db_get_size (table) !=
       context->noaddr_count * sizeof(RoadMapRangeNoAddress)) {
      roadmap_log (ROADMAP_FATAL, "invalid range/noaddr structure");
   }

   return context;
}

static void roadmap_street_range_activate (void *context) {

   RoadMapRangeContext *this = (RoadMapRangeContext *) context;

   if (this != NULL) {
      
      if (this->type != RoadMapRangeType) {
         roadmap_log (ROADMAP_FATAL, "cannot activate (bad context type)");
      }

      if (this->RoadMapStreetPrefix == NULL) {
         this->RoadMapStreetPrefix = roadmap_dictionary_open ("prefix");
      }
      if (this->RoadMapStreetNames == NULL) {
         this->RoadMapStreetNames = roadmap_dictionary_open ("street");
      }
      if (this->RoadMapText2Speech == NULL) {
         this->RoadMapText2Speech = roadmap_dictionary_open ("text2speech");
      }
      if (this->RoadMapStreetType == NULL) {
         this->RoadMapStreetType = roadmap_dictionary_open ("type");
      }
      if (this->RoadMapStreetSuffix == NULL) {
         this->RoadMapStreetSuffix = roadmap_dictionary_open ("suffix");
      }
      if (this->RoadMapCityNames == NULL) {
         this->RoadMapCityNames = roadmap_dictionary_open ("city");
      }
      if (this->RoadMapStreetNames  == NULL ||
            this->RoadMapStreetPrefix == NULL ||
            this->RoadMapStreetType   == NULL ||
            this->RoadMapStreetSuffix == NULL ||
            this->RoadMapCityNames    == NULL) {
         roadmap_log (ROADMAP_FATAL, "cannot open dictionary");
      }
   }
   RoadMapRangeActive = this;
}

static void roadmap_street_range_unmap (void *context) {

   RoadMapRangeContext *this = (RoadMapRangeContext *) context;

   if (this->type != RoadMapRangeType) {
      roadmap_log (ROADMAP_FATAL, "cannot unmap (bad context type)");
   }
   if (RoadMapRangeActive == this) {
      RoadMapRangeActive = NULL;
   }
   free (this);
}

roadmap_db_handler RoadMapRangeHandler = {
   "range",
   roadmap_street_range_map,
   roadmap_street_range_activate,
   roadmap_street_range_unmap
};


typedef struct roadmap_street_identifier {

   RoadMapString prefix;
   RoadMapString name;
   RoadMapString suffix;
   RoadMapString type;

} RoadMapStreetIdentifier;


static void roadmap_street_locate (const char *name,
                                   RoadMapStreetIdentifier *street) {

   char *space;
   char  buffer[128];

   street->prefix = 0;
   street->suffix = 0;
   street->type   = 0;

   /* Search for a prefix. If found, remove the prefix from the name
    * by shifting the name's pointer.
    */
   space = strchr (name, ' ');

   if (space != NULL) {

      unsigned  length;

      length = (unsigned)(space - name);

      if (length < sizeof(buffer)) {

         strncpy (buffer, name, length);
         buffer[length] = 0;

         street->prefix =
            roadmap_dictionary_locate
               (RoadMapRangeActive->RoadMapStreetPrefix, buffer);

         if (street->prefix != ROADMAP_INVALID_STRING) {
            name = name + length;
            while (*name == ' ') ++name;
         }
      }

      /* Search for a street type. If found, extract the street name
       * and stor it in the temporary buffer, which will be substituted
       * for the name.
       */
      space = strrchr (name, ' ');

      if (space != NULL) {

         char *trailer;

         trailer = space + 1;

         street->type =
            roadmap_dictionary_locate
               (RoadMapRangeActive->RoadMapStreetType, trailer);

         if (street->type != ROADMAP_INVALID_STRING) {

            length = (unsigned)(space - name);

            if (length < sizeof(buffer)) {

               strncpy (buffer, name, length);
               buffer[length] = 0;

               while (buffer[length-1] == ' ') buffer[--length] = 0;

               space = strrchr (buffer, ' ');

               if (space != NULL) {

                  trailer = space + 1;

                  street->suffix =
                     roadmap_dictionary_locate
                        (RoadMapRangeActive->RoadMapStreetSuffix, trailer);

                  if (street->suffix != ROADMAP_INVALID_STRING) {

                     while (*space == ' ') {
                        *space = 0;
                        --space;
                     }
                  }
               }

               name = buffer;
            }
         }
      }
   }

   street->name =
      roadmap_dictionary_locate (RoadMapRangeActive->RoadMapStreetNames, name);
   if (street->prefix == ROADMAP_INVALID_STRING) street->prefix = 0;
   if (street->suffix == ROADMAP_INVALID_STRING) street->suffix = 0;
   if (street->type   == ROADMAP_INVALID_STRING) street->type   = 0;
}


static int roadmap_street_block_by_county_subdivision
              (RoadMapStreetIdentifier *street,
               int city,
               RoadMapBlocks *blocks, int size) {

   int i;
   int j;
   int count = 0;
   int range_count = RoadMapRangeActive->RoadMapByStreetCount;


   for (i = 0; i < range_count; i++) {

      RoadMapStreet *this_street = RoadMapStreetActive->RoadMapStreets + i;
      RoadMapRangeByStreet *by_street = RoadMapRangeActive->RoadMapByStreet + i;

      if (this_street->fename == street->name &&
          (street->prefix == 0 || this_street->fedirp == street->prefix) &&
          (street->suffix == 0 || this_street->fedirs == street->suffix) &&
          (street->type   == 0 || this_street->fetype == street->type  ) &&
          by_street->count_range >= 0) {

         int range_index;
         int range_end;

         range_index = by_street->first_range;
         range_end  =
             by_street->first_range + by_street->count_range;

         if ((range_index == range_end) && (city < 0)) {

            blocks[count].street = i;
            blocks[count].first  = range_index;
            blocks[count].city   = 0;
            blocks[count].count  = 0;

            if (++count >= size) {
               return count;
            }

            continue;
         }

         for (j = by_street->first_city; range_index < range_end; j++) {

            RoadMapRangeByCity *by_city = RoadMapRangeActive->RoadMapByCity + j;

            if ((city < 0) || (by_city->city == city)) {

               blocks[count].street = i;
               blocks[count].first  = range_index;
               blocks[count].city   = by_city->city;
               blocks[count].count  = by_city->count;

               if (++count >= size) {
                  return count;
               }
               if (city > 0) break;
            }

            range_index += by_city->count;
         }
      }
   }
   return count;
}


static RoadMapDictionaryMask roadmap_street_get_city_search_mask (int city) {

   static int current_mask_city = -1;
   static RoadMapDictionaryMask mask;
   int i;
   int j;
   int range_count = RoadMapRangeActive->RoadMapByStreetCount;

   if (current_mask_city == city) return mask;

   if (mask) roadmap_dictionary_mask_free (mask);
   mask = roadmap_dictionary_mask_new (RoadMapRangeActive->RoadMapStreetNames);

   for (i = 0; i < range_count; i++) {

      RoadMapStreet *this_street = RoadMapStreetActive->RoadMapStreets + i;
      RoadMapRangeByStreet *by_street = RoadMapRangeActive->RoadMapByStreet + i;

      if (by_street->count_range >= 0) {

         int range_index;
         int range_end;

         range_index = by_street->first_range;
         range_end  =
             by_street->first_range + by_street->count_range;

         for (j = by_street->first_city; range_index < range_end; j++) {

            RoadMapRangeByCity *by_city = RoadMapRangeActive->RoadMapByCity + j;

            if (by_city->city == city) {
               roadmap_dictionary_mask_set
                  (RoadMapRangeActive->RoadMapStreetNames,
                   this_street->fename, mask);
            }

            range_index += by_city->count;
         }
      }
   }

   return mask;
}


int roadmap_street_blocks_by_city
       (const char *street_name, const char *city_name,
        RoadMapBlocks *blocks,
        int size) {

   /* FIXME: see below comment about city mapping.
   int i;
   int place_count;
   */

   int count;
   int total = 0;

   int city;
   RoadMapStreetIdentifier street;


   if (RoadMapRangeActive == NULL) return 0;

   roadmap_street_locate (street_name, &street);
   if (street.name == ROADMAP_INVALID_STRING) {
      return ROADMAP_STREET_NOSTREET;
   }

   if (city_name[0] == '?') {

      city = -1;

   } else {

      city = roadmap_dictionary_locate (RoadMapRangeActive->RoadMapCityNames,
                                           city_name);
      if (city == ROADMAP_INVALID_STRING) {
            return ROADMAP_STREET_NOCITY;
      }
   }

   /* FIXME: at this time the place -> city mapping is totally unreliable,
    * because the two do not always match. For example, the Pasadena city
    * and the Los Angeles place intersect, which cause all of Pasadena to
    * be included in Los Angeles. Bummer..
    * The long term solution is to have two indexes: one for cities and one
    * for places.
    *
    * START OF FILTERED CODE.
   place_count = RoadMapRangeActive->RoadMapPlaceCount;

   for (i = 0; i < place_count; i++) {

      RoadMapRangePlace *this_place = RoadMapRangeActive->RoadMapPlace + i;

      if ((city < 0) ||
          ((this_place->place == city) && (this_place->city  != city))) {

         count = roadmap_street_block_by_county_subdivision
                              (&street, this_place->city, blocks, size);

         if (count > 0) {

            total  += count;

            blocks += count;
            size   -= count;

            if (size <= 0) {
               return total;
            }
         }
      }
   }
   * END OF FILTERED CODE. */

   count = roadmap_street_block_by_county_subdivision
                                (&street, city, blocks, size);

   if (count > 0) {
      total += count;
   }

   return total;
}


int roadmap_street_blocks_by_zip
       (const char *street_name, int zip,
        RoadMapBlocks *blocks,
        int size) {

   int i;
   int j;
   int zip_index = 0;
   int zip_count;
   int range_count;
   int count = 0;

   RoadMapStreetIdentifier street;


   if ((RoadMapRangeActive == NULL) || (RoadMapZipActive == NULL)) return 0;

   zip_count   = RoadMapZipActive->RoadMapZipCodeCount;
   range_count = RoadMapRangeActive->RoadMapByStreetCount;

   roadmap_street_locate (street_name, &street);

   if (street.name == ROADMAP_INVALID_STRING) {
      return ROADMAP_STREET_NOSTREET;
   }

   for (i = 1; i < zip_count; i++) {
      if (RoadMapZipActive->RoadMapZipCode[i] == zip) {
         zip_index = i;
         break;
      }
   }
   if (zip_index <= 0) {
      return ROADMAP_STREET_NOSTREET;
   }

   for (i = 0; i < range_count; i++) {

      RoadMapStreet *this_street = RoadMapStreetActive->RoadMapStreets + i;
      RoadMapRangeByStreet *by_street = RoadMapRangeActive->RoadMapByStreet + i;

      if (this_street->fename == street.name && by_street->count_range >= 0) {

         int range_index;
         int range_end;

         range_index = by_street->first_range;
         range_end  = by_street->first_range + by_street->count_range;

         for (j = by_street->first_zip; range_index < range_end; j++) {

            RoadMapRangeByZip *by_zip = RoadMapRangeActive->RoadMapByZip + j;

            if (by_zip->zip == zip_index) {

               blocks[count].street = i;
               blocks[count].first  = range_index;
               blocks[count].count  = by_zip->count;

               if (++count >= size) {
                  return count;
               }
               break;
            }

            range_index += by_zip->count;
         }
      }
   }
   return count;
}


int roadmap_street_get_ranges
       (RoadMapBlocks *blocks, int count, RoadMapStreetRange *ranges) {

   int i;
   int end;

   int fradd;
   int toadd;

   int total = 0;


   if (RoadMapRangeActive == NULL) return 0;

   for (i = blocks->first, end = blocks->first + blocks->count; i < end; ++i) {

      RoadMapRange *this_addr = RoadMapRangeActive->RoadMapAddr + i;

      if (HAS_CONTINUATION(this_addr)) {

         fradd = ((int)(this_addr->fradd) & 0xffff)
                    + (((int)(this_addr[1].fradd) & 0xffff) << 16);
         toadd = ((int)(this_addr->toadd) & 0xffff)
                    + (((int)(this_addr[1].toadd) & 0xffff) << 16);

         ++i; /* Because this range occupies two entries. */

      } else {

         fradd = this_addr->fradd;
         toadd = this_addr->toadd;
      }

      if (total >= count) return total;

      ranges->fradd = fradd;
      ranges->toadd = toadd;
      ranges += 1;
      total  += 1;
   }

   return total;
}


int roadmap_street_get_position (RoadMapBlocks *blocks,
                                 int number,
                                 RoadMapPosition *position) {

   int i;
   int end;
   int number_max;
   int number_min;

   int fradd;
   int toadd;

   RoadMapPosition from;
   RoadMapPosition to;


   if (RoadMapRangeActive == NULL) return -1;

   for (i = blocks->first, end = blocks->first + blocks->count; i < end; ++i) {

      RoadMapRange *this_addr = RoadMapRangeActive->RoadMapAddr + i;

      if (((number & 1) != (this_addr->fradd & 1)) &&
          ((number & 1) != (this_addr->toadd & 1))) continue;

      if (HAS_CONTINUATION(this_addr)) {

         fradd = ((int)(this_addr->fradd) & 0xffff)
                    + (((int)(this_addr[1].fradd) & 0xffff) << 16);
         toadd = ((int)(this_addr->toadd) & 0xffff)
                    + (((int)(this_addr[1].toadd) & 0xffff) << 16);

         ++i;

      } else {

         fradd = this_addr->fradd;
         toadd = this_addr->toadd;
      }

      if (fradd > toadd) {
         number_max = fradd;
         number_min = toadd;
      } else {
         number_max = toadd;
         number_min = fradd;
      }

      if (number >= number_min && number <= number_max) {

         int line = this_addr->line & (~ CONTINUATION_FLAG);

         roadmap_line_from (line, &from);
         roadmap_line_to   (line, &to);

         if (number_max == number_min) {

            position->longitude = (from.longitude + to.longitude) / 2;
            position->latitude  = (from.latitude + to.latitude) / 2;

         } else {

            position->longitude =
               from.longitude -
                  ((from.longitude - to.longitude)
                      * abs(fradd - number)) / (number_max - number_min);

            position->latitude =
               from.latitude -
                  ((from.latitude - to.latitude)
                     * abs(fradd - number)) / (number_max - number_min);
         }

         return line;
      }
   }
   return -1;
}


static int roadmap_street_get_distance_with_shape
              (const RoadMapPosition *position,
               int line, int cfcc, int first_shape, int last_shape,
               RoadMapNeighbour *neighbour) {

   int i;
   int smallest_distance;
   RoadMapNeighbour current;
   int fips = roadmap_locator_active();

   roadmap_plugin_set_line
      (&current.line, ROADMAP_PLUGIN_ID, line, cfcc, fips);

   /* Note: the position of a shape point is relative to the position
    * of the previous point, starting with the from point.
    * The logic here takes care of this property.
    */
   smallest_distance = 0x7fffffff;
   roadmap_line_from (line, &current.from);

   current.to = current.from; /* Initialize the shape position (relative). */

   for (i = first_shape; i <= last_shape; i++) {

      roadmap_shape_get_position (i, &current.to);

      if (roadmap_math_line_is_visible (&current.from, &current.to) &&
          roadmap_math_get_visible_coordinates (&current.from, &current.to,
                                                NULL, NULL)) {

         current.distance =
            roadmap_math_get_distance_from_segment
               (position, &current.from, &current.to,
                &current.intersection, NULL);

         if (current.distance < smallest_distance) {
            smallest_distance = current.distance;
            *neighbour = current;
         }
      }

      current.from = current.to;
   }

   roadmap_line_to (line, &current.to);

   if (roadmap_math_line_is_visible (&current.from, &current.to) &&
          roadmap_math_get_visible_coordinates (&current.from, &current.to,
                                                NULL, NULL)) {

      current.distance =
         roadmap_math_get_distance_from_segment
            (position, &current.to, &current.from, &current.intersection, NULL);

      if (current.distance < smallest_distance) {
         smallest_distance = current.distance;
         *neighbour = current;
      }
   }

   return smallest_distance < 0x7fffffff;
}


static int roadmap_street_get_distance_no_shape
              (const RoadMapPosition *position, int line, int cfcc,
               RoadMapNeighbour *neighbour) {

   roadmap_line_from (line, &neighbour->from);
   roadmap_line_to   (line, &neighbour->to);

   if (roadmap_math_line_is_visible (&neighbour->from, &neighbour->to) &&
       roadmap_math_get_visible_coordinates (&neighbour->from, &neighbour->to,
                                             NULL, NULL)) {

      neighbour->distance =
         roadmap_math_get_distance_from_segment
            (position, &neighbour->from, &neighbour->to,
             &neighbour->intersection, NULL);

      roadmap_plugin_set_line
         (&neighbour->line,
          ROADMAP_PLUGIN_ID,
          line,
          cfcc,
          roadmap_locator_active ());

      return 1;
   }

   return 0;
}


int roadmap_street_replace
              (RoadMapNeighbour *neighbours, int count, int max,
               const RoadMapNeighbour *this) {

   int farthest;
   int distance;


   if (count < max) {

      /* The list is not yet saturated: take the next available spot. */

      neighbours[count] = *this;

      return count + 1;
   }

   /* The list is saturated: take the farthest out (either an existing one,
    * or this new one).
    */
   farthest = -1;
   distance = this->distance;

   for (count = max - 1; count >= 0; --count) {

      if (neighbours[count].distance > distance) {
         farthest = count;
         distance = neighbours[count].distance;
      }
   }

   if (farthest >= 0) {
      neighbours[farthest] = *this;
   }
   return max;
}


int roadmap_street_get_distance
        (const RoadMapPosition *position, int line, int cfcc,
         RoadMapNeighbour *result) {

   int found = 0;
   int square;
   int first_shape_line;
   int last_shape_line;
   int first_shape;
   int last_shape;
   RoadMapPosition line_from_position;

   if (RoadMapRangeActive == NULL) return 0;

   roadmap_line_from (line, &line_from_position);
   square = roadmap_square_search (&line_from_position);

   if (roadmap_shape_in_square (square, &first_shape_line,
                                        &last_shape_line) > 0) {

      if (roadmap_shape_of_line (line, first_shape_line,
                                       last_shape_line,
                                       &first_shape, &last_shape) > 0) {

         found = roadmap_street_get_distance_with_shape
                    (position, line, cfcc, first_shape, last_shape, result);
      }
   }

   if (!found) {
      found =
         roadmap_street_get_distance_no_shape
                     (position, line, cfcc, result);
   }
   return found;
}


static int roadmap_street_get_closest_in_square
              (const RoadMapPosition *position, int square, int cfcc,
               RoadMapNeighbour *neighbours, int count, int max) {

   int line;
   int found;
   int first_line;
   int last_line;
   int first_shape_line;
   int last_shape_line;
   int first_shape;
   int last_shape;

   RoadMapNeighbour this;

   int fips = roadmap_locator_active ();


   if (roadmap_line_in_square (square, cfcc, &first_line, &last_line) > 0) {

      if (roadmap_shape_in_square (square, &first_shape_line,
                                           &last_shape_line) > 0) {

         for (line = first_line; line <= last_line; line++) {
            
            if (roadmap_plugin_override_line (line, cfcc, fips)) continue;

            if (roadmap_shape_of_line (line, first_shape_line,
                                             last_shape_line,
                                             &first_shape, &last_shape) > 0) {

               found =
                  roadmap_street_get_distance_with_shape
                     (position, line, cfcc, first_shape, last_shape, &this);

            } else {
               found =
                   roadmap_street_get_distance_no_shape
                           (position, line, cfcc, &this);
            }
            if (found) {
               count = roadmap_street_replace (neighbours, count, max, &this);
            }
         }

      } else {

         for (line = first_line; line <= last_line; line++) {

            if (roadmap_street_get_distance_no_shape
                        (position, line, cfcc, &this)) {
               count = roadmap_street_replace (neighbours, count, max, &this);
            }
         }
      }
   }

   if (roadmap_line_in_square2 (square, cfcc, &first_line, &last_line) > 0) {

      int previous_square  = -1;
      int real_square;
      int line_cursor;
      int shape_count = 0;
      RoadMapPosition reference_position;

      for (line_cursor = first_line; line_cursor <= last_line; ++line_cursor) {

         line = roadmap_line_get_from_index2 (line_cursor);

         roadmap_line_from (line, &reference_position);
         real_square = roadmap_square_search (&reference_position);
          
         if (real_square != previous_square) {
            shape_count =
               roadmap_shape_in_square
                  (real_square, &first_shape_line, &last_shape_line);
            previous_square = real_square;
         }
         
         if (roadmap_plugin_override_line (line, cfcc, fips)) continue;

         if (shape_count > 0 &&
             roadmap_shape_of_line (line, first_shape_line,
                                    last_shape_line,
                                    &first_shape, &last_shape) > 0) {

               found =
                  roadmap_street_get_distance_with_shape
                     (position, line, cfcc, first_shape, last_shape, &this);

         } else {
             found =
                roadmap_street_get_distance_no_shape
                         (position, line, cfcc, &this);
         }

         if (found) {
            count = roadmap_street_replace (neighbours, count, max, &this);
         }
      }
   }

   return count;
}


static int roadmap_street_get_closest_in_long_lines
              (const RoadMapPosition *position, int cfcc,
               RoadMapNeighbour *neighbours, int count, int max) {

   int index = 0;
   int found;
   int first_shape_line;
   int last_shape_line;
   int first_shape;
   int last_shape;
   int line_cfcc;
   RoadMapArea area;
   int line;
   int previous_square = -1;

   RoadMapNeighbour this;

   int fips = roadmap_locator_active ();

   while (roadmap_line_long (index++, &line, &area, &line_cfcc)) {
      int shape_count = 0;
      int real_square;
      RoadMapPosition reference_position;
      RoadMapPosition to_position;

      if (cfcc != line_cfcc) continue;

      if (!roadmap_math_is_visible (&area)) {
         continue;
      }

      roadmap_line_to (line, &to_position);

      if (roadmap_math_point_is_visible (&to_position)) {
         continue;
      }

      roadmap_line_from (line, &reference_position);
      real_square = roadmap_square_search (&reference_position);

      if (real_square != previous_square) {
         shape_count =
            roadmap_shape_in_square
            (real_square, &first_shape_line, &last_shape_line);
         previous_square = real_square;
      }

      if (roadmap_plugin_override_line (line, cfcc, fips)) continue;

      if (shape_count > 0 &&
            roadmap_shape_of_line (line, first_shape_line,
               last_shape_line,
               &first_shape, &last_shape) > 0) {

         found =
            roadmap_street_get_distance_with_shape
            (position, line, cfcc, first_shape, last_shape, &this);

      } else {
         found =
            roadmap_street_get_distance_no_shape
            (position, line, cfcc, &this);
      }

      if (found) {
         count = roadmap_street_replace (neighbours, count, max, &this);
      }
   }

   return count;
}


int roadmap_street_get_closest
       (const RoadMapPosition *position,
        int *categories, int categories_count,
        RoadMapNeighbour *neighbours, int max) {

   static int *fips = NULL;

   int i;
   int county;
   int county_count;
   int square;

   int count = 0;


   if (RoadMapRangeActive == NULL) return 0;

   county_count = roadmap_locator_by_position (position, &fips);

   /* - For each candidate county: */

   for (county = county_count - 1; county >= 0; --county) {

      /* -- Access the county's database. */

      if (roadmap_locator_activate (fips[county]) != ROADMAP_US_OK) continue;

      /* -- Look for the square the current location fits in. */

      square = roadmap_square_search (position);

      if (square >= 0) {

         /* The current location fits in one of the county's squares.
          * We might be in that county, search for the closest streets.
          */
         for (i = 0; i < categories_count; ++i) {

            count =
               roadmap_street_get_closest_in_square
                  (position, square, categories[i], neighbours, count, max);
         }
      }

      for (i = 0; i < categories_count; ++i) {

         count =
            roadmap_street_get_closest_in_long_lines
            (position, categories[i], neighbours, count, max);
      }
   }

   return count;
}


static int roadmap_street_check_street (int street, int line) {

   int i;
   int range_end;
   RoadMapRange *range;
   RoadMapRangeByStreet *by_street;


   if (RoadMapRangeActive == NULL) return -1;

   range     = RoadMapRangeActive->RoadMapAddr;
   by_street = RoadMapRangeActive->RoadMapByStreet + street;
   range_end = by_street->first_range + by_street->count_range;

   for (i = by_street->first_range; i < range_end; i++) {

      if ((range[i].line & (~CONTINUATION_FLAG)) == (unsigned)line) {
         return i;
      }
   }

   return -1;
}


static int roadmap_street_check_other_side (int street,
                                            int line,
                                            int first_range) {

   int range_end;
   RoadMapRange *range;
   RoadMapRangeByStreet *by_street;


   if (RoadMapRangeActive == NULL) return -1;

   range     = RoadMapRangeActive->RoadMapAddr;
   by_street = RoadMapRangeActive->RoadMapByStreet + street;
   range_end = by_street->first_range + by_street->count_range;

   if (HAS_CONTINUATION ((range + first_range))) {
      first_range++;
   }
   
   first_range++;

   if (first_range >= range_end) return -1;

   if ((range[first_range].line & (~CONTINUATION_FLAG)) == (unsigned)line) {
         return first_range;
   }

   return -1;
}

static void roadmap_street_extract_range (int range_index,
                                          RoadMapStreetRange *range) {
   
   RoadMapRange *this_address =
      &(RoadMapRangeActive->RoadMapAddr[range_index]);

   if (HAS_CONTINUATION(this_address)) {

      range->fradd =
         ((int)(this_address->fradd) & 0xffff)
         + (((int)(this_address[1].fradd) & 0xffff) << 16);
      range->toadd =
         ((int)(this_address->toadd) & 0xffff)
         + (((int)(this_address[1].toadd) & 0xffff) << 16);

   } else {

      range->fradd = this_address->fradd;
      range->toadd = this_address->toadd;
   }

}

static int roadmap_street_get_city (int street, int range) {

   int i;
   int next;

   RoadMapRangeByStreet *by_street =
      RoadMapRangeActive->RoadMapByStreet + street;

   RoadMapRangeByCity *by_city =
      RoadMapRangeActive->RoadMapByCity + by_street->first_city;

   int range_end = by_street->first_range + by_street->count_range;


   for (i = by_street->first_range; i < range_end; i = next) {

      next = i + by_city->count;

      if (range >= i && range < next) {
         return by_city->city; /* We found the city we was looking for. */
      }
      by_city += 1;
   }

   return 0; /* No city. */
}


#define STREET_HASH_MODULO  513

typedef struct {

   RoadMapRange  *range;
   unsigned short next;
   unsigned short side;

} RoadMapRangesHashEntry;

typedef struct {

   RoadMapRangesHashEntry *list;
   int list_size;
   int list_cursor;

   unsigned short head[STREET_HASH_MODULO];

} RoadMapRangesHash;

static int roadmap_street_hash_code (RoadMapPosition *position) {

   int hash_code =
          (position->latitude - position->longitude) % STREET_HASH_MODULO;

   if (hash_code < 0) return 0 - hash_code;

   return hash_code;
}

static RoadMapRange *roadmap_street_hash_search (RoadMapRangesHash *hash,
                                                 int erase,
                                                 RoadMapPosition *position) {

   int i;
   int line;
   RoadMapPosition position2;
   RoadMapRangesHashEntry *entry;

   int hash_code = roadmap_street_hash_code (position);


   for (i = hash->head[hash_code]; i < 0xffff; i = entry->next) {

       entry = hash->list + i;

       if (entry->side == 0xffff) continue; /* Already matched. */

       line = entry->range->line & (~CONTINUATION_FLAG);

       if (entry->side == 0) {
          roadmap_line_from(line, &position2);
       } else {
          roadmap_line_to(line, &position2);
       }

       if (position2.latitude == position->latitude &&
           position2.longitude == position->longitude) {
          if (erase) {
             entry->side = 0xffff;
          }
          return entry->range;
       }
   }
   return NULL;
}

static void roadmap_street_hash_add (RoadMapRangesHash *hash,
                                     RoadMapRange *this_addr,
                                     int side,
                                     RoadMapPosition *position) {

   if (roadmap_street_hash_search (hash, 0, position) == NULL) {

       int hash_code = roadmap_street_hash_code (position);

       hash->list[hash->list_cursor].range = this_addr;
       hash->list[hash->list_cursor].side  = side;
       hash->list[hash->list_cursor].next = hash->head[hash_code];
       hash->head[hash_code] = hash->list_cursor;
       hash->list_cursor += 1;
   }
}

static int roadmap_street_intersection_county
               (int fips,
                RoadMapStreetIdentifier *street1,
                RoadMapStreetIdentifier *street2,
                RoadMapStreetIntersection *intersection,
                int count) {

   static int Initialized = 0;
   static RoadMapRangesHash Hash;

   int i, j;
   int line;
   int results = 0;
   int range_end;

   RoadMapPosition position;


   /* First loop: build a hash table of all the street block endpoint
    * positions for the 1st street.
    */
   if (! Initialized) {
      Hash.list = NULL;
      Hash.list_size = 0;
      Hash.list_cursor = 1; /* Force an initial reset. */
      Initialized = 1;
   }

   if (Hash.list_cursor > 0) {

      /* This hash has been used before: reset it. */

      for (i = STREET_HASH_MODULO - 1; i >= 0; --i) {
         Hash.head[i] = 0xffff;
      }
      Hash.list_cursor = 0;
   }

   for (i = RoadMapRangeActive->RoadMapByStreetCount - 1; i >= 0; --i) {

       RoadMapStreet *this_street = RoadMapStreetActive->RoadMapStreets + i;

       if (this_street->fename == street1->name &&
           (street1->prefix == 0 || this_street->fedirp == street1->prefix) &&
           (street1->suffix == 0 || this_street->fedirs == street1->suffix) &&
           (street1->type   == 0 || this_street->fetype == street1->type)) {

           RoadMapRangeByStreet *by_street;

           by_street = RoadMapRangeActive->RoadMapByStreet + i;

           if (by_street->count_range > 0) {

               int required = 2 * (Hash.list_cursor + by_street->count_range);

               range_end = by_street->first_range + by_street->count_range;

               if (required > Hash.list_size) {

                  Hash.list_size = required * 2;
                  if (Hash.list_size >= 0xffff) break;

                  Hash.list =
                      realloc (Hash.list,
                               Hash.list_size * sizeof(RoadMapRangesHashEntry));
                  roadmap_check_allocated(Hash.list);
               }

               for (j = by_street->first_range; j < range_end; ++j) {

                   RoadMapRange *this_addr =
                      RoadMapRangeActive->RoadMapAddr + j;

                   if (HAS_CONTINUATION(this_addr)) ++j;

                   line = this_addr->line & (~CONTINUATION_FLAG);

                   roadmap_line_from(line, &position);
                   roadmap_street_hash_add (&Hash, this_addr, 0, &position);

                   roadmap_line_to(line, &position);
                   roadmap_street_hash_add (&Hash, this_addr, 1, &position);
               }
           }
       }
   }
   if (Hash.list_cursor == 0) return 0;


   /* Second loop: match each street block endpoint positions for the 2nd
    * street with the positions for the 1st street.
    */
   for (i = RoadMapRangeActive->RoadMapByStreetCount - 1; i >= 0; --i) {

       RoadMapStreet *this_street = RoadMapStreetActive->RoadMapStreets + i;

       if (this_street->fename == street2->name &&
           (street2->prefix == 0 || this_street->fedirp == street2->prefix) &&
           (street2->suffix == 0 || this_street->fedirs == street2->suffix) &&
           (street2->type   == 0 || this_street->fetype == street2->type)) {

           RoadMapRangeByStreet *by_street;

           by_street = RoadMapRangeActive->RoadMapByStreet + i;

           if (by_street->count_range > 0) {

               range_end = by_street->first_range + by_street->count_range;

               for (j = by_street->first_range; j < range_end; ++j) {

                   RoadMapRange *this_addr1;
                   RoadMapRange *this_addr2 =
                      RoadMapRangeActive->RoadMapAddr + j;

                   if (HAS_CONTINUATION(this_addr2)) ++j;

                   line = this_addr2->line & (~CONTINUATION_FLAG);

                   roadmap_line_from(line, &position);
                   this_addr1 =
                       roadmap_street_hash_search (&Hash, 1, &position);

                   if (this_addr1 == NULL) {
                      roadmap_line_to(line, &position);
                      this_addr1 =
                          roadmap_street_hash_search (&Hash, 1, &position);
                   }

                   if (this_addr1 != NULL) {

                      intersection[results].fips = fips;
                      intersection[results].line1 =
                          this_addr1->line & (~CONTINUATION_FLAG);
                      intersection[results].line2 =
                          this_addr2->line & (~CONTINUATION_FLAG);
                      intersection[results].position = position;

                      ++results;
                      if (results >= count) return results;
                   }
               }
           }
       }
   }

   return results;
}


int roadmap_street_intersection (const char *state,
                                 const char *street1_name,
                                 const char *street2_name,
                                 RoadMapStreetIntersection *intersection,
                                 int count) {

   static int *fips = NULL;

   int i;
   int results = 0;
   int county_count;

   RoadMapStreetIdentifier street1, street2;


   if (RoadMapRangeActive == NULL) return 0;

   county_count = roadmap_locator_by_state (state, &fips);

   for (i = county_count - 1; i >= 0; --i) {

      if (roadmap_locator_activate (fips[i]) != ROADMAP_US_OK) continue;

      roadmap_street_locate (street1_name, &street1);
      if (street1.name == ROADMAP_INVALID_STRING) continue;

      roadmap_street_locate (street2_name, &street2);
      if (street2.name == ROADMAP_INVALID_STRING) continue;

      results += roadmap_street_intersection_county
                     (fips[i], &street1, &street2,
                      intersection + results,
                      count - results);
   }

   return results;
}


void roadmap_street_get_properties
        (int line, RoadMapStreetProperties *properties) {

   RoadMapRangeBySquare *bysquare;
   RoadMapRangeNoAddress *noaddr;

   int range_index = -1;
   int hole;
   int stop;
   int street;
   int square;
   RoadMapPosition position;


   memset (properties, 0, sizeof(RoadMapStreetProperties));
   properties->street = -1;

   if (RoadMapRangeActive == NULL) return;

   roadmap_line_from (line, &position);
   square = roadmap_square_search (&position);

   if (square >= 0) {
      square = roadmap_square_index (square);
   }

   if (square >= 0) {

      bysquare = RoadMapRangeActive->bysquare + square;

      street = 0;

      for (hole = 0; hole < ROADMAP_RANGE_HOLES; hole++) {

         stop = street + bysquare->hole[hole].included;

         while (street < stop) {

            range_index = roadmap_street_check_street (street, line);
            if (range_index >= 0) {
               goto found_street_with_address;
            }
            street += 1;
         }

         street += bysquare->hole[hole].excluded;
      }

      stop = RoadMapRangeActive->RoadMapByStreetCount;

      while (street < stop) {

         range_index = roadmap_street_check_street (street, line);
         if (range_index >= 0) {
            goto found_street_with_address;
         }
         street += 1;
      }

      /* Could not find the street: maybe it has no address. */

      noaddr = RoadMapRangeActive->noaddr;
      stop   = bysquare->noaddr_start + bysquare->noaddr_count;

      for (street = bysquare->noaddr_start; street < stop; street++) {

         if (noaddr[street].line == line) {
            street = noaddr[street].street;
            goto found_street;
         }
      }
   }

   return; /* Really found nothing. */


found_street_with_address:

    {

        roadmap_street_extract_range (range_index, &properties->first_range);

        range_index =
           roadmap_street_check_other_side (street, line, range_index);
        if (range_index != -1) {
            roadmap_street_extract_range
               (range_index, &properties->second_range);
        } else {
           properties->second_range.fradd =
              properties->second_range.toadd = -1;
        }
    }
    properties->city = roadmap_street_get_city (street, range_index);

found_street:

   properties->street = street;
}


static void roadmap_street_append (char *name, char *item) {
    
    if (item != NULL && item[0] != 0) {
        strcat (name, item);
        strcat (name, " ");
    }
}


const char *roadmap_street_get_street_address
                (const RoadMapStreetProperties *properties) {

    static char RoadMapStreetAddress [32];
    int min;
    int max;

    if (properties->first_range.fradd == -1) {
        return "";
    }

    if (properties->first_range.fradd > properties->first_range.toadd) {
       min = properties->first_range.toadd;
       max = properties->first_range.fradd;
    } else {
       min = properties->first_range.fradd;
       max = properties->first_range.toadd;
    }

    if (properties->second_range.fradd != -1) {

       if (properties->second_range.fradd < min) {
          min = properties->second_range.fradd;
       }
       if (properties->second_range.fradd > max) {
          max = properties->second_range.fradd;
       }
       if (properties->second_range.toadd < min) {
          min = properties->second_range.toadd;
       }
       if (properties->second_range.toadd > max) {
          max = properties->second_range.toadd;
       }
    }

    sprintf (RoadMapStreetAddress, "%d - %d", min, max);

    return RoadMapStreetAddress;
}


const char *roadmap_street_get_street_name
                (const RoadMapStreetProperties *properties) {

    static char RoadMapStreetName [512];

    char *p;
    RoadMapStreet *this_street;

    
    if (properties->street < 0) {

        return "";
    }
    
    this_street =
        RoadMapStreetActive->RoadMapStreets + properties->street;
    
    RoadMapStreetName[0] = 0;

    roadmap_street_append
        (RoadMapStreetName,
         roadmap_dictionary_get
            (RoadMapRangeActive->RoadMapStreetPrefix, this_street->fedirp));
        
    roadmap_street_append
        (RoadMapStreetName,
         roadmap_dictionary_get
            (RoadMapRangeActive->RoadMapStreetNames, this_street->fename));
        
    roadmap_street_append
        (RoadMapStreetName,
         roadmap_dictionary_get
            (RoadMapRangeActive->RoadMapStreetType, this_street->fetype));
        
    roadmap_street_append
        (RoadMapStreetName,
         roadmap_dictionary_get
            (RoadMapRangeActive->RoadMapStreetSuffix, this_street->fedirs));

    /* trim the resulting string on both sides (right then left): */

    p = RoadMapStreetName + strlen(RoadMapStreetName) - 1;
    while (*p == ' ') *(p--) = 0;

    for (p = RoadMapStreetName; *p == ' '; p++) ;

    return p;
}


const char *roadmap_street_get_city_name
                (const RoadMapStreetProperties *properties) {
    
    if (RoadMapRangeActive == NULL) return 0;

    return roadmap_dictionary_get
                (RoadMapRangeActive->RoadMapCityNames, properties->city);
}


const char *roadmap_street_get_full_name
                (const RoadMapStreetProperties *properties) {
    
    static char RoadMapStreetName [512];

    const char *address;
    const char *city;


    if (properties->street <= 0) {
        return "";
    }

    address = roadmap_street_get_street_address (properties);
    city    = roadmap_street_get_city_name      (properties);
    
    snprintf (RoadMapStreetName, sizeof(RoadMapStreetName),
              "%s%s%s%s%s",
              address,
              (address[0])? " " : "",
              roadmap_street_get_street_name (properties),
              (city[0])? ", " : "",
              city);
    
    return RoadMapStreetName;
}


const char *roadmap_street_get_street_fename
                (const RoadMapStreetProperties *properties) {

    RoadMapStreet *this_street;

    if (properties->street < 0) {

        return "";
    }
    
    this_street =
        RoadMapStreetActive->RoadMapStreets + properties->street;
    
    return
         roadmap_dictionary_get
            (RoadMapRangeActive->RoadMapStreetNames, this_street->fename);
}


const char *roadmap_street_get_street_fetype
                (const RoadMapStreetProperties *properties) {

    RoadMapStreet *this_street;

    if (properties->street < 0) {

        return "";
    }
    
    this_street =
        RoadMapStreetActive->RoadMapStreets + properties->street;
    
    return
         roadmap_dictionary_get
            (RoadMapRangeActive->RoadMapStreetType, this_street->fetype);
}


const char *roadmap_street_get_street_fedirs
                (const RoadMapStreetProperties *properties) {
                
    RoadMapStreet *this_street;

    if (properties->street < 0) {

        return "";
    }
    
    this_street =
        RoadMapStreetActive->RoadMapStreets + properties->street;
    
    return
         roadmap_dictionary_get
            (RoadMapRangeActive->RoadMapStreetType, this_street->fedirs);
}


const char *roadmap_street_get_street_fedirp
                (const RoadMapStreetProperties *properties) {
                
    RoadMapStreet *this_street;

    if (properties->street < 0) {

        return "";
    }
    
    this_street =
        RoadMapStreetActive->RoadMapStreets + properties->street;
    
    return
         roadmap_dictionary_get
            (RoadMapRangeActive->RoadMapStreetType, this_street->fedirp);
}


const char *roadmap_street_get_street_t2s
                (const RoadMapStreetProperties *properties) {

    RoadMapStreet *this_street;

    if (RoadMapRangeActive->RoadMapText2Speech == NULL) return "";

    if (properties->street < 0) {
        return "";
    }
    
    this_street =
        RoadMapStreetActive->RoadMapStreets + properties->street;

    return
        roadmap_dictionary_get
            (RoadMapRangeActive->RoadMapText2Speech, this_street->t2s);
}


const char *roadmap_street_get_street_city
                (const RoadMapStreetProperties *properties, int side) {

//TODO add street sides search

    return roadmap_street_get_city_name (properties);
}


const char *roadmap_street_get_street_zip
                (const RoadMapStreetProperties *properties, int side) {
//TODO add implement get_street_zip
   return "";
}


void roadmap_street_get_street_range
   (const RoadMapStreetProperties *properties, int range, int *from, int *to) {

   if (range == 1) {
      *from = properties->first_range.fradd;
      *to = properties->first_range.toadd;
      
   } else if (range == 2) {
      *from = properties->second_range.fradd;
      *to = properties->second_range.toadd;

   } else {
      roadmap_log (ROADMAP_ERROR, "Illegal range number: %d", range);
   }
}


static int roadmap_street_search_results (RoadMapString index,
                                          const char *string,
                                          void *data) {

   RoadMapStreetIdentifier street = {0, index, 0, 0};
   RoadMapBlocks blocks[MAX_SEARCH_NAMES];
   StreetSearchContext *context = (StreetSearchContext *)data;
   int i;
   int count;

   if (RoadMapStreetSearchCount == MAX_SEARCH_NAMES) return 0;

   count = roadmap_street_block_by_county_subdivision
                          (&street, context->city, blocks, MAX_SEARCH_NAMES);

   if (count <= 0) return 1;

   for (i=0; i < count; i++) {

      char tmp[255];
      RoadMapStreet *this_street;

      this_street =
         RoadMapStreetActive->RoadMapStreets + blocks[i].street;
    
      if ((context->prefix != ROADMAP_INVALID_STRING) &&
          (context->prefix != this_street->fedirp)) continue;

      snprintf (tmp, sizeof(tmp), "%s %s, %s",
         roadmap_dictionary_get
            (RoadMapRangeActive->RoadMapStreetPrefix, this_street->fedirp),
         roadmap_dictionary_get
            (RoadMapRangeActive->RoadMapStreetNames, this_street->fename),
         roadmap_dictionary_get
                (RoadMapRangeActive->RoadMapCityNames, blocks[i].city));

      if (RoadMapStreetSearchNames[RoadMapStreetSearchCount]) {
         free (RoadMapStreetSearchNames[RoadMapStreetSearchCount]);
      }

      RoadMapStreetSearchNames[RoadMapStreetSearchCount++] = strdup(tmp);
      if (RoadMapStreetSearchCount == MAX_SEARCH_NAMES) return 0;
   }

   return 1;
}

static int roadmap_street_prefix_search_results (RoadMapString index,
                                                 const char *string,
                                                 void *context) {
   char str[255];

   if (RoadMapStreetSearchCount == MAX_SEARCH_NAMES) return 0;

   snprintf (str, sizeof(str), "%s - ", string);

   if (RoadMapStreetSearchNames[RoadMapStreetSearchCount]) {
      free (RoadMapStreetSearchNames[RoadMapStreetSearchCount]);
   }

   RoadMapStreetSearchNames[RoadMapStreetSearchCount++] = strdup(str);

   if (RoadMapStreetSearchCount == MAX_SEARCH_NAMES) return 0;

   return 1;
}


int roadmap_street_search (const char *city, const char *str,
                           RoadMapDictionaryCB cb,
                           void *data) {

   int i;
   int count = 0;

   StreetSearchContext context =
      {ROADMAP_INVALID_STRING, ROADMAP_INVALID_STRING};

   RoadMapDictionaryMask mask;
   char *space;

   if (!strlen(city)) {
      context.city = -1;
      mask = NULL;
   } else {
      context.city = roadmap_dictionary_locate
         (RoadMapRangeActive->RoadMapCityNames, city);

      if (context.city == ROADMAP_INVALID_STRING) return 0;
      mask = roadmap_street_get_city_search_mask (context.city);
   }

   RoadMapStreetSearchCount = 0;

   /* Search for a prefix. If found, remove the prefix from the name
    * by shifting the name's pointer.
    */
   space = strchr (str, ' ');

   if (space != NULL) {

      unsigned  length;
      char  buffer[255];

      length = (unsigned)(space - str);

      if (length < sizeof(buffer)) {

         strncpy (buffer, str, length);
         buffer[length] = 0;

         context.prefix = roadmap_dictionary_locate
                            (RoadMapRangeActive->RoadMapStreetPrefix, buffer);
         if (context.prefix != ROADMAP_INVALID_STRING) {
            str = str + length;
            while (*str == ' ') ++str;
         }
      }
   }

   count += roadmap_dictionary_search_all
                      (RoadMapRangeActive->RoadMapStreetNames,
                       mask,
                       str,
                       1,
                       cb != NULL ? roadmap_street_search_results : NULL,
                       (void *)&context);

   if (context.prefix == ROADMAP_INVALID_STRING) {
      count += roadmap_dictionary_search_all
                      (RoadMapRangeActive->RoadMapStreetPrefix,
                       NULL,
                       str,
                       0,
                       cb != NULL ? roadmap_street_prefix_search_results : NULL,
                       NULL);
   }

   for (i=0; i<RoadMapStreetSearchCount; i++) {
      (*cb) (-1, RoadMapStreetSearchNames[i], data);
   }

   return count;
}

