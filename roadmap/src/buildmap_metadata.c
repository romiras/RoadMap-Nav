/* buildmap_metadata.c - Build a table containing the map's metadata.
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
 *   void buildmap_metadata_add_attribute (const char *category,
 *                                         const char *name,
 *                                         const char *value);
 *
 *   void buildmap_metadata_add_value (const char *category,
 *                                     const char *name,
 *                                     const char *value);
 *
 * These functions are used to build a table of attributes that describe
 * what this map file contains.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "roadmap.h"
#include "roadmap_db_metadata.h"

#include "roadmap_hash.h"

#include "buildmap.h"
#include "buildmap_metadata.h"

/* metadata is somewhat special, in that if we're creating more than
 * one rdm file in a single invocation, the metadata will be the same
 * for all those files.  this only happens in buildmap_osm, and only
 * when asked to create quadtiles to cover an entire region.  in all
 * other cases (i.e., when buildmap is creating from tiger data, or
 * when buildmap_osm is creating a single country iso), the metadata
 * will be used just once before we exit.  so:  we skip doing a reset
 * of the metadata table, which allows it to be duplicated for all
 * .rdm files created in a single invocation.
 */
#define DO_METADATA_RESET 0

#define BUILDMAP_MAX_VALUES 64

typedef struct {

    RoadMapString category;
    RoadMapString name;

    RoadMapString values[BUILDMAP_MAX_VALUES];
    short         count;

} BuildMapAttribute;

static int AttributeCount = 0;
static BuildMapAttribute *Attribute[BUILDMAP_BLOCK] = {NULL};

static RoadMapHash *AttributeByName = NULL;

BuildMapDictionary AttributeDictionary = NULL;


static void buildmap_metadata_register (void);


static void buildmap_metadata_initialize (void) {

   AttributeByName =
      roadmap_hash_new ("AttributeByName", BUILDMAP_BLOCK);

   Attribute[0] = calloc (BUILDMAP_BLOCK, sizeof(RoadMapAttribute));
   if (Attribute[0] == NULL) {
      buildmap_fatal (0, "no more memory");
   }

   AttributeDictionary = buildmap_dictionary_open ("attributes");

   buildmap_metadata_register ();
}


void buildmap_metadata_add_attribute (const char *category,
                                      const char *name,
                                      const char *value) {

   int i;
   int block;
   int offset;
   BuildMapAttribute *this_attribute;

   RoadMapString coded_category;
   RoadMapString coded_name;
   RoadMapString coded_value;

   if (AttributeByName == NULL) buildmap_metadata_initialize();

   /* First check if the attribute is already known. */

   coded_category =
      buildmap_dictionary_add (AttributeDictionary, category, strlen(category));

   coded_name =
      buildmap_dictionary_add (AttributeDictionary, name, strlen(name));

   coded_value =
      buildmap_dictionary_add (AttributeDictionary, value, strlen(value));

   for (i = roadmap_hash_get_first (AttributeByName, coded_name);
        i >= 0;
        i = roadmap_hash_get_next (AttributeByName, i)) {

      this_attribute = Attribute[i / BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);

      if ((this_attribute->name == coded_name) &&
          (this_attribute->category == coded_category)) {
          
         if (this_attribute->values[0] != coded_value) {
            buildmap_fatal (0, "attribute %s.%s changed to %s",
                         category, name, value);
         }
         return;
      }
   }


   /* This is a new attribute: create a new entry. */

   block = AttributeCount / BUILDMAP_BLOCK;
   offset = AttributeCount % BUILDMAP_BLOCK;

   if (Attribute[block] == NULL) {

      /* We need to add a new block to the table. */

      Attribute[block] = calloc (BUILDMAP_BLOCK, sizeof(RoadMapAttribute));
      if (Attribute[block] == NULL) {
         buildmap_fatal (0, "no more memory");
      }

      roadmap_hash_resize (AttributeByName, (block+1) * BUILDMAP_BLOCK);
   }

   roadmap_hash_add (AttributeByName, coded_name, AttributeCount);

   this_attribute = Attribute[block] + offset;

   this_attribute->category = coded_category;
   this_attribute->name = coded_name;
   this_attribute->values[0] = coded_value;
   this_attribute->count = 1;

   AttributeCount += 1;
}


void buildmap_metadata_add_value (const char *category,
                                  const char *name,
                                  const char *value) {

   int i;
   BuildMapAttribute *this_attribute;


   RoadMapString coded_category =
      buildmap_dictionary_locate (AttributeDictionary, category);

   RoadMapString coded_name =
      buildmap_dictionary_locate (AttributeDictionary, name);

   RoadMapString coded_value =
      buildmap_dictionary_add (AttributeDictionary, value, strlen(value));

   for (i = roadmap_hash_get_first (AttributeByName, coded_name);
        i >= 0;
        i = roadmap_hash_get_next (AttributeByName, i)) {

      this_attribute = Attribute[i / BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);

      if ((this_attribute->name == coded_name) &&
          (this_attribute->category == coded_category)) {
          
         if (this_attribute->count >= BUILDMAP_MAX_VALUES) {
            buildmap_fatal (0, "too many values for attribute %s.%s",
                         category, name);
         }

         this_attribute->values[this_attribute->count++] = coded_value;

         return;
      }
   }

   buildmap_fatal (0, "attribute %s.%s not found", category, name);
}


static int buildmap_metadata_save (void) {

   int i;
   int j;
   int values_count;
   int values_cursor;

   BuildMapAttribute *one_attribute;
   RoadMapAttribute  *db_attributes;
   RoadMapString     *db_values;
   
   buildmap_db *root;
   buildmap_db *table_attributes;
   buildmap_db *table_values;


   buildmap_info ("saving %d attributes...", AttributeCount);

   values_count = 0;
   for (i = 0; i < AttributeCount; ++i) {

      one_attribute = Attribute[i/BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);
      values_count += one_attribute->count;
   }

   root = buildmap_db_add_section (NULL, "metadata");

   table_attributes = buildmap_db_add_section (root, "attributes");
   buildmap_db_add_data
      (table_attributes, AttributeCount, sizeof(RoadMapAttribute));

   table_values = buildmap_db_add_section (root, "values");
   buildmap_db_add_data
      (table_values, values_count, sizeof(RoadMapString));

   db_attributes = (RoadMapAttribute *) buildmap_db_get_data (table_attributes);
   db_values     = (RoadMapString *) buildmap_db_get_data (table_values);


   values_cursor = 0;

   for (i = 0; i < AttributeCount; ++i) {

      one_attribute = Attribute[i/BUILDMAP_BLOCK] + (i % BUILDMAP_BLOCK);

      db_attributes[i].category = one_attribute->category;
      db_attributes[i].name     = one_attribute->name;

      db_attributes[i].value_first = values_cursor;
      db_attributes[i].value_count = one_attribute->count;

      for (j = 0; j < one_attribute->count; ++j) {
         db_values[values_cursor++] = one_attribute->values[j];
      }
   }

   return 0;
}


static void buildmap_metadata_summary (void) {

   fprintf (stderr,
            "-- metadata table statistics: %d attributes, %d bytes used\n",
            AttributeCount, (int)(AttributeCount * sizeof(RoadMapAttribute)));
}


static void buildmap_metadata_reset (void) {

#if DO_METADATA_RESET
   int i;

   for (i = 0; i < BUILDMAP_BLOCK; i++) {
      if (Attribute[i] != NULL) {
         free (Attribute[i]);
         Attribute[i] = NULL;
      }
   }

   AttributeCount = 0;

   roadmap_hash_delete (AttributeByName);
   AttributeByName = NULL;
#endif
}


static buildmap_db_module BuildMapMetadataModule = {
   "metadata",
   NULL,
   buildmap_metadata_save,
   buildmap_metadata_summary,
   buildmap_metadata_reset
}; 
      
         
static void buildmap_metadata_register (void) {
   buildmap_db_register (&BuildMapMetadataModule);
}

