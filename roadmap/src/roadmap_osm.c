/* roadmap_osm.c - helper utilities for OpenStreetMap maps
 *
 * LICENSE:
 *
 *   Copyright 2007 Paul G. Fox
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
 * These functions are used to work with the OSM maps in quadtile form.
 *
 * These functions are used both by roadmap and by buildmap.
 */

/*
 * Notes about quadtiles...
 *
 * The openstreetmap wiki has a good page on quadtiles:
 *   http://wiki.openstreetmap.org/index.php/QuadTiles
 *
 * We use a slightly modified format for describing the tiles, since
 * the full address of the smallest quadtile as described on that page
 * would take 32 bits, and we need to set some aside.
 * First, we want to avoid the sign bit, so we lose one there.
 * Second, we want to encode the number of bits used in the encoding,
 * in the tileid itself.  There's a tradeoff here -- the more bits we
 * set aside for describing the encoding bits, the fewer we have for
 * addressing tiles.  We've chosen 4 bits to describe the bit encoding,
 * leaving 27 bits for tile address.  So our encoding looks something
 * like this:
 *   31                                      0
 *   |STTT|TTTT|TTTT|TTTT||TTTT|TTTT|TTTT|BBBB|
 *
 * 'S' is the sign bit, when we pass tileids to the rest
 * or roadmap, we negate them to distinguish them from "regular"
 * county fips values.
 *
 * TTTT....TTT is the tile address.  this value is LEFT justified
 * in these bit position.  this was done so that when the name is
 * split up to form a directory path (to prevent any single directory
 * from getting too big), the paths will look more uniform for differing
 * values of bits.
 *
 * BBBB is the number of bits used to encode this tile.  the available
 * encodings of 0-15 are mapped to the actual values of 12 through 27.
 * Thus, a tile encoded with a 19 bit address will show a hex '7' in the
 * final nibble of the tileid.
 * 
 * Except for the semantics of the sign bit, these encodings are
 * entirely contained within a few macros in roadmap_osm.h.  The use
 * of negation to distinguish fips values from tileids is mainly here
 * (this code deals with positive tileids) and in roadmap_locator.c
 * (which knows that tileids are negative).
 *
 * A few of the routines in this file which transform lat/lon pairs
 * to tileids, and vice versa, were translated from the php code
 * found in bmap.php, the OSM Binary Mobile protocol proxy server.
 *   http://wiki.openstreetmap.org/index.php/OSM_Mobile_Binary_Protocol
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_config.h"
#include "roadmap_math.h"
#include "roadmap_scan.h"
#include "roadmap_file.h"
#include "roadmap_osm.h"

static RoadMapConfigDescriptor RoadMapConfigOSMBits =
                        ROADMAP_CONFIG_ITEM ("Map", "QuadTile bits");

int RoadMapOSMBits;

int roadmap_osm_latlon2tileid(int lat, int lon, int bits) {

    int i, id, bit;

    id = 0;
    bit = (1 << (bits - 1));
    for (i = 0; i < bits; i++) {
            if (i & 1) {
                    if (lat >= ( MaxLat - (MaxLat >> i/2) ) ) {
                            id |= bit;
                    } else {
                            lat += (MaxLat >> i/2);
                    }
            } else {
                    if (lon >= ( MaxLon - (MaxLon >> i/2) ) ) {
                            id |= bit;
                    } else {
                            lon += (MaxLon >> i/2);
                    }
            }
	    bit >>= 1;
    }
    return mktileid(id, bits);
}

void roadmap_osm_tileid_to_bbox(int tileid, RoadMapArea *edges) {

    int i, bits, bit, bmaptileid;

    bits = tileid2bits(tileid);
    bmaptileid = tileid2trutile(tileid);

    edges->west = -MaxLon;
    edges->south = -MaxLat;

    bit = (1 << (bits-1));
    for (i = 0; i < bits; i++) {
            if (bmaptileid & bit) {
                    if (i & 1) {
                            edges->south += (MaxLat >> i/2);
                    } else {
                            edges->west += (MaxLon >> i/2);
                    }
            }
	    bit >>= 1;
    }
    edges->north = edges->south + (MaxLat >> (bits-2)/2);
    edges->east = edges->west + (MaxLon >> (bits-1)/2);
}

#define LAT 0
#define LON 1
char *tile_neighbors[] = {
/*   se    s     sw     e     w    ne    n     nw       */
    "-+", "- ", "--", " +", " -", "++", "+ ", "+-",
};

#if TESTING
static void roadmap_osm_bbox_print_dimensions
    (char *msg, const RoadMapArea *edges, int bits)
{
    RoadMapPosition pos1, pos2;
    static int obits = -1;

    if (obits == bits) return;

    roadmap_math_use_imperial ();
    pos1.longitude = edges->west;
    pos1.latitude = edges->north;
    pos2.longitude = edges->east;
    pos2.latitude = edges->south;
    roadmap_math_set_center (&pos1);
    roadmap_log(ROADMAP_DEBUG,
        "%s: bits %d, tile diagonal is %s miles\n", msg, bits,
     roadmap_math_to_floatstring
        (NULL, roadmap_math_to_trip_distance_tenths
            (roadmap_math_distance(&pos1, &pos2)) , TENTHS));

    obits = bits;
}
#endif

/* return the neighboring tile in the given direction */
int roadmap_osm_tileid_to_neighbor(int tileid, int dir) {
    RoadMapArea edges[1];
    int gridlon, gridlat;
    int lon, lat;
    char *delta;
    int newtileid;

    int bits = tileid2bits(tileid);

    gridlon = (MaxLon >> ((bits-1) >> 1));
    gridlat = (MaxLat >> ((bits-2) >> 1));

    roadmap_osm_tileid_to_bbox(tileid, edges);
#if TESTING
    roadmap_osm_bbox_print_dimensions("tile size", edges, bits);
#endif

    lon = edges->west + gridlon/2;
    lat = edges->south + gridlat/2;

    delta = tile_neighbors[dir];

    if (delta[LAT] == '+')
            lat += gridlat;
    else if (delta[LAT] == '-')
            lat -= gridlat;
    
    if (delta[LON] == '+')
            lon += gridlon;
    else if (delta[LON] == '-')
            lon -= gridlon;

    newtileid = roadmap_osm_latlon2tileid(lat, lon, bits);

    /* can happen if lat/lon go out of bounds (+-90/+-180) */
    if (newtileid == tileid) {
	roadmap_log(ROADMAP_DEBUG, "attempt to compute out-of-bounds tileid");
	return -1;
    }

    return newtileid;
                
}

char *roadmap_osm_filename(char *buf, int dirpath, int tileid, char *suffix) {
 
    int a, b;
    char *n;
    static char staticbuf[128];

    /* we're usually called with a real tileid, but roadmap_locator
     * calls us with the negated version that we show to the "outside".
     */
    if (tileid < 0) tileid = -tileid;

    if (!buf)
        buf = staticbuf;

    n = buf;

    if (dirpath) {
        a = (tileid >> 24) & 0xff;
        b = (tileid >> 16) & 0xff;

        n += sprintf(n, "qt%d/%02x/%02x/", tileid2bits(tileid), a, b);
    }
    n += sprintf(n, "qt%08x%s", tileid, suffix);

    return buf;
}

void roadmap_osm_tilesplit(int tileid, int *ntiles, int howmanybits) {
    int i;
    int bits = tileid2bits(tileid);
    int trutile = tileid2trutile(tileid);
    for (i = 0; i < (1 << howmanybits); i++) {
        ntiles[i] = mktileid((trutile << howmanybits) | i, bits + howmanybits);
    }
}


/* these aren't really the "master home" of these pointers.  they're
 * static simply to simplify the parameters used in the recursive tile
 * splitting.  they actually are set/restored from the *fips parameter
 * in roadmap_osm_by_position(), and so are really the same as the fipslistp
 * passed from roadmap_locator.c
 */
static int *roadmap_osm_tilelist;
static int roadmap_osm_tilelist_len;

/* this is the action routine for the recursive tile walker */
static void roadmap_osm_add_tile_to_list(int tileid) {
    roadmap_osm_tilelist_len++;
    roadmap_osm_tilelist = 
        realloc(roadmap_osm_tilelist, 
            roadmap_osm_tilelist_len * sizeof(*roadmap_osm_tilelist));
    roadmap_check_allocated(roadmap_osm_tilelist);
    roadmap_osm_tilelist[roadmap_osm_tilelist_len-1] = -tileid;
}

// FIXME -- need to make these accessible, so they can be
// reset manually, or when we somehow know new maps are available.
static int roadmap_osm_maps_available = -1;
static int roadmap_osm_maps_biggest, roadmap_osm_maps_smallest;
static const char *roadmap_osm_bits_paths[32];

/* we want to add a tile, but if it doesn't exist, we want to see
 * if its children exist, so we can add them instead.
 */
static void roadmap_osm_add_if_exists(int tileid) {

    int i;
    int bits;
    
    bits = tileid2bits(tileid);

    if (roadmap_osm_bits_paths[bits] &&
        roadmap_file_exists (roadmap_osm_bits_paths[bits],
		roadmap_osm_filename(NULL, 1, tileid, ".rdm"))) {
        roadmap_osm_add_tile_to_list(tileid);
    } else {
        if (bits < roadmap_osm_maps_smallest) {
            int children[2];
            roadmap_osm_tilesplit(tileid, children, 1);
            for (i = 0; i < 2; i++) {
                roadmap_osm_add_if_exists(children[i]);
            }
        }
    }
}

/* load the externally-provided list with tileids. */
int roadmap_osm_by_position
        (const RoadMapPosition *position,
         const RoadMapArea *focus, int **fips, int in_count) {

    int i, d, width;
    int tileid;
    char eswn[] = { TILE_EAST, TILE_SOUTH, TILE_WEST, TILE_NORTH};
    RoadMapArea tileedges;
    int reallyfound, oreally;

    if (RoadMapOSMBits <= 0) return in_count;  /* master shutoff */

    if (roadmap_osm_maps_available < 0) {
	char bitsdir[16];
	 const char *path;
	int bits;

	roadmap_osm_maps_available = 0;
	for (bits = RoadMapOSMBits; bits <= TILE_MAXBITS; bits++) {
	    sprintf(bitsdir, "qt%d", bits);
	    path = roadmap_scan ("maps", bitsdir);
	    if (path) {
		if (!roadmap_osm_maps_biggest)
		    roadmap_osm_maps_biggest = bits;
		roadmap_osm_maps_smallest = bits;
		roadmap_osm_bits_paths[bits] = path;
		roadmap_osm_maps_available = 1;
	    }
	}
    }

    if ( !roadmap_osm_maps_available )
	return in_count;

    roadmap_osm_tilelist_len = in_count;
    roadmap_osm_tilelist = *fips;

    tileid = roadmap_osm_latlon2tileid
            (position->latitude, position->longitude, roadmap_osm_maps_biggest);
    roadmap_osm_add_if_exists(tileid);


// FIXME -- the "no more tiles" checks below aren't quite right. 
// we're computing a contiguous square of tiles by spiraling outward,
// moving northwest after every loop.  if we're on the "edge of the
// world", we'll either hit an invalid tile right away, or after we've
// gone some way around.  with the current checks we'll bail out right
// away -- we should actually keep checking, at least once per side,
// for more valid tiles.  changing the algorithm would help:  compute
// the next ring out based on the outward neighbors of the existing
// ring, rather than as a chain.

    width = 1;
    oreally = -1;
    reallyfound = 0;
    while (reallyfound != oreally) {
        oreally = reallyfound;
        tileid = roadmap_osm_tileid_to_neighbor(tileid, TILE_NORTHWEST);
	if (tileid == -1) /* no more tiles */
	    break;
        width += 2;
        for (d = 0; d < 4; d++) {
            for (i = 0; i < width-1; i++) {
                tileid = roadmap_osm_tileid_to_neighbor(tileid, eswn[d]);
		if (tileid == -1) /* no more tiles */
		    break;
                roadmap_osm_tileid_to_bbox(tileid, &tileedges);
                if (roadmap_math_areas_intersect (&tileedges, focus))
                    reallyfound++;

                /* we accumulate all squares until a trip around the
                 * block says that we haven't found any "real" visibility
                 * overlaps in a full circuit.  including this outer 
                 * ring in the "found" results causes us to render lines
                 * that may start in those tiles which terminate in, or
                 * cross, the "truly" visible tiles.
                 */
                roadmap_osm_add_if_exists(tileid);
            }
        }
    }

    *fips = roadmap_osm_tilelist;
    roadmap_osm_tilelist = NULL;

    return roadmap_osm_tilelist_len;
}

void roadmap_osm_initialize (void) {

    roadmap_config_declare
        ("preferences", &RoadMapConfigOSMBits,  ROADMAP_OSM_DEFAULT_BITS);

    /* this is our preference for the tilesize to load.  set it to 0
     * to suppress all OSM tiles */
    RoadMapOSMBits = roadmap_config_get_integer (&RoadMapConfigOSMBits);

}

