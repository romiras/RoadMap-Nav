/* buildmap_line_speed.c - Build line speeds table
 *
 * LICENSE:
 *
 *   Copyright 2006 Ehud Shabtai
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "roadmap_db_line_speed.h"
#include "roadmap_hash.h"

#include "buildmap.h"
#include "buildmap_line.h"
#include "buildmap_line_speed.h"

#define MAX_SPEED_SLOTS 48

struct buildmap_line_speeds_line_struct {

   int tlid;
   int against_dir;
   int speed_ref;
};

typedef struct buildmap_line_speeds_line_struct BuildMapSpeedLine;

struct buildmap_line_speed_avg_struct {

   RoadMapLineSpeedAvg record;
   int line;
};

typedef struct buildmap_line_speed_avg_struct BuildMapSpeedAvg;

static int SpeedAvgCount = 0;
static BuildMapSpeedAvg *SpeedsAvg[BUILDMAP_BLOCK] = {NULL};

static int MaxSlotsPerLine = 0;
static int SpeedsCount = 0;
static int SpeedSlotsCount = 0;
static int LinesCount = 0;
static BuildMapSpeed *Speeds[BUILDMAP_BLOCK] = {NULL};
static BuildMapSpeedLine *Lines[BUILDMAP_BLOCK] = {NULL};

static RoadMapHash *SpeedByLine   = NULL;
static RoadMapHash *SpeedsHash    = NULL;

static int hash_speeds (const BuildMapSpeed *speeds) {

   int hash = 0;
   int i;

   for (i=0; i<speeds->count; i++) {
      hash += speeds->speeds[i].time_slot * speeds->speeds[i].speed;
   }

   return hash;
}


static RoadMapLineSpeedRef *dup_speeds(RoadMapLineSpeedRef *speeds, int count) {

   int i;
   RoadMapLineSpeedRef *dup = calloc (count, sizeof(RoadMapLineSpeedRef));

   for (i=0; i<count; i++) {
      dup[i] = speeds[i];
   }

   return dup;
}


static unsigned short buildmap_line_speed_get_ref (int tlid, int against_dir) {

   int index;

   BuildMapSpeedLine *this_line;

   for (index = roadmap_hash_get_first (SpeedByLine, tlid);
        index >= 0;
        index = roadmap_hash_get_next (SpeedByLine, index)) {

      this_line = Lines[index / BUILDMAP_BLOCK] + (index % BUILDMAP_BLOCK);

      if ((this_line->tlid == tlid) &&
          (this_line->against_dir == against_dir)) {

         return this_line->speed_ref;
      }
   }

   return INVALID_SPEED;
}


BuildMapSpeed *buildmap_line_speed_new (void) {

   static BuildMapSpeed speed;

   if (speed.speeds == NULL) {
      speed.speeds = calloc (MAX_SPEED_SLOTS, sizeof(RoadMapLineSpeedRef));
   }

   speed.count = 0;

   return &speed;
}


void buildmap_line_speed_free (BuildMapSpeed *speed) {
   speed->count = 0;
}


void buildmap_line_speed_add_slot (BuildMapSpeed *speeds, int time_slot,
                                   int speed) {
   if (speeds->count == MAX_SPEED_SLOTS) {

      buildmap_fatal (0, "buildmap_line_speed_add_slot: Too many slots.");
   }

   if (speeds->count == 0) {
      /* set first time slot to 0 */
      time_slot = 0;

   } else if (speeds->speeds[speeds->count - 1].time_slot >= time_slot) {

      buildmap_fatal (0, "buildmap_line_speed_add_slot: inconsistent time slots");

   } else if (speeds->speeds[speeds->count - 1].speed == speed) {
      /* The speed has not changed from the previous time slot */
      return;
   }

   speeds->speeds[speeds->count].time_slot = time_slot;
   speeds->speeds[speeds->count].speed = speed;
   speeds->count++;
}


void buildmap_line_speed_initialize (void) {

   SpeedByLine = roadmap_hash_new ("SpeedByLine", BUILDMAP_BLOCK);
   SpeedsHash  = roadmap_hash_new ("SpeedsHash", BUILDMAP_BLOCK);

   SpeedsCount = 0;
   SpeedAvgCount = 0;
   SpeedSlotsCount = 0;
   LinesCount = 0;
}


int  buildmap_line_speed_add_avg
        (unsigned char from_avg_speed,
         unsigned char to_avg_speed,
         int line) {

   int block;
   int offset;
   BuildMapSpeedAvg *this_avg;

   block = line / BUILDMAP_BLOCK;
   offset = line % BUILDMAP_BLOCK;

   if (SpeedsAvg[block] == NULL) {

      /* We need to add a new block to the table. */

      SpeedsAvg[block] = calloc (BUILDMAP_BLOCK, sizeof(BuildMapSpeedAvg));
      if (SpeedsAvg[block] == NULL) {
         buildmap_fatal (0, "no more memory");
      }
   }

   this_avg = SpeedsAvg[block] + offset;

   this_avg->record.from_avg_speed = from_avg_speed;
   this_avg->record.to_avg_speed = to_avg_speed;
   this_avg->line = line;

   if (line >= SpeedAvgCount) SpeedAvgCount = line + 1;

   return SpeedAvgCount;
}


int  buildmap_line_speed_add (BuildMapSpeed *speeds, int tlid, int opposite) {

   int index;
   int block;
   int offset;
   int hash;
   int speed_ref = -1;

   BuildMapSpeedLine *this_line;
   BuildMapSpeed     *this_speed;

   hash = hash_speeds (speeds);
   for (index = roadmap_hash_get_first (SpeedsHash, hash);
        index >= 0;
        index = roadmap_hash_get_next (SpeedsHash, index)) {

      this_speed = Speeds[index / BUILDMAP_BLOCK] + (index % BUILDMAP_BLOCK);

      if (this_speed->count == speeds->count) {

         int i;

         for (i=0; i<this_speed->count; i++) {

            if ((this_speed->speeds[i].speed != speeds->speeds[i].speed) ||
                (this_speed->speeds[i].time_slot !=
                  speeds->speeds[i].time_slot)) {

               break;
            }
         }

         if (i == this_speed->count) {
            /* Found an equal speed series */
            speed_ref = index;
            break;
         }
      }
   }

   if (speed_ref == -1) {
      /* We need to create a new speed series */

      if (SpeedsCount == 0xffff) {
         buildmap_fatal (0, "Too many speed series.");
      }

      block = SpeedsCount / BUILDMAP_BLOCK;
      offset = SpeedsCount % BUILDMAP_BLOCK;

      if (Speeds[block] == NULL) {

         /* We need to add a new block to the table. */

         Speeds[block] = calloc (BUILDMAP_BLOCK, sizeof(BuildMapSpeed));
         if (Speeds[block] == NULL) {
            buildmap_fatal (0, "no more memory");
         }
         roadmap_hash_resize (SpeedsHash, (block+1) * BUILDMAP_BLOCK);
      }

      this_speed = Speeds[block] + offset;

      this_speed->speeds = dup_speeds(speeds->speeds, speeds->count);
      this_speed->count = speeds->count;

      roadmap_hash_add (SpeedsHash, hash, SpeedsCount);

      speed_ref = SpeedsCount;
      SpeedSlotsCount += speeds->count;
      if (speeds->count > MaxSlotsPerLine) MaxSlotsPerLine = speeds->count;
      SpeedsCount += 1;
   }


   /* Lines storage */

   block = LinesCount / BUILDMAP_BLOCK;
   offset = LinesCount % BUILDMAP_BLOCK;

   if (Lines[block] == NULL) {

      /* We need to add a new block to the table. */

      Lines[block] = calloc (BUILDMAP_BLOCK, sizeof(BuildMapSpeedLine));
      if (Lines[block] == NULL) {
         buildmap_fatal (0, "no more memory");
      }
      roadmap_hash_resize (SpeedByLine, (block+1) * BUILDMAP_BLOCK);
   }

   this_line = Lines[block] + offset;

   this_line->tlid = tlid;
   this_line->speed_ref = speed_ref;
   this_line->against_dir = opposite;

   roadmap_hash_add (SpeedByLine, tlid, LinesCount);

   LinesCount += 1;

   return speed_ref;
}


void buildmap_line_speed_sort (void) {}

void  buildmap_line_speed_save (void) {

   int i;
   int slot;
   int slot_count = 0;
   RoadMapLineSpeed     *db_speed;
   BuildMapSpeedAvg     *one_avg_speed;
   RoadMapLineSpeedAvg  *db_avg;
   BuildMapSpeed        *one_speed_ref;
   RoadMapLineSpeedRef  *db_speed_ref;
   int                  *db_index;

   buildmap_db *root;
   buildmap_db *table_avg;
   buildmap_db *table_line_speed;
   buildmap_db *table_index;
   buildmap_db *table_data;

   int max_line_id = 0;

   /* Find the max line id, as we need it to calculate the size of the
    * speed ref index for each line.
    */
   for (i = 0; i < LinesCount; i++) {
      if (Lines[i/BUILDMAP_BLOCK] != NULL) {
         int line_id;

         BuildMapSpeedLine *this_line =
            Lines[i/BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);

         line_id = buildmap_line_find_sorted(this_line->tlid);
         if (line_id > max_line_id) max_line_id = line_id;
      }
   }

   buildmap_info ("saving line speed...");

   root = buildmap_db_add_section (NULL, "line_speed");
   if (root == NULL) buildmap_fatal (0, "Can't add a new section");

   if (SpeedAvgCount) {
      table_avg = buildmap_db_add_child
                  (root, "avg", SpeedAvgCount, sizeof(RoadMapLineSpeedAvg));
      db_avg   = (RoadMapLineSpeedAvg *) buildmap_db_get_data (table_avg);
   }

   if (max_line_id) {

      table_line_speed = buildmap_db_add_child
                  (root, "line_speed", max_line_id, sizeof(RoadMapLineSpeed));
      db_speed = (RoadMapLineSpeed *) buildmap_db_get_data (table_line_speed);

      table_index = buildmap_db_add_child
                  (root, "index", SpeedsCount, sizeof(int));
      table_data = buildmap_db_add_child
                  (root, "data", SpeedSlotsCount, sizeof(RoadMapLineSpeedRef));

      db_index = (int *) buildmap_db_get_data (table_index);
      db_speed_ref = (RoadMapLineSpeedRef *) buildmap_db_get_data (table_data);
   }

   if (SpeedsAvg) {
      buildmap_info ("saving line speed avg...");

      for (i = 0; i < SpeedAvgCount; i++) {

         if (SpeedsAvg[i/BUILDMAP_BLOCK] == NULL) {
            memset (&db_avg[i], 0, sizeof (RoadMapLineSpeedAvg));
         } else {
            one_avg_speed = SpeedsAvg[i/BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);
            db_avg[i] = one_avg_speed->record;
         }
      }
   }

   if (max_line_id) {

      buildmap_info ("saving line speed slots...");

      for (i = 0; i < max_line_id; i++) {
         RoadMapLineSpeed *this_speed = db_speed + i;
         int tlid = buildmap_line_get_id_sorted (i);
         this_speed->from_speed_ref = buildmap_line_speed_get_ref (tlid, 0);
         this_speed->to_speed_ref   = buildmap_line_speed_get_ref (tlid, 1);
      }

      for (i = 0; i < SpeedsCount; i++) {
         one_speed_ref = Speeds[i/BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);
         db_index[i] = slot_count;
         for (slot = 0; slot < one_speed_ref->count; slot++) {
            db_speed_ref[slot_count++] = one_speed_ref->speeds[slot];
         }
         db_speed_ref[slot_count-1].time_slot |= SPEED_EOL;
      }

      if (switch_endian) {
         int i;

         for (i=0; i<SpeedsCount; i++) {
            switch_endian_int(db_index + i);
         }
      }
   }
}


void buildmap_line_speed_summary (void) {

}


void buildmap_line_speed_reset (void) {

   int i;

   for (i = 0; i < BUILDMAP_BLOCK; i++) {
      if (SpeedsAvg[i] != NULL) {
         free (SpeedsAvg[i]);
         SpeedsAvg[i] = NULL;
      }
   }

   for (i = 0; i < SpeedsCount; i++) {
      BuildMapSpeed *this_speed;
      this_speed = Speeds[i / BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);
      free (this_speed->speeds);
   }

   for (i = 0; i < BUILDMAP_BLOCK; i++) {
      if (Speeds[i] != NULL) {
         free (Speeds[i]);
         Speeds[i] = NULL;
      }
   }

   for (i = 0; i < BUILDMAP_BLOCK; i++) {
      if (Lines[i] != NULL) {
         free (Lines[i]);
         Lines[i] = NULL;
      }
   }

   SpeedsCount = 0;
   SpeedAvgCount = 0;
   SpeedSlotsCount = 0;

   LinesCount = 0;
}

