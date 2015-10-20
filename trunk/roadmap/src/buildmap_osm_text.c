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
#include <errno.h>

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

/* OSM has over 3G nodes already -- enough to overflow a 32 bit signed
 * integer.  nodeid_t should really be "long long".  i think that may have
 * repercussions for the roadmap_hash layer, though.
 *
 * NB: the way->id and node->id elements passed in from readosm
 * are already long longs.
 */
typedef unsigned osm_id_t;
typedef osm_id_t nodeid_t;
typedef osm_id_t wayid_t;
typedef osm_id_t relid_t;

struct relinfo {
    relid_t id;  // must be first
    int layer;
    int flags;
    char *name;
};
typedef struct relinfo relinfo;

struct wayinfo {
    wayid_t id;  // must be first
    int lineid, lineid2;
    int layer;
    int flags;
    char *name;
    unsigned relation_id;
    int relation_layer;
    int relation_flags;
    nodeid_t from;
    nodeid_t to;
    int ring;
};
typedef struct wayinfo wayinfo;


/**
 * @brief some global variables
 */
static int      LandmarkId = 0;
static int      PolygonId = 0;
static int      LineId = 0;

static int l_shoreline, l_boundary, l_lake, l_island;
static int nRels, nWays, nNodes;

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

int
qsort_compare_osm_ids(const void *id1, const void *id2)
{
    wayinfo *w1, *w2;
    w1 = (wayinfo *)id1;
    w2 = (wayinfo *)id2;
    if (w1->id < w2->id)
	    return -1;
    else if (w1->id > w2->id)
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
        nodeid_t    id;
        int         point;
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
buildmap_osm_text_point_add(nodeid_t id, int point)
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
buildmap_osm_text_point_get(nodeid_t id)
{
    int     i;

    for (i = roadmap_hash_get_first(PointsHash, id);
	    i >= 0;
	    i = roadmap_hash_get_next(PointsHash, i))
    if (Points[i].id == id)
	    return Points[i].point;
    return -1;
}

static int maxRelTable = 0;
int nRelTable = 0;
relinfo *RelTable = NULL;


/**
 * @brief  we mark a relation as interesting by adding it to RelTable.  this
 *	is later sorted, and we can use it later to quickly check if
 *	we thought it was interesing.
 * @param relid
 */
static void
saveInterestingRelation(relid_t id, char *name,
		int layer, int flags)
{
	relinfo *rp;

	if (nRelTable == maxRelTable) {
		if (RelTable)
		    maxRelTable *= 2;
		else
		    maxRelTable = 1000;
		RelTable = (relinfo *) realloc(RelTable,
				sizeof(relinfo) * maxRelTable);
		buildmap_check_allocated(RelTable);
	}

	rp = &RelTable[nRelTable];
	memset(rp, 0, sizeof(*rp));

	rp->id = id;
	rp->name = name;
	rp->layer = layer;
	rp->flags = flags;

	nRelTable++;
}

/**
 * @brief after RelTable has been sorted, see if this relation is interesting
 * @param relid
 * @return whether we thought relation was interesting when we found it
 */
static relinfo *
isRelationInteresting(relid_t relid)
{
	relinfo *r, rel;
	rel.id = relid;
	r = (relinfo *)bsearch(&rel, RelTable, nRelTable,
                             sizeof(*RelTable), qsort_compare_osm_ids);
	return r;
}



static int maxWayTable = 0;
int nWayTable = 0;
int nSearchableWays = 0;
wayinfo *WayTable = NULL;


/**
 * @brief  we mark a way as interesting by adding it to WayTable.  this
 *	is later sorted, and we can use it later to quickly check if
 *	we thought it was interesing.
 * @param wayid
 */
static void
saveInterestingWay(wayid_t id, const readosm_way *way,
		char *name,
		int layer, int flags,
		int relation_layer, int relation_flags,
		relid_t rel_id)
{
	wayinfo *wp;

	if (nWayTable == maxWayTable) {
		if (WayTable)
		    maxWayTable *= 2;
		else
		    maxWayTable = 1000;
		WayTable = (wayinfo *) realloc(WayTable,
				sizeof(wayinfo) * maxWayTable);
		buildmap_check_allocated(WayTable);
	}

	wp = &WayTable[nWayTable];
	memset(wp, 0, sizeof(*wp));

	wp->id = id;
	wp->name = name;
	if (layer) {
	    wp->layer = layer;
	    wp->flags = flags;
	}
	if (rel_id) {
	    wp->relation_id = rel_id;
	    wp->relation_layer = relation_layer;
	    wp->relation_flags = relation_flags;
	}

	if (way) {
	    wp->from = way->node_refs[0];
	    wp->to = way->node_refs[way->node_ref_count-1];
	}


	nWayTable++;
}

/**
 * @brief after WayTable has been sorted, see if this way is interesting
 * @param wayid
 * @return whether we thought way was interesting when we found it
 */
static wayinfo *
isWayInteresting(wayid_t wayid)
{
	wayinfo *r, w;
	w.id = wayid;
	r = (wayinfo *)bsearch(&w, WayTable, nSearchableWays,
                             sizeof(*WayTable), qsort_compare_osm_ids);

	if (r && r->layer )
	    return r;

	if (r && r->relation_layer)
	    return r;

	return NULL;
}

static int maxNodeTable = 0;
static int nNodeTable = 0;
static nodeid_t *NodeTable = NULL;


/*
 * @brief creates our .cov table, later used by our neighbors so they
 *    know which ways are already being taken care of by us.
 */
void
buildmap_osm_text_save_wayids(const char *path, const char *outfile)
{
    char nfn[1024];
    char *p;
    FILE *fp = 0;
    wayinfo *wp;
    relinfo *rp;
    relinfo delim;

    strcpy(nfn, outfile);
    p = strrchr(nfn, '.');
    if (p) *p = '\0';
    strcat(nfn, ".cov");

    fp = roadmap_file_fopen (path, nfn, "w");
    if (!fp) buildmap_fatal(0, "can't open %s/%s to write ways", path, nfn);

    if (nWayTable) {
	for (wp = WayTable; wp < &WayTable[nWayTable]; wp++) {
	    if (wp->id) {
		fwrite(wp, sizeof(wp->id), 1, fp);
	    }
	}
    }

    delim.id = ~0;
    fwrite(&delim, sizeof(rp->id), 1, fp);

    if (nRelTable) {
	for (rp = RelTable; rp < &RelTable[nRelTable]; rp++) {
	    if (rp->id) {
		fwrite(rp, sizeof(rp->id), 1, fp);
	    }
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
 * @brief neighbor_coverage keeps track of ways our neighbors are already 
 *	taking care of.
 */
struct neighbor_coverage {
    int tileid;		    // these are the ways for this tileid
    RoadMapFileContext fc;  // file context for the mmap
    int waycount;	    // how many ways
    const wayid_t *wayids; // the way ids for that tileid.
    int relcount;	    // how many relations
    const relid_t *relids; // the relations ids for that tileid.
} Neighbor_Coverage[8];

/*
 * @brief memory maps the way list for the give tileid, if it exists
 */
void
buildmap_osm_text_load_neighbor_coverage(int neighbor,
			struct neighbor_coverage *neighbor_coverage)
{
	const wayid_t *covmap;
        RoadMapFileContext fc;
	const wayid_t *idp;
	int fs = 0;

	covmap = (wayid_t *)roadmap_file_map(BuildMapResult,
		    roadmap_osm_filename(0, 1, neighbor, ".cov"), "r", &fc);
	if (covmap)
	    fs = roadmap_file_size(fc) / sizeof(wayid_t);

	neighbor_coverage->tileid = neighbor;

	if (!covmap || fs <= 1) {
	    if (covmap) roadmap_file_unmap (&fc);
	    neighbor_coverage->fc = fc;
	    neighbor_coverage->waycount = 0;
	    neighbor_coverage->relcount = 0;
	    return;
	}

	idp = covmap;
	while (*idp != (wayid_t)~0) {
	    idp++;
	    if ((idp - covmap) == fs) // didn't find delimiter
		break;
	}

	neighbor_coverage->waycount = idp - covmap;
	if (neighbor_coverage->waycount)
	    neighbor_coverage->wayids = covmap;
	else
	    neighbor_coverage->wayids = 0;

	neighbor_coverage->relcount = fs - neighbor_coverage->waycount - 1;
	if (neighbor_coverage->relcount > 0)
	    neighbor_coverage->relids = (relid_t *)(idp + 1);
	else
	    neighbor_coverage->relids = 0;
}

void
buildmap_osm_text_unload_neighbor_coverage(int i)
{
	roadmap_file_unmap (&Neighbor_Coverage[i].fc);
	Neighbor_Coverage[i].waycount = Neighbor_Coverage[i].relcount = 0;
}

/*
 * @brief populates the correct way maps for all of our neighbors.
 *        we don't really care which neighbor map is which.
 */
void
buildmap_osm_text_neighbor_way_maps(int tileid)
{
	struct neighbor_coverage new_neighbor_coverage[8];
	int neighbor_tile;
	int i, j;

	/* stage a new set of neighbor way lists.  we reuse those
	 * that we can, and load new lists when we can't.
	 */
	for (i = 0; i < 8; i++) {
	    neighbor_tile = roadmap_osm_tileid_to_neighbor(tileid, i);

	    /* if we already have that neighbor's ways, reuse */
	    for (j = 0; j < 8; j++) {
		if (Neighbor_Coverage[j].tileid == neighbor_tile) {
		    new_neighbor_coverage[i] = Neighbor_Coverage[j];
		    Neighbor_Coverage[i].waycount = Neighbor_Coverage[i].relcount = 0;
		    break;
		}
	    }

	    /* didn't find it -- fetch the neighbor's ways */
	    if (j == 8) {
		buildmap_osm_text_load_neighbor_coverage(neighbor_tile,
				&new_neighbor_coverage[i]);
	    }
	}

	/* release any old way lists we're not reusing */
	for (i = 0; i < 8; i++)
	    if (Neighbor_Coverage[i].waycount || Neighbor_Coverage[i].relcount )
		buildmap_osm_text_unload_neighbor_coverage(i);

	/* our new set of reused or freshly loaded way lists into place */
	for (i = 0; i < 8; i++)
	    Neighbor_Coverage[i] = new_neighbor_coverage[i];

}

/*
 * @brief check to see if the given way exists in any of
 *        our neighbors.
 */
static int
buildmap_osm_text_check_neighbor_way(wayid_t wayid)
{
    int i;
    wayid_t *r;

    for (i = 0; i < 8; i++) {
	if (Neighbor_Coverage[i].waycount) {
	    r = bsearch(&wayid, Neighbor_Coverage[i].wayids,
			Neighbor_Coverage[i].waycount,
			sizeof(*(Neighbor_Coverage[i].wayids)),
		     qsort_compare_osm_ids);
	    if (r)
		return 1;
	}
    }
    return 0;
}

/*
 * @brief check to see if the given relation exists in any of
 *        our neighbors.
 */
static int
buildmap_osm_text_check_neighbor_relation(relid_t relid)
{
    int i;
    relid_t *r;

    for (i = 0; i < 8; i++) {
	if (Neighbor_Coverage[i].relcount) {
	    r = bsearch(&relid, Neighbor_Coverage[i].relids,
			Neighbor_Coverage[i].relcount,
			sizeof(*(Neighbor_Coverage[i].relids)),
		     qsort_compare_osm_ids);
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
parse_node_final(const void *is_tile, const readosm_node * node)
{
    const readosm_tag *tag;
    const char *name = NULL, *place = NULL;
    int layer = 0, flags = 0;
    int point;
    RoadMapString s;
    int i;
    int lat, lon;
    // int places = 0;

    nNodes++;

    for (i = 0; i < node->tag_count; i++)
    {
	tag = node->tags + i;
	if (strcasecmp(tag->key, "name") == 0) {
	    name = tag->value;
        } else if (buildmap_osm_get_layer(PLACE, tag->key, tag->value,
			    &flags, &layer)) {
	    if (flags & PLACE)
		place = tag->value;
	    // fprintf(stderr, "t/v %s/%s, got place %d: %s %s\n",
	    // 		tag->key, tag->value, places, place, name);
	    // places++;
	}
    }

    /* unnamed, not a place, and not referenced by a way.  skip it */
    i = isNodeInteresting(node->id);
    if (!name && (flags != PLACE) && !isNodeInteresting(node->id)) {
	// fprintf(stderr, "dropping node %s, flags %d, interesting %d\n",
	//     name, flags, i);
	return READOSM_OK;
    }
	// fprintf(stderr, "keeping node %s, flags %d, place %s\n",
	//     name, flags, place);


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
	    s = str2dict (DictionaryCity, name);
	    p = buildmap_place_add(s, layer, point);
	    buildmap_debug("finishing %lld %1.7f %1.7f %s (%d) %s layer: %d",
		node->id,
		node->latitude, node->longitude,
		place, p, name, layer);
	} else {
	    buildmap_debug( "dropping %s %s", place, name);
	}
    }

    return READOSM_OK;
}

/**
 * @brief combine OSM's "name" and "ref" tags into a name we can label with.
 */
const char *
road_name(const char *name, const char *ref)
{
    const char *n;
    char *d;
    const char *s;
    static char compound_name[1024];

    if (ref) {
	n = d = compound_name;
	s = ref;
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
	*d = '\0';

	if (name) {
	    // sprintf(d, ", %s", name);
	    sprintf(d, "%s%s", ENDASHSEP, name);
	}
    } else {
	n = name;
    }
    return n;
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
    const char *name = 0, *ref = 0;
    wayinfo *wp;
    int relation_layer, relation_flags;
    int i;

    nWays++;

    /* if this way was referred to in a relation, fetch what we
     * found then. */
    wp = isWayInteresting(way->id);
    if (wp) {
	relation_layer = wp->relation_layer;
	relation_flags = wp->relation_flags;
	layer = wp->layer;
	flags = wp->flags;
    }

    if (way->node_ref_count < 2) {
	return READOSM_OK;
    }

    /* if we're processing a quadtile, don't include any
     * ways that our neighbors already include */
    if (is_tile && buildmap_osm_text_check_neighbor_way(way->id) && !relation_layer) {
	buildmap_verbose("dropping way %lld because a neighbor "
		    "already has it", way->id);
	return READOSM_OK;
    }

    for (i = 0; i < way->tag_count; i++) {
	tag = way->tags + i;

	if (strcasecmp(tag->key, "name") == 0) {
	    name = tag->value;
	} else if (strcasecmp(tag->key, "ref") == 0) {
	    ref = tag->value;
	} else if (strcasecmp(tag->key, "building") == 0) {
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

	// in some cases we set the way's layer in parse_relation(),
	// so we don't want to override it here.  this also means
	// that otherwise, first tag to set the layer wins.
	if (!layer)
	    buildmap_osm_get_layer(ANY, tag->key, tag->value, &flags, &layer);
    }


    /* only save buildings that are amenities or touristic */
    if (is_building) {
	if (tourism) {
	    buildmap_osm_get_layer(PLACE, "tourism", tourism, &flags, &layer);
	} else if (amenity) {
	    buildmap_osm_get_layer(PLACE, "amenity", amenity, &flags, &layer);
	} else {
	    goto out;
	}
    }

    if (layer == 0) {
	if (is_coast) {
	    layer = l_shoreline;
	} else if (adminlevel) {
	    /* national == 2, state == 4, ignore lesser boundaries */
	    /* also ignore territorial (marine) borders */
	    if  (adminlevel > 4 || is_territorial) {
		    goto out;
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
			goto out;
		}
	    }
#endif

	    layer = l_boundary;
	}
    }

    if (flags & PLACE) {
	if (!tourism && !amenity)
	    flags &= ~PLACE;
	if (way->node_refs[0] == way->node_refs[way->node_ref_count-1])
	    flags |= AREA;
    }
    if (flags & AREA) {
	if (way->node_refs[0] != way->node_refs[way->node_ref_count-1])
	    flags &= ~AREA;
    }


out:
    if (layer || relation_layer) {
	const char *n = road_name(name, ref);

	if (wp) {
	    if (n)
		wp->name = strdup(n);
	    if (!wp->layer) {
		if (layer) {
		    wp->layer = layer;
		    wp->flags = flags;
		} else {
		    wp->layer = relation_layer;
		    wp->flags = 0; // relation_flags;
		}
	    }
	    wp->from = way->node_refs[0];
	    wp->to = way->node_refs[way->node_ref_count-1];
	} else {
	    saveInterestingWay( way->id, way, n ? strdup(n) : 0, layer, flags, 0, 0, 0);
	}
	for (i = 0; i < way->node_ref_count; i++)
	    saveInterestingNode(way->node_refs[i]);
    }

    return READOSM_OK;
}

void adjust_polybox(RoadMapArea *pbox, int lat, int lon)
{
      if (lon < pbox->west) {
         pbox->west = lon;
      } else if (lon > pbox->east) {
         pbox->east = lon;
      }

      if (lat < pbox->south) {
         pbox->south = lat;
      } else if (lat > pbox->north) {
         pbox->north = lat;
      }
}

static void
add_line_shapes(const readosm_way *way, int from, int to, RoadMapArea *pbox)
{
    /* The lonsbuf/latsbuf are never freed, need to be
     * preserved for shape registration which happens
     * at the end of the program run, so exit() will
     * free this for us.
     */
    int     *lonsbuf, *latsbuf;
    int i, j;

    lonsbuf = calloc(to - from + 1, sizeof(way->node_refs[0]));
    latsbuf = calloc(to - from + 1, sizeof(way->node_refs[0]));

    for (i = 0, j = from; j <= to; i++, j++) {

	    int point = buildmap_osm_text_point_get(way->node_refs[j]);
	    int lon = buildmap_point_get_longitude(point);
	    int lat = buildmap_point_get_latitude(point);

	    /* Keep info for the shapes */
	    lonsbuf[i] = lon;
	    latsbuf[i] = lat;

	    if (pbox) adjust_polybox(pbox, lat, lon);

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
    shapes[nShapes].count = to - from + 1;
    shapes[nShapes].lineid = LineId;

    nShapes++;
}

static int
add_line(wayinfo *wp, const readosm_way *way, int rms_name, int layer,
		int polygon, RoadMapArea *pbox)
{
    RoadMapString rms_dirp, rms_dirs, rms_type;
    int from_point, to_point, line, street;
    


    rms_dirp = str2dict(DictionaryPrefix, "");
    rms_dirs = str2dict(DictionarySuffix, "");
    rms_type = str2dict(DictionaryType, "");



    /* Map begin and end points to internal point id */
    from_point = buildmap_osm_text_point_get(way->node_refs[0]);
    to_point = buildmap_osm_text_point_get(way->node_refs[way->node_ref_count-1]);

    if (from_point == to_point && way->node_ref_count > 1) {
	/* the line code really doesn't want circular ways -- i.e.,
	 * lines where from and to are the same.  split those cases
	 * into two lines.
	 */

	int mid_count = way->node_ref_count / 2;
	int mid_point = buildmap_osm_text_point_get(way->node_refs[mid_count]);

	/* first half */
	LineId++;
	line = buildmap_line_add(LineId, layer,
		from_point, mid_point, ROADMAP_LINE_DIRECTION_BOTH);
	street = buildmap_street_add(layer,
			rms_dirp, rms_name, rms_type,
			rms_dirs, line);
	buildmap_range_add_no_address(line, street);

	add_line_shapes(way, 0, mid_count, pbox);

	wp->lineid = LineId;

	if (polygon)
	    buildmap_polygon_add_line (0, PolygonId, LineId, POLYGON_SIDE_RIGHT);

	/* second half */
	LineId++;
	line = buildmap_line_add(LineId, layer,
		mid_point, to_point, ROADMAP_LINE_DIRECTION_BOTH);
	street = buildmap_street_add(layer,
			rms_dirp, rms_name, rms_type,
			rms_dirs, line);
	buildmap_range_add_no_address(line, street);

	add_line_shapes(way, mid_count, way->node_ref_count-1, pbox);

	wp->lineid2 = LineId;

	if (polygon)
	    buildmap_polygon_add_line (0, PolygonId, LineId, POLYGON_SIDE_RIGHT);

    } else {
	LineId++;
	line = buildmap_line_add(LineId,
		layer, from_point, to_point, ROADMAP_LINE_DIRECTION_BOTH);

	street = buildmap_street_add(layer,
			rms_dirp, rms_name, rms_type,
			rms_dirs, line);
	buildmap_range_add_no_address(line, street);

	add_line_shapes(way, 0, way->node_ref_count-1, pbox);

	wp->lineid = LineId;

	if (polygon)
	    buildmap_polygon_add_line (0, PolygonId, LineId, POLYGON_SIDE_RIGHT);
    }
// FIXME  see comment below about lineids from ways that have been
//  split in two.  we should be recording both LineIds.
    return LineId;
}
/**
 * @brief callback for final way parsing, called via readosm_parse()
 */
static int
parse_way_final(const void *is_tile, const readosm_way *way)
{
    wayinfo *wp;
    int j;
    RoadMapString rms_name;

    wp = isWayInteresting(way->id);
    if (!wp)
	return READOSM_OK;

    if (way->node_ref_count == 0)
	return READOSM_OK;

    rms_name = str2dict(DictionaryStreet, wp->name);

    if (wp->flags & PLACE) {
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

	    if (wp->layer) {
		RoadMapString s;
		int p;
		s = str2dict (DictionaryCity, wp->name);
		p = buildmap_place_add(s, wp->layer, point);
		// buildmap_debug( "wayplace: finishing %f %f %s (%d) %s layer: %d",
		buildmap_debug( "wayplace: finishing %f %f (%d) %s layer: %d",
		    (float)lat/1000000.0, (float)lon/1000000.0,
		    // tourism ? tourism : amenity, p, wp->name, wp->layer);
		    p, wp->name, wp->layer);
	    } 

	    if (wp->relation_layer && (wp->relation_flags & AREA)) {
		goto add_as_poly;
	    } else if (wp->relation_layer) {
		goto add_as_way;
	    }

    } else if (wp->flags & AREA) {
	    RoadMapArea *polyarea;
	add_as_poly:
	    LandmarkId++;
	    PolygonId++;

	    buildmap_info("adding %s as polygon %d", wp->name, PolygonId);
	    buildmap_polygon_add_landmark (LandmarkId, wp->layer, rms_name);
	    buildmap_polygon_add(LandmarkId, 0, PolygonId, &polyarea);

	    add_line(wp, way, rms_name, wp->layer, 1, polyarea);
    } else {
	add_as_way:
	    buildmap_debug ("Way %lld [%s]", way->id,
			    wp->name ? wp->name : "");

	    add_line(wp, way, rms_name,
		wp->layer ? wp->layer: wp->relation_layer, 0, NULL);
    }
    return READOSM_OK;
}

static int
parse_relation(const void *is_tile, const readosm_relation * relation)
{
    const readosm_tag *tag;
    const readosm_member *member;
    const char *name = 0;
    int is_multipolygon = 0;
    int layer = 0, flags = 0;
    int is_building = 0;
    int is_water = 0;
    const char *tourism = NULL, *amenity = NULL;
    int i;

    nRels++;

    if (relation->member_count == 0)
	return READOSM_OK;

    /* if we're processing a quadtile, don't include any
     * relations that our neighbors already include */
    if (is_tile && buildmap_osm_text_check_neighbor_relation(relation->id)) {
	buildmap_verbose("dropping relation %lld because a neighbor "
		    "already has it", relation->id);
	return READOSM_OK;
    }


    for (i = 0; i < relation->tag_count; i++)
    {
	tag = relation->tags + i;

	if (strcasecmp(tag->key, "type") == 0) {
	    if (strcasecmp(tag->value, "multipolygon") == 0) {
		is_multipolygon = 1;
	    }
	} else if (strcasecmp(tag->key, "name") == 0) {
	    name = tag->value;
	} else if (strcasecmp(tag->key, "building") == 0) {
	    is_building = 1;
	} else if (strcasecmp(tag->key, "tourism") == 0) {
	    tourism = tag->value;
	} else if (strcasecmp(tag->key, "amenity") == 0) {
	    amenity = tag->value;
	} else if (strcasecmp(tag->key, "natural") == 0 &&
		    strcasecmp(tag->value, "water") == 0) {
	    is_water = 1;
        }
	
	buildmap_osm_get_layer(AREA, tag->key, tag->value, &flags, &layer);
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


    if (is_multipolygon && layer) {
	for (i = 0; i < relation->member_count; i++)
	{
	    int innerlayer = 0;
	    int innerflags = 0;

	    member = relation->members + i;
	    switch(member->member_type) {
	    case READOSM_MEMBER_WAY:
		if (is_water) {
		    if (strcmp(member->role, "inner") == 0) {
			innerlayer = l_island;
		    } else if (strcmp(member->role, "outer") == 0
			    || strcmp(member->role, "") == 0) {
			innerlayer = l_lake;
		    }
		    innerflags = AREA;
		}
		if (!isWayInteresting(member->id)) {
		    saveInterestingWay(member->id, 0, 0, innerlayer,
		    	innerflags, layer, flags, relation->id);
		    qsort(WayTable, nWayTable, sizeof(*WayTable),
				qsort_compare_osm_ids);
    		    nSearchableWays = nWayTable;
		}
		break;

	    case READOSM_MEMBER_NODE:
	    case READOSM_MEMBER_RELATION:
	    default:
		break;
	    }
	}
	saveInterestingRelation(relation->id, name ?  strdup(name) : 0,
			layer, flags);
    }

    return READOSM_OK;
}

static void add_multipolygon(wayinfo **wayinfos, int layer,
		int rms_name, int count, char *name)
{
    wayinfo *wp, *wp2;
    nodeid_t from = 0, to = 0;
    int ring = 0;
    int i, j;
    // RoadMapArea *polyarea;

    for (i = 0; i < count; i++) {
	wp = wayinfos[i];
	if (wp->ring) continue;
	wp->ring = ++ring;

	PolygonId++;
    buildmap_info("add_multi: adding %s as polygon %d", name, PolygonId);
	buildmap_polygon_add(LandmarkId, 0, PolygonId, 0); // &polyarea);
	buildmap_polygon_add_line (0, PolygonId, wp->lineid, POLYGON_SIDE_RIGHT);
	if (wp->lineid2)
	    buildmap_polygon_add_line (0, PolygonId, wp->lineid2, POLYGON_SIDE_RIGHT);

	from = wp->from;
	to = wp->to;
	if (to == from)
	    continue;

	for (j = i; j < count; ) {
	    wp2 = wayinfos[j];
	    if (wp2->ring) {
		j++;
		continue;
	    }

	    if (wp2->from == to) {
		wp2->ring = ring;
// FIXME it's possible that a way may have more than one lineid, as
// a result of being a circular way that's been split in two in add_line().
// we should record both and add both here.
		buildmap_polygon_add_line (0, PolygonId, wp2->lineid,
			POLYGON_SIDE_RIGHT);
		if (wp2->lineid2)
		    buildmap_polygon_add_line (0, PolygonId, wp2->lineid2,
			    POLYGON_SIDE_RIGHT);
		to = wp2->to;
		j = i;
	    } else if (wp2->to == to) {
		wp2->ring = ring;
		buildmap_polygon_add_line (0, PolygonId, wp2->lineid,
			POLYGON_SIDE_LEFT);
		if (wp2->lineid2)
		    buildmap_polygon_add_line (0, PolygonId, wp2->lineid2,
			    POLYGON_SIDE_LEFT);
		to = wp2->from;
		j = i;
	    } else {
		/* no match */
		j++;
		continue;
	    }
	    if (to == from) { /* done with this ring */
		break;
	    }
	}
    }
    /* clear the ring indicators, in case these ways are part
     * of another multipolygon, later. */
    for (i = 0; i < count; i++) {
	wp = wayinfos[i];
	wp->ring = 0;
    }
}

static int
parse_relation_final(const void *user_data, const readosm_relation * relation)
{
    relinfo *rp;
    wayinfo *wp, **wayinfos, **innerwayinfos;
    const readosm_member *member;
    int i, wc, iwc;
    RoadMapString rms_name;

    rp = isRelationInteresting(relation->id);
    if (!rp)
	return READOSM_OK;

    buildmap_info("warning: relation %s is interesting", rp->name);

    // note: assumes this is a multipolygon relation

    LandmarkId++;

    rms_name = str2dict(DictionaryStreet, rp->name);

    wayinfos = calloc(relation->member_count, sizeof(*wayinfos));
    buildmap_check_allocated(wayinfos);
    innerwayinfos = calloc(relation->member_count, sizeof(*wayinfos));
    buildmap_check_allocated(innerwayinfos);

    wc = iwc = 0;
    for (i = 0; i < relation->member_count; i++)
    {
	member = relation->members + i;
	switch(member->member_type) {
	case READOSM_MEMBER_WAY:
	    wp = isWayInteresting(member->id);
	    if (!wp->lineid) buildmap_fatal(0, "found null lineid");
	    if (strcmp(member->role, "inner") == 0) {
		innerwayinfos[iwc++] = wp;
	    } else {
		wayinfos[wc++] = wp;
	    }
	    break;

	case READOSM_MEMBER_NODE:
	case READOSM_MEMBER_RELATION:
	default:
	    break;
	}
    }

    buildmap_polygon_add_landmark (LandmarkId, rp->layer, rms_name);

    add_multipolygon(wayinfos, rp->layer, rms_name, wc, rp->name);
    // FIXME:  shouldn't _always_ be l_island
    add_multipolygon(innerwayinfos, l_island, rms_name, iwc, rp->name);

    free(wayinfos);
    free(innerwayinfos);
    return READOSM_OK;
}

static int text_file_is_pipe;
FILE *buildmap_osm_text_fopen(char *fn)
{
    FILE *fp;
    int len;

    len = strlen(fn);
    errno = 0;
    if (len > 3 && strcmp(&fn[len-3], ".gz") == 0) {
	char command[1024];
	sprintf(command, "gzip -d -c %s", fn);
	fp = popen(command, "r");
	text_file_is_pipe = 1;
    } else {
	fp = fopen(fn, "r");
	text_file_is_pipe = 0;
    }

    if (fp == NULL) {
            buildmap_fatal(0, "couldn't open \"%s\", %s", fn, strerror(errno));
            return NULL;
    }

    return fp;
}

int buildmap_osm_text_fclose(FILE *fp)
{
    if (text_file_is_pipe)
	return pclose(fp);
    else
	return fclose(fp);
}
/**
 * @brief rather than trying to put an entire file's contents in memory,
 *	we parse it in two passes.  this routine is called for each pass.
 */
void buildmap_readosm_pass(int pass, char *fn, int tileid)
{
    int ret;
    const void *handle;
    void *is_tile = 0;
    FILE *fp;

    if (tileid) is_tile = (void *)1;

    buildmap_info("Starting pass %d", pass);

    fp = buildmap_osm_text_fopen(fn);
    ret = readosm_fopen(fp, READOSM_OSM_FORMAT, &handle);
    if (ret != READOSM_OK) {
	buildmap_fatal(0, "buildmap_osm_text: couldn't open \"%s\", %s",
		fn, readosm_errors[-ret]);
	return;
    }

    switch (pass) {
    case 1: ret = readosm_parse(handle, is_tile,
    		NULL, NULL, parse_relation);
	    break;
    case 2: ret = readosm_parse(handle, is_tile,
    		NULL, parse_way, NULL);
	    break;
    case 3: ret = readosm_parse(handle, is_tile,
    		parse_node_final, parse_way_final, parse_relation_final);
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
    if (buildmap_osm_text_fclose(fp)) {
	buildmap_fatal(0, "buildmap_osm_text pass %d: %s",
		pass, strerror(errno));
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
    l_lake = buildmap_layer_get("lakes");;
    l_island = buildmap_layer_get("islands");;

    nRels = 0;
    nWays = 0;
    nNodes = 0;
    nShapes = 0;
    nWayTable = 0;
    nSearchableWays = 0;
    nNodeTable = 0;
    PolygonId = 0;
    LineId = 0;

    /* pass 1:
     *  record entire interesting relations, including layer and name, and
     *  record the ways and nodes they refer to.
     */
    buildmap_readosm_pass(1, fn, tileid);

    qsort(RelTable, nRelTable, sizeof(*RelTable), qsort_compare_osm_ids);
    qsort(WayTable, nWayTable, sizeof(*WayTable), qsort_compare_osm_ids);
    nSearchableWays = nWayTable;

    /* pass 2:
     *  record interesting ways, including layer and and name.  if there's
     * FIXME:  how is naming done?  bbox of entire relation?  assigned
     * to biggest member?  at what point do i have all the information
     * present to calculate this?
     */
    buildmap_readosm_pass(2, fn, tileid);

    /* we've added to the WayTable, so need to sort again, and bump
     * the number of searchable ways.
     */
    qsort(WayTable, nWayTable, sizeof(*WayTable), qsort_compare_osm_ids);
    nSearchableWays = nWayTable;

    qsort(NodeTable, nNodeTable, sizeof(*NodeTable), qsort_compare_unsigneds);

    /* pass 3:
     *  save interesting nodes, either previously recorded,
     *      or which are interesting by themselves.
     *  save recorded ways
     *  save recorded relations.  assign relation's layer and/or name to
     *      some or all of the related ways.
     */
    buildmap_readosm_pass(3, fn, tileid);

    buildmap_osm_text_ways_shapeinfo();

    buildmap_info("Relations %d", nRels);
    buildmap_info("Ways %d, interesting %d", nWays, nWayTable);
    buildmap_info("Number of nodes : %d, interesting %d", nNodes, nNodeTable);
    buildmap_info("Number of points: %d", nPoints);
}
