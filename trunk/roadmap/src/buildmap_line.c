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
 *   int  buildmap_line_add (int tlid, int layer, int from, int to);
 *
 *   int  buildmap_line_get_sorted  (int line);
 *   void buildmap_line_find_sorted (int tlid);
 *   int  buildmap_line_get_id_sorted (int line);
 *   void buildmap_line_get_points_sorted (int line, int *from, int *to);
 *   void buildmap_line_get_position (int line, int *longitude, int *latitude);
 *   void buildmap_line_get_position_sorted
 *           (int line, int *longitude, int *latitude);
 *   void buildmap_line_get_square_sorted (int line);
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
   int layer;
   int square_from;
   int square_to;
} BuildMapLine;


static int LineCount = 0;
static int LineCrossingCount = 0;
static BuildMapLine *Line[BUILDMAP_BLOCK] = {NULL};

static RoadMapHash *LineById = NULL;

static int *SortedLine = NULL;
static int *SortedLine2 = NULL;


static void buildmap_line_register (void);


static void buildmap_line_initialize (void) {

   LineById = roadmap_hash_new ("LineById", BUILDMAP_BLOCK);

   Line[0] = calloc (BUILDMAP_BLOCK, sizeof(BuildMapLine));
   if (Line[0] == NULL) {
      buildmap_fatal (0, "no more memory");
   }

   LineCount = 0;

   buildmap_line_register();
}


int buildmap_line_add (int tlid, int layer, int from, int to) {

   int block;
   int offset;
   BuildMapLine *this_line;


   if (LineById == NULL) buildmap_line_initialize();


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
   if (layer <= 0) {
      buildmap_fatal (0, "invalid layer");
   }
   this_line->tlid = tlid;
   this_line->layer = layer;
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

   if (record1->layer != record2->layer) {
      return record1->layer - record2->layer;
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

   if (record1->layer != record2->layer) {
      return record1->layer - record2->layer;
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

   if (LineCount == 0) return; /* No line to sort. */


   buildmap_point_sort ();

   buildmap_info ("counting crossings...");

   LineCrossingCount = 0;

   for (i = 0; i < LineCount; ++i) {
      one_line = Line[i/BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);
      if (buildmap_point_get_square(one_line->record.from) !=
          buildmap_point_get_square(one_line->record.to)) {
         ++LineCrossingCount;
      }
   }

   buildmap_info ("sorting lines...");

   SortedLine = malloc (LineCount * sizeof(int));
   if (SortedLine == NULL) {
      buildmap_fatal (0, "no more memory");
   }

   for (i = 0; i < LineCount; ++i) {
      SortedLine[i] = i;
      one_line = Line[i/BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);
      one_line->record.from = buildmap_point_get_sorted (one_line->record.from);
      one_line->record.to   = buildmap_point_get_sorted (one_line->record.to);
   }

   qsort (SortedLine, LineCount, sizeof(int), buildmap_line_compare);

   for (i = 0; i < LineCount; ++i) {
      j = SortedLine[i];
      one_line = Line[j/BUILDMAP_BLOCK] + (j % BUILDMAP_BLOCK);
      one_line->sorted = i;
      one_line->sorted = i;
   }


   SortedLine2 = malloc (LineCrossingCount * sizeof(int));
   if (SortedLine2 == NULL) {
      buildmap_fatal (0, "no more memory");
   }

   for (i = 0, j = 0; i < LineCount; ++i) {

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


static void buildmap_line_save (void) {

   int i;
   int j;
   int square;
   int square_current;
   int layer_current;
   int square_count;
   int layer1_count;
   int layer2_count;
   int layer_sublist;

   BuildMapLine *one_line;

   int *db_layer1;
   int *db_layer2;
   int *db_index2;
   RoadMapLine *db_lines;
   RoadMapLineBySquare *db_square1;
   RoadMapLineBySquare *db_square2;

   buildmap_db *root;
   buildmap_db *data_table;
   buildmap_db *square1_table;
   buildmap_db *layer1_table;
   buildmap_db *square2_table;
   buildmap_db *layer2_table;
   buildmap_db *index2_table;


   buildmap_info ("saving lines...");

   square_count = buildmap_square_get_count();


   /* We need to calculate how much space we will need for line/bylayer1
    * and line/bylayer2. We take this opportunity to make some coherency
    * checks, which adds to the code.
    */
   square_current = -1;
   layer_current  = 0;
   layer1_count   = 0;

   for (i = 0; i < LineCount; ++i) {

      j = SortedLine[i];
      one_line = Line[j/BUILDMAP_BLOCK] + (j % BUILDMAP_BLOCK);

      square = buildmap_point_get_square_sorted (one_line->record.from);

      if (square != square_current) {

         if (square < square_current) {
            buildmap_fatal (0, "abnormal square order: %d following %d",
                               square, square_current);
         }
         if (square_current >= 0) {
            /* Lets compute how much space is needed for the just finished
             * square.
             */
            if (layer_current <= 0) {
                buildmap_fatal (0, "empty square %d has lines?", square);
            }
            layer1_count += (layer_current + 1); /* 1 slot for the end line. */
         }
         square_current = square;

         layer_current = 0; /* Restart from the 1st layer. */
      }

      if (one_line->layer < layer_current) {
         buildmap_fatal (0, "abnormal layer order: %d following %d",
                            one_line->layer, layer_current);
      }

      layer_current = one_line->layer;
      one_line->square_from = square;
   }

   if (square_current >= 0) {
      /* Lets compute how much space is needed for the last square. */
      if (layer_current < 0) {
         buildmap_fatal (0, "empty square %d has lines?", square);
      }
      layer1_count += (layer_current + 1); /* 1 slot for the end line. */
   }

   square_current = -1;
   layer_current  = 0;
   layer2_count   = 0;

   for (i = 0; i < LineCrossingCount; ++i) {

      j = SortedLine2[i];
      one_line = Line[j/BUILDMAP_BLOCK] + (j % BUILDMAP_BLOCK);

      square = buildmap_point_get_square_sorted (one_line->record.to);

      if (square == one_line->square_from) {
         buildmap_fatal (0, "non crossing line in the crossing line table");
      }

      if (square != square_current) {

         if (square < square_current) {
            buildmap_fatal (0, "abnormal square order: d following %d",
                               square, square_current);
         }
         if (square_current >= 0) {
            /* Lets compute how much space is needed for the just finished
             * square.
             */
            if (layer_current <= 0) {
                buildmap_fatal (0, "empty square %d has lines?", square);
            }
            layer2_count += (layer_current + 1); /* 1 slot for the end line. */
         }
         square_current = square;

         layer_current = 0; /* Restart from the 1st layer. */
      }

      if (one_line->layer < layer_current) {
         buildmap_fatal (0, "abnormal layer order: %d following %d",
                            one_line->layer, layer_current);
      }
      layer_current = one_line->layer;
      one_line->square_to = square;
   }

   if (square_current >= 0) {
      /* Lets compute how much space is needed for the last square. */
      if (layer_current < 0) {
         buildmap_fatal (0, "empty square %d has lines?", square);
      }
      layer2_count += (layer_current + 1); /* 1 slot for the end line. */
   }


   /* Create the database space */

   root = buildmap_db_add_section (NULL, "line");
   if (root == NULL) buildmap_fatal (0, "Can't add a new section");

   data_table = buildmap_db_add_section (root, "data");
   if (data_table == NULL) buildmap_fatal (0, "Can't add a new section");
   buildmap_db_add_data (data_table, LineCount, sizeof(RoadMapLine));

   square1_table = buildmap_db_add_section (root, "bysquare1");
   if (square1_table == NULL) buildmap_fatal (0, "Can't add a new section");
   buildmap_db_add_data (square1_table,
                         square_count, sizeof(RoadMapLineBySquare));

   layer1_table = buildmap_db_add_section (root, "bylayer1");
   if (layer1_table == NULL) buildmap_fatal (0, "Can't add a new section");
   buildmap_db_add_data (layer1_table, layer1_count, sizeof(int));

   square2_table = buildmap_db_add_section (root, "bysquare2");
   if (square2_table == NULL) buildmap_fatal (0, "Can't add a new section");
   buildmap_db_add_data (square2_table,
                         square_count, sizeof(RoadMapLineBySquare));

   layer2_table = buildmap_db_add_section (root, "bylayer2");
   if (layer2_table == NULL) buildmap_fatal (0, "Can't add a new section");
   buildmap_db_add_data (layer2_table, layer2_count, sizeof(int));

   index2_table = buildmap_db_add_section (root, "index2");
   if (index2_table == NULL) buildmap_fatal (0, "Can't add a new section");
   buildmap_db_add_data (index2_table, LineCrossingCount, sizeof(int));

   db_lines   = (RoadMapLine *) buildmap_db_get_data (data_table);
   db_square1 = (RoadMapLineBySquare *) buildmap_db_get_data (square1_table);
   db_layer1  = (int *) buildmap_db_get_data (layer1_table);
   db_square2 = (RoadMapLineBySquare *) buildmap_db_get_data (square2_table);
   db_layer2  = (int *) buildmap_db_get_data (layer2_table);
   db_index2  = (int *) buildmap_db_get_data (index2_table);


   square_current = -1;
   layer_current  = 0;
   layer_sublist  = 0;

   for (i = 0; i < LineCount; ++i) {

      j = SortedLine[i];

      one_line = Line[j/BUILDMAP_BLOCK] + (j % BUILDMAP_BLOCK);

      db_lines[i] = one_line->record;

      square = one_line->square_from;

      if (square != square_current) {

         if (square_current >= 0) {
            
            /* Complete the previous square. */

            db_layer1[layer_sublist+layer_current] = i;
            db_square1[square_current].first = layer_sublist;
            db_square1[square_current].count = layer_current;

            /* Move on to the next square. */

            layer_sublist += (layer_current + 1);
            if (layer_sublist >= layer1_count) {
               buildmap_fatal (0, "invalid line/bylayer1 count");
            }
         }
         square_current = square;
         layer_current = 0; /* Restart at the first layer. */
      }

      /* The current line is the start of every layer up to and
       * including this layer. If there are skipped layers, they
       * will have 0 lines since they start at the same place as
       * the next layer.
       */
      while (layer_current < one_line->layer) {
         db_layer1[layer_sublist+layer_current] = i;
         layer_current += 1;
      }
   }

   /* Complete the last square. */
   if (square_current >= 0) {

      db_layer1[layer_sublist+layer_current] = i;
      db_square1[square_current].first = layer_sublist;
      db_square1[square_current].count = layer_current;

      if (layer_sublist+layer_current+1 != layer1_count) {
         buildmap_fatal (0, "invalid line/bylayer1 count");
      }
   }


   /* Generate the second table (crossing streets). */

   square_current = -1;
   layer_current  = 0;
   layer_sublist  = 0;

   for (i = 0; i < LineCrossingCount; ++i) {

      j = SortedLine2[i];

      one_line = Line[j/BUILDMAP_BLOCK] + (j % BUILDMAP_BLOCK);

      db_index2[i] = one_line->sorted;

      square = one_line->square_to;

      if (square != square_current) {

         if (square_current >= 0) {
            
            /* Complete the previous square. */

            db_layer2[layer_sublist+layer_current] = i;
            db_square2[square_current].first = layer_sublist;
            db_square2[square_current].count = layer_current;

            /* Move on to the next square. */

            layer_sublist += (layer_current + 1);
            if (layer_sublist >= layer2_count) {
               buildmap_fatal (0, "invalid line/bylayer2 count");
            }
         }
         square_current = square;
         layer_current = 0; /* Restart at the first layer. */
      }

      while (layer_current < one_line->layer) {
         db_layer2[layer_sublist+layer_current] = i;
         layer_current += 1;
      }
   }

   if (square_current >= 0) {
      db_layer2[layer_sublist+layer_current] = i;
      db_square2[square_current].first = layer_sublist;
      db_square2[square_current].count = layer_current;

      if (layer_sublist+layer_current+1 != layer2_count) {
         buildmap_fatal (0, "invalid line/bylayer2 count");
      }
   }
}


static void buildmap_line_summary (void) {

   fprintf (stderr,
            "-- line table statistics: %d lines, %d crossing\n",
            LineCount, LineCrossingCount);
}


static void buildmap_line_reset (void) {

   int i;

   for (i = 0; i < BUILDMAP_BLOCK; ++i) {
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
   LineCrossingCount = 0;

   roadmap_hash_delete (LineById);
   LineById = NULL;
}


static buildmap_db_module BuildMapLineModule = {
   "line",
   buildmap_line_sort,
   buildmap_line_save,
   buildmap_line_summary,
   buildmap_line_reset
}; 
      
         
static void buildmap_line_register (void) {
   buildmap_db_register (&BuildMapLineModule);
}
