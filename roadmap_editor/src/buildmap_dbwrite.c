/* buildmap_dbwrite.c - a module to write a roadmap database.
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
 *   #include "buildmap.h"
 *
 *   void buildmap_db_open (char *path, char *name);
 *
 *   buildmap_db *buildmap_db_add_section (buildmap_db *parent, char *name);
 *   void   buildmap_db_add_data (buildmap_db *section, int count, int size);
 *   char  *buildmap_db_get_data (buildmap_db *section);
 *
 *   void buildmap_db_close (void);
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "roadmap_types.h"
#include "roadmap_file.h"
#include "roadmap_db.h"

#include "buildmap.h"

#ifdef J2MEMAP
int switch_endian = 1;
#else
int switch_endian = 0;
#endif

#define BUILDMAP_DB_BLOCK_SIZE 0x100000


static char         *BuildmapCurrentDbName = NULL;
static char         *BuildmapCurrentDbBase = NULL;
static int           BuildmapCurrentDbSize = 0;
RoadMapFileContext   BuildmapCurrentDbBaseMapContext = NULL;
static buildmap_db   BuildmapDbRoot;

static buildmap_db *BuildmapCurrentDbSection = NULL;


static void buildmap_db_repair_tree (buildmap_db *section) {

   buildmap_db *child;

   section->head = (struct roadmap_db_section *)
      (BuildmapCurrentDbBase + section->head_offset);

   for (child = section->first; child != NULL; child = child->next) {

      buildmap_db_repair_tree (child);
   }
}


static int buildmap_db_extend (int size) {

   if (size >= BuildmapCurrentDbSize) {

      const char *old_base = BuildmapCurrentDbBase;

      if (BuildmapCurrentDbBase != NULL) {
         roadmap_file_unmap (&BuildmapCurrentDbBaseMapContext);
         BuildmapCurrentDbBaseMapContext = NULL;
      }

      BuildmapCurrentDbSize =
         (((size + (BUILDMAP_DB_BLOCK_SIZE / 2)) / BUILDMAP_DB_BLOCK_SIZE) + 1)
            * BUILDMAP_DB_BLOCK_SIZE;

      if (roadmap_file_truncate (NULL, BuildmapCurrentDbName, BuildmapCurrentDbSize-1) < 0) {
         buildmap_error (0, "cannot resize database %s", BuildmapCurrentDbName);
         return -1;
      }

      if (roadmap_file_map
                  ("maps",
                   BuildmapCurrentDbName,
                   NULL,
                   "rw",
                   &BuildmapCurrentDbBaseMapContext) == NULL) {
         buildmap_error (0, "cannot remap database %s", BuildmapCurrentDbName);
         return -1;
      }

      BuildmapCurrentDbBase = roadmap_file_base (BuildmapCurrentDbBaseMapContext);

      if ((BuildmapCurrentDbBase != old_base) && (old_base != NULL)) {

         buildmap_db_repair_tree (&BuildmapDbRoot);
      }
   }

   return 0;
}


static void buildmap_db_propagate (buildmap_db *parent, int size) {

   while (parent != NULL) {
      parent->head->count = 1; /* Just a convention, not really used. */
      parent->head->size += size;
      parent = parent->parent;
   }
}


static void buildmap_db_update_tree
               (buildmap_db *parent, buildmap_db *section) {

   buildmap_db *child;

   if (section->first == NULL) {
      section->head->first = 0;
   } else {
      section->head->first = section->first->head_offset;
   }

   for (child = section->first; child != NULL; child = child->next) {

      if (child->next == NULL) {
         child->head->next = 0;
      } else {
         child->head->next = child->next->head_offset;
      }
      buildmap_db_update_tree (section, child);
   }
}


static void buildmap_db_switch_endian
               (buildmap_db *parent, buildmap_db *section) {

   buildmap_db *child;
   struct roadmap_db_section *db;

   for (child = section->first; child != NULL; child = child->next) {

      buildmap_db_switch_endian (section, child);
   }

   db = section->head;

   switch_endian_int(&db->first);
   switch_endian_int(&db->next);
   switch_endian_int(&db->size);
   switch_endian_int(&db->count);
}


int buildmap_db_open (const char *path, const char *name) {

   struct roadmap_db_section *root;

   if (path == NULL) {
      path = ".";
   }

   BuildmapCurrentDbName =
      malloc (strlen(path) + strlen(name) + strlen(ROADMAP_DB_TYPE) + 4);

   if (BuildmapCurrentDbName == NULL) {
      buildmap_error (0, "no more memory");
      return -1;
   }

   strcpy (BuildmapCurrentDbName, path);
   strcat (BuildmapCurrentDbName, "/");
   strcat (BuildmapCurrentDbName, name);
   strcat (BuildmapCurrentDbName, ROADMAP_DB_TYPE);

   roadmap_file_save (NULL, BuildmapCurrentDbName, "", 1);

   BuildmapCurrentDbSize = 0;
   if (buildmap_db_extend (0) < 0) { /* Set the size to its minimum. */
      return -1;
   }

   root = (struct roadmap_db_section *) BuildmapCurrentDbBase;
   strcpy (root->name, "roadmap");
   root->size = 0;

   BuildmapDbRoot.head = root;
   BuildmapDbRoot.head_offset = 0;
   BuildmapDbRoot.data = sizeof(struct roadmap_db_section);
   BuildmapDbRoot.parent = NULL;

   return 0;
}


buildmap_db *buildmap_db_add_section (buildmap_db *parent, const char *name) {

   int offset;
   int aligned;
   buildmap_db *new;


   if (parent == NULL) {
      parent = &BuildmapDbRoot;
   }

   if (strlen(name) >= sizeof(parent->head->name)) {
      buildmap_error (0, "invalid section name %s (too long)", name);
      return NULL;
   }

   offset = parent->data + parent->head->size;
   aligned = (offset + 3) & (~3);

   if (buildmap_db_extend (aligned + sizeof(struct roadmap_db_section)) < 0) {
      return NULL;
   }

   new = malloc (sizeof(buildmap_db));
   if (new == NULL) {
      buildmap_error (0, "no more memory");
      return NULL;
   }

   new->parent = parent;
   new->head_offset = aligned;
   new->head = (struct roadmap_db_section *) (BuildmapCurrentDbBase + aligned);
   new->data = aligned + sizeof(struct roadmap_db_section);
   new->first = NULL;
   new->last = NULL;
   new->next = NULL;

   strcpy (new->head->name, name);
   new->head->size = 0;

   if (parent->last == NULL) {
      parent->first = new;
   } else {
      parent->last->next = new;
   }
   parent->last = new;

   buildmap_db_propagate (parent, sizeof(struct roadmap_db_section));

   BuildmapCurrentDbSection = new;

   return new;
}


int buildmap_db_add_data (buildmap_db *section, int count, int size) {

   int offset;
   int total_size;
   int aligned_size;

   if (section != BuildmapCurrentDbSection) {
      buildmap_error (0, "invalid database write sequence");
      return -1;
   }

   offset = section->data + section->head->size;
   if (count > 0) {
      total_size = count * size;
   } else {
      total_size = size;
   }
   aligned_size = (total_size + 3) & (~3);

   section->head->size += total_size;

   if (buildmap_db_extend (offset + aligned_size) < 0) {
      return -1;
   }

   buildmap_db_propagate (section->parent, aligned_size);

   section->head->count = count;

   /* return the actual size of the DB after the data addition */
   return sizeof(*BuildmapDbRoot.head) + BuildmapDbRoot.head->size;
}


char *buildmap_db_get_data (buildmap_db *section) {

   return BuildmapCurrentDbBase + section->data;
}


static void buildmap_db_free (buildmap_db *node) {

   buildmap_db *cursor;
   buildmap_db *next;

   for (cursor = node; cursor != NULL; cursor = next) {

      next = cursor->next;

      buildmap_db_free (cursor->first);

      free (cursor);
   }
}


void buildmap_db_close (void) {

   int actual_size = 0;

   if (BuildmapCurrentDbBase != NULL) {

      actual_size = sizeof(*BuildmapDbRoot.head) + BuildmapDbRoot.head->size;

      buildmap_db_update_tree (NULL, &BuildmapDbRoot);

      if (switch_endian) {
         buildmap_db_switch_endian (NULL, &BuildmapDbRoot);
      }

      roadmap_file_unmap (&BuildmapCurrentDbBaseMapContext);
      BuildmapCurrentDbBaseMapContext = NULL;

      BuildmapCurrentDbBase = NULL;
      BuildmapCurrentDbSize = 0;
   }

   if (BuildmapCurrentDbName != NULL) {
      roadmap_file_truncate (NULL, BuildmapCurrentDbName, actual_size);
      free (BuildmapCurrentDbName);
      BuildmapCurrentDbName = NULL;
   }

   buildmap_db_free (BuildmapDbRoot.first);
   BuildmapDbRoot.first = NULL;
   BuildmapDbRoot.last  = NULL;
   BuildmapDbRoot.head  = NULL;
   BuildmapDbRoot.head_offset = 0;
   BuildmapDbRoot.data = 0;

   BuildmapCurrentDbSection = NULL;
}


buildmap_db *buildmap_db_add_child (buildmap_db *parent,
                                    char *name,
                                    int count,
                                    int size) {

   buildmap_db *table;
   
   table = buildmap_db_add_section (parent, name);

   if (table == NULL) {
      buildmap_fatal (0, "Can't add a new section %s", name);
   }

   if (buildmap_db_add_data (table, count, size) < 0) {
      buildmap_fatal (0, "Can't add data into section %s", name);
   }

   return table;
}


