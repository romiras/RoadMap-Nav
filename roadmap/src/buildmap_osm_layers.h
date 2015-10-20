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
    char *osm_vname;
    int  *layerp;
    int  flags;
} value_info_t;

typedef struct {
    char *osm_tname;
    value_info_t *value_list;
    int flags;
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


BuildMapDictionary DictionaryPrefix;
BuildMapDictionary DictionaryStreet;
BuildMapDictionary DictionaryType;
BuildMapDictionary DictionarySuffix;
BuildMapDictionary DictionaryCity;


value_info_t highway_to_layer[] = {
        { 0,                    NULL,           0 },
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
	{ "track",              &Trails,        LINE },
	{ "cycleway",           &Trails,        LINE },
	{ "bridleway",          &Trails,        LINE },
	{ "footway",            &Trails,        LINE },
	{ "steps",              &Trails,        LINE },
	{ "pedestrian",         &Trails,        LINE },
	{ "pathway",            &Trails,        LINE },
	{ "road",		&Streets,	LINE },
	{ "secondary_link",	&Streets,	LINE },
	{ "path",		&Trails,	LINE },
        { 0,                    NULL,           0 },
};

value_info_t cycleway_to_layer[] = {
        { 0,                    NULL,           0 },
	{ "lane",               &Trails,        LINE },
	{ "track",              &Trails,        LINE },
	{ "opposite_lane",      &Trails,        LINE },
	{ "opposite_track",     &Trails,        LINE },
        { 0,                    NULL,           0 },
};

value_info_t aeroway_to_layer[] = {
        { 0,                    NULL,           0 },
	{ "runway",             &Airports,      AREA },
	{ "aerodrome",          &Airports,      AREA },
        { 0,                    NULL,           0 },
};

value_info_t landuse_to_layer[] = {
        { 0,                    NULL,           0 },
        { "reservoir",          &Lakes,         AREA },
        { "conservation",       &Nature,         AREA },
        { 0,                    NULL,           0 },
};

value_info_t waterway_to_layer[] = {
        { 0,                    NULL,           0 },
	{ "river",              &Rivers,        LINE },
	{ "canal",              &Rivers,        LINE },
	{ "stream",             &Rivers,        LINE },
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
	{ "riverbank",          &Rivers,        LINE },
	{ "ditch",              &Rivers,        LINE },
#endif
        { 0,                    NULL,           0 },
};

value_info_t railway_to_layer[] = {
	{ 0,			NULL,           0 },
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
        { 0,                    NULL,           0 },
        { "coastline",          &Shore,  	LINE },
        { "water",              &Lakes,         AREA },
        { "wood",               &Nature,        AREA },
        { "peak",               &Peak,          PLACE },
        { "volcano",		&Peak,		PLACE },
        { 0,                    NULL,           0 },
};

value_info_t amenity_to_layer[] = {
        { 0,                    NULL,           0 },
	{ "hospital",           &Hospitals,     LINE },
	{ "pub",                &Drinks,        PLACE },
	// { "parking",            &Parking,       AREA },
	// { "post_office",        &Postoffice,    LINE },
	{ "fuel",               &Fuel,          PLACE },
	{ "school",             &Schools,       AREA },
	// { "supermarket",        NULL,	        LINE },
	// { "library",            NULL,           LINE },
	// { "police",             &Police,        LINE },
	// { "fire_station",       NULL,           LINE },
	{ "restaurant",         &Food,          PLACE },
	{ "fast_food",          &Food,          PLACE },
	// { "place_of_worship",   NULL,           LINE },
	{ "cafe",               &Cafe,          PLACE },
	// { "bicycle_parking",    NULL,           AREA },
	// { "public_building",    NULL,           AREA },
	{ "grave_yard",         &Parks,         AREA },
	{ "university",         &Schools,       AREA },
	{ "college",            &Schools,       AREA },
	// { "townhall",           NULL,           AREA },
	{ "food_court",         &Food,          PLACE },
	{ "bbq",                &Food,          PLACE },
	{ "bar",                &Drinks,        PLACE },
	{ "biergarten",         &Drinks,        PLACE },
	// { "ferry_terminal",     NULL,           AREA },
	// { "car_sharing",        NULL,           AREA },
	// { "taxi",               NULL,           AREA },
	{ "atm",                &ATM,           PLACE },
	// { "arts_centre",        NULL,           AREA },
	// { "community_centre",   NULL,           AREA },
	// { "social_centre",      NULL,           AREA },
	// { "courthouse",         NULL,           LINE },
	// { "crematorium",        NULL,           LINE },
	// { "embassy",            NULL,           LINE },
	// { "marketplace",        NULL,           AREA },
	// { "prison",             NULL,           AREA },
	// { "shelter",            NULL,           AREA },
        { 0,                    NULL,           0 },
};

value_info_t place_to_layer[] = {
        { 0,                    NULL,           0 },
        // { "continent",          NULL,           LINE },
        // { "country",            NULL,           LINE },
        // { "state",              NULL,           LINE },
        // { "region",             NULL,           LINE },
        // { "county",             NULL,           LINE },
        { "city",               &City,          PLACE },
        { "town",               &Town,          PLACE },
        { "village",            &Village,       PLACE },
        { "hamlet",             &Hamlet,        PLACE },
        { "suburb",             &Suburbs,       PLACE },
        { 0,                    NULL,           0 },
};

value_info_t leisure_to_layer[] = {
        { 0,                    NULL,           0 },
        { "park",               &Parks,         AREA },
        { "common",             &Parks,         AREA },
        { "garden",             &Parks,         AREA },
        { "nature_reserve",     &Parks,         AREA },
        { "golf_course",        &Parks,         AREA },
        { "dog_park",           &Parks,         AREA },
        { "playground",         &Parks,         AREA },
        { "golf course",        &Parks,         AREA },
        { 0,                    NULL,           0 },
};

value_info_t historic_to_layer[] = {
        { 0,                    NULL,           0 },
        { "castle",             &Castle,        PLACE },
        // { "archaeological_site",NULL,           AREA },
        // { "ruins",              NULL,           AREA },
        { 0,                    NULL,           0 },
};

value_info_t tourism_to_layer[] = {
        { 0,                    NULL,           0 },
        { "hotel",              &Hotel,         PLACE },
        { "motel",              &Motel,         PLACE },
        { "guest_house",        &Guesthouse,	PLACE },
        { "hostel",             &Hostel,	PLACE },
        { "alpine_hut",         &Hut,           PLACE },
        { 0,                    NULL,           0 },
};


/* set the third column to a specific type only if that table
 * contains _only_ that type */
tag_info_t tag_info[] = {
        {0,                     NULL,			0	},
        {"highway",             highway_to_layer,	ANY	},
        {"cycleway",            cycleway_to_layer,	ANY	},
        {"waterway",            waterway_to_layer,	ANY	},
        {"railway",             railway_to_layer,	ANY	},
        {"leisure",             leisure_to_layer,	ANY	},
        {"amenity",             amenity_to_layer,	ANY	},
        {"tourism",             tourism_to_layer,	PLACE	},
        {"historic",            historic_to_layer,	PLACE	},
        {"landuse",             landuse_to_layer,	ANY	},
        {"aeroway",             aeroway_to_layer,	ANY	},
        {"natural",             natural_to_layer,	ANY	},
        {"place",               place_to_layer,		PLACE	},
        { 0,                    NULL,			0	},
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

