/*
 * LICENSE:
 *
 *   Copyright 2007 Stephen Woodbridge, 2015, Paul Fox
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


#define LINE	1
#define AREA	2
#define PLACE	4
#define ANY	7

typedef struct {
    char *value;
    int  *layerp;
    int  flags;
} value_info_t;

typedef struct {
    char *tag;
    value_info_t *value_table;
} tag_info_t;


/* declare storage for the layer indices */
#define layer_define(name) static int name;
#include "buildmap_osm_layer_list.h"

/* declare index/name table */
struct layer_index_name {
    int *layer;
    char *name;
} layer_index_name[] = {
#define layer_define(name) { &name, #name },
#include "buildmap_osm_layer_list.h"
};


value_info_t highway_to_layer[] = {
	{ "motorway",           &Freeways,      LINE },
	{ "motorway_link",      &Freeways,      LINE },
	{ "trunk",              &Freeways,      LINE },
	{ "trunk_link",         &Freeways,      LINE },
	{ "primary",            &Highways,      LINE },
	{ "primary_link",       &Highways,      LINE },
	{ "secondary",          &Streets,       LINE },
	{ "tertiary",           &Streets,       LINE },
	{ "unclassified",       &Streets,       LINE },
	{ "minor",              &Streets,       LINE },
	{ "residential",        &Streets,       LINE },
	{ "service",            &Streets,       LINE },
	{ "road",		&Streets,	LINE },
	{ "secondary_link",	&Streets,	LINE },
	{ "track",              &Trails,        LINE },
	{ "cycleway",           &Trails,        LINE },
	{ "bridleway",          &Trails,        LINE },
	{ "footway",            &Trails,        LINE },
	{ "steps",              &Trails,        LINE },
	{ "pedestrian",         &Trails,        LINE },
	{ "pathway",            &Trails,        LINE },
	{ "path",		&Trails,	LINE },
        { 0,                    NULL,           0 },
};

value_info_t cycleway_to_layer[] = {
	{ "lane",               &Trails,        LINE },
	{ "track",              &Trails,        LINE },
	{ "opposite_lane",      &Trails,        LINE },
	{ "opposite_track",     &Trails,        LINE },
        { 0,                    NULL,           0 },
};

value_info_t aeroway_to_layer[] = {
	{ "runway",             &Airports,      AREA },
	{ "aerodrome",          &Airports,      AREA },
        { 0,                    NULL,           0 },
};

value_info_t landuse_to_layer[] = {
        { "reservoir",          &Lakes,         AREA },
        { "conservation",       &Nature,        AREA },
        { "recreation_ground",  &Parks,         AREA },
        { "village_green",      &Parks,         AREA },
        { 0,                    NULL,           0 },
};

value_info_t waterway_to_layer[] = {
	{ "river",              &Rivers,        LINE },
	{ "canal",              &Rivers,        LINE },
	{ "stream",             &Rivers,        LINE },
	{ "riverbank",          &Rivers,        AREA },
#if 0
	{ "drain",              &Rivers,        LINE },
	{ "dock",               &Rivers,        LINE },
	{ "lock_gate",          &Rivers,        LINE },
	{ "turning_point",      &Rivers,        LINE },
	{ "aquaduct",           &Rivers,        LINE },
	{ "boatyard",           &Rivers,        LINE },
	{ "water_point",        &Rivers,        LINE },
	{ "weir",               &Rivers,        LINE },
	{ "dam",                &Rivers,        LINE },
	{ "ditch",              &Rivers,        LINE },
#endif
        { 0,                    NULL,           0 },
};

value_info_t railway_to_layer[] = {
	{ "rail",               &Railroads,     LINE },
	{ "tram",               &Railroads,     LINE },
	{ "light_rail",         &Railroads,     LINE },
	{ "subway",             &Railroads,     LINE },
	{ "preserved",          &Railroads,     LINE },
	{ "disused",            &Railroads,     LINE },
	{ "abandoned",          &Railroads,     LINE },
	{ "narrow_gauge",       &Railroads,     LINE },
	{ "monorail",           &Railroads,     LINE },
	{ "halt",               &Railroads,     LINE },
	{ "tram_stop",          &Railroads,     LINE },
	{ "viaduct",            &Railroads,     LINE },
	{ "crossing",           &Railroads,     LINE },
	{ "level_crossing",     &Railroads,     LINE },
	{ "subway_entrance",    &Railroads,     LINE },
	{ "turntable",          &Railroads,     LINE },
	{ 0,                    NULL,           0 },
};

value_info_t natural_to_layer[] = {
        { "coastline",          &Shore,  	LINE },
        { "water",              &Lakes,         AREA },
        { "wood",               &Nature,        AREA },
        { "peak",               &Peak,          PLACE },
        { "volcano",		&Peak,		PLACE },
        { 0,                    NULL,           0 },
};

value_info_t amenity_to_layer[] = {
	{ "school",             &Schools,       AREA },
	{ "university",         &Schools,       AREA },
	{ "college",            &Schools,       AREA },
	{ "hospital",           &Hospitals,     LINE },
	{ "pub",                &Drinks,        PLACE },
	{ "fuel",               &Fuel,          PLACE },
	{ "restaurant",         &Food,          PLACE },
	{ "fast_food",          &Food,          PLACE },
	{ "food_court",         &Food,          PLACE },
	{ "cafe",               &Cafe,          PLACE },
	{ "grave_yard",         &Parks,         AREA },
	{ "bar",                &Drinks,        PLACE },
	{ "biergarten",         &Drinks,        PLACE },
	{ "atm",                &ATM,           PLACE },
	// { "parking",            &Parking,       AREA },
        { 0,                    NULL,           0 },
};

value_info_t place_to_layer[] = {
        { "city",               &City,          PLACE },
        { "town",               &Town,          PLACE },
        { "village",            &Village,       PLACE },
        { "hamlet",             &Hamlet,        PLACE },
        { "suburb",             &Suburbs,       PLACE },
        { "island",             &Islands,       AREA },
        { 0,                    NULL,           0 },
};

value_info_t leisure_to_layer[] = {
        { "park",               &Parks,         AREA },
        { "common",             &Parks,         AREA },
        { "garden",             &Parks,         AREA },
        { "nature_reserve",     &Parks,         AREA },
        { "golf_course",        &Parks,         AREA },
        { "dog_park",           &Parks,         AREA },
        { "playground",         &Parks,         AREA },
        { "golf course",        &Parks,         AREA },
        { "recreation_ground",  &Parks,         AREA },
        { 0,                    NULL,           0 },
};

value_info_t historic_to_layer[] = {
        { "castle",             &Castle,        PLACE },
        { 0,                    NULL,           0 },
};

value_info_t tourism_to_layer[] = {
        { "hotel",              &Hotel,         PLACE },
        { "motel",              &Motel,         PLACE },
        { "guest_house",        &Guesthouse,	PLACE },
        { "hostel",             &Hostel,	PLACE },
        { "alpine_hut",         &Hut,           PLACE },
        { 0,                    NULL,           0 },
};


tag_info_t tag_table[] = {
	{"highway",		highway_to_layer	},
	{"cycleway",		cycleway_to_layer	},
	{"waterway",		waterway_to_layer	},
	{"railway",		railway_to_layer	},
	{"leisure",		leisure_to_layer	},
	{"amenity",		amenity_to_layer	},
	{"tourism",		tourism_to_layer	},
	{"historic",		historic_to_layer	},
	{"landuse",		landuse_to_layer	},
	{"aeroway",		aeroway_to_layer	},
	{"natural",		natural_to_layer	},
	{"place",		place_to_layer		},
	{ 0,			NULL			},
};

/**
 * @brief initialize layers
 */
void buildmap_osm_common_find_layers (void) {
    /* fetch the layer indices from the class file */
#define layer_define( name ) \
    name = buildmap_layer_get(#name);
#include "buildmap_osm_layer_list.h"

}

