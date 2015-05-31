/*
 * LICENSE:
 *
 *   Copyright (c) 2007 Paul Fox
 *   Copyright (c) 2008, 2010, 2011, Danny Backx
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
 * @brief a module to convert OSM (OpenStreetMap) data into RoadMap maps.
 *
 * This file contains the static data.
 *
 * See http://wiki.openstreetmap.org/wiki/Map_Features
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

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

/* Road layers. */

int BuildMapLayerFreeway;
int BuildMapLayerRamp;
int BuildMapLayerMain;
int BuildMapLayerStreet;
int BuildMapLayerTrail;
int BuildMapLayerRail;

/* Area layers. */

int BuildMapLayerPark;
int BuildMapLayerHospital;
int BuildMapLayerAirport;
int BuildMapLayerStation;
int BuildMapLayerMall;
int BuildMapLayerSchool;

int BuildMapLayerNature;
int BuildMapLayerAmenity;
int BuildMapLayerCity;
int BuildMapLayerTown;
int BuildMapLayerVillage;
int BuildMapLayerHamlet;
int BuildMapLayerSuburbs;
int BuildMapLayerPeak;

/* Water layers. */

int BuildMapLayerShoreline;
int BuildMapLayerRiver;
int BuildMapLayerCanal;
int BuildMapLayerLake;
int BuildMapLayerSea;

int BuildMapLayerBoundary;

int BuildMapLayerFood;
int BuildMapLayerCafe;
int BuildMapLayerDrinks;
int BuildMapLayerFuel;
int BuildMapLayerATM;

/* These defines are simply shorthand notation, to make
 * the tables below easier to manage
 */
#define FREEWAY     &BuildMapLayerFreeway     
#define RAMP        &BuildMapLayerRamp        
#define MAIN        &BuildMapLayerMain        
#define STREET      &BuildMapLayerStreet      
#define TRAIL       &BuildMapLayerTrail       
#define RAIL        &BuildMapLayerRail       

#define PARK        &BuildMapLayerPark
#define HOSPITAL    &BuildMapLayerHospital
#define AIRPORT     &BuildMapLayerAirport
#define STATION     &BuildMapLayerStation
#define MALL        &BuildMapLayerMall
#define SCHOOL      &BuildMapLayerSchool

#define SHORELINE   &BuildMapLayerShoreline   
#define RIVER       &BuildMapLayerRiver       
#define CANAL       &BuildMapLayerCanal       
#define LAKE        &BuildMapLayerLake        
#define SEA         &BuildMapLayerSea         

#define	NATURE      &BuildMapLayerNature
#define	PEAK        &BuildMapLayerPeak
#define AMENITY     &BuildMapLayerAmenity
#define BOUNDARY    &BuildMapLayerBoundary

#define	CITY        &BuildMapLayerCity
#define	TOWN        &BuildMapLayerTown
#define	VILLAGE     &BuildMapLayerVillage
#define	HAMLET      &BuildMapLayerHamlet
#define	SUBURBS     &BuildMapLayerSuburbs

#define	FOOD        &BuildMapLayerFood
#define	CAFE        &BuildMapLayerCafe
#define	DRINKS      &BuildMapLayerDrinks
#define	FUEL        &BuildMapLayerFuel
#define	ATM         &BuildMapLayerATM

BuildMapDictionary DictionaryPrefix;
BuildMapDictionary DictionaryStreet;
BuildMapDictionary DictionaryType;
BuildMapDictionary DictionarySuffix;
BuildMapDictionary DictionaryCity;

/**
 * @brief initialize layers
 */
void buildmap_osm_common_find_layers (void) {

   BuildMapLayerFreeway   = buildmap_layer_get ("freeways");
   BuildMapLayerRamp      = buildmap_layer_get ("ramps");
   BuildMapLayerMain      = buildmap_layer_get ("highways");
   BuildMapLayerStreet    = buildmap_layer_get ("streets");
   BuildMapLayerTrail     = buildmap_layer_get ("trails");
   BuildMapLayerRail      = buildmap_layer_get ("railroads");

   BuildMapLayerPark      = buildmap_layer_get ("parks");
   BuildMapLayerHospital  = buildmap_layer_get ("hospitals");
   BuildMapLayerAirport   = buildmap_layer_get ("airports");
   BuildMapLayerStation   = buildmap_layer_get ("stations");
   BuildMapLayerMall      = buildmap_layer_get ("malls");
   BuildMapLayerSchool    = buildmap_layer_get ("schools");

   BuildMapLayerShoreline = buildmap_layer_get ("shore");
   BuildMapLayerRiver     = buildmap_layer_get ("rivers");
   BuildMapLayerCanal     = buildmap_layer_get ("canals");
   BuildMapLayerLake      = buildmap_layer_get ("lakes");
   BuildMapLayerSea       = buildmap_layer_get ("sea");

   BuildMapLayerNature    = buildmap_layer_get ("nature");
   BuildMapLayerPeak	  = buildmap_layer_get ("peak");
   BuildMapLayerAmenity   = buildmap_layer_get ("amenity");
   BuildMapLayerBoundary = buildmap_layer_get ("boundaries");

   BuildMapLayerCity      = buildmap_layer_get ("city");
   BuildMapLayerTown      = buildmap_layer_get ("town");
   BuildMapLayerVillage   = buildmap_layer_get ("village");
   BuildMapLayerHamlet    = buildmap_layer_get ("hamlet");
   BuildMapLayerSuburbs   = buildmap_layer_get ("suburbs");

   BuildMapLayerFuel      = buildmap_layer_get ("fuel");
   BuildMapLayerFood      = buildmap_layer_get ("food");
   BuildMapLayerCafe      = buildmap_layer_get ("cafe");
   BuildMapLayerDrinks    = buildmap_layer_get ("drinks");
   BuildMapLayerATM       = buildmap_layer_get ("atm");
}

char *stringtype[] = {
        "name",
        "name_left",
        "name_right",
        "name_other",
        "int_name",
        "nat_name",
        "reg_name",
        "loc_name",
        "old_name",
        "ref",
        "int_ref",
        "nat_ref",
        "reg_ref",
        "loc_ref",
        "old_ref",
        "ncn_ref",
        "rcn_ref",
        "lcn_ref",
        "icao",
        "iata",
        "place_name",
        "place_numbers",
        "postal_code",
        "is_in",
        "note",
        "description",
        "image",
        "source",
        "source_ref",
        "created_by",
};

char *datetype[] = {
        "date_on",
        "date_off",
        "start_date",
        "end_date",
};

#if NEEDED
char *numeric_type[] = {
        "lanes",
        "layer",
        "ele",
        "width",
        "est_width",
        "maxwidth",
        "maxlength",
        "maxspeed",
        "minspeed",
        "day_on",
        "day_off",
        "hour_on",
        "hour_off",
        "maxweight",
        "maxheight",
};
#endif

layer_info_t highway_to_layer[] = {
        { 0,                    NULL,           0 },
        { "motorway",           FREEWAY,        0 },            /* 1 */
        { "motorway_link",      FREEWAY,        0 },            /* 2 */
        { "trunk",              FREEWAY,        0 },            /* 3 */
        { "trunk_link",         FREEWAY,        0 },            /* 4 */
        { "primary",            MAIN,           0 },            /* 5 */
        { "primary_link",       MAIN,           0 },            /* 6 */
        { "secondary",          STREET,         0 },            /* 7 */
        { "tertiary",           STREET,         0 },            /* 8 */
        { "unclassified",       STREET,         0 },            /* 9 */
        { "minor",              STREET,         0 },            /* 10 */
        { "residential",        STREET,         0 },            /* 11 */
        { "service",            STREET,         0 },            /* 12 */
        { "track",              TRAIL,          0 },            /* 13 */
        { "cycleway",           TRAIL,          0 },            /* 14 */
        { "bridleway",          TRAIL,          0 },            /* 15 */
        { "footway",            TRAIL,          0 },            /* 16 */
        { "steps",              TRAIL,          0 },            /* 17 */
        { "pedestrian",         TRAIL,          0 },            /* 18 */
        { "pathway",            TRAIL,          0 },            /* 19 */
	{ "road",		STREET,		0 },		/* 20 */	/* New - ok ? */
	{ "secondary_link",	STREET,		0 },		/* 21 */	/* New - ok ? */
	{ "path",		TRAIL,		0 },		/* 22 */	/* New - ok ? */
        { 0,                    NULL,           0 },
};

layer_info_t cycleway_to_layer[] = {
        { 0,                    NULL,           0 },
        { "lane",               TRAIL,          0 },            /* 1 */
        { "track",              TRAIL,          0 },            /* 2 */
        { "opposite_lane",      TRAIL,          0 },            /* 3 */
        { "opposite_track",     TRAIL,          0 },            /* 4 */
        { "opposite",           NULL,           0 },            /* 5 */
        { 0,                    NULL,           0 },
};

layer_info_t waterway_to_layer[] = {
        { 0,                    NULL,           0 },
        { "river",              RIVER,          0 },            /* 1 */
        { "canal",              RIVER,          0 },            /* 2 */
	{ "stream",		RIVER,		0 },		/* 3 */
	{ "drain",		RIVER,		0 },		/* 4 */
	{ "dock",		RIVER,		0 },		/* 5 */
	{ "lock_gate",		RIVER,		0 },		/* 6 */
	{ "turning_point",	RIVER,		0 },		/* 7 */
	{ "aquaduct",		RIVER,		0 },		/* 8 */
	{ "boatyard",		RIVER,		0 },		/* 9 */
	{ "water_point",	RIVER,		0 },		/* 10 */
	{ "weir",		RIVER,		0 },		/* 11 */
	{ "dam",		RIVER,		0 },		/* 12 */
	{ "riverbank",		RIVER,		0 },		/* 13 */	/* New - ok ? */
	{ "ditch",		RIVER,		0 },		/* 14 */	/* New - ok ? */
        { 0,                    NULL,           0 },
};

layer_info_t abutters_to_layer[] = {
        { 0,                    NULL,           0 },
        { "residential",        NULL,           0 },            /* 1 */
        { "retail",             NULL,           0 },            /* 2 */
        { "industrial",         NULL,           0 },            /* 3 */
        { "commercial",         NULL,           0 },            /* 4 */
        { "mixed",              NULL,           0 },            /* 5 */
        { 0,                    NULL,           0 },
};

layer_info_t railway_to_layer[] = {
	{ 0,			NULL,           0 },
	{ "rail",		RAIL,           0 },            /* 1 */
	{ "tram",		RAIL,           0 },            /* 2 */
	{ "light_rail",		RAIL,           0 },            /* 3 */
	{ "subway",		RAIL,           0 },            /* 4 */
	{ "station",		NULL,           0 },            /* 5 */
	{ "preserved",		RAIL,		0 },            /* 6 */
	{ "disused",		RAIL,           0 },            /* 7 */
	{ "abandoned",		RAIL,		0 },		/* 8 */
	{ "narrow_gauge",	RAIL,		0 },            /* 9 */
	{ "monorail",		RAIL,           0 },            /* 10 */
	{ "halt",		RAIL,           0 },            /* 11 */
	{ "tram_stop",		RAIL,           0 },            /* 12 */
	{ "viaduct",		RAIL,           0 },            /* 13 */
	{ "crossing",		RAIL,           0 },            /* 14 */
	{ "level_crossing",	RAIL,           0 },            /* 15 */
	{ "subway_entrance",	RAIL,           0 },            /* 16 */
	{ "turntable",		RAIL,           0 },            /* 17 */
	{ 0,			NULL,           0 },
};

layer_info_t natural_to_layer[] = {
        { 0,                    NULL,           0 },
        { "coastline",          SHORELINE,      0 },            /* 1 */
        { "water",              LAKE,           AREA },         /* 2 */
        { "wood",               NATURE,         AREA },         /* 3 */
        { "peak",               PEAK,           PLACE },        /* 4 */
        { "land",		NULL,		AREA },		/* 5 */
        { "bay",		NULL,		AREA },		/* 6 */
        { "beach",		NULL,		AREA },		/* 7 */
        { "cave_entrance",	NULL,		AREA },		/* 8 */
        { "cliff",		NULL,		AREA },		/* 9 */
        { "fell",		NULL,		AREA },		/* 10 */
        { "glacier",		NULL,		AREA },		/* 11 */
        { "heath",		NULL,		AREA },		/* 12 */
        { "marsh",		NULL,		AREA },		/* 13 */
        { "mud",		NULL,		AREA },		/* 14 */
        { "sand",		NULL,		AREA },		/* 15 */
        { "scree",		NULL,		AREA },		/* 16 */
        { "scrub",		NULL,		AREA },		/* 17 */
        { "sprint",		NULL,		AREA },		/* 18 */
        { "stone",		NULL,		AREA },		/* 19 */
        { "tree",		NULL,		AREA },		/* 20 */
        { "volcano",		PEAK,		PLACE },	/* 21 */
        { "wetland",		NULL,		AREA },		/* 22 */
        { 0,                    NULL,           0 },
};

layer_info_t boundary_to_layer[] = {
        { 0,                    NULL,           0 },
        { "administrative",     NULL,           0 },         	/* 1 */
        { "civil",              NULL,           AREA },         /* 2 */
        { "political",          NULL,           0 },            /* 3 */
        { "national_park",      NULL,           AREA },         /* 4 */
	{ "world_country",	NULL,		0 },		/* 5 */
        { 0,                    NULL,           0 },
};

layer_info_t amenity_to_layer[] = {
        { 0,                    NULL,           0 },
        { "hospital",           HOSPITAL,       0 },            /* 1 */
        { "pub",                DRINKS,         PLACE },        /* 2 */
        { "parking",            AMENITY,        AREA },         /* 3 */
        { "post_office",        AMENITY,        0 },            /* 4 */
        { "fuel",               FUEL,           PLACE },        /* 5 */
        { "telephone",          NULL,           0 },            /* 6 */
        { "toilets",            NULL,           0 },            /* 7 */
        { "post_box",           NULL,           0 },            /* 8 */
        { "school",             SCHOOL,        AREA },         /* 9 */
        { "supermarket",        AMENITY,        0 },            /* 10 */
        { "library",            AMENITY,        0 },            /* 11 */
        { "theatre",            NULL,           0 },            /* 12 */
        { "cinema",             NULL,           0 },            /* 13 */
        { "police",             AMENITY,	0 },            /* 14 */
        { "fire_station",       AMENITY,	0 },            /* 15 */
        { "restaurant",         FOOD,           PLACE },        /* 16 */
        { "fast_food",          FOOD,           PLACE },        /* 17 */	/* Changed */
        { "bus_station",        NULL,           0 },            /* 18 */
        { "place_of_worship",   AMENITY,	0 },            /* 19 */
        { "cafe",               CAFE,           PLACE },        /* 20 */
        { "bicycle_parking",    AMENITY,        AREA },         /* 21 */
        { "public_building",    AMENITY,        AREA },         /* 22 */
        { "grave_yard",         PARK,           AREA },         /* 23 */
        { "university",         SCHOOL,        AREA },         /* 24 */
        { "college",            SCHOOL,        AREA },         /* 25 */
        { "townhall",           AMENITY,        AREA },         /* 26 */
        { "food_court",         FOOD,           PLACE },        /* 27 */
        { "drinking_water",     NULL,           0 },            /* 28 */
        { "bbq",                FOOD,           PLACE },        /* 28 */
        { "bar",                DRINKS,         PLACE },        /* 29 */
        { "biergarten",         DRINKS,         PLACE },        /* 30 */
        { "ice_cream",          NULL,           0 },            /* 31 */
        { "kindergarten",       NULL,           0 },            /* 32 */
        { "ice_cream",          NULL,           0 },            /* 33 */
        { "ferry_terminal",     AMENITY,	AREA },         /* 34 */
        { "bicycle_rental",     NULL,           0 },            /* 35 */
        { "car_rental",         NULL,           0 },            /* 36 */
        { "car_sharing",        AMENITY,        AREA },         /* 37 */
        { "car_wash",           NULL,           0 },            /* 38 */
        { "grit_bin",           NULL,           0 },            /* 39 */
        { "taxi",               AMENITY,        AREA },         /* 40 */
        { "atm",                ATM,            PLACE },        /* 41 */
        { "bank",               NULL,           0 },            /* 42 */
        { "bureau_de_change",   NULL,           0 },            /* 43 */
        { "pharmacy",           NULL,           0 },            /* 44 */
        { "baby_hatch",         NULL,           0 },            /* 45 */
        { "dentist",            NULL,           0 },            /* 46 */
        { "doctor",             NULL,           0 },            /* 47 */
        { "social_facility",    NULL,           0 },            /* 48 */
        { "veterinary",         NULL,           0 },            /* 49 */
        { "architect_office",   NULL,           0 },            /* 50 */
        { "arts_centre",        AMENITY,	AREA },         /* 51 */
        { "community_centre",   AMENITY,	AREA },         /* 52 */
        { "social_centre",      AMENITY,	AREA },         /* 53 */
        { "fountain",           NULL,           0 },            /* 51 */
        { "nightclub",          NULL,           0 },            /* 51 */
        { "stripclub",          NULL,           0 },            /* 51 */
        { "studio",             NULL,           0 },            /* 54 */
        { "bench",              NULL,           0 },            /* 55 */
        { "brothel",            NULL,           0 },            /* 56 */
        { "clock",              NULL,           0 },            /* 57 */
        { "courthouse",         AMENITY,        0 },            /* 58 */
        { "crematorium",        AMENITY,        0 },            /* 59 */
        { "embassy",            AMENITY,        0 },            /* 60 */
        { "hunting_stand",      NULL,           0 },            /* 61 */
        { "marketplace",        AMENITY,        AREA },         /* 62 */
        { "prison",             AMENITY,        AREA },         /* 63 */
        { "recycling",          NULL,           0 },            /* 64 */
        { "sauna",              NULL,           0 },            /* 65 */
        { "shelter",            AMENITY,        AREA },         /* 66 */
        { "vending_machine",    NULL,           0 },            /* 67 */
        { "waste_basket",       NULL,           0 },            /* 68 */
        { "waste_disposal",     NULL,           0 },            /* 69 */
        { "watering_place",     NULL,           0 },            /* 70 */
        { "bicycle parking",    AMENITY,        AREA },         /* 71 */
        { "public building",    AMENITY,        AREA },         /* 72 */
        { 0,                    NULL,           0 },
};

layer_info_t place_to_layer[] = {
        { 0,                    NULL,           0 },
        { "continent",          NULL,           0 },         /* 1 */
        { "country",            NULL,           0 },         /* 2 */
        { "state",              NULL,           0 },         /* 3 */
        { "region",             NULL,           0 },         /* 4 */
        { "county",             NULL,           0 },         /* 5 */
        { "city",               CITY,           PLACE },     /* 6 */
        { "town",               TOWN,           PLACE },     /* 7 */
        { "village",            VILLAGE,        PLACE },     /* 8 */
        { "hamlet",             HAMLET,         PLACE },     /* 9 */
        { "suburb",             SUBURBS,        PLACE },     /* 10 */
        { 0,                    NULL,           0 },
};

layer_info_t leisure_to_layer[] = {
        { 0,                    NULL,           0 },
        { "park",               PARK,           AREA },         /* 1 */
        { "common",             PARK,           AREA },         /* 2 */
        { "garden",             PARK,           AREA },         /* 3 */
        { "nature_reserve",     PARK,           AREA },         /* 4 */
        { "fishing",            NULL,           AREA },         /* 5 */
        { "slipway",            NULL,           0 },            /* 6 */
        { "water_park",         NULL,           AREA },         /* 7 */
        { "pitch",              AMENITY,        AREA },         /* 8 */
        { "track",              AMENITY,        AREA },         /* 9 */
        { "marina",             AMENITY,        AREA },         /* 10 */
        { "stadium",            AMENITY,        AREA },         /* 11 */
        { "golf_course",        PARK,           AREA },         /* 12 */
        { "sports_centre",      AMENITY,        AREA },         /* 13 */
        { "sports centre",      AMENITY,        AREA },         /* 14 */
        { "dog_park",           PARK,           AREA },         /* 15 */
        { "playground",         PARK,           AREA },         /* 16 */
        { "ice_rink",           AMENITY,        AREA },         /* 17 */
        { "miniature_golf",     AMENITY,        AREA },         /* 18 */
        { "dance",              AMENITY,        AREA },         /* 19 */
        { "swimming_pool",      AMENITY,        AREA },         /* 20 */
        { "golf course",        PARK,           AREA },         /* 21 */
        { 0,                    NULL,           0 },
};

layer_info_t historic_to_layer[] = {
        { 0,                    NULL,           0 },
        { "castle",             NULL,           0 },            /* 1 */
        { "monument",           NULL,           0 },            /* 2 */
        { "museum",             NULL,           0 },            /* 3 */
        { "archaeological_site",NULL,           AREA },         /* 4 */
        { "icon",               NULL,           0 },            /* 5 */
        { "ruins",              NULL,           AREA },         /* 6 */
        { 0,                    NULL,           0 },
};

#if NEEDED
char *oneway_type[] = {
        0,
        "no",                   /* 0 */
        "yes",                  /* 1 */
        "-1"                    /* 2 */
};
#endif

layer_info_t office_to_layer[] = {
        { 0,                    NULL,           0 },
	{ "accountant",		NULL,		0 },
	{ "architect",		NULL,		0 },
	{ "company",		NULL,		0 },
	{ "employment_agency",	NULL,		0 },
	{ "estate_agent",	NULL,		0 },
	{ "government",		NULL,		0 },
	{ "insurance",		NULL,		0 },
	{ "it",			NULL,		0 },
	{ "lawyer",		NULL,		0 },
	{ "newspaper",		NULL,		0 },
	{ "ngo",		NULL,		0 },
	{ "quango",		NULL,		0 },
	{ "research",		NULL,		0 },
	{ "telecommunication",	NULL,		0 },
	{ "travel_agent",	NULL,		0 },
	{ 0,			NULL,		0 },
};

layer_info_t barrier_to_layer[] = {
	{ 0,			NULL,		0 },
	{ "hedge",		NULL,		0 },
	{ "fence",		NULL,		0 },
	{ "wall",		NULL,		0 },
	{ "ditch",		NULL,		0 },
	{ "retaining_wall",	NULL,		0 },
	{ "city_wall",		NULL,		0 },
	{ "bollard",		NULL,		0 },
	{ "cycle_barrier",	NULL,		0 },
	{ "block",		NULL,		0 },
	{ "cattle_grid",	NULL,		0 },
	{ "toll_booth",		NULL,		0 },
	{ "entrance",		NULL,		0 },
	{ "gate",		NULL,		0 },
	{ "lift_gate",		NULL,		0 },
	{ "stile",		NULL,		0 },
	{ "horse_stile",	NULL,		0 },
	{ "kissing_gate",	NULL,		0 },
	{ "sally_port",		NULL,		0 },
	{ "turnstile",		NULL,		0 },
	{ "kent_carriage_gap",	NULL,		0 },
	{ 0,			NULL,		0 },
};

layer_info_t craft_to_layer[] = {
	{ 0,				NULL,           0 },
	{ "agricultural_engines",	NULL,		0 },
	{ "basket_maker",		NULL,		0 },
	{ "beekeeper",			NULL,		0 },
	{ "blacksmith",			NULL,		0 },
	{ "boatbuilder",		NULL,		0 },
	{ "carpenter",			NULL,		0 },
	{ "carpet_layer",		NULL,		0 },
	{ "caterer",			NULL,		0 },
	{ "clockmaker",			NULL,		0 },
	{ "confectionery",		NULL,		0 },
	{ "electrician",		NULL,		0 },
	{ "gardener",			NULL,		0 },
	{ "handicraft",			NULL,		0 },
	{ "hvac",			NULL,		0 },
	{ "jeweller",			NULL,		0 },
	{ "locksmith",			NULL,		0 },
	{ "metal_construction",		NULL,		0 },
	{ "optician",			NULL,		0 },
	{ "painter",			NULL,		0 },
	{ "photographer",		NULL,		0 },
	{ "photographic_laboratory",	NULL,		0 },
	{ "plasterer",			NULL,		0 },
	{ "plumber",			NULL,		0 },
	{ "pottery",			NULL,		0 },
	{ "roofer",			NULL,		0 },
	{ "shoemaker",			NULL,		0 },
	{ "scaffolder",			NULL,		0 },
	{ "stonemason",			NULL,		0 },
	{ "sweep",			NULL,		0 },
	{ "tailor",			NULL,		0 },
	{ "tiler",			NULL,		0 },
	{ "sailmaker",			NULL,		0 },
	{ "rigger",			NULL,		0 },
	{ "saddler",			NULL,		0 },
	{ "sculptor",			NULL,		0 },
	{ "upholsterer",		NULL,		0 },
	{ 0,				NULL,		0 },
};

layer_info_t emergency_to_layer[] = {
        { 0,                    NULL,           0 },
	{ "ambulance_station",	NULL,		AREA },
	{ "fire_extinguisher",	NULL,		0 },
	{ "fire_flapper",	NULL,		0 },
	{ "fire_hose",		NULL,		0 },
	{ "fire_hydrant",	NULL,		0 },
	{ "phone",		NULL,		0 },
	{ "ses_station",	NULL,		0 },
	{ "siren",		NULL,		0 },
	{ 0,			NULL,		0 },
};

layer_info_t geological_to_layer[] = {
        { 0,				NULL,           0 },
	{ "palaeontological_site",	NULL,		0 },
	{ 0,				NULL,		0 },
};

layer_info_sublist_t list_info[] = {
        {0,                     NULL,                 NULL },
        {"highway",             highway_to_layer,     NULL },
        {"cycleway",            cycleway_to_layer,    NULL },
        {"tracktype",           NULL,                 TRAIL },
        {"waterway",            waterway_to_layer,    RIVER },
        {"railway",             railway_to_layer,     NULL },
        {"aeroway",             NULL,                 NULL },
        {"aerialway",           NULL,                 NULL },
        {"power",               NULL,                 NULL },
        {"man_made",            NULL,                 NULL },
        {"leisure",             leisure_to_layer,     NULL },
        {"amenity",             amenity_to_layer,     NULL },
        {"shop",                NULL,                 NULL },
        {"tourism",             NULL,                 NULL },
        {"historic",            historic_to_layer,    NULL },
        {"landuse",             NULL,                 NULL },
        {"military",            NULL,                 NULL },
        {"natural",             natural_to_layer,     NULL },
        {"route",               NULL,                 NULL },
        {"boundary",            boundary_to_layer,    NULL },
        {"sport",               NULL,                 NULL },
        {"abutters",            abutters_to_layer,    NULL },
        {"fenced",              NULL,                 NULL },
        {"lit",                 NULL,                 NULL },
        {"area",                NULL,                 NULL },
        {"bridge",              NULL,                 MAIN },
        {"cutting",             NULL,                 NULL },
        {"embankment",          NULL,                 NULL },
        {"surface",             NULL,                 NULL },
        {"access",              NULL,                 NULL },
        {"bicycle",             NULL,                 NULL },
        {"foot",                NULL,                 NULL },
        {"goods",               NULL,                 NULL },
        {"hgv",                 NULL,                 NULL },
        {"horse",               NULL,                 NULL },
        {"motorcycle",          NULL,                 NULL },
        {"motorcar",            NULL,                 NULL },
        {"psv",                 NULL,                 NULL },
        {"motorboat",           NULL,                 NULL },
        {"boat",                NULL,                 NULL },
        {"oneway",              NULL,                 NULL },
        {"noexit",              NULL,                 NULL },
        {"toll",                NULL,                 NULL },
        {"place",               place_to_layer,       NULL },
        {"lock",                NULL,                 NULL },
        {"attraction",          NULL,                 NULL },
        {"wheelchair",          NULL,                 NULL },
        {"junction",            NULL,                 NULL },
	{"office",		office_to_layer,	NULL },
	{"barrier",		barrier_to_layer,	NULL },
	{"craft",		craft_to_layer,		NULL },
	{"emergency",		emergency_to_layer,	NULL },
	{"geological",		geological_to_layer,	NULL },
        { 0,                    NULL,           0 },
};

