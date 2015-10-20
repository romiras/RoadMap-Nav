/*
 * LICENSE:
 *
 *   Copyright (c) 2008, 2009, 2010, Danny Backx.
 *
 *   Based on code Copyright 2007 Paul Fox that interprets the OSM
 *   binary stream.
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
 * @brief a module to read OSM text format
 */

/*
 * this module uses libreadosm to process OSM input.  see:
 *  http://www.gaia-gis.it/gaia-sins/readosm-1.0.0b-doxy-doc/index.html
 * it's packaged for debian and others.  for fedora, see:
 *  http://rpmfind.net//linux/RPM/fedora/updates/18/armhfp/readosm-1.0.0b-1.fc18.armv7hl.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <readosm.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_math.h"
#include "roadmap_path.h"
#include "roadmap_file.h"
#include "roadmap_osm.h"
#include "roadmap_line.h"
#include "roadmap_hash.h"

#include "buildmap.h"
#include "buildmap_square.h"
#include "buildmap_point.h"
#include "buildmap_place.h"
#include "buildmap_line.h"
#include "buildmap_street.h"
#include "buildmap_range.h"
#include "buildmap_area.h"
#include "buildmap_shape.h"
#include "buildmap_polygon.h"

#include "buildmap_layer.h"
#include "buildmap_osm_text.h"

#include "buildmap_osm_layers.h"

extern char *BuildMapResult;

/* OSM has over 2G nodes already -- enough to overflow a 32 bit signed int */
typedef unsigned nodeid_t;
typedef unsigned wayid_t;


/**
 * @brief some global variables
 */
static int      PolygonId = 0;
static int      LineId = 0;

static int l_shoreline, l_boundary;
static int nWays, nNodes;

/**
 * @brief add string to dictionary, taking note of null inputs
 * @param d
 * @param string
 * @return
 */
static RoadMapString str2dict (BuildMapDictionary d, const char *string) {

   if (string == 0 || *string == 0) {
      return buildmap_dictionary_add (d, "", 0);
   }

   return buildmap_dictionary_add (d, (char *) string, strlen(string));
}

int
qsort_compare_unsigneds(const void *id1, const void *id2)
{
    if (*(unsigned int *)id1 < *(unsigned int *)id2)
	    return -1;
    else if (*(unsigned int *)id1 > *(unsigned int *)id2)
	    return 1;
    else
	    return 0;
}

/**
 * @brief incoming lat/lon nodes are translated to roadmap points,
 *	and a hash is maintained
 */
static int nPointsAlloc = 0;
static int nPoints = 0;
static struct points {
        int     id;
        int     point;
} *Points = 0;

RoadMapHash	*PointsHash = NULL;

static void
buildmap_osm_text_point_hash_reset(void)
{
    nPoints = 0;
    PointsHash = 0;
    if (Points) free(Points);
    Points = 0;
    nPointsAlloc = 0;
}

static void
buildmap_osm_text_point_add(int id, int point)
{
        if (nPoints == nPointsAlloc) {
		if (Points)
		    nPointsAlloc *= 2;
		else
		    nPointsAlloc = 10000;
		if (PointsHash == NULL)
			PointsHash = roadmap_hash_new("PointsHash", nPointsAlloc);
		else
			roadmap_hash_resize(PointsHash, nPointsAlloc);

                Points = realloc(Points, sizeof(struct points) * nPointsAlloc);
		buildmap_check_allocated(Points);
        }

	roadmap_hash_add(PointsHash, id, nPoints);

        Points[nPoints].id = id;
        Points[nPoints++].point = point;
}

/**
 * @brief look up a point by id
 * @param id the external representation
 * @return the internal index
 */
static int
buildmap_osm_text_point_get(int id)
{
        int     i;

        for (i = roadmap_hash_get_first(PointsHash, id);
			i >= 0;
			i = roadmap_hash_get_next(PointsHash, i))
                if (Points[i].id == id)
                        return Points[i].point;
        return -1;
}



static int maxWayTable = 0;
int nWayTable = 0;
wayid_t *WayTable = NULL;

wayid_t *WayTableCopy = NULL;

static void
copyWayTable(void)
{
	WayTableCopy = (wayid_t *) malloc(sizeof(wayid_t) * nWayTable);
	buildmap_check_allocated(WayTableCopy);
	memcpy(WayTableCopy, WayTable, nWayTable * sizeof(wayid_t));
}

/**
 * @brief  we mark a way as interesting by adding it to WayTable.  this
 *	is later sorted, and we can use it later to quickly check if
 *	we thought it was interesing.
 * @param wayid
 */
static void
saveInterestingWay(wayid_t wayid)
{

	if (nWayTable == maxWayTable) {
		if (WayTable)
		    maxWayTable *= 2;
		else
		    maxWayTable = 1000;
		WayTable = (wayid_t *) realloc(WayTable,
				sizeof(wayid_t) * maxWayTable);
		buildmap_check_allocated(WayTable);
	}

	WayTable[nWayTable] = wayid;
	nWayTable++;
}

/**
 * @brief after WayTable has been sorted, see if this way is interesting
 * @param wayid
 * @return whether we thought way was interesting when we found it
 */
static wayid_t *
isWayInteresting(wayid_t wayid)
{
	wayid_t *r;
	r = (wayid_t *)bsearch(&wayid, WayTable, nWayTable,
                             sizeof(*WayTable), qsort_compare_unsigneds);
	return r;
}

static int maxNodeTable = 0;
static int nNodeTable = 0;
static nodeid_t *NodeTable = NULL;


/*
 * @brief creates our .ways table, later used by our neighbors so they
 *    know which ways are already being taken care of by us.
 */
void
buildmap_osm_text_save_wayids(const char *path, const char *outfile)
{
    char nfn[1024];
    char *p;
    FILE *fp;
    wayid_t *wp;

    strcpy(nfn, outfile);
    p = strrchr(nfn, '.');
    if (p) *p = '\0';
    strcat(nfn, ".ways");

    if (!nWayTable) {
	unlink(nfn);
	return;
    }

    fp = roadmap_file_fopen (path, nfn, "w");

    /* write out the list.  it's already sorted, and unwanted entries
     * have been zeroed.  */
    for (wp = WayTableCopy; wp < &WayTableCopy[nWayTable]; wp++) {
	if (*wp) {
	    fwrite(wp, sizeof(wayid_t), 1, fp);
	}
    }

    fclose(fp);
}


/**
 * @brief  we mark a node as interesting by adding it to NodeTable.  this
 *	is later sorted, and we can use it later to quickly check if
 *	we thought it was interesting.
 * @param nodeid
 */
static void
saveInterestingNode(nodeid_t nodeid)
{

	if (nNodeTable == maxNodeTable) {
		if (NodeTable)
		    maxNodeTable *= 2;
		else
		    maxNodeTable = 1000;
		NodeTable = (nodeid_t *) realloc(NodeTable,
				sizeof(*NodeTable) * maxNodeTable);
		buildmap_check_allocated(NodeTable);
	}

	NodeTable[nNodeTable] = nodeid;
	nNodeTable++;
}

/**
 * @brief after NodeTable has been sorted, see if this node is interesting
 * @param nodeid
 * @return whether we thought way was interesting when we found it
 */
static int
isNodeInteresting(nodeid_t nodeid)
{
	int *r;

        r = bsearch(&nodeid, NodeTable, nNodeTable,
                             sizeof(*NodeTable), qsort_compare_unsigneds);
	if (!r) return 0;

	return *r;
}


/**
 * @brief  checks to see if the tag and value found in the input data
 *	corresponds to a layer we're interested in recording in the map.
 *      the layer is returned if so (but we don't touch it if not).  the
 *	returned flags indicate what sort of layer this is (PLACE, AREA).
 * @param lookfor limits the search to matching types (PLACE, AREA)
 */

int
buildmap_osm_get_layer(int lookfor, const char *tag, const char *value,
		int *flags, int *layer)
{
    int		i,j;
    value_info_t	*value_list;

    for (i=1; tag_info[i].osm_tname != 0; i++) {
	if ((lookfor & tag_info[i].flags) &&
		strcmp(tag, tag_info[i].osm_tname) == 0) {
	    value_list = tag_info[i].value_list;
	    if (value_list) {
		for (j=1; value_list[j].osm_vname; j++) {
		    if (strcmp(value, value_list[j].osm_vname) == 0) {
			*flags = value_list[j].flags;
			if (value_list[j].layerp)
				*layer = *(value_list[j].layerp);
			return 1;
		    }
		}
	    }
	    break;
	}
    }
    return 0;
}


/**
 * @brief neighbor_ways keeps track of ways our neighbors are already 
 *	taking care of.
 */
struct neighbor_ways {
    int tileid;		    // these are the ways for this tileid
    RoadMapFileContext fc;  // file context for the mmap
    int count;	    	    // how many ways
    const wayid_t *ways;    // the way ids for that tileid.
			    // (also serves as the "populated" flag.)
} Neighbor_Ways[8];

/*
 * @brief memory maps the way list for the give tileid, if it exists
 */
void
buildmap_osm_text_load_neighbor_ways(int neighbor,
			struct neighbor_ways *neighbor_ways)
{
	const char *waysmap;
        RoadMapFileContext fc;

	waysmap = roadmap_file_map(BuildMapResult,
		    roadmap_osm_filename(0, 1, neighbor, ".ways"), "r", &fc);
	if (waysmap) {
	    neighbor_ways->tileid = neighbor;
	    neighbor_ways->fc = fc;
	    neighbor_ways->count = roadmap_file_size(fc) / sizeof(wayid_t);
	}
	neighbor_ways->ways = (wayid_t *)waysmap;
}

void
buildmap_osm_text_unload_neighbor_ways(int i)
{
	roadmap_file_unmap (&Neighbor_Ways[i].fc);
	Neighbor_Ways[i].ways = 0;
}

/*
 * @brief populates the correct way maps for all of our neighbors.
 *        we don't really care which neighbor map is which.
 */
void
buildmap_osm_text_neighbor_way_maps(int tileid)
{
	struct neighbor_ways new_neighbor_ways[8];
	int neighbor_tile;
	int i, j;

	/* stage a new set of neighbor way lists.  we reuse those
	 * that we can, and load new lists when we can't.
	 */
	for (i = 0; i < 8; i++) {
	    neighbor_tile = roadmap_osm_tileid_to_neighbor(tileid, i);

	    /* if we already have that neighbor's ways, reuse */
	    for (j = 0; j < 8; j++) {
		if (Neighbor_Ways[j].tileid == neighbor_tile) {
		    new_neighbor_ways[i] = Neighbor_Ways[j];
		    Neighbor_Ways[j].ways = 0;
		    break;
		}
	    }

	    /* didn't find it -- fetch the neighbor's ways */
	    if (j == 8) {
		buildmap_osm_text_load_neighbor_ways(neighbor_tile,
				&new_neighbor_ways[i]);
	    }
	}

	/* release any old way lists we're not reusing */
	for (i = 0; i < 8; i++)
	    if (Neighbor_Ways[i].ways)
		buildmap_osm_text_unload_neighbor_ways(i);

	/* our new set of reused or freshly loaded way lists into place */
	for (i = 0; i < 8; i++)
	    Neighbor_Ways[i] = new_neighbor_ways[i];

}

/*
 * @brief check to see if the given way exists in any of
 *        our neighbors.
 */
int
buildmap_osm_text_check_neighbors(wayid_t wayid)
{
	int i;
	wayid_t *r;

	for (i = 0; i < 8; i++) {
		if (Neighbor_Ways[i].ways) {
			r = bsearch(&wayid, Neighbor_Ways[i].ways,
				Neighbor_Ways[i].count,
				sizeof(*(Neighbor_Ways[i].ways)),
				 qsort_compare_unsigneds);
			if (r)
				return 1;
		}
	}
	return 0;
}

/**
 * @brief this structure keeps shape data for a postprocessing step
 */
struct shapeinfo {
    int lineid;
    int count;
    int *lons;
    int *lats;
};

static int nShapes = 0;
static int nShapesAlloc = 0;
static struct shapeinfo *shapes;

/**
 * @brief a postprocessing step to load shape info
 *
 * this needs to be a separate step because lines need to be sorted
 *
 * The shapes don't include the first and last points of a line, hence the
 * strange loop beginning/end for the inner loop.
 */
static int
buildmap_osm_text_ways_shapeinfo(void)
{
    int i, j, count, lineid;  /* , need; */
    int *lons, *lats;  /* , *used; */
    int line_index;

    buildmap_info("loading shape info (from %d ways) ...", nShapes);

    buildmap_line_sort();

    for (i = 0; i < nShapes; i++) {

        count = shapes[i].count;

        if (count <= 2)
            continue;

        lineid = shapes[i].lineid;
        line_index = buildmap_line_find_sorted(lineid);

        if (line_index >= 0) {
            lons = shapes[i].lons;
            lats = shapes[i].lats;

            /* Add the shape points here, don't include beginning and end
             * point */
            for (j = 1; j < count - 1; j++) {
		buildmap_shape_add
		    (line_index, i, lineid, j - 1, lons[j], lats[j]);
            }
        }
    }

    return 1;
}


char *readosm_errors[] = {
    "READOSM_OK",
    "READOSM_INVALID_SUFFIX",
    "READOSM_FILE_NOT_FOUND",
    "READOSM_NULL_HANDLE",              
    "READOSM_INVALID_HANDLE",           
    "READOSM_INSUFFICIENT_MEMORY",      
    "READOSM_CREATE_XML_PARSER_ERROR",  
    "READOSM_READ_ERROR",               
    "READOSM_XML_ERROR",                
    "READOSM_INVALID_PBF_HEADER",       
    "READOSM_UNZIP_ERROR",              
    "READOSM_ABORT"                     
};

/**
 * @brief callback for initial node parsing, called via readosm_parse()
 */
static int
parse_node(const void *is_tile, const readosm_node * node)
{
    const readosm_tag *tag;
    const char *name = NULL, *place = NULL;
    int layer = 0, flags = 0;
    int i;

    nNodes++;

    for (i = 0; i < node->tag_count; i++)
    {
	tag = node->tags + i;
	if (strcasecmp(tag->key, "name") == 0) {
	    name = tag->value;
	    break;
        } else if (buildmap_osm_get_layer(PLACE, tag->key, tag->value,
			    &flags, &layer)) {
	    if (flags & PLACE)
		place = tag->value;
	    break;
	}
    }

    if (name || (flags & PLACE)) {
	buildmap_debug( "saving %s %s", place, name);
	saveInterestingNode((nodeid_t)(node->id));
    }

    return READOSM_OK;
}

/**
 * @brief convert from 12.3456789 to 12345679.  note the rounding.
 */
int
osmfloat_to_rdmint(double f)
{
    int i;
    i = 10000000. * f;
    if (i < 0)
	return (i-5)/10;
    else
	return (i+5)/10;
}

/**
 * @brief callback for final node parsing, called via readosm_parse()
 */
static int
parse_node_finalize(const void *is_tile, const readosm_node * node)
{
    const readosm_tag *tag;
    const char *name = NULL, *place = NULL;
    int layer = 0, flags = 0;
    int point;
    RoadMapString s;
    int i;
    int lat, lon;

    if (!isNodeInteresting(node->id))
	return READOSM_OK;

    for (i = 0; i < node->tag_count; i++)
    {
	tag = node->tags + i;
	if (strcasecmp(tag->key, "name") == 0) {
	    name = tag->value;
        } else if (buildmap_osm_get_layer(PLACE, tag->key, tag->value,
			    &flags, &layer)) {
	    if (flags & PLACE)
		place = tag->value;
	}
    }

    /* Add this interesting node as a point */
    /* libreadosm uses float to handle lat/lon, which introduces
     * small errors.  try and ensure we get the same lat/lon as OSM
     * provided.  it doesn't make a difference in practice, but can
     * be annoying when debugging, or comparing values later.
     */
    lat = osmfloat_to_rdmint(node->latitude);
    lon = osmfloat_to_rdmint(node->longitude);

    point = buildmap_point_add(lon, lat);

    buildmap_osm_text_point_add(node->id, point);

    /* If it's a place, add it to the place table as well */
    if (place) {
	int p;
	if (layer) {
	    if (name) {
		s = str2dict (DictionaryCity, (char *)name);
		p = buildmap_place_add(s, layer, point);
	    }
	    buildmap_debug( "finishing %1.7f %1.7f [%d %d] %s (%d) %s layer: %d",
		node->latitude, node->longitude,
		lat, lon, place, p, name, layer);
	} else {
	    buildmap_debug( "dropping %s %s", place, name);
	}
    }

    return READOSM_OK;
}

/**
 * @brief callback for initial way parsing, called via readosm_parse()
 */
static int
parse_way(const void *is_tile, const readosm_way *way)
{
    const readosm_tag *tag;
    const char *tourism = NULL, *amenity = NULL;
    int adminlevel = 0;
    int is_building = 0, is_territorial = 0, is_coast = 0;  // booleans
    int layer = 0, flags = 0;
    int i;

    nWays++;

    if (way->node_ref_count == 0)
	return READOSM_OK;

    /* if we're processing a quadtile, don't include any
     * ways that our neighbors already include */
    if (is_tile && buildmap_osm_text_check_neighbors(way->id)) {
	buildmap_verbose("dropping way %d because a neighbor "
		    "already has it", way->id);
	return READOSM_OK;
    }

    for (i = 0; i < way->tag_count; i++)
    {
	tag = way->tags + i;

	if (strcasecmp(tag->key, "building") == 0) {
	    is_building = 1;
#ifdef BUILDMAP_NAVIGATION_SUPPORT
	} else if (strcasecmp(tag->key, "oneway") == 0) {
	    is_oneway = !strcasecmp(tag->value, "yes");
#endif
	} else if (strcasecmp(tag->key, "tourism") == 0) {
	    tourism = tag->value;
	} else if (strcasecmp(tag->key, "amenity") == 0) {
	    amenity = tag->value;
	} else if (strcasecmp(tag->key, "admin_level") == 0) {
	    adminlevel = atoi(tag->value);
	} else if (strcasecmp(tag->key, "border_type") == 0) {
	    is_territorial = !strcasecmp(tag->value, "territorial");
	} else if (strcasecmp(tag->key, "natural") == 0 &&
		    strcasecmp(tag->key, "coastline") == 0) {
	    is_coast = 1;
	}

	// really?
	buildmap_osm_get_layer(ANY, tag->key, tag->value, &flags, &layer);
    }


    /* only save buildings that are amenities or touristic */
    if (is_building) {
	if (tourism) {
	    buildmap_osm_get_layer(PLACE, "tourism", tourism, &flags, &layer);
	} else if (amenity) {
	    buildmap_osm_get_layer(PLACE, "amenity", amenity, &flags, &layer);
	} else {
	    return READOSM_OK;
	}
    }

    if (layer == 0) {
	if (is_coast) {
	    layer = l_shoreline;
	} else if (adminlevel) {
	    /* national == 2, state == 4, ignore lesser boundaries */
	    /* also ignore territorial (marine) borders */
	    if  (adminlevel > 4 || is_territorial) {
		    return READOSM_OK;
	    }
#if LATER /* we don't actually have the points yet in pass 1 */
	      else if (adminlevel > 2) {
		from_point = buildmap_osm_text_point_get(WayNodes[0]);
		fromlon = buildmap_point_get_longitude(from_point);
		fromlat = buildmap_point_get_latitude(from_point);

		/* if we're not (roughly) in north america,
		 * discard state boundaries as well.  */
		if (fromlon > -32 || fromlat < 17) {
			/* east of the azores or south of mexico */
			return READOSM_OK;
		}
	    }
#endif

	    layer = l_boundary;
	}
    }

    if (layer) {
	saveInterestingWay((wayid_t)(way->id));
	for (i = 0; i < way->node_ref_count; i++)
	    saveInterestingNode((nodeid_t)way->node_refs[i]);
    }

    return READOSM_OK;
}

/**
 * @brief callback for final way parsing, called via readosm_parse()
 */
static int
parse_way_finalize(const void *is_tile, const readosm_way *way)
{
    const readosm_tag *tag;
    const char *tourism = NULL, *amenity = NULL, *name = NULL, *ref = NULL;
    int is_oneway = 0;  // booleans
    int layer = 0, flags = 0;
    int from_point, to_point, line, street;
    RoadMapString rms_dirp, rms_dirs, rms_type, rms_name;
    char compound_name[1024];
    const char *n;
    int i, j;

    if (!isWayInteresting(way->id))
	return READOSM_OK;

    if (way->node_ref_count == 0)
	return READOSM_OK;

    for (i = 0; i < way->tag_count; i++)
    {
	tag = way->tags + i;

	if (strcasecmp(tag->key, "name") == 0) {
	    name = tag->value;
	} else if (strcasecmp(tag->key, "ref") == 0) {
	    ref = tag->value;
#ifdef BUILDMAP_NAVIGATION_SUPPORT
	} else if (strcasecmp(tag->key, "oneway") == 0) {
	    is_oneway = !strcasecmp(tag->value, "yes");
#endif
	} else if (strcasecmp(tag->key, "tourism") == 0) {
	    tourism = tag->value;
	} else if (strcasecmp(tag->key, "amenity") == 0) {
	    amenity = tag->value;
	}

	// this will preserve the layer/flags for the last tag that
	// indicates a layer.
	buildmap_osm_get_layer(ANY, tag->key, tag->value, &flags, &layer);
    }

    if ( layer == 0) {
	    wayid_t *wp;
	    buildmap_debug("discarding way %d, not interesting (%s)", way->id, name);
	    wp = isWayInteresting(way->id);
	    if (wp) {
		WayTableCopy[wp-WayTable] = 0;
	    }
	    return READOSM_OK;
    }

    rms_dirp = str2dict(DictionaryPrefix, "");
    rms_dirs = str2dict(DictionarySuffix, "");
    rms_type = str2dict(DictionaryType, "");
    rms_name = 0;


    if ((flags & PLACE) && (tourism || amenity)) {
	/* we're finishing a way, but the flags may say PLACE if
	 * we're treating a polygon as a place, for instance.  find
	 * the center of the bounding box (which is good enough, for
	 * these purposes), and make it into a place.
	 */
	    int minlat = 999999999, maxlat = -999999999;
	    int minlon = 999999999, maxlon = -999999999;
	    int point, lon, lat;

	    for (j = 0; j < way->node_ref_count; j++) {
		    point = buildmap_osm_text_point_get(way->node_refs[j]);
		    lon = buildmap_point_get_longitude(point);
		    lat = buildmap_point_get_latitude(point);

		    if (lat < minlat) minlat = lat;
		    if (lat > maxlat) maxlat = lat;
		    if (lon < minlon) minlon = lon;
		    if (lon > maxlon) maxlon = lon;
	    }

	    lat = (maxlat + minlat) / 2;
	    lon = (maxlon + minlon) / 2;

	    /* this code looks like buildmap_osm_text_node_finish() */
	    point = buildmap_point_add(lon, lat);

	    if (layer) {
		RoadMapString s;
		int p;
		s = str2dict (DictionaryCity, (char *)name);
		p = buildmap_place_add(s, layer, point);
		buildmap_debug( "wayplace: finishing %f %f %s (%d) %s layer: %d",
		    (float)lat/1000000.0, (float)lon/1000000.0,
		    tourism ? tourism : amenity, p, name, layer);
	    } else {
		buildmap_debug( "dropping %s %s",
		    tourism ? tourism : amenity, name);
	    }


    } else if ((flags & AREA) &&
		(way->node_refs[0] == way->node_refs[way->node_ref_count-1])) {
	    /* see http://wiki.openstreetmap.org/wiki/The_Future_of_Areas
	     * for why the above conditions are simplistic */

	    /*
	     * Detect an AREA -> create a polygon
	     */
	    PolygonId++;

	    rms_name = str2dict(DictionaryStreet, name);
	    buildmap_polygon_add_landmark (PolygonId, layer, rms_name);
	    buildmap_polygon_add(PolygonId, 0, PolygonId);

	    for (j=1; j<way->node_ref_count; j++) {
		    int prevpoint =
			buildmap_osm_text_point_get(way->node_refs[j-1]);
		    int point =
			buildmap_osm_text_point_get(way->node_refs[j]);

		    LineId++;
		    buildmap_line_add
			    (LineId, layer, prevpoint, point,
			     ROADMAP_LINE_DIRECTION_BOTH);
		    buildmap_polygon_add_line (0, PolygonId, LineId, POLYGON_SIDE_RIGHT);
	    }
    } else {
	    /*
	     * Register the way
	     *
	     * Need to do this several different ways :
	     * - begin and end points form a "line"
	     * - register street name
	     * - adjust the square
	     * - keep memory of the point coordinates so we can
	     *   postprocess the so called shapes (otherwise we only have
	     *   straight lines)
	     */

	    int     *lonsbuf, *latsbuf;

	    /* Street name */
	    buildmap_debug ("Way %d [%s] ref [%s]", way->id,
			    name ? name : "", ref ? ref : "");

	    if (ref) {
		char *d = compound_name;
		const char *s = ref;
#define ENDASHSEP " \xe2\x80\x93 "
#define EMDASHSEP " \xe2\x80\x94 "
		while (*s) {  /* OSM separates multi-value refs with ';' */
		    if (*s == ';') {
			d += sprintf(d, ENDASHSEP);
		    } else {
			*d++ = *s;
		    }
		    s++;
		}
		n = compound_name;
		*d = '\0';

		if (name) {
		    // sprintf(d, ", %s", name);
		    sprintf(d, "%s%s", ENDASHSEP, name);
		}
	    } else {
		n = name;
	    }
	    rms_name = str2dict(DictionaryStreet, n);

	    LineId++;
	    /* Map begin and end points to internal point id */
	    from_point = buildmap_osm_text_point_get(way->node_refs[0]);
	    to_point = buildmap_osm_text_point_get(way->node_refs[way->node_ref_count-1]);

	    line = buildmap_line_add(LineId,
		    layer, from_point, to_point, is_oneway);

	    street = buildmap_street_add(layer,
			    rms_dirp, rms_name, rms_type,
			    rms_dirs, line);
	    buildmap_range_add_no_address(line, street);

	    /*
	     * We're passing too much here - the endpoints of
	     * the line don't need to be passed to the shape
	     * module.  We're keeping them here just to be on
	     * the safe side, they'll be ignored in
	     * buildmap_osm_text_ways_shapeinfo().
	     *
	     * The lonsbuf/latsbuf are never freed, need to be
	     * preserved for shape registration which happens
	     * at the end of the program run, so exit() will
	     * free this for us.
	     */

	    lonsbuf = calloc(way->node_ref_count, sizeof(way->node_refs[0]));
	    latsbuf = calloc(way->node_ref_count, sizeof(way->node_refs[0]));

	    for (j=0; j < way->node_ref_count; j++) {
		    int point =
			buildmap_osm_text_point_get(way->node_refs[j]);
		    int lon = buildmap_point_get_longitude(point);
		    int lat = buildmap_point_get_latitude(point);

		    /* Keep info for the shapes */
		    lonsbuf[j] = lon;
		    latsbuf[j] = lat;

		    buildmap_square_adjust_limits(lon, lat);
	    }

	    if (nShapes == nShapesAlloc) {
		    /* Allocate additional space (in big
		     * chunks) when needed */
		    if (shapes)
			nShapesAlloc *= 2;
		    else
			nShapesAlloc = 1000;
		    shapes = realloc(shapes,
			nShapesAlloc * sizeof(struct shapeinfo));
		    buildmap_check_allocated(shapes);
	    }

	    buildmap_debug("lineid %d way->node_ref_count %d",
		    LineId, way->node_ref_count);
	    /* Keep info for the shapes */
	    shapes[nShapes].lons = lonsbuf;
	    shapes[nShapes].lats = latsbuf;
	    shapes[nShapes].count = way->node_ref_count;
	    shapes[nShapes].lineid = LineId;

	    nShapes++;
    }
    return READOSM_OK;
}

#if NEEDED
static int
parse_relation(const void *user_data, const readosm_relation * relation)
{
    return READOSM_OK;
}
#endif

/**
 * @brief rather than trying to put an entire file's contents in memory,
 *	we parse it in two passes.  this routine is called for each pass.
 */
void buildmap_readosm_pass(int pass, char *fn, int tileid)
{
    int ret;
    const void *handle;
    void *is_tile = 0;

    if (tileid) is_tile = (void *)1;

    buildmap_info("Starting pass %d", pass);
    // buildmap_set_source("pass %d", pass);

    ret = readosm_open(fn, &handle);
    if (ret != READOSM_OK) {
	buildmap_fatal(0, "buildmap_osm_text: couldn't open \"%s\", %s",
		fn, readosm_errors[-ret]);
	return;
    }

    switch (pass) {
    case 1: ret = readosm_parse(handle, is_tile,
    		parse_node, parse_way, NULL);
	    break;
    case 2: ret = readosm_parse(handle, is_tile,
    		parse_node_finalize, parse_way_finalize, NULL);
	    break;
    }
    if (ret != READOSM_OK) {
	buildmap_fatal(0, "buildmap_osm_text pass %d: %s",
		pass, readosm_errors[-ret]);
	return;
    }

    ret = readosm_close(handle);
    if (ret != READOSM_OK) {
	buildmap_fatal(0, "buildmap_osm_text pass %d: %s",
		pass, readosm_errors[-ret]);
	return;
    }
}


/**
 * @brief This is the gut of buildmap_osm_text : parse an OSM XML file
 * @param fdata an open file pointer, this will get read twice
 * @param country_num the country id that we're working for
 * @param division_num the country subdivision id that we're working for
 *
 * currently this only handles tiles.  the country and division parameters
 *  are ignored.
 *
 */
void 
buildmap_osm_text_read(char *fn, int tileid,
			int country_num, int division_num)
{

    if (tileid)
	buildmap_osm_text_neighbor_way_maps(tileid);

    buildmap_osm_text_point_hash_reset();

    DictionaryPrefix = buildmap_dictionary_open("prefix");
    DictionaryStreet = buildmap_dictionary_open("street");
    DictionaryType = buildmap_dictionary_open("type");
    DictionarySuffix = buildmap_dictionary_open("suffix");
    DictionaryCity = buildmap_dictionary_open("city");

    buildmap_osm_common_find_layers ();
    l_shoreline = buildmap_layer_get("shore");;
    l_boundary = buildmap_layer_get("boundaries");;

    nWays = 0;
    nNodes = 0;
    nShapes = 0;
    nWayTable = 0;
    nNodeTable = 0;
    PolygonId = 0;
    LineId = 0;

    /* pass 1: look at all ways and nodes, decide if they're interesting */
    buildmap_readosm_pass(1, fn, tileid);

    qsort(WayTable, nWayTable, sizeof(*WayTable), qsort_compare_unsigneds);
    qsort(NodeTable, nNodeTable, sizeof(*NodeTable), qsort_compare_unsigneds);

    /* we've finished saving ways.  we need a fresh place to mark
     * the ones we're discarding, while still keeping the original list
     * quickly searchable. */
    copyWayTable();

    /* pass 2: look at ways again, and finalize */
    buildmap_readosm_pass(2, fn, tileid);

    buildmap_osm_text_ways_shapeinfo();

    buildmap_info("Ways %d, interesting %d",
	    nWays, nWayTable);
    buildmap_info("Number of nodes : %d, interesting %d", nNodes, nNodeTable);
    buildmap_info("Number of points: %d", nPoints);
}
