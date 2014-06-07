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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_math.h"
#include "roadmap_path.h"
#include "roadmap_file.h"
#include "roadmap_osm.h"
#include "roadmap_line.h"
#include "roadmap_hash.h"

#include "buildmap.h"
#include "buildmap_zip.h"
#include "buildmap_city.h"
#include "buildmap_square.h"
#include "buildmap_point.h"
#include "buildmap_line.h"
#include "buildmap_street.h"
#include "buildmap_range.h"
#include "buildmap_area.h"
#include "buildmap_shape.h"
#include "buildmap_polygon.h"

#include "buildmap_layer.h"
#include "buildmap_osm_common.h"
#include "buildmap_osm_text.h"


/**
 * @brief a couple of variables to keep track of the way we're dealing with
 */
static struct WayInfo {
    int      inWay;              /**< are we in a way (its id) */
    int      nWayNodes;          /**< number of nodes known for 
						     this way */
    int      WayLayer;           /**< the layer for this way */
    char     *WayStreetName;     /**< the street name */
    char     *WayStreetRef;	 /**< street code,
				       to be used when no name (e.g. motorway) */
    int      WayFlags;           /**< properties of this way, from
						     the table flags */
    int      WayInvalid;         /**< this way contains invalid nodes */
    int      WayIsOneWay;        /**< is this way one direction only */
    int      WayAdminLevel;	 /**< boundaries */
    int      WayCoast;           /**< coastline */
    int      WayNotInteresting;  /**< this way is not interesting for RoadMap */
} wi;

/**
 * @brief variables referring to the current node
 */
static struct NodeInfo {
    int      NodeId;             /**< which node */
    char     *NodePlace;         /**< what kind of place is this */
    char     *NodeTownName;      /**< which town */
    char     *NodePostalCode;    /**< postal code */
    int      NodeLon,            /**< coordinates */
	     NodeLat;            /**< coordinates */
    int      NodeFakeFips;       /**< fake postal code */
} ni;

/**
 * @brief some global variables
 */
static int      LineNo;                 /**< line number in the input file */
static int      nPolygons = 0;          /**< current polygon id (number of
                                                        polygons until now) */
static int      LineId = 0;             /**< for buildmap_line_add */

/**
 * @brief table for translating the names in XML strings into readable format
 */
static struct XmlIsms {
        char    *code;          /**< the piece between & and ; */
        char    *string;        /**< translates into this */
} XmlIsms[] = {
        { "apos",       "'" },
        { "gt",         ">" },
        { "lt",         "<" },
        { "quot",       """" },
        { "amp",        "&" },
        { NULL,         NULL }  /* end */
};

/**
 * @brief remove XMLisms such as &apos; and strdup
 * limitation : names must be shorter than 256 bytes
 * @param s input string
 * @return duplicate, to be freed elsewhere
 */
static char *FromXmlAndDup(const char *s)
{
        int             i, j, k, l, found;
        static char     res[256];

        for (i=j=0; s[i]; i++)
                if (s[i] == '&') {
                        found = 0;
                        for (k=0; XmlIsms[k].code && !found; k++) {
                                for (l=0; s[l+i+1] == XmlIsms[k].code[l]; l++)
                                        ;
                                /* When not equal, must be at end of code */
                                if (XmlIsms[k].code[l] == '\0' &&
                                                s[l+i+1] == ';') {
                                        found = 1;
                                        i += l+1;
                                        for (l=0; XmlIsms[k].string[l]; l++)
                                                res[j++] = XmlIsms[k].string[l];
                                }
                        }
                        if (!found)
                                res[j++] = s[i];
                } else {
                        res[j++] = s[i];
                }
        res[j] = '\0';
        return strdup(res);
}

/**
 * @brief reset all the info about this way
 */
static void
buildmap_osm_text_reset_way(void)
{
	if (wi.WayStreetName) free(wi.WayStreetName); 
	if (wi.WayStreetRef) free(wi.WayStreetRef);
	memset(&wi, 0, sizeof(wi));
}

/**
 * @brief reset all the info about this node
 */
static void
buildmap_osm_text_reset_node(void)
{
//	buildmap_info("reset node %d", ni.NodeId);

	if (ni.NodePlace) free(ni.NodePlace);
	if (ni.NodeTownName) free(ni.NodeTownName);
	if (ni.NodePostalCode) free(ni.NodePostalCode);
	memset(&ni, 0, sizeof(ni));
}

/**
 * @brief
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

/**
 * @brief simplistic way to gather point data
 */
static int nPointsAlloc = 0;
static int nPoints = 0;
static struct points {
        int     id;
        int     npoint;
} *points = 0;

#define	NPOINTSINC	10000

RoadMapHash	*PointsHash = NULL;

static void
buildmap_osm_text_point_hash_reset(void)
{
    nPoints = 0;
    PointsHash = 0;
    if (points) free(points);
    points = 0;
    nPointsAlloc = 0;
}

static void
buildmap_osm_text_point_add(int id, int npoint)
{
        if (nPoints == nPointsAlloc) {
                nPointsAlloc += NPOINTSINC;

		if (PointsHash == NULL)
			PointsHash = roadmap_hash_new("PointsHash", nPointsAlloc);
		else
			roadmap_hash_resize(PointsHash, nPointsAlloc);

                points = realloc(points, sizeof(struct points) * nPointsAlloc);
                if (!points)
                        buildmap_fatal(0, "allocate %d points", nPointsAlloc);
        }

	roadmap_hash_add(PointsHash, id, nPoints);

        points[nPoints].id = id;
        points[nPoints++].npoint = npoint;
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
                if (points[i].id == id)
                        return points[i].npoint;
        return -1;
}

/**
 * @brief collect node data in pass 1
 * @param data
 * @return an error indicator
 *
 * The input line is discarded if a bounding box is specified and
 * the node is outside.
 *
 * Example input line :
 *   <node id="123295" timestamp="2005-07-05T03:26:11Z" user="LA2"
 *      lat="50.4443626" lon="3.6855288"/>
 */
int
buildmap_osm_text_node(char *data)
{
    int         nchars, r;
#if 0
    double      flat, flon;
#endif
    char        *p;
    int         NodeLatRead, NodeLonRead;
    char	tag[512], value[512];

    sscanf(data, "node id=%*[\"']%d%*[\"']%n", &ni.NodeId, &nchars);
    p = data + nchars;

    ni.NodeLat = ni.NodeLon = 0;
    NodeLatRead = NodeLonRead = 0;

    while (NodeLatRead == 0 || NodeLonRead == 0) {
            for (; *p && isspace(*p); p++) ;
            r = sscanf(p, "%[a-zA-Z0-9_]=%*[\"']%[^\"']%*[\"']%n",
                        tag, value, &nchars);

	    if (r != 2)
		buildmap_error(0, "bad tag read at '%s'\n", p);

#if 0
            if (strcmp(tag, "lat") == 0) {
                    sscanf(value, "%lf", &flat);
                    ni.NodeLat = flat * 1000000;
                    NodeLatRead++;
            } else if (strcmp(tag, "lon") == 0) {
                    sscanf(value, "%lf", &flon);
                    ni.NodeLon = flon * 1000000;
                    NodeLonRead++;
            }
#else
            if (strcmp(tag, "lat") == 0) {
                    ni.NodeLat = roadmap_math_from_floatstring(value, MILLIONTHS);
                    NodeLatRead++;
            } else if (strcmp(tag, "lon") == 0) {
                    ni.NodeLon = roadmap_math_from_floatstring(value, MILLIONTHS);
                    NodeLonRead++;
            }
#endif

            p += nchars;
    }

    return 0;
}

/**
 * @brief At the end of a node, process its data
 * @param data point to the line buffer
 * @return error indication
 */
int
buildmap_osm_text_node_end_and_process(char *data)
{
	int npoints;

	if (ni.NodeFakeFips) {
	    if (ni.NodePlace && (strcmp(ni.NodePlace, "town") == 0
			|| strcmp(ni.NodePlace, "village") == 0
			|| strcmp(ni.NodePlace, "city") == 0)) {
                /* We have a town, process it */

                if (ni.NodeTownName) {
                        ni.NodeFakeFips++;
                        int year = 2008;
                        RoadMapString s;

                        s = buildmap_dictionary_add (DictionaryCity,
                                (char *) ni.NodeTownName, strlen(ni.NodeTownName));
                        buildmap_city_add(ni.NodeFakeFips, year, s);
                }
                if (ni.NodePostalCode) {
                        int zip = 0;
                        sscanf(ni.NodePostalCode, "%d", &zip);
                        if (zip)
                                buildmap_zip_add(zip, ni.NodeLon, ni.NodeLat);
                }
	    }
        }

	/* Add the node */
	npoints = buildmap_point_add(ni.NodeLon, ni.NodeLat);
	buildmap_osm_text_point_add(ni.NodeId, npoints);

        buildmap_osm_text_reset_node();
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

static int numshapes = 0;
static int nallocshapes = 0;

/**
 * @brief ?
 */
static struct shapeinfo *shapes;

/**
 * @brief
 * @param data
 * @return
 *
 * Sample XML :
 *   <way id="75146" timestamp="2006-04-28T15:24:05Z" user="Mercator">
 *     <nd ref="997466"/>
 *     <nd ref="997470"/>
 *     <nd ref="1536769"/>
 *     <nd ref="997472"/>
 *     <nd ref="1536770"/>
 *     <nd ref="997469"/>
 *     <tag k="highway" v="residential"/>
 *     <tag k="name" v="Rue de Thiribut"/>
 *     <tag k="created_by" v="JOSM"/>
 *   </way>
 */
static int
buildmap_osm_text_way(char *data)
{
        /* Severely cut in pieces.
         * This only remembers which way we're in...
         */
        sscanf(data, "way id=%*[\"']%d%*[\"']", &wi.inWay);
        wi.WayNotInteresting = 0;

        if (wi.inWay == 0)
                buildmap_fatal(0, "buildmap_osm_text_way(%s) error", data);

        return 0;
}

static int maxWayTable = 0;
int nWayTable = 0;
int *WayTable = NULL;

static void
WayIsInteresting(int wayid)
{

	if (nWayTable == maxWayTable) {
		if (WayTable)
		    maxWayTable *= 2;
		else
		    maxWayTable = 1000;
		WayTable = (int *) realloc(WayTable,
				sizeof(int) * maxWayTable);
	}

	WayTable[nWayTable] = wayid;
	nWayTable++;
}

int
qsort_compare_ints(const void *id1, const void *id2)
{
    return *(int *)id1 - *(int *)id2;
}

/**
 * @brief find out if this way is interesting
 * @param wayid
 * @return whether we thought way was interesting when we found it
 *
 * Note : relies on the order of ways encountered in the file, for performance
 */
static int
IsWayInteresting(int wayid)
{
	int *r;
	r = bsearch(&wayid, WayTable, nWayTable,
                             sizeof(*WayTable), qsort_compare_ints);
	if (!r) return 0;

	return *r;
}

static int maxNodeTable = 0;
static int nNodeTable = 0;
static int *NodeTable = NULL;


void
buildmap_osm_text_save_wayids(const char *path, const char *outfile)
{
    char nfn[1024];
    char *p;

    strcpy(nfn, outfile);
    p = strrchr(nfn, '.');
    if (p) *p = '\0';
    strcat(nfn, ".ways");


    roadmap_file_save(path, nfn, WayTable, nWayTable * sizeof(int));
}


static void
NodeIsInteresting(int node)
{

	if (nNodeTable == maxNodeTable) {
		if (NodeTable)
		    maxNodeTable *= 2;
		else
		    maxNodeTable = 1000;
		NodeTable = (int *) realloc(NodeTable,
				sizeof(int) * maxNodeTable);
	}

	NodeTable[nNodeTable] = node;
	nNodeTable++;
}

static int
IsNodeInteresting(int nodeid)
{
	int *r;

        r = bsearch(&nodeid, NodeTable, nNodeTable,
                             sizeof(*NodeTable), qsort_compare_ints);
	if (!r) return 0;

	return *r;
}

static int
buildmap_osm_text_node_interesting(char *data)
{
	/*
	 * Avoid figuring out whether we're in a
	 *	<node ... />
	 * or
	 *	<node ... >
	 *	..
	 *	</node>
	 * case, by resetting first if needed.
	 */
	if (ni.NodeId && IsNodeInteresting(ni.NodeId))
		buildmap_osm_text_node_end_and_process("");

        if (sscanf(data, "node id=%*[\"']%d%*[\"']", &ni.NodeId) != 1) {
                return -1;
	}

	buildmap_osm_text_node(data);

	return 0;
}

/**
 * @build this is called on every node, but should figure out whether
 *	it is interesting
 */
static int
buildmap_osm_text_node_interesting_end(char *data)
{
	if (IsNodeInteresting(ni.NodeId))
		buildmap_osm_text_node_end_and_process(data);
	return 0;
}


/**
 * @brief
 * @param data points into the line of text being processed
 * @return error indication
 *
 * Example line :
 *     <nd ref="997470"/>
 */

int      nWayNodeAlloc; /**< size of the allocation of the array */
int      *WayNodes;     /**< the array to keep track of this way's nodes */

static int
buildmap_osm_text_nd(char *data)
{
        int     node, ix;

        if (! wi.inWay)
                buildmap_fatal(0, "Wasn't in a way (%s)", data);

        if (sscanf(data, "nd ref=%*[\"']%d%*[\"']", &node) != 1)
                return -1;

        ix = buildmap_osm_text_point_get(node);
        if (ix < 0) {
                return 0;
        }

        if (wi.nWayNodes == nWayNodeAlloc) {
		if (WayNodes)
                    nWayNodeAlloc *= 2;
		else
                    nWayNodeAlloc = 1000;
                WayNodes = 
                    (int *)realloc(WayNodes, sizeof(int) * nWayNodeAlloc);
                if (WayNodes == 0)
                        buildmap_fatal
                            (0, "allocation failed for %d ints", nWayNodeAlloc);
        }
        WayNodes[wi.nWayNodes++] = node;

        return 0;
}

/**
 * @brief deal with tag lines outside of ways
 * @param data points into the line of text being processed
 * @return error indication
 */
static int
buildmap_osm_text_node_tag(char *data, int catalog)
{
        static char *tagk = 0;
        static char *tagv = 0;

        if (! tagk)
                tagk = malloc(512);
        if (! tagv)
                tagv = malloc(512);

        sscanf(data, "tag k=%*['\"]%[^\"']%*['\"] v=%*['\"]%[^\"']%*['\"]",
                        tagk, tagv);

        if (strcmp(tagk, "postal_code") == 0) {
                /* <tag k="postal_code" v="3020"/> */
                if (ni.NodePostalCode)
                        free(ni.NodePostalCode);
                ni.NodePostalCode = strdup(tagv);
		if (catalog) NodeIsInteresting(ni.NodeId);
        } else if (strcmp(tagk, "place") == 0) {
                /* <tag k="place" v="town"/> */
                if (ni.NodePlace)
                        free(ni.NodePlace);
                ni.NodePlace = strdup(tagv);
		if (catalog) NodeIsInteresting(ni.NodeId);
        } else if (strcmp(tagk, "name") == 0) {
                /* <tag k="name" v="Herent"/> */
                if (ni.NodeTownName)
                        free(ni.NodeTownName);
                ni.NodeTownName = FromXmlAndDup(tagv);
		if (catalog) NodeIsInteresting(ni.NodeId);
        }

        return 0;
}

/**
 * @brief deal with tag lines inside a <way> </way> pair
 * @param data points into the line of text being processed
 * @return error indication
 *
 * Example line :
 *     <tag k="highway" v="residential"/>
 *     <tag k="name" v="Rue de Thiribut"/>
 *     <tag k="created_by" v="JOSM"/>
 *     <tag k="ref" v="E40">
 *     <tag k="int_ref" v="E 40">
 */
static int
buildmap_osm_text_way_tag(char *data)
{
	static char	*tag = 0, *value = 0;
	int		i, found;
	layer_info_t	*list;
	int		ret = 0;

	if (! tag) tag = malloc(512);
	if (! value) value = malloc(512);

	sscanf(data, "tag k=%*[\"']%[^\"']%*[\"'] v=%*[\"']%[^\"']%*[\"']", tag, value);

	/* street names */
	if (strcasecmp(tag, "name") == 0) {
		if (wi.WayStreetName)
			free(wi.WayStreetName);
		wi.WayStreetName = FromXmlAndDup(value);
		return 0;	/* FIX ME ?? */
	} else if (strcasecmp(tag, "landuse") == 0) {
		wi.WayNotInteresting = 1;
	} else if (strcasecmp(tag, "oneway") == 0 && strcasecmp(value, "yes") == 0) {
		wi.WayIsOneWay = ROADMAP_LINE_DIRECTION_ONEWAY;
	} else if (strcasecmp(tag, "building") == 0) {
		if (strcasecmp(value, "yes") == 0) {
			wi.WayNotInteresting = 1;
		}
	} else if (strcasecmp(tag, "ref") == 0) {
		if (wi.WayStreetRef)
			free(wi.WayStreetRef);
		wi.WayStreetRef = FromXmlAndDup(value);
		return 0;	/* FIX ME ?? */
	} else if (strcasecmp(tag, "admin_level") == 0) {
		wi.WayAdminLevel = atoi(value);
	} else if (strcasecmp(tag, "natural") == 0 && strcasecmp(value, "coastline") == 0) {
		wi.WayCoast = 1;
	}

	/* Scan list_info
	 *
	 * This will map tags such as highway and cycleway.
	 */
	found = 0;
	for (i=1; found == 0 && list_info[i].name != 0; i++) {
		if (strcmp(tag, list_info[i].name) == 0) {
			list = list_info[i].list;
			found = 1;
			break;
		}
	}

	if (found) {
		if (list) {
			for (i=1; list[i].name; i++) {
				if (strcmp(value, list[i].name) == 0) {
					wi.WayFlags = list[i].flags;
					if (list[i].layerp)
						ret = *(list[i].layerp);
				}
			}
		} else {
			/* */
		}
	}

	/* FIX ME When are we supposed to do this */
	if (ret)
		wi.WayLayer = ret;

	return 0;
}

/**
 * @brief We found an end tag for a way, so we must have read all
 *  the required data.  Process it.
 * @param data points to the line of text being processed
 * @return error indication
 */
static int
buildmap_osm_text_way_end(char *data)
{
        int             from_point, to_point, line, street;
        int             fromlon, tolon, fromlat, tolat;
        int             j;
	static int	l_shoreline = 0,
			l_boundary = 0;
       
	if (l_shoreline == 0)
		l_shoreline = buildmap_layer_get("shore");;
	if (l_boundary == 0)
		l_boundary = buildmap_layer_get("boundaries");;

        if (wi.WayInvalid) { // REMOVEME
                buildmap_osm_text_reset_way();
                return 0;
	}

        if (wi.inWay == 0)
                buildmap_fatal(0, "Wasn't in a way (%s)", data);

	/* if a way is both a coast and a boundary, treat it only as coast */
	if (wi.WayCoast) {
		wi.WayNotInteresting = 0;
		wi.WayLayer = l_shoreline;
	} else if (wi.WayAdminLevel) {

		/* national == 2, state == 4, ignore lesser boundaries */
		if  (wi.WayAdminLevel > 4) {
			wi.WayNotInteresting = 1;
		}

		wi.WayLayer = l_boundary;
	}

        if (wi.WayNotInteresting || wi.WayLayer == 0) {
                buildmap_verbose("discarding way %d, not interesting (%s)", wi.inWay, data);

                wi.WayNotInteresting = 0;
                buildmap_osm_text_reset_way();
                return 0;
        }

	if (wi.nWayNodes < 1) {
                buildmap_osm_text_reset_way();
		wi.WayNotInteresting = 1;
                return 0;
	}

        RoadMapString rms_dirp = str2dict(DictionaryPrefix, "");
        RoadMapString rms_dirs = str2dict(DictionarySuffix, "");
        RoadMapString rms_type = str2dict(DictionaryType, "");
        RoadMapString rms_name = 0;

        from_point = buildmap_osm_text_point_get(WayNodes[0]);
        to_point = buildmap_osm_text_point_get(WayNodes[wi.nWayNodes-1]);

        fromlon = buildmap_point_get_longitude(from_point);
        fromlat = buildmap_point_get_latitude(from_point);
        tolon = buildmap_point_get_longitude(to_point);
        tolat = buildmap_point_get_latitude(to_point);

        if ((wi.WayFlags & AREA)  && (fromlon == tolon) && (fromlat == tolat)) {
                static int polyid = 0;
                static int cenid = 0;

                /*
                 * Detect an AREA -> create a polygon
                 */
                nPolygons++;
                cenid++;
                polyid++;

                rms_name = str2dict(DictionaryStreet, wi.WayStreetName);
                buildmap_polygon_add_landmark (nPolygons, wi.WayLayer, rms_name);
                buildmap_polygon_add(nPolygons, cenid, polyid);

                for (j=1; j<wi.nWayNodes; j++) {
                        int prevpoint =
                            buildmap_osm_text_point_get(WayNodes[j-1]);
                        int point =
                            buildmap_osm_text_point_get(WayNodes[j]);

                        LineId++;
                        buildmap_line_add
                                (LineId, wi.WayLayer, prevpoint, point,
				 ROADMAP_LINE_DIRECTION_BOTH);
			buildmap_polygon_add_line (cenid, polyid, LineId, POLYGON_SIDE_RIGHT);
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

		/* Map begin and end points to internal point id */
		// from_point = buildmap_osm_text_point_get(WayNodes[0]);
		// to_point = buildmap_osm_text_point_get(WayNodes[wi.nWayNodes-1]);

		/* Street name */
		if (wi.WayStreetName)
			rms_name = str2dict(DictionaryStreet, wi.WayStreetName);
		else if (wi.WayStreetRef)
			rms_name = str2dict(DictionaryStreet, wi.WayStreetRef);
		buildmap_verbose ("Way %d [%s] ref [%s]", wi.inWay,
				wi.WayStreetName ? wi.WayStreetName : "",
				wi.WayStreetRef ? wi.WayStreetRef : "");

		LineId++;
		line = buildmap_line_add(LineId,
			wi.WayLayer, from_point, to_point, wi.WayIsOneWay);

		street = buildmap_street_add(wi.WayLayer,
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

		lonsbuf = calloc(wi.nWayNodes, sizeof(int));
		latsbuf = calloc(wi.nWayNodes, sizeof(int));

		for (j=0; j<wi.nWayNodes; j++) {
			int point =
			    buildmap_osm_text_point_get(WayNodes[j]);
			int lon = buildmap_point_get_longitude(point);
			int lat = buildmap_point_get_latitude(point);

			/* Keep info for the shapes */
			lonsbuf[j] = lon;
			latsbuf[j] = lat;

			buildmap_square_adjust_limits(lon, lat);
		}

		if (numshapes == nallocshapes) {
			/* Allocate additional space (in big
			 * chunks) when needed */
			if (shapes)
			    nallocshapes *= 2;
			else
			    nallocshapes = 1000;
			shapes = realloc(shapes,
			    nallocshapes * sizeof(struct shapeinfo));
			buildmap_check_allocated(shapes);
		}

		buildmap_verbose("lineid %d wi.nWayNodes %d\n",
			LineId, wi.nWayNodes);
		/* Keep info for the shapes */
		shapes[numshapes].lons = lonsbuf;
		shapes[numshapes].lats = latsbuf;
		shapes[numshapes].count = wi.nWayNodes;
		shapes[numshapes].lineid = LineId;

		numshapes++;
	}

        buildmap_osm_text_reset_way();

        return 0;
}

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

    buildmap_info("loading shape info (from %d ways) ...", numshapes);

    buildmap_line_sort();

    for (i = 0; i < numshapes; i++) {

        count = shapes[i].count;

        if (count <= 2)
            continue;

	buildmap_verbose("trying line %d, %d points", lineid, count);
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

/**
 * @brief This is the gut of buildmap_osm_text : parse an OSM XML file
 * @param fdata an open file pointer, this will get read twice
 * @param country_num the country id that we're working for
 * @param division_num the country subdivision id that we're working for
 * @return error indication
 *
 * This is a simplistic approach to parsing the OSM text (XML) files.
 * It scans the XML twice, to cope with out of order information.
 * (Note that this simplistic approach may raise eyebrows, but this is
 * not a big time consumer !)
 *
 * Pass 1 deals with way definitions -- make a list of interesting ways.
 * Pass 2 catalogs interesting nodes, based on references from ways.
 * Pass 3 interprets ways and a few tags.
 *
 * All underlying processing is passed to other functions.
 */
int
buildmap_osm_text_read(FILE * fdata, int country_num, int division_num)
{
    char	*got;
    static char	buf[LINELEN];
    int		ret = 0;
    char	*p;
    time_t	t[10];
    int		passid, NumNodes, NumWays;
    struct stat st;
    int		interesting_way;
    int		in_relation;

    fstat(fileno(fdata), &st);

    ni.NodeFakeFips = 1000000 + country_num * 1000 + division_num;

    buildmap_osm_text_point_hash_reset();

    (void) time(&t[0]);
    passid = 1;

    DictionaryPrefix = buildmap_dictionary_open("prefix");
    DictionaryStreet = buildmap_dictionary_open("street");
    DictionaryType = buildmap_dictionary_open("type");
    DictionarySuffix = buildmap_dictionary_open("suffix");
    DictionaryCity = buildmap_dictionary_open("city");

    buildmap_osm_common_find_layers ();

    /*
     * Pass 1 - just figure out which ways are interesting
     * Currently this is *all ways* but this may change.
     */
    buildmap_info("Starting pass %d", passid);
    buildmap_set_source("pass 1");
    LineNo = 0;
    NumWays = 0;

    while (! feof(fdata)) {
	buildmap_progress(ftell(fdata), st.st_size);
        buildmap_set_line(++LineNo);
        got = fgets(buf, LINELEN, fdata);
        if (got == NULL) {
            if (feof(fdata))
                break;
            buildmap_fatal(0, "short read (length)");
        }

        /* Figure out the XML */
        for (p=buf; *p && isspace(*p); p++) ;
        if (*p == '\n' || *p == '\r') 
                continue;
        if (*p != '<') {
		/*
		 * Assume we're in a continuation line such as
		 *
		 * <tag k='opening_hours' v='Mo 09:30-19:00;
		 * Tu 09:30-17:00;
		 * We 09:30-17:00;
		 * Th 09:30-19:00;
		 * Fr 09:30-17:00;
		 * Sa 09:30-16:00;
		 * Su 10:00-14:00'/>
		 *
		 * and just continue with the next line and hope we'll pick up
		 * a new tag soon.
		 */
		continue;
	}

        p++; /* point to character after '<' now */
        for (; *p && isspace(*p); p++) ;

	in_relation = 0;
        if (strncasecmp(p, "relation", 8) == 0) {
		in_relation = 1;
        } else if (strncasecmp(p, "/relation", 9) == 0) {
		in_relation = 0;
        } else if (in_relation) {
		continue;
        } else if (strncasecmp(p, "way", 3) == 0) {
		wi.WayNotInteresting = 0;
		if (sscanf(p, "way id=%*[\"']%d%*[\"']", &wi.inWay) != 1) {
		    wi.inWay = 0;
		}
		NumWays++;
                continue;
        } else if (strncasecmp(p, "/way", 4) == 0) {
		if (wi.inWay && ! wi.WayNotInteresting)
		    WayIsInteresting(wi.inWay);
		buildmap_osm_text_reset_way();
                continue;
        } else if (strncasecmp(p, "tag", 3) == 0) {
		if (! wi.inWay)
			ret += buildmap_osm_text_node_tag(p, 1);
		else
                	ret += buildmap_osm_text_way_tag(p);
                continue;
        } 
    }
    buildmap_progress(ftell(fdata), st.st_size);
    putchar('\n');

    qsort(WayTable, nWayTable, sizeof(*WayTable), qsort_compare_ints);

    (void) time(&t[passid]);
    buildmap_info("Pass %d : %d lines read (%d seconds)",
		    passid, LineNo, t[passid] - t[passid - 1]);
    passid++;

    /*
     * Pass 2 - flag interesting nodes : any node (a <nd>) in a way,
     * but e.g. nodes that represent town definitions as well.
     */
    LineNo = 0;
    buildmap_set_source("pass 2");
    fseek(fdata, 0L, SEEK_SET);
    buildmap_osm_text_reset_way();
    buildmap_osm_text_reset_node();

    while (! feof(fdata)) {
	buildmap_progress(ftell(fdata), st.st_size);
        buildmap_set_line(++LineNo);
        got = fgets(buf, LINELEN, fdata);
        if (got == NULL) {
            if (feof(fdata))
                break;
            buildmap_fatal(0, "short read (length)");
        }

        /* Figure out the XML */
        for (p=buf; *p && isspace(*p); p++) ;
        if (*p == '\n' || *p == '\r') 
                continue;
        if (*p != '<') // continuation line?
		continue;

	in_relation = 0;
        p++; /* point to character after '<' now */
        for (; *p && isspace(*p); p++) ;

        if (strncasecmp(p, "relation", 8) == 0) {
		in_relation = 1;
        } else if (strncasecmp(p, "/relation", 9) == 0) {
		in_relation = 0;
        } else if (in_relation) {
		continue;
        } else if (strncasecmp(p, "way", 3) == 0) {
		if (sscanf(p, "way id=%*[\"']%d%*[\"']", &wi.inWay) == 1) {
		    interesting_way = IsWayInteresting(wi.inWay);
		} else {
		    wi.inWay = 0;
		    interesting_way = 0;
		}
                continue;
        } else if (strncasecmp(p, "/way", 4) == 0) {
		buildmap_osm_text_reset_way();
                continue;
        } else if (strncasecmp(p, "nd", 2) == 0) {
		/* nodes referenced by interesting ways are interesting */
                if (wi.inWay && interesting_way) {
		    int     node;
		    if (sscanf(p, "nd ref=%*[\"']%d%*[\"']", &node) == 1) {
			NodeIsInteresting(node);
		    }
		}
                continue;
        }
    }
    buildmap_progress(ftell(fdata), st.st_size);
    putchar('\n');

    qsort(NodeTable, nNodeTable, sizeof(*NodeTable), qsort_compare_ints);

    (void) time(&t[passid]);
    buildmap_info("Pass %d : %d lines read (%d seconds)",
		    passid, LineNo, t[passid] - t[passid - 1]);
    passid++;

    /*
     * Pass 3 - look for all <node>s, define the interesting ones.
     * Pass 3 - define ways flagged as interesting
     */
    LineNo = 0;
    NumNodes = 0;
    buildmap_set_source("pass 3");
    fseek(fdata, 0L, SEEK_SET);
    buildmap_osm_text_reset_way();
    buildmap_osm_text_reset_node();

    while (! feof(fdata)) {
	buildmap_progress(ftell(fdata), st.st_size);
        buildmap_set_line(++LineNo);
        got = fgets(buf, LINELEN, fdata);
        if (got == NULL) {
            if (feof(fdata))
                break;
            buildmap_fatal(0, "short read (length)");
        }

        /* Figure out the XML */
        for (p=buf; *p && isspace(*p); p++) ;
        if (*p == '\n' || *p == '\r') 
                continue;
        if (*p != '<') // continuation line?
		continue;

	in_relation = 0;
        p++; /* point to character after '<' now */
        for (; *p && isspace(*p); p++) ;

        if (strncasecmp(p, "relation", 8) == 0) {
		in_relation = 1;
        } else if (strncasecmp(p, "/relation", 9) == 0) {
		in_relation = 0;
        } else if (in_relation) {
		continue;
        } else if (strncasecmp(p, "node", 4) == 0) {
		// nodes get added to the buildmap tables here...
		ret += buildmap_osm_text_node_interesting(p);
		NumNodes++;
                continue;
        } else if (strncasecmp(p, "/node", 5) == 0) {
		// ...and here
		ret += buildmap_osm_text_node_interesting_end(p);
                continue;
        } else if (strncasecmp(p, "tag", 3) == 0) {
		if (! wi.inWay)
			ret += buildmap_osm_text_node_tag(p, 0);
		else
                	ret += buildmap_osm_text_way_tag(p);
                continue;
	} else if (strncasecmp(p, "way", 3) == 0) {
		// ways get added to the buildmap tables (if interesting) here...
                ret += buildmap_osm_text_way(p);
                continue;
        } else if (strncasecmp(p, "/way", 4) == 0) {
		// ...and here
                ret += buildmap_osm_text_way_end(p);
                continue;
        } else if (strncasecmp(p, "nd", 2) == 0) {
		// the nd node references gets put on WayNodes list
                ret += buildmap_osm_text_nd(p);
                continue;
        }
    }
    buildmap_progress(ftell(fdata), st.st_size);
    putchar('\n');

    (void) time(&t[passid]);
    buildmap_info("Pass %d : %d lines read (%d seconds)",
		    passid, LineNo, t[passid] - t[passid - 1]);
    passid++;


    buildmap_osm_text_ways_shapeinfo();

    (void) time(&t[passid]);
    buildmap_info("Ways %d, interesting %d",
	    NumWays, nWayTable);
    buildmap_info("Number of nodes : %d, interesting %d", NumNodes, nNodeTable);
    buildmap_info("Final: (%d seconds)",
		    passid, t[passid] - t[passid - 1]);


    return ret;
}
