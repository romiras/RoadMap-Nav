/* buildmap_line.c - Build a line table & index for RoadMap.
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
 *   void buildmap_line_initialize (void);
 *   int  buildmap_line_add (int tlid, int cfcc, int from, int to);
 *   void buildmap_line_sort (void);
 *   int  buildmap_line_get_sorted  (int line);
 *   void buildmap_line_find_sorted (int tlid);
 *   int  buildmap_line_get_id_sorted (int line);
 *   void buildmap_line_get_points_sorted (int line, int *from, int *to);
 *   void buildmap_line_get_position (int line, int *longitude, int *latitude);
 *   void buildmap_line_get_position_sorted
 *           (int line, int *longitude, int *latitude);
 *   void buildmap_line_get_square_sorted (int line);
 *   void buildmap_line_save    (void);
 *   void buildmap_line_summary (void);
 *   void buildmap_line_reset   (void);
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

#include "roadmap_db_line.h"

#include "roadmap_hash.h"

#include "buildmap.h"
#include "buildmap_point.h"
#include "buildmap_square.h"
#include "buildmap_line.h"


typedef struct {
   RoadMapLine record;
   int tlid;
   int sorted;
   int cfcc;
} BuildMapLine;


static int LineCount = 0;
static int LineTableSize = 0;
static int LineCrossingCount = 0;
static BuildMapLine *Line[BUILDMAP_BLOCK] = {NULL};

static RoadMapHash *LineById = NULL;

static int *SortedLine = NULL;
static int *SortedLine2 = NULL;


void buildmap_line_initialize (void) {

   LineById = roadmap_hash_new ("LineById", BUILDMAP_BLOCK);

   Line[0] = calloc (BUILDMAP_BLOCK, sizeof(BuildMapLine));
   if (Line[0] == NULL) {
      buildmap_fatal (0, "no more memory");
   }

   LineTableSize = 0;
   LineCount = 0;
}


int buildmap_line_add (int tlid, int cfcc, int from, int to) {

   int block;
   int offset;
   BuildMapLine *this_line;


   block = LineCount / BUILDMAP_BLOCK;
   offset = LineCount % BUILDMAP_BLOCK;

   if (Line[block] == NULL) {

      /* We need to add a new block to the table. */

      Line[block] = calloc (BUILDMAP_BLOCK, sizeof(BuildMapLine));
      if (Line[block] == NULL) {
         buildmap_fatal (0, "no more memory");
      }

      roadmap_hash_resize (LineById, (block+1) * BUILDMAP_BLOCK);
   }

   this_line = Line[block] + offset;

   if ((from < 0) || (to < 0)) {
     buildmap_fatal (0, "invalid points");
   }
   this_line->tlid = tlid;
   this_line->cfcc = cfcc;
   this_line->record.from = from;
   this_line->record.to   = to;

   roadmap_hash_add (LineById, tlid, LineCount);

   return LineCount++;
}


int  buildmap_line_find_sorted (int tlid) {

   int index;
   BuildMapLine *this_line;

   if (SortedLine == NULL) {
      buildmap_fatal (0, "lines not sorted yet");
   }

   for (index = roadmap_hash_get_first (LineById, tlid);
        index >= 0;
        index = roadmap_hash_get_next (LineById, index)) {

      this_line = Line[index / BUILDMAP_BLOCK] + (index % BUILDMAP_BLOCK);

      if (this_line->tlid == tlid) {
         return this_line->sorted;
      }
   }

   return -1;
}


static BuildMapLine *buildmap_line_get_record (int line) {

   if ((line < 0) || (line > LineCount)) {
      buildmap_fatal (0, "invalid line index %d", line);
   }

   return Line[line/BUILDMAP_BLOCK] + (line % BUILDMAP_BLOCK);
}


static BuildMapLine *buildmap_line_get_record_sorted (int line) {

   if ((line < 0) || (line > LineCount)) {
      buildmap_fatal (0, "invalid line index %d", line);
   }

   if (SortedLine == NULL) {
      buildmap_fatal (0, "lines not sorted yet");
   }

   line = SortedLine[line];

   return Line[line/BUILDMAP_BLOCK] + (line % BUILDMAP_BLOCK);
}


void buildmap_line_get_points_sorted (int line, int *from, int *to) {

   BuildMapLine *this_line = buildmap_line_get_record_sorted (line);

   *from = this_line->record.from;
   *to   = this_line->record.to;
}

void buildmap_line_get_position (int line, int *longitude, int *latitude) {

   BuildMapLine *this_line = buildmap_line_get_record (line);

   *longitude = buildmap_point_get_longitude (this_line->record.from);
   *latitude  = buildmap_point_get_latitude  (this_line->record.from);
}


void buildmap_line_get_position_sorted
          (int line, int *longitude, int *latitude) {

   BuildMapLine *this_line = buildmap_line_get_record_sorted (line);

   *longitude = buildmap_point_get_longitude_sorted (this_line->record.from);
   *latitude  = buildmap_point_get_latitude_sorted  (this_line->record.from);
}


int  buildmap_line_get_sorted (int line) {

   BuildMapLine *this_line = buildmap_line_get_record (line);

   if (SortedLine == NULL) {
      buildmap_fatal (0, "lines not sorted yet");
   }

   return this_line->sorted;
}


int buildmap_line_get_id_sorted (int line) {

   return buildmap_line_get_record_sorted(line)->tlid;
}


int buildmap_line_get_square_sorted (int line) {

   return buildmap_point_get_square_sorted
             (buildmap_line_get_record_sorted(line)->record.from);
}


static int buildmap_line_compare (const void *r1, const void *r2) {

   int square1;
   int square2;

   int index1 = *((int *)r1);
   int index2 = *((int *)r2);

   BuildMapLine *record1;
   BuildMapLine *record2;

   record1 = Line[index1/BUILDMAP_BLOCK] + (index1 % BUILDMAP_BLOCK);
   record2 = Line[index2/BUILDMAP_BLOCK] + (index2 % BUILDMAP_BLOCK);


   /* The lines are first sorted by square.
    * Within a square, lines are sorted by category.
    * Within a category, lines are sorted by the "from" and "to" points.
    */

   square1 = buildmap_point_get_square_sorted (record1->record.from);
   square2 = buildmap_point_get_square_sorted (record2->record.from);

   if (square1 != square2) {
      return square1 - square2;
   }

   if (record1->cfcc != record2->cfcc) {
      return record1->cfcc - record2->cfcc;
   }

   if (record1->record.from != record2->record.from) {
      return record1->record.from - record2->record.from;
   }

   return record1->record.to - record2->record.to;
}

static int buildmap_line_compare2 (const void *r1, const void *r2) {

   int square1;
   int square2;

   int index1 = *((int *)r1);
   int index2 = *((int *)r2);

   BuildMapLine *record1;
   BuildMapLine *record2;

   record1 = Line[index1/BUILDMAP_BLOCK] + (index1 % BUILDMAP_BLOCK);
   record2 = Line[index2/BUILDMAP_BLOCK] + (index2 % BUILDMAP_BLOCK);


   /* The lines are first sorted by square.
    * Within a square, lines are sorted by category.
    * Within a category, lines are sorted by the "from" and "to" points.
    */

   square1 = buildmap_point_get_square_sorted (record1->record.to);
   square2 = buildmap_point_get_square_sorted (record2->record.to);

   if (square1 != square2) {
      return square1 - square2;
   }

   if (record1->cfcc != record2->cfcc) {
      return record1->cfcc - record2->cfcc;
   }

   square1 = buildmap_point_get_square_sorted (record1->record.from);
   square2 = buildmap_point_get_square_sorted (record2->record.from);

   if (square1 != square2) {
      return square1 - square2;
   }

   if (record1->record.to != record2->record.to) {
      return record1->record.to - record2->record.to;
   }

   return record1->record.from - record2->record.from;
}

void buildmap_line_sort (void) {

   int i;
   int j;
   BuildMapLine *one_line;

   if (SortedLine != NULL) return; /* Sort was already performed. */

   buildmap_point_sort ();

   buildmap_info ("counting crossings...");

   LineCrossingCount = 0;

   for (i = 0; i < LineCount; i++) {
      one_line = Line[i/BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);
      if (buildmap_point_get_square(one_line->record.from) !=
          buildmap_point_get_square(one_line->record.to)) {
         LineCrossingCount++;
      }
   }

   buildmap_info ("sorting lines...");

   SortedLine = malloc (LineCount * sizeof(int));
   if (SortedLine == NULL) {
      buildmap_fatal (0, "no more memory");
   }

   for (i = 0; i < LineCount; i++) {
      SortedLine[i] = i;
      one_line = Line[i/BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);
      one_line->record.from = buildmap_point_get_sorted (one_line->record.from);
      one_line->record.to   = buildmap_point_get_sorted (one_line->record.to);
   }

   qsort (SortedLine, LineCount, sizeof(int), buildmap_line_compare);

   for (i = 0; i < LineCount; i++) {
      j = SortedLine[i];
      one_line = Line[j/BUILDMAP_BLOCK] + (j % BUILDMAP_BLOCK);
      one_line->sorted = i;
      one_line->sorted = i;
   }


   SortedLine2 = malloc (LineCrossingCount * sizeof(int));
   if (SortedLine2 == NULL) {
      buildmap_fatal (0, "no more memory");
   }

   for (i = 0, j = 0; i < LineCount; i++) {

      one_line = Line[i/BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);

      if (buildmap_point_get_square_sorted (one_line->record.to) !=
          buildmap_point_get_square_sorted (one_line->record.from)) {
         SortedLine2[j++] = i;
      }
   }
   if (j != LineCrossingCount) {
      buildmap_fatal (0, "non matching crossing count");
   }

   qsort (SortedLine2, LineCrossingCount, sizeof(int), buildmap_line_compare2);
}


void buildmap_line_save (void) {

   int i;
   int j;
   int k;
   int square;
   int square_current;
   int cfcc_current;
   int square_count;

   BuildMapLine *one_line;

   int *db_index2;
   RoadMapLine *db_lines;
   RoadMapLineBySquare *db_square1;
   RoadMapLineBySquare *db_square2;

   buildmap_db *root;
   buildmap_db *data_table;
   buildmap_db *square1_table;
   buildmap_db *index2_table;
   buildmap_db *square2_table;


   buildmap_info ("saving lines...");

   square_count = buildmap_square_get_count();


   /* Create the database space */

   root = buildmap_db_add_section (NULL, "line");

   data_table = buildmap_db_add_section (root, "data");
   buildmap_db_add_data (data_table, LineCount, sizeof(RoadMapLine));

   square1_table = buildmap_db_add_section (root, "bysquare1");
   buildmap_db_add_data (square1_table,
                         square_count, sizeof(RoadMapLineBySquare));

   index2_table = buildmap_db_add_section (root, "index2");
   buildmap_db_add_data (index2_table, LineCrossingCount, sizeof(int));

   square2_table = buildmap_db_add_section (root, "bysquare2");
   buildmap_db_add_data (square2_table,
                         square_count, sizeof(RoadMapLineBySquare));

   db_lines   = (RoadMapLine *) buildmap_db_get_data (data_table);
   db_square1 = (RoadMapLineBySquare *) buildmap_db_get_data (square1_table);
   db_index2  = (int *) buildmap_db_get_data (index2_table);
   db_square2 = (RoadMapLineBySquare *) buildmap_db_get_data (square2_table);

   square_current = -1;
   cfcc_current   = -1;

   for (i = 0; i < LineCount; i++) {

      j = SortedLine[i];

      one_line = Line[j/BUILDMAP_BLOCK] + (j % BUILDMAP_BLOCK);

      db_lines[i] = one_line->record;

      square = buildmap_point_get_square_sorted (one_line->record.from);

      if (square != square_current) {

         if (square < square_current) {
            buildmap_fatal (0, "abnormal square order: %d following %d",
                               square, square_current);
         }
         if (square_current >= 0) {
            db_square1[square_current].last = i - 1;
         }
         square_current = square;

         for (k = 0; k < ROADMAP_CATEGORY_RANGE; k++) {
            db_square1[square].first[k] = -1;
         }
         cfcc_current = -1; /* Force cfcc change. */
      }

      if (one_line->cfcc != cfcc_current) {

         if (one_line->cfcc < cfcc_current) {
            buildmap_fatal (0, "abnormal cfcc order: %d following %d",
                               one_line->cfcc, cfcc_current);
         }
         if (one_line->cfcc <= 0 || one_line->cfcc > ROADMAP_CATEGORY_RANGE) {
            buildmap_fatal (0, "illegal cfcc value");
         }
         cfcc_current = one_line->cfcc;

         db_square1[square].first[cfcc_current-1] = i;
      }
   }

   db_square1[square_current].last = LineCount - 1;

   /* Generate the second table (complement). */

   square_current = -1;
   cfcc_current   = -1;

   for (i = 0; i < LineCrossingCount; i++) {

      j = SortedLine2[i];

      one_line = Line[j/BUILDMAP_BLOCK] + (j % BUILDMAP_BLOCK);

      db_index2[i] = one_line->sorted;

      square = buildmap_point_get_square_sorted (one_line->record.to);

      if (square == buildmap_point_get_square_sorted (one_line->record.from)) {
         buildmap_fatal (0, "non crossing line in the crossing line table");
      }

      if (square != square_current) {

         if (square < square_current) {
            buildmap_fatal (0, "abnormal square order: d following %d",
                               square, square_current);
         }
         if (square_current >= 0) {
            db_square2[square_current].last = i - 1;
         }
         square_current = square;

         for (k = 0; k < ROADMAP_CATEGORY_RANGE; k++) {
            db_square2[square].first[k] = -1;
         }
         cfcc_current = -1; /* Force cfcc change. */
      }

      if (one_line->cfcc != cfcc_current) {

         if (one_line->cfcc < cfcc_current) {
            buildmap_fatal (0, "abnormal cfcc order: %d following %d",
                               one_line->cfcc, cfcc_current);
         }
         if (one_line->cfcc <= 0 || one_line->cfcc > ROADMAP_CATEGORY_RANGE) {
            buildmap_fatal (0, "illegal cfcc value");
         }
         cfcc_current = one_line->cfcc;

         db_square2[square].first[cfcc_current-1] = i;
      }
   }

   if (square_current >= 0) {
      db_square2[square_current].last = LineCrossingCount - 1;
   }
}


void buildmap_line_summary (void) {

   fprintf (stderr,
            "-- line table statistics: %d lines, %d crossing\n",
            LineCount, LineCrossingCount);
}


void buildmap_line_reset (void) {

   int i;

   for (i = 0; i < BUILDMAP_BLOCK; i++) {
      if (Line[i] != NULL) {
         free(Line[i]);
         Line[i] = NULL;
      }
   }

   free (SortedLine);
   SortedLine = NULL;

   free (SortedLine2);
   SortedLine2 = NULL;

   LineCount = 0;
   LineTableSize = 0;
   LineCrossingCount = 0;

   LineById = NULL;
}

