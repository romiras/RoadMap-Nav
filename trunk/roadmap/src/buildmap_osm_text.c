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

extern char *BuildMapResult;

/* OSM has over 2G nodes already -- enough to overflow a 32 bit signed int */
typedef unsigned int nodeid_t;
typedef unsigned int wayid_t;

/**
 * @brief a couple of variables to keep track of the way we're dealing with
 */
static struct WayInfo {
    wayid_t WayId;              /**< are we in a way (its id) */
    int      nWayNodes;          /**< number of nodes known for 
						     this way */
    int      WayLayer;           /**< the layer for this way */
    char     *WayStreetName;     /**< the street name */
    char     *WayStreetRef;	 /**< street code,
				       to be used when no name (e.g. motorway) */
    int      WayFlags;           /**< properties of this way, from
						     the table flags */
    int      WayIsOneWay;        /**< is this way one direction only */
    int      WayAdminLevel;	 /**< boundaries */
    int      WayCoast;           /**< coastline */
    int      WayIsInteresting;   /**< this way is interesting for RoadMap */
} wi;

/**
 * @brief variables referring to the current node
 */
static struct NodeInfo {
    nodeid_t      NodeId;             /**< which node */
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
 * limitation : names must be shorter than 512 bytes
 * @param s input string
 * @return duplicate, to be freed elsewhere
 */
static char *FromXmlAndDup(const char *s)
{
        int             i, j, k, l, found;
        static char     res[512];

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
		buildmap_check_allocated(points);
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
void
buildmap_osm_text_node_read_lat_lon(char *data)
{
    int         nchars;
    char        *p;
    int         NodeLatRead, NodeLonRead;
    char	tag[512], value[512];
    int		s;

    s = sscanf(data, "node id=%*[\"']%u%*[\"']%n", &ni.NodeId, &nchars);
    if (s != 1)
	    buildmap_fatal(0, "buildmap_osm_text(%s) node error", data);

    p = data + nchars;

    ni.NodeLat = ni.NodeLon = 0;
    NodeLatRead = NodeLonRead = 0;

    while (NodeLatRead == 0 || NodeLonRead == 0) {

            for (; *p && isspace(*p); p++) ;

            s = sscanf(p, "%[a-zA-Z0-9_]=%*[\"']%[^\"']%*[\"']%n",
                        	tag, value, &nchars);
	    if (s != 2)
		    buildmap_fatal(0, "bad tag read at '%s'\n", p);

            if (strcmp(tag, "lat") == 0) {
                    ni.NodeLat = roadmap_math_from_floatstring(value, MILLIONTHS);
                    NodeLatRead++;
            } else if (strcmp(tag, "lon") == 0) {
                    ni.NodeLon = roadmap_math_from_floatstring(value, MILLIONTHS);
                    NodeLonRead++;
            }

            p += nchars;
    }

}

/**
 * @brief At the end of a node, process its data
 * @param data point to the line buffer
 * @return error indication
 */
void
buildmap_osm_text_node_finish(void)
{
	int npoints, s;

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
                        s = sscanf(ni.NodePostalCode, "%d", &zip);
			if (s != 1)
				buildmap_fatal(0, "bad zip read at '%s'\n", ni.NodePostalCode);
                        if (zip)
                                buildmap_zip_add(zip, ni.NodeLon, ni.NodeLat);
                }
	    }
        }

	/* Add the node */
	npoints = buildmap_point_add(ni.NodeLon, ni.NodeLat);
	buildmap_osm_text_point_add(ni.NodeId, npoints);

        buildmap_osm_text_reset_node();
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

struct neighbor_ways {
    int tileid;		    // these are the ways for this tileid
    RoadMapFileContext fc;  // file context for the mmap
    int count;	    	    // how many ways
    const wayid_t *ways;    // the way ids for that tileid.
			    // (also serves as the "populated" flag.)
} Neighbor_Ways[8];

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
 * @brief find out if this way is interesting
 * @param wayid
 * @return whether we thought way was interesting when we found it
 *
 * Note : relies on the order of ways encountered in the file, for performance
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


static void
saveInterestingNode(nodeid_t node)
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

	NodeTable[nNodeTable] = node;
	nNodeTable++;
}

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
 * @brief
 * @param data points into the line of text being processed
 * @return error indication
 *
 * Example line :
 *     <nd ref="997470"/>
 */

int      nWayNodeAlloc; /**< size of the allocation of the array */
int      *WayNodes;     /**< the array to keep track of this way's nodes */

void
buildmap_osm_text_save_way_nodes(char *data)
{
        nodeid_t node;
	int s;

        if (!wi.WayId)
                buildmap_fatal(0, "Wasn't in a way (%s)", data);

        s = sscanf(data, "nd ref=%*[\"']%u%*[\"']", &node);
        if (s != 1)
		buildmap_fatal(0, "fail to scanf interesting nd");

        if (buildmap_osm_text_point_get(node) < 0) {
                return;
        }

        if (wi.nWayNodes == nWayNodeAlloc) {
		if (WayNodes)
                    nWayNodeAlloc *= 2;
		else
                    nWayNodeAlloc = 1000;
                WayNodes = 
                    (int *)realloc(WayNodes, sizeof(*WayNodes) * nWayNodeAlloc);
		buildmap_check_allocated(WayNodes);
        }
        WayNodes[wi.nWayNodes++] = node;
}

/**
 * @brief deal with tag lines outside of ways
 * @param data points into the line of text being processed
 * @return error indication
 */
void
buildmap_osm_text_node_tag(char *data, int catalog)
{
        static char *tagk = 0;
        static char *tagv = 0;
	int s;

        if (! tagk)
                tagk = malloc(512);
        if (! tagv)
                tagv = malloc(512);

        s = sscanf(data, "tag k=%*['\"]%[^\"']%*['\"] v=%*['\"]%[^\"']%*['\"]",
                        tagk, tagv);
	if (s != 2)
		buildmap_fatal(0, "fail to scanf tag k and v (%s)", data);

        if (strcmp(tagk, "postal_code") == 0) {
                /* <tag k="postal_code" v="3020"/> */
                if (ni.NodePostalCode)
                        free(ni.NodePostalCode);
                ni.NodePostalCode = strdup(tagv);
		if (catalog) saveInterestingNode(ni.NodeId);
        } else if (strcmp(tagk, "place") == 0) {
                /* <tag k="place" v="town"/> */
                if (ni.NodePlace)
                        free(ni.NodePlace);
                ni.NodePlace = strdup(tagv);
		if (catalog) saveInterestingNode(ni.NodeId);
        } else if (strcmp(tagk, "name") == 0) {
                /* <tag k="name" v="Herent"/> */
                if (ni.NodeTownName)
                        free(ni.NodeTownName);
                ni.NodeTownName = FromXmlAndDup(tagv);
		if (catalog) saveInterestingNode(ni.NodeId);
        }
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
void
buildmap_osm_text_way_tag(char *data)
{
	static char	*tag = 0, *value = 0;
	int		i, s;
	layer_info_t	*list;

	/* have we already decided this way is uninteresting? */
	if (!wi.WayIsInteresting)
		return;

	if (! tag) tag = malloc(512);
	if (! value) value = malloc(512);

	s = sscanf(data, "tag k=%*[\"']%[^\"']%*[\"'] v=%*[\"']%[^\"']%*[\"']",
			tag, value);
	if (s != 2)
		buildmap_fatal(0, "fail to scanf tag and value (%s)", data);

	/* street names */
	if (strcasecmp(tag, "name") == 0) {
		if (wi.WayStreetName)
			free(wi.WayStreetName);
		wi.WayStreetName = FromXmlAndDup(value);
		return;
	} else if (strcasecmp(tag, "ref") == 0) {
		if (wi.WayStreetRef)
			free(wi.WayStreetRef);
		wi.WayStreetRef = FromXmlAndDup(value);
		return;
	} else if (strcasecmp(tag, "landuse") == 0) {
		wi.WayIsInteresting = 0;
		return;
	} else if (strcasecmp(tag, "oneway") == 0 && strcasecmp(value, "yes") == 0) {
		wi.WayIsOneWay = ROADMAP_LINE_DIRECTION_ONEWAY;
	} else if (strcasecmp(tag, "building") == 0) {
		if (strcasecmp(value, "yes") == 0) {
			wi.WayIsInteresting = 0;
			return;
		}
	} else if (strcasecmp(tag, "admin_level") == 0) {
		wi.WayAdminLevel = atoi(value);
	} else if (strcasecmp(tag, "natural") == 0 &&
			strcasecmp(value, "coastline") == 0) {
		wi.WayCoast = 1;
	}

	/*
	 * Get layer info
	 */
	for (i=1; list_info[i].name != 0; i++) {
	    if (strcmp(tag, list_info[i].name) == 0) {
		list = list_info[i].list;
		if (list) {
		    for (i=1; list[i].name; i++) {
			if (strcmp(value, list[i].name) == 0) {
			    wi.WayFlags = list[i].flags;
			    if (list[i].layerp)
				    wi.WayLayer = *(list[i].layerp);
			    break;
			}
		    }
		}
		break;
	    }
	}
}

/**
 * @brief We found an end tag for a way, so we must have read all
 *  the required data.  Process it.
 * @param data points to the line of text being processed
 * @return error indication
 */
void
buildmap_osm_text_way_finish(char *data)
{
        int             from_point, to_point, line, street;
        int             fromlon, tolon, fromlat, tolat;
        int             j;
	char compound_name[1024];
	char *n;
	static int	l_shoreline = 0,
			l_boundary = 0;
       
	if (l_shoreline == 0)
		l_shoreline = buildmap_layer_get("shore");;
	if (l_boundary == 0)
		l_boundary = buildmap_layer_get("boundaries");;

        if (wi.WayId == 0)
                buildmap_fatal(0, "Wasn't in a way (%s)", data);

	/* if a way is both a coast and a boundary, treat it only as coast */
	if (wi.WayCoast) {
		wi.WayLayer = l_shoreline;
	} else if (wi.WayAdminLevel) {
		/* national == 2, state == 4, ignore lesser boundaries */
		if  (wi.WayAdminLevel > 4) {
			wi.WayIsInteresting = 0;
		}

		wi.WayLayer = l_boundary;
	}

        if ( !wi.WayIsInteresting || wi.WayLayer == 0) {
		wayid_t *wp;
                buildmap_verbose("discarding way %d, not interesting (%s)", wi.WayId, data);
		wp = isWayInteresting(wi.WayId);
		if (wp) {
		    WayTableCopy[wp-WayTable] = 0;
		}

                buildmap_osm_text_reset_way();
                return;
        }

	if (wi.nWayNodes < 1) {
                buildmap_osm_text_reset_way();
                return;
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
		buildmap_verbose ("Way %d [%s] ref [%s]", wi.WayId,
				wi.WayStreetName ? wi.WayStreetName : "",
				wi.WayStreetRef ? wi.WayStreetRef : "");

		if (wi.WayStreetRef) {
		    char *d = compound_name;
		    char *s = wi.WayStreetRef;
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

		    if (wi.WayStreetName) {
        		// sprintf(d, ", %s", wi.WayStreetName);
        		sprintf(d, "%s%s", ENDASHSEP, wi.WayStreetName);
        	    }
		} else {
        	    n = wi.WayStreetName;
		}
		rms_name = str2dict(DictionaryStreet, n);

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

}

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
	    // fprintf(stderr, "loading ways list for neighbor 0x%x\n", neighbor_tile);

	    /* if we already have that neighbor's ways, reuse */
	    for (j = 0; j < 8; j++) {
		if (Neighbor_Ways[j].tileid == neighbor_tile) {
		    // fprintf(stderr, "reusing neighbor 0x%x\n", neighbor_tile);
		    new_neighbor_ways[i] = Neighbor_Ways[j];
		    Neighbor_Ways[j].ways = 0;
		    break;
		}
	    }

	    /* didn't find it -- fetch the neighbor's ways */
	    if (j == 8) {
		// fprintf(stderr, "fetching neighbor 0x%x\n", neighbor_tile);
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

	// buildmap_verbose("trying line %d, %d points", lineid, count);
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


static int text_file_is_pipe;
FILE *buildmap_osm_text_fopen(char *fn)
{
    FILE *fdata;
    int len;

    len = strlen(fn);
    if (len > 3 && strcmp(&fn[len-3], ".gz") == 0) {
	char command[1024];
	sprintf(command, "gzip -d -c %s", fn);
	fdata = popen(command, "r");
	text_file_is_pipe = 1;
    } else {
	fdata = fopen(fn, "r");
	text_file_is_pipe = 0;
    }
    
    if (fdata == NULL) {
            buildmap_fatal(0, "couldn't open \"%s\"", fn);
            return NULL;
    }

    return fdata;
}

int buildmap_osm_text_fclose(FILE *fp)
{
    if (text_file_is_pipe)
	return pclose(fp);
    else
	return fclose(fp);
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
void
buildmap_osm_text_read(char *fn, int tileid, int country_num, int division_num)
{
    FILE 	*fdata;
    int		lines;
    char	*got;
    static char	buf[LINELEN];
    char	*p;
    time_t	t[10];
    int		passid, NumNodes, NumWays;
    struct stat st;
    wayid_t interesting_way;
    int		in_relation;
    int		need_xml_header = 1;
    int		need_osm_header = 1;
    int		need_osm_trailer = 1;
    int		s;

    fdata = buildmap_osm_text_fopen(fn);
    fstat(fileno(fdata), &st);

    ni.NodeFakeFips = 1000000 + country_num * 1000 + division_num;

    if (tileid)
	    buildmap_osm_text_neighbor_way_maps(tileid);

    buildmap_osm_text_point_hash_reset();
    buildmap_osm_text_reset_way();
    buildmap_osm_text_reset_node();

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
     */
    buildmap_info("Starting pass %d", passid);
    buildmap_set_source("pass 1");
    LineNo = 0;
    NumWays = 0;
    numshapes = 0;
    nPolygons = 0;
    LineId = 0;
    nWayTable = 0;
    nNodeTable = 0;

    while (! feof(fdata) && need_osm_trailer) {
        buildmap_set_line(++LineNo);
	if (LineNo % 1000 == 0) buildmap_progress(LineNo, 0);
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

	/* do some error checking in the first pass */
	if (need_xml_header) {
	    if (strncmp(p, "<?xml ", 6))
		buildmap_fatal(0, "bad input from %s, no <?xml line", fn);
	    need_xml_header = 0;
	    continue;
	}
	if (need_osm_header) {
	    if (strncmp(p, "<osm ", 5))
		buildmap_fatal(0, "bad input from %s, no <osm line", fn);
	    need_osm_header = 0;
	    continue;
	}

        if (*p != '<') {
		/*
		 * Assume we're in a continuation line such as
		 * <tag k='opening_hours' v='Sa 09:30-19:00;
		 * Su 10:00-14:00'/>
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
		continue;

        } else if (strncasecmp(p, "/relation", 9) == 0) {

		in_relation = 0;
		continue;

        } else if (in_relation) {

		continue;

        } else if (strncasecmp(p, "way", 3) == 0) {

		// assume ways are interesting, to begin with
		wi.WayIsInteresting = 1;
		s = sscanf(p, "way id=%*[\"']%u%*[\"']", &wi.WayId);
		if (s != 1)
			buildmap_fatal(0, "buildmap_osm_text(%s) way error", p);

		NumWays++;
                continue;

        } else if (strncasecmp(p, "/way", 4) == 0) {

		/* if we're processing a quadtile, don't include any
		 * ways that our neighbors already include */
		if (tileid && wi.WayId && wi.WayIsInteresting &&
			    buildmap_osm_text_check_neighbors(wi.WayId)) {
			buildmap_verbose("dropping way %d because a neighbor "
				"already has it", wi.WayId);
			wi.WayIsInteresting = 0;
		}

		/* if the way is still flagged interesting, save it */
		if (wi.WayId && wi.WayIsInteresting)
			saveInterestingWay(wi.WayId);

		buildmap_osm_text_reset_way();
                continue;

        } else if (strncasecmp(p, "tag", 3) == 0) {

		if (wi.WayId)
                	buildmap_osm_text_way_tag(p);
                continue;

        } else if (strncasecmp(p, "/osm>", 5) == 0) {

		need_osm_trailer = 0;

        } 
    }
    buildmap_progress(LineNo, 0);
    putchar('\n');
    lines = LineNo;

    if (need_osm_trailer)
	buildmap_fatal(0, "bad input from %s, no </osm> line", fn);

    qsort(WayTable, nWayTable, sizeof(*WayTable), qsort_compare_unsigneds);

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
    buildmap_osm_text_fclose(fdata);
    fdata = buildmap_osm_text_fopen(fn);
    buildmap_osm_text_reset_way();
    buildmap_osm_text_reset_node();

    while (! feof(fdata)) {
	buildmap_progress(LineNo, lines);
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
		continue;

        } else if (strncasecmp(p, "/relation", 9) == 0) {

		in_relation = 0;
		continue;

        } else if (in_relation) {

		continue;

        } else if (strncasecmp(p, "way", 3) == 0) {

		s = sscanf(p, "way id=%*[\"']%u%*[\"']", &wi.WayId);
		if (s != 1)
			buildmap_fatal(0, "buildmap_osm_text(%s) way error", p);

		interesting_way = !!isWayInteresting(wi.WayId);
                continue;

        } else if (strncasecmp(p, "/way", 4) == 0) {

		wi.WayId = 0;
		buildmap_osm_text_reset_way();
                continue;

        } else if (strncasecmp(p, "nd", 2) == 0) {

		nodeid_t     node;
		/* nodes referenced by interesting ways are interesting */
                if (wi.WayId && interesting_way) {
		    s = sscanf(p, "nd ref=%*[\"']%u%*[\"']", &node);
		    if (s != 1)
			buildmap_fatal(0, "buildmap_osm_text(%s) way error", p);

		    saveInterestingNode(node);
		}
                continue;

        } else if (strncasecmp(p, "tag", 3) == 0) {

		if (!wi.WayId)
			buildmap_osm_text_node_tag(p, 1);
                continue;
        }
    }
    buildmap_progress(LineNo, lines);
    putchar('\n');

    qsort(NodeTable, nNodeTable, sizeof(*NodeTable), qsort_compare_unsigneds);

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
    buildmap_osm_text_fclose(fdata);
    fdata = buildmap_osm_text_fopen(fn);
    buildmap_osm_text_reset_way();
    buildmap_osm_text_reset_node();

    /* we've finished saving ways.  we need a fresh place to mark
     * the ones we're discarding, while still keeping the original list
     * quickly searchable. */
    copyWayTable();

    while (! feof(fdata)) {
	buildmap_progress(LineNo, lines);
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
		continue;

        } else if (strncasecmp(p, "/relation", 9) == 0) {
		in_relation = 0;
		continue;

        } else if (in_relation) {
		continue;

        } else if (strncasecmp(p, "node", 4) == 0) {
		/*
		 * Avoid figuring out whether we're in a
		 *	<node ... />
		 * or
		 *	<node ... >
		 *	..
		 *	</node>
		 * case, by resetting first if needed.
		 */
		if (ni.NodeId && isNodeInteresting(ni.NodeId))
			buildmap_osm_text_node_finish();

		s = sscanf(p, "node id=%*[\"']%u%*[\"']", &ni.NodeId);
		if (s != 1)
                	buildmap_fatal(0, "buildmap_osm_text(%s) node error", p);

		buildmap_osm_text_node_read_lat_lon(p);

		NumNodes++;
                continue;
        } else if (strncasecmp(p, "/node", 5) == 0) {

		if (isNodeInteresting(ni.NodeId))
			buildmap_osm_text_node_finish();
                continue;

        } else if (strncasecmp(p, "tag", 3) == 0) {

		if (!wi.WayId)
			buildmap_osm_text_node_tag(p, 0);
		else
                	buildmap_osm_text_way_tag(p);
                continue;

	} else if (strncasecmp(p, "way", 3) == 0) {

        	s = sscanf(p, "way id=%*[\"']%u%*[\"']", &wi.WayId);
        	if (s != 1)
                	buildmap_fatal(0, "buildmap_osm_text(%s) way error", p);

        	wi.WayIsInteresting = !!isWayInteresting(wi.WayId);
                continue;

        } else if (strncasecmp(p, "/way", 4) == 0) {

		if (wi.WayIsInteresting)
                	buildmap_osm_text_way_finish(p);
                continue;

        } else if (strncasecmp(p, "nd", 2) == 0) {

		// the nd node references gets put on WayNodes list
		if (wi.WayIsInteresting)
                	buildmap_osm_text_save_way_nodes(p);
                continue;
        }
    }
    buildmap_progress(LineNo, lines);
    putchar('\n');

    buildmap_osm_text_fclose(fdata);

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

}
