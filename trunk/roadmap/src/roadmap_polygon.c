/* roadmap_polygon.c - Manage the tiger polygons.
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
 *   int  roadmap_polygon_count (void);
 *   int  roadmap_polygon_category (int polygon);
 *   void roadmap_polygon_edges (int polygon, RoadMapArea *edges);
 *   int  roadmap_polygon_lines (int polygon, int *list, int size);
 *
 * These functions are used to retrieve the polygons to draw.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "roadmap.h"
#include "roadmap_dbread.h"
#include "roadmap_db_polygon.h"

#include "roadmap_polygon.h"
#include "roadmap_locator.h"

static char *RoadMapPolygonType = "RoadMapPolygonContext";

/* for older RoadMapPolygon1 polygons, these accessors for the "count"
 * and "first" values, which are 20 bits each -- 16 in an unsigned
 * short, and 4 in each nibble of the "hi_f_c" element.
 *
 * newer maps have 32bit "first" and "count" elements.
 */

#define hifirst(t) (((t)->hi_f_c >> 4) & 0xf)
#define hicount(t) (((t)->hi_f_c >> 0) & 0xf)

#define sethifirst(t,i) { (t)->hi_f_c = \
        ((t)->hi_f_c & 0x0f) + (((i) & 0xf) << 4); }
#define sethicount(t,i) { (t)->hi_f_c = \
        ((t)->hi_f_c & 0xf0) + (((i) & 0xf) << 0); }

#define roadmap_polygon_get_count(this) \
        ((this)->count + (hicount(this) * 65536))
#define roadmap_polygon_get_first(this) \
        ((this)->first + (hifirst(this) * 65536))

#define buildmap_polygon_set_count(this, c) do { \
                (this)->count = (c) % 65536; \
                sethicount(this, (c) / 65536); \
        } while(0)
#define buildmap_polygon_set_first(this, f) do { \
                (this)->first = (f) % 65536; \
                sethifirst(this, (f) / 65536); \
        } while(0)

int roadmap_polygon_db_version;

typedef struct {

   char *type;

   RoadMapPolygon *Polygons;
   int             PolygonCount;

   RoadMapPolygonLine *PolygonLines;
   int                 PolygonLineCount;

} RoadMapPolygonContext;

static RoadMapPolygonContext *RoadMapPolygonActive = NULL;


static void *roadmap_polygon_map (roadmap_db *root) {

   RoadMapPolygonContext *context;

   roadmap_db *head_table;
   roadmap_db *point_table;
   roadmap_db *line_table;


   context = malloc (sizeof(RoadMapPolygonContext));
   roadmap_check_allocated(context);

   context->type = RoadMapPolygonType;

   head_table  = roadmap_db_get_subsection (root, "head");
   point_table = roadmap_db_get_subsection (root, "point");
   line_table = roadmap_db_get_subsection (root, "line");

   context->Polygons = roadmap_db_get_data (head_table);
   context->PolygonCount = roadmap_db_get_count (head_table);

   if (roadmap_db_get_size (head_table) ==
	    context->PolygonCount * sizeof(RoadMapPolygon1)) {
	roadmap_polygon_db_version = 0;
   } else if (roadmap_db_get_size (head_table) ==
	    context->PolygonCount * sizeof(RoadMapPolygon)) {
	roadmap_polygon_db_version = 1;
   } else {
	roadmap_log (ROADMAP_FATAL, "invalid polygon/head structure");
   }

   /* an "old" (1.1.0 and earlier) set of maps may have a "point"
    * table and no "line" table.  later, we only use the line table.
    */
   if (line_table) {
      context->PolygonLines =
         (RoadMapPolygonLine *) roadmap_db_get_data (line_table);
      context->PolygonLineCount = roadmap_db_get_count (line_table);

      if (roadmap_db_get_size (line_table) !=
          context->PolygonLineCount * sizeof(RoadMapPolygonLine)) {
         roadmap_log (ROADMAP_FATAL, "invalid polygon/line structure");
      }
   } else {
      if (point_table) {
         roadmap_log (ROADMAP_INFO, "Found old-style polygon data -- skipping.");
      }
      context->PolygonLines = NULL;
      context->PolygonLineCount = 0;
   }


   return context;
}

static void roadmap_polygon_activate (void *context) {

   RoadMapPolygonContext *polygon_context = (RoadMapPolygonContext *) context;

   if ((polygon_context != NULL) &&
       (polygon_context->type != RoadMapPolygonType)) {
      roadmap_log (ROADMAP_FATAL, "cannot activate (invalid context type)");
   }
   RoadMapPolygonActive = polygon_context;
}

static void roadmap_polygon_unmap (void *context) {

   RoadMapPolygonContext *polygon_context = (RoadMapPolygonContext *) context;

   if (polygon_context->type != RoadMapPolygonType) {
      roadmap_log (ROADMAP_FATAL, "cannot activate (invalid context type)");
   }
   if (RoadMapPolygonActive == polygon_context) {
      RoadMapPolygonActive = NULL;
   }
   free (polygon_context);
}

roadmap_db_handler RoadMapPolygonHandler = {
   "polygons",
   roadmap_polygon_map,
   roadmap_polygon_activate,
   roadmap_polygon_unmap
};



int  roadmap_polygon_count (void) {

   if (RoadMapPolygonActive == NULL) return 0;

   if (RoadMapPolygonActive->PolygonLines == NULL) return 0;

   return RoadMapPolygonActive->PolygonCount;
}

int  roadmap_polygon_category1 (int polygon)
{
   RoadMapPolygon1 *polygons;
   polygons = (RoadMapPolygon1 *)(RoadMapPolygonActive->Polygons);
   return polygons[polygon].cfcc;
}

int  roadmap_polygon_category2 (int polygon)
{
   RoadMapPolygon *polygons;
   polygons = RoadMapPolygonActive->Polygons;
   return polygons[polygon].cfcc;
}


void roadmap_polygon_edges1 (int polygon, RoadMapArea *edges)
{

   RoadMapPolygon1 *polygons;
   polygons = (RoadMapPolygon1 *)(RoadMapPolygonActive->Polygons);

   edges->west = polygons[polygon].west;
   edges->east = polygons[polygon].east;
   edges->north = polygons[polygon].north;
   edges->south = polygons[polygon].south;
}

void roadmap_polygon_edges2 (int polygon, RoadMapArea *edges)
{

   RoadMapPolygon *polygons;
   polygons = RoadMapPolygonActive->Polygons;

   *edges = polygons[polygon].area;
}

int  roadmap_polygon_lines1 (int polygon, int **listp)
{

   RoadMapPolygon1      *polygons;

   int count;
   int first;

   polygons = (RoadMapPolygon1 *)(RoadMapPolygonActive->Polygons);
   polygons += polygon;

   count = roadmap_polygon_get_count(polygons);
   first = roadmap_polygon_get_first(polygons);

   *listp = RoadMapPolygonActive->PolygonLines + first;

   return count;
}

int  roadmap_polygon_lines2 (int polygon, int **listp)
{

   RoadMapPolygon      *polygons;

   int count;
   int first;

   polygons = RoadMapPolygonActive->Polygons;
   polygons += polygon;

   count = polygons->count;
   first = polygons->first;

   *listp = RoadMapPolygonActive->PolygonLines + first;

   return count;
}

struct roadmap_polygon_version {
    int (*category)(int polygon);
    void (*edges)(int polygon, RoadMapArea *edges);
    int (*lines)(int polygon, int **listp);
};

struct roadmap_polygon_version roadmap_polygon_versions[] = {
    {
	roadmap_polygon_category1,
	roadmap_polygon_edges1,
	roadmap_polygon_lines1
    }, {
	roadmap_polygon_category2,
	roadmap_polygon_edges2,
	roadmap_polygon_lines2
    }
};

int roadmap_polygon_category(int polygon) {

    int layer = (roadmap_polygon_versions[roadmap_polygon_db_version].category)
                (polygon);

    return roadmap_locator_layer_to_roadmap(layer);

}

void roadmap_polygon_edges(int polygon, RoadMapArea *edges) {

    (roadmap_polygon_versions[roadmap_polygon_db_version].edges)
                (polygon, edges);

}

int roadmap_polygon_lines(int polygon, int **listp) {

    return (roadmap_polygon_versions[roadmap_polygon_db_version].lines)
                (polygon, listp);

}

