/* buildmap_range.c - Build a street address range table & index for RoadMap.
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
 *   void buildmap_range_initialize (void);
 *   int  buildmap_range_add
 *           (int line, int street, int fradd, int toadd, RoadMapZip zip);
 *   void buildmap_range_add_place (RoadMapString place, RoadMapString city);
 *   void buildmap_range_sort (void);
 *   void buildmap_range_save (void);
 *   void buildmap_range_print (int index);
 *   void buildmap_range_summary (void);
 *   void buildmap_range_reset   (void);
 *
 * These functions are used to build a table of street ranges from
 * the Tiger maps. The objective is double: (1) reduce the size of
 * the Tiger data by sharing all duplicated information and
 * (2) produce the index data to serve as the basis for a fast
 * search mechanism for streets in RoadMap.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "roadmap_db_range.h"

#include "roadmap_hash.h"

#include "buildmap.h"
#include "buildmap_zip.h"
#include "buildmap_range.h"
#include "buildmap_line.h"
#include "buildmap_street.h"


typedef struct {

   int street;
   RoadMapZip zip;
   RoadMapString city;

   int line;
   int fradd;
   int toadd;

} BuildMapRange;


static int RangeDuplicates = 0;

static int RangeCount = 0;
static int RangeMaxStreet = 0;
static BuildMapRange *Range[BUILDMAP_BLOCK] = {NULL};

static int RangePlaceCount = 0;
static RoadMapRangePlace *RangePlace[BUILDMAP_BLOCK] = {NULL};

static RoadMapHash *RangeByLine          = NULL;
static RoadMapHash *RangePlaceByPlace    = NULL;
static RoadMapHash *RangeNoAddressByLine = NULL;

static int RangeAddCount = 0;

static int *SortedRange = NULL;
static int *SortedNoAddress = NULL;

void buildmap_range_initialize (void) {

   RangeByLine       = roadmap_hash_new ("RangeByLine",       BUILDMAP_BLOCK);
   RangePlaceByPlace = roadmap_hash_new ("RangePlaceByPlace", BUILDMAP_BLOCK);
   RangeNoAddressByLine =
      roadmap_hash_new ("RangeNoAddressByLine", BUILDMAP_BLOCK);

   RangeMaxStreet = 0;
   RangeAddCount = 0;
   RangeCount = 0;
}


static BuildMapRange *buildmap_range_new (void) {

   int block = RangeCount / BUILDMAP_BLOCK;
   int offset = RangeCount % BUILDMAP_BLOCK;

   if (block >= BUILDMAP_BLOCK) {
      buildmap_fatal (0, "too many range records");
   }

   if (Range[block] == NULL) {

      /* We need to add a new block to the table. */

      Range[block] = calloc (BUILDMAP_BLOCK, sizeof(BuildMapRange));
      if (Range[block] == NULL) {
         buildmap_fatal (0, "no more memory");
      }
      roadmap_hash_resize (RangeByLine,   (block+1) * BUILDMAP_BLOCK);
   }

   RangeCount += 1;

   return Range[block] + offset;
}


void buildmap_range_merge (int frleft,  int toleft,
                           int frright, int toright,
                           int *from,   int *to) {

   int fradd = frright;
   int toadd = toright;

   if (fradd < toadd) {
      if (fradd > frleft) fradd = frleft;
      if (toadd < toleft) toadd = toleft;

      /* The RoadMap logic requires that the merged from and to
       * have different odd/even properties: this is how RoadMap
       * knows the range covers both sides of the street.
       * So we make the range somewhat larger by one.
       */
      if ((fradd & 1) == (toadd & 1)) {
         if ((fradd & 1) == 1) --fradd;
         if ((toadd & 1) == 0) ++toadd;
      }

   } else {

      if (fradd < frleft) fradd = frleft;
      if (toadd > toleft) toadd = toleft;

      /* The RoadMap logic requires that the merged from and to
       * have different odd/even properties: this is how RoadMap
       * knows the range covers both sides of the street.
       * So we make the range somewhat larger by one.
       */
      if ((fradd & 1) == (toadd & 1)) {
         if ((fradd & 1) == 0) ++fradd;
         if ((toadd & 1) == 1) --toadd;
      }
   }

   *from = fradd;
   *to   = toadd;
}


void buildmap_range_add_no_address (int line, int street) {
   buildmap_range_add (line, street, 0, 0, 0, 0);
}


int buildmap_range_add
       (int line, int street,
        int fradd, int toadd, RoadMapZip zip, RoadMapString city) {

   int index;
   BuildMapRange *this_range;


   if (line < 0) {
      buildmap_fatal (0, "negative line index");
   }
   if (street < 0) {
      buildmap_fatal (0, "negative street index");
   }
   if (fradd < 0 || toadd < 0) {
      buildmap_fatal (0, "negative street address");
   }

   RangeAddCount += 1;

   /* First search if that range is not known yet. */

   for (index = roadmap_hash_get_first (RangeByLine, line);
        index >= 0;
        index = roadmap_hash_get_next (RangeByLine, index)) {

       if (index >= RangeCount) {
          buildmap_fatal (0, "hash returned out of range index");
       }

       this_range = Range[index / BUILDMAP_BLOCK] + (index % BUILDMAP_BLOCK);

       if ((this_range->street == street) &&
           (this_range->zip    == zip   ) &&
           (this_range->city   == city  ) &&
           ((this_range->line & 0x7fffffff) == line  ) &&
           (this_range->fradd  == fradd ) &&
           (this_range->toadd  == toadd )) {

          return index;
        }
   }

   /* This Range was not known yet: create a new one. */

   index = RangeCount;

   this_range = buildmap_range_new();

   this_range->street = street;
   this_range->zip    = zip;
   this_range->city   = city;
   this_range->fradd  = fradd;
   this_range->toadd  = toadd;

   this_range->line  = line;

   roadmap_hash_add (RangeByLine,   line,   index);

   if (fradd > 0xffff || toadd > 0xffff) {

      /* because the street numbers do not fit into a 16 bit integer,
       * we must use an extension record for the high order bits.
       */

      this_range->line |= CONTINUATION_FLAG;

      this_range = buildmap_range_new();

      this_range->line   = line;
      this_range->street = street;
      this_range->city   = city;
      this_range->zip    = zip;
      this_range->fradd  = fradd;
      this_range->toadd  = toadd;
   }


   if (street > RangeMaxStreet) {
      RangeMaxStreet = street;
   }
   if (street < 0) {
      buildmap_fatal (0, "negative street index");
   }

   return index;
}


void buildmap_range_add_place (RoadMapString place, RoadMapString city) {

   int index;
   RoadMapRangePlace *this_place;

   int block = RangePlaceCount / BUILDMAP_BLOCK;
   int offset = RangePlaceCount % BUILDMAP_BLOCK;


   if (city <= 0) {
      return;
   }

   if (place <= 0) {
      return;
   }

   /* First search if that place is not known yet. */

   for (index = roadmap_hash_get_first (RangePlaceByPlace, place);
        index >= 0;
        index = roadmap_hash_get_next (RangePlaceByPlace, index)) {

       if (index >= RangePlaceCount) {
          buildmap_fatal (0, "hash returned out of range index");
       }

       this_place = RangePlace[index / BUILDMAP_BLOCK]
                            + (index % BUILDMAP_BLOCK);

       if ((this_place->place == place) && (this_place->city == city)) {
          return;
       }
   }

   if (block >= BUILDMAP_BLOCK) {
      buildmap_fatal (0, "too many place records");
   }

   if (RangePlace[block] == NULL) {

      /* We need to add a new block to the table. */

      RangePlace[block] = calloc (BUILDMAP_BLOCK, sizeof(RoadMapRangePlace));
      if (RangePlace[block] == NULL) {
         buildmap_fatal (0, "no more memory");
      }
      roadmap_hash_resize (RangePlaceByPlace, (block+1) * BUILDMAP_BLOCK);
   }

   this_place = RangePlace[block] + offset;

   this_place->place = place;
   this_place->city = city;

   roadmap_hash_add (RangePlaceByPlace, place, RangePlaceCount);

   RangePlaceCount += 1;
}


void buildmap_range_print (FILE *file, int index) {

   BuildMapRange *this_range;
   BuildMapDictionary cities = buildmap_dictionary_open ("city");

   this_range = Range[index/BUILDMAP_BLOCK] + (index % BUILDMAP_BLOCK);

   fprintf (file, "%d to %d ", this_range->fradd, this_range->toadd);

   buildmap_street_print_sorted (file, this_range->street);

   fprintf (file, ", city %s, zip %d, line %d\n",
            buildmap_dictionary_get (cities, this_range->city),
            buildmap_zip_get_zip_code (this_range->zip),
            buildmap_line_get_id_sorted
               (this_range->line & (~ CONTINUATION_FLAG)));
}


static int buildmap_range_compare (const void *r1, const void *r2) {

   int result;

   int i1 = *((int *)r1);
   int i2 = *((int *)r2);

   BuildMapRange *record1;
   BuildMapRange *record2;

   record1 = Range[i1/BUILDMAP_BLOCK] + (i1 % BUILDMAP_BLOCK);
   record2 = Range[i2/BUILDMAP_BLOCK] + (i2 % BUILDMAP_BLOCK);

   result = record1->street - record2->street;

   if (result != 0) {
      return result;
   }

   result = (int) (record1->city) - (int) (record2->city);

   if (result != 0) {
      return result;
   }

   result =
      buildmap_line_get_sorted (record1->line & (~ CONTINUATION_FLAG)) -
      buildmap_line_get_sorted (record2->line & (~ CONTINUATION_FLAG));

   if (result != 0) {
      return result;
   }

   /* Preserve the order of range insertion */

   return i1 - i2;
}


void buildmap_range_sort (void) {

   int i;
   BuildMapRange *this_range;

   if (SortedRange != NULL) return; /* Sort was already performed. */


   buildmap_line_sort();
   buildmap_street_sort();

   buildmap_info ("sorting ranges...");

   SortedRange = malloc (RangeCount * sizeof(int));
   if (SortedRange == NULL) {
      buildmap_fatal (0, "no more memory");
   }

   for (i = 0; i < RangeCount; i++) {

      int line;

      SortedRange[i] = i;
      this_range = Range[i/BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);

      this_range->street = buildmap_street_get_sorted (this_range->street);

      line = buildmap_line_get_sorted
                  (this_range->line & (~ CONTINUATION_FLAG));

      buildmap_line_set_street_sorted (line, this_range->street);
      this_range->line = line | (this_range->line & CONTINUATION_FLAG);
   }

   qsort (SortedRange, RangeCount, sizeof(int), buildmap_range_compare);
}


void  buildmap_range_save (void) {

   int i;
   int unsorted_index = -1;
   int street_index;

   int city_index;
   int city_current;
   int city_count;

   int zip_index;
   int zip_current;
   int zip_count;

   int first_range_in_city;
   int first_range;
   int is_continuation;

   BuildMapRange         *this_range;

   RoadMapRangeByZip     *db_zip;
   RoadMapRange          *db_ranges;
   RoadMapRangeByStreet  *db_streets;
   RoadMapRangeByCity    *db_city;
   RoadMapRangePlace     *db_place;

   buildmap_db *root;
   buildmap_db *table_street;
   buildmap_db *table_addr;
   buildmap_db *table_zip;
   buildmap_db *table_city;
   buildmap_db *table_place;

   root  = buildmap_db_add_section (NULL, "range");
   if (root == NULL) buildmap_fatal (0, "Can't add a new section");

   buildmap_info ("saving ranges...");

   /* Compute the space required for the "byCity" & "byzip" indexes. */

   street_index = -1;

   city_current = -1;
   city_count = 0;

   zip_current = -1;
   zip_count = 0;

   for (i = 0; i < RangeCount; i++) {

      unsorted_index = SortedRange[i];
      this_range = Range[unsorted_index / BUILDMAP_BLOCK]
                      + (unsorted_index % BUILDMAP_BLOCK);

      if (this_range->street != street_index) {

         street_index = this_range->street;
         city_current = -1;
         zip_current  = -1;
      }

      if (this_range->city != city_current) {

         city_current = this_range->city;
         city_count += 1;
      }

      if (this_range->zip != zip_current) {

         zip_current = this_range->zip;
         zip_count += 1;
      }
   }


   /* Create the database space. */

   table_street = buildmap_db_add_child 
      (root, "bystreet", buildmap_street_count(), sizeof(RoadMapRangeByStreet));

   table_city = buildmap_db_add_child
                  (root, "bycity", city_count, sizeof(RoadMapRangeByCity));

   table_place = buildmap_db_add_child
                  (root, "place", RangePlaceCount, sizeof(RoadMapRangePlace));

   table_zip = buildmap_db_add_child
                  (root, "byzip", zip_count, sizeof(RoadMapRangeByZip));

   table_addr = buildmap_db_add_child
                  (root, "addr", RangeCount, sizeof(RoadMapRange));

   db_streets = (RoadMapRangeByStreet *) buildmap_db_get_data (table_street);
   db_city    = (RoadMapRangeByCity *) buildmap_db_get_data (table_city);
   db_place   = (RoadMapRangePlace *) buildmap_db_get_data (table_place);
   db_ranges  = (RoadMapRange *) buildmap_db_get_data (table_addr);
   db_zip     = (RoadMapRangeByZip *) buildmap_db_get_data (table_zip);


   /* Fill in the index and the range data. */

   street_index = -1;
   city_index   = -1;
   zip_index    = -1;

   first_range  = 0;
   first_range_in_city   = 0;

   city_current = -1;
   zip_current  = -1;

   is_continuation = 0;

   for (i = 0; i < RangeCount; i++) {

      unsorted_index = SortedRange[i];

      this_range = Range[unsorted_index / BUILDMAP_BLOCK]
                      + (unsorted_index % BUILDMAP_BLOCK);

      if (this_range->street != street_index) {

         if (street_index > this_range->street) {
            buildmap_fatal (0, "inconsistent range order");
         }

         if (street_index >= 0) {
            db_streets[street_index].count_range = i - first_range;
         }

         street_index += 1;

         while (street_index < this_range->street) {
            db_streets[street_index].first_range = 0;
            db_streets[street_index].first_city  = 0;
#ifndef J2MEMAP
            db_streets[street_index].first_zip   = 0;
#endif            
            db_streets[street_index].count_range = 0;
            street_index += 1;
         }

         db_streets[street_index].first_range = i;
         db_streets[street_index].first_city  = city_index + 1;
#ifndef J2MEMAP
         db_streets[street_index].first_zip   = zip_index + 1;
#endif         
         first_range = i;

         city_current = -1; /* Force a city change. */
         zip_current  = -1;
      }

      if (this_range->city != city_current) {

         if (city_index >= 0) {

            if (i - first_range_in_city < 0) {
               buildmap_fatal (0, "negative count");
            }
            if (i - first_range_in_city >= 0x10000) {
               buildmap_fatal (0, "too many street in a single city");
            }
         }

         city_index += 1;

         city_current = this_range->city;
         first_range_in_city  = i;

         db_city[city_index].city = this_range->city;
         db_city[city_index].count = 0;
      }
      db_city[city_index].count += 1;

      if (this_range->zip != zip_current) {

         zip_index += 1;

         db_zip[zip_index].count = 0;
         db_zip[zip_index].zip = this_range->zip;

         zip_current = this_range->zip;
      }
      db_zip[zip_index].count += 1;

      db_ranges[i].line = this_range->line;

      if (is_continuation) {
         db_ranges[i].fradd = (short)((this_range->fradd >> 16) & 0xffff);
         db_ranges[i].toadd = (short)((this_range->toadd >> 16) & 0xffff);
         is_continuation = 0;
      } else {
         db_ranges[i].fradd = (short)(this_range->fradd & 0xffff);
         db_ranges[i].toadd = (short)(this_range->toadd & 0xffff);
         is_continuation = HAS_CONTINUATION(this_range);
      }
   }

   db_streets[street_index].count_range = i - first_range;

   if (i - first_range_in_city < 0) {
      buildmap_fatal (0, "negative count");
   }
   if (i - first_range_in_city >= 0x10000) {
      buildmap_fatal (0, "too many street in a single city");
   }

   if (street_index >= buildmap_street_count()) {
      buildmap_fatal (0, "out of bound street");
   }

   for (i = 0; i < RangePlaceCount; i++) {
      db_place[i] = RangePlace[i / BUILDMAP_BLOCK][i % BUILDMAP_BLOCK];
   }

   if (switch_endian) {
      int i;

      for (i=0; i<buildmap_street_count(); i++) {
         switch_endian_int(&db_streets[i].first_range);
         switch_endian_int(&db_streets[i].first_city);
#ifndef J2MEMAP
         switch_endian_int(&db_streets[i].first_zip);
#endif
         switch_endian_int(&db_streets[i].count_range);
      }

      for (i=0; i<city_count; i++) {
         switch_endian_short(&db_city[i].city);
         switch_endian_short(&db_city[i].count);
      }

      for (i=0; i<RangePlaceCount; i++) {
         switch_endian_short(&db_place[i].place);
         switch_endian_short(&db_place[i].city);
      }

      for (i=0; i<zip_count; i++) {
         switch_endian_short(&db_zip[i].zip);
         switch_endian_short(&db_zip[i].count);
      }

      for (i=0; i<RangeCount; i++) {
         switch_endian_int(&db_ranges[i].line);
         switch_endian_short(&db_ranges[i].fradd);
         switch_endian_short(&db_ranges[i].toadd);
      }
   }
}


void buildmap_range_summary (void) {

   fprintf (stderr,
            "-- range table: %d items, %d add, %d bytes used, %d duplicates\n",
            RangeCount,
            RangeAddCount,
            RangeCount * sizeof(RoadMapRange),
            RangeDuplicates);
}


void buildmap_range_reset (void) {

   int i;

   RangeCount = 0;
   RangePlaceCount = 0;
   RangeMaxStreet = 0;

   for (i = 0; i < BUILDMAP_BLOCK; i++) {
      if (Range[i] != NULL) {
         free (Range[i]);
         Range[i] = NULL;
      }
   }

   for (i = 0; i < BUILDMAP_BLOCK; i++) {
      if (RangePlace[i] != NULL) {
         free (RangePlace[i]);
         RangePlace[i] = NULL;
      }
   }

   RangeByLine = NULL;
   RangePlaceByPlace = NULL;
   RangeNoAddressByLine = NULL;

   RangeAddCount = 0;

   free (SortedRange);
   SortedRange = NULL;

   free (SortedNoAddress);
   SortedNoAddress = NULL;
}

