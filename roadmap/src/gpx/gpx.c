/*
    Access GPX data files.

    Copyright (C) 2002, 2003, 2004, 2005 Robert Lipe, robertlipe@usa.net

    Modified for use with RoadMap, Paul Fox, 2005

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111 USA

 */

#ifdef ROADMAP_USES_EXPAT

#include "defs.h"

int roadmap_math_from_floatstring(const char *f, int fracdigits);
char *roadmap_math_to_floatstring(char *buf, int value, int fracdigits);
#define HUNDREDTHS  2
#define THOUSANDTHS 3
#define MILLIONTHS  6

#include <expat.h>

static XML_Parser psr;

static xml_tag *cur_tag;
static vmem_t cdatastr;
#if ROADMAP_UNNEEDED
static char *opt_logpoint = NULL; // "Create waypoints from geocache log entries"
#endif
static int logpoint_ct = 0;

static int gpx_tag_found;
static const char *gpx_version;
static char *gpx_wversion = "1.0";   // "Target GPX version for output"
static int gpx_wversion_num;
static const char *gpx_creator;
static char *xsi_schema_loc = NULL;

static int reading_weepoints;

static int gpx_synthesize_shortnames = 0;

static char *gpx_charset_name = "US-ASCII";



static char *gpx_email = NULL;
static char *gpx_author = NULL;
static vmem_t current_tag;

static waypoint *wpt_tmp;
static route_head *trk_tmp;
static route_head *rte_tmp;

#if ROADMAP_UNNEEDED
static int cache_descr_is_html;
#endif
static FILE *cb_file;  /* shared between gpx_write and callback functions */
                     /* note:  the name "ofd" is used in all
                      * lower-level output routines (called from
                      * the callback functions) as the name of
                      * the passed-in FILE *.  the name was
                      * formerly a global, and was kept to reduce
                      * diff size.  */
static short_handle mkshort_handle;

static format_specific_data **fs_ptr;

static time_t file_time;

static char *snlen = "32";   //  "Length of generated shortnames"
static char *suppresswhite = NULL; // "Suppress whitespace in generated shortnames"
#if ROADMAP_UNNEEDED
static char *urlbase = NULL; //  "Base URL for link tag in output"
#endif

static queue_head *cur_waypoint_list;
static queue_head *cur_route_list;
static queue_head *cur_track_list;

static int doing_no_waypoints, doing_no_routes, doing_no_tracks;

/* used for bounds calculation on output */
static bounds all_bounds;


#define MYNAME "GPX"
#define MY_CBUF_SZ 4096
#define DEFAULT_XSI_SCHEMA_LOC "http://www.topografix.com/GPX/1/0 http://www.topografix.com/GPX/1/0/gpx.xsd"
#define DEFAULT_XSI_SCHEMA_LOC_FMT "\"http://www.topografix.com/GPX/%c/%c http://www.topografix.com/GPX/%c/%c/gpx.xsd\""
#ifndef CREATOR_NAME_URL
#  define CREATOR_NAME_URL "RoadMap - http://roadmap.digitalomaha.net (xml based on GPSBabel - http://www.gpsbabel.org)" 
#endif


typedef enum {
        tt_unknown = 0,
        tt_gpx,
        tt_author,
        tt_desc,
        tt_email,
        tt_time,
        tt_wpt,
        tt_wpt_cmt,
        tt_wpt_desc,
        tt_wpt_name,
#if ROADMAP_UNNEEDED
        tt_wpt_sym,
        tt_wpt_url,
#endif
        tt_wpt_ele,
        tt_wpt_time,
        tt_wpt_type,
#if ROADMAP_UNNEEDED
        tt_wpt_urlname,
        tt_wpt_link,            /* New in GPX 1.1 */
        tt_wpt_link_text,       /* New in GPX 1.1 */
#endif
#if ROADMAP_UNNEEDED
        tt_pdop,                /* PDOPS are common for all three */
        tt_hdop,                /* PDOPS are common for all three */
        tt_vdop,                /* PDOPS are common for all three */
        tt_fix,
        tt_sat,
#endif
#if ROADMAP_UNNEEDED
        tt_cache,
        tt_cache_name,
        tt_cache_container,
        tt_cache_type,
        tt_cache_difficulty,
        tt_cache_terrain,
        tt_cache_hint,
        tt_cache_desc_short,
        tt_cache_desc_long,
        tt_cache_log_wpt,
        tt_cache_log_type,
        tt_cache_log_date,
        tt_cache_placer,
#endif
        tt_rte,
        tt_rte_name,
        tt_rte_desc,
        tt_rte_cmt,
        tt_rte_number,
        tt_rte_rtept,
        tt_rte_rtept_ele,
        tt_rte_rtept_name,
        tt_rte_rtept_desc,
#if ROADMAP_UNNEEDED
        tt_rte_rtept_sym,
#endif
        tt_rte_rtept_time,
        tt_rte_rtept_cmt,
#if ROADMAP_UNNEEDED
        tt_rte_rtept_url,
        tt_rte_rtept_urlname,
#endif
        tt_trk,
        tt_trk_desc,
        tt_trk_name,
        tt_trk_trkseg,
        tt_trk_number,
        tt_trk_trkseg_trkpt,
        tt_trk_trkseg_trkpt_cmt,
        tt_trk_trkseg_trkpt_name,
#if ROADMAP_UNNEEDED
        tt_trk_trkseg_trkpt_sym,
        tt_trk_trkseg_trkpt_url,
        tt_trk_trkseg_trkpt_urlname,
#endif
        tt_trk_trkseg_trkpt_desc,
        tt_trk_trkseg_trkpt_ele,
        tt_trk_trkseg_trkpt_time,
        tt_trk_trkseg_trkpt_course,     
        tt_trk_trkseg_trkpt_speed,
} tag_type;

typedef struct tag_mapping {
        tag_type tag_type;              /* enum from above for this tag */
        int tag_passthrough;            /* true if we don't generate this */
        const char *tag_name;           /* xpath-ish tag name */
} tag_mapping;

/*
 * xpath(ish) mappings between full tag paths and internal identifers.
 * These appear in the order they appear in the GPX specification.
 * If it's not a tag we explictly handle, it doesn't go here.
 */

static tag_mapping tag_path_map[] = {
        { tt_gpx, 0, "/gpx" },
        { tt_time, 0, "/gpx/time" },
        { tt_author, 0, "/gpx/author" },
        { tt_email, 0, "/gpx/email" },
        { tt_time, 0, "/gpx/time" },
        { tt_desc, 0, "/gpx/desc" },

        { tt_wpt, 0, "/gpx/wpt" },
        { tt_wpt_ele, 0, "/gpx/wpt/ele" },
        { tt_wpt_time, 0, "/gpx/wpt/time" },
        { tt_wpt_name, 0, "/gpx/wpt/name" },
        { tt_wpt_cmt, 0, "/gpx/wpt/cmt" },
        { tt_wpt_desc, 0, "/gpx/wpt/desc" },
#if ROADMAP_UNNEEDED
        { tt_wpt_url, 0, "/gpx/wpt/url" },
        { tt_wpt_urlname, 0, "/gpx/wpt/urlname" },
        { tt_wpt_link, 0, "/gpx/wpt/link" },                    /* GPX 1.1 */
        { tt_wpt_link_text, 0, "/gpx/wpt/link/text" },          /* GPX 1.1 */
        { tt_wpt_sym, 0, "/gpx/wpt/sym" },
#endif
        { tt_wpt_type, 1, "/gpx/wpt/type" },
#if ROADMAP_UNNEEDED
        { tt_cache, 1, "/gpx/wpt/groundspeak:cache" },
        { tt_cache_name, 1, "/gpx/wpt/groundspeak:cache/groundspeak:name" },
        { tt_cache_container, 1, "/gpx/wpt/groundspeak:cache/groundspeak:container" },
        { tt_cache_type, 1, "/gpx/wpt/groundspeak:cache/groundspeak:type" },
        { tt_cache_difficulty, 1, "/gpx/wpt/groundspeak:cache/groundspeak:difficulty" },
        { tt_cache_terrain, 1, "/gpx/wpt/groundspeak:cache/groundspeak:terrain" },
        { tt_cache_hint, 1, "/gpx/wpt/groundspeak:cache/groundspeak:encoded_hints" },
        { tt_cache_desc_short, 1, "/gpx/wpt/groundspeak:cache/groundspeak:short_description" },
        { tt_cache_desc_long, 1, "/gpx/wpt/groundspeak:cache/groundspeak:long_description" },
        { tt_cache_log_wpt, 1, "/gpx/wpt/groundspeak:cache/groundspeak:logs/groundspeak:log/groundspeak:log_wpt" },
        { tt_cache_log_type, 1, "/gpx/wpt/groundspeak:cache/groundspeak:logs/groundspeak:log/groundspeak:type" },
        { tt_cache_log_date, 1, "/gpx/wpt/groundspeak:cache/groundspeak:logs/groundspeak:log/groundspeak:date" },
        { tt_cache_placer, 1, "/gpx/wpt/groundspeak:cache/groundspeak:owner" },
#endif

        { tt_rte, 0, "/gpx/rte" },
        { tt_rte_name, 0, "/gpx/rte/name" },
        { tt_rte_desc, 0, "/gpx/rte/desc" },
        { tt_rte_number, 0, "/gpx/rte/number" },
        { tt_rte_rtept, 0, "/gpx/rte/rtept" },
        { tt_rte_rtept_ele, 0, "/gpx/rte/rtept/ele" },
        { tt_rte_rtept_time, 0, "/gpx/rte/rtept/time" },
        { tt_rte_rtept_name, 0, "/gpx/rte/rtept/name" },
        { tt_rte_rtept_cmt, 0, "/gpx/rte/rtept/cmt" },
        { tt_rte_rtept_desc, 0, "/gpx/rte/rtept/desc" },
#if ROADMAP_UNNEEDED
        { tt_rte_rtept_url, 0, "/gpx/rte/rtept/url" },
        { tt_rte_rtept_urlname, 0, "/gpx/rte/rtept/urlname" },
        { tt_rte_rtept_sym, 0, "/gpx/rte/rtept/sym" },
#endif

        { tt_trk, 0, "/gpx/trk" },
        { tt_trk_name, 0, "/gpx/trk/name" },
        { tt_trk_desc, 0, "/gpx/trk/desc" },
        { tt_trk_trkseg, 0, "/gpx/trk/trkseg" },
        { tt_trk_number, 0, "/gpx/trk/number" },
        { tt_trk_trkseg_trkpt, 0, "/gpx/trk/trkseg/trkpt" },
        { tt_trk_trkseg_trkpt_ele, 0, "/gpx/trk/trkseg/trkpt/ele" },
        { tt_trk_trkseg_trkpt_time, 0, "/gpx/trk/trkseg/trkpt/time" },
        { tt_trk_trkseg_trkpt_name, 0, "/gpx/trk/trkseg/trkpt/name" },
        { tt_trk_trkseg_trkpt_cmt, 0, "/gpx/trk/trkseg/trkpt/cmt" },
        { tt_trk_trkseg_trkpt_desc, 0, "/gpx/trk/trkseg/trkpt/desc" },
#if ROADMAP_UNNEEDED
        { tt_trk_trkseg_trkpt_url, 0, "/gpx/trk/trkseg/trkpt/url" },
        { tt_trk_trkseg_trkpt_urlname, 0, "/gpx/trk/trkseg/trkpt/urlname" },
        { tt_trk_trkseg_trkpt_sym, 0, "/gpx/trk/trkseg/trkpt/sym" },
#endif
        { tt_trk_trkseg_trkpt_course, 0, "/gpx/trk/trkseg/trkpt/course" },
        { tt_trk_trkseg_trkpt_speed, 0, "/gpx/trk/trkseg/trkpt/speed" },

#if ROADMAP_UNNEEDED
        /* Common to tracks, routes, and waypts */
        { tt_fix,  0, "/gpx/wpt/fix" },
        { tt_fix,  0, "/gpx/trk/trkseg/trkpt/fix" },
        { tt_fix,  0, "/gpx/rte/rtept/fix" },
        { tt_sat,  0, "/gpx/wpt/sat" },
        { tt_sat,  0, "/gpx/trk/trkseg/trkpt/sat" },
        { tt_sat,  0, "/gpx/rte/rtept/sat" },
        { tt_pdop, 0, "/gpx/wpt/pdop" },
        { tt_pdop, 0, "/gpx/trk/trkseg/trkpt/pdop" },
        { tt_pdop, 0, "/gpx/rte/rtept/pdop" },
        { tt_hdop, 0, "/gpx/wpt/hdop" },
        { tt_hdop, 0, "/gpx/trk/trkseg/trkpt/hdop" },
        { tt_hdop, 0, "/gpx/rte/rtept/hdop" },
        { tt_vdop, 0, "/gpx/wpt/vdop" },
        { tt_vdop, 0, "/gpx/trk/trkseg/trkpt/vdop" },
        { tt_vdop, 0, "/gpx/rte/rtept/hdop" },
#endif
        { 0, 0, 0 }
};

static tag_type
get_tag(const char *tp, int *passthrough)
{
        tag_mapping *tm;
        for (tm = tag_path_map; tm->tag_type != 0; tm++) {
                if (0 == strcmp(tm->tag_name, tp)) {
                        *passthrough = tm->tag_passthrough;
                        return tm->tag_type;
                }
        }
        *passthrough = 1;
        return tt_unknown;
}

static void
tag_gpx(const char **attrv)
{
        const char **avp = &attrv[0];
        while (*avp) {
                if (strcmp(avp[0], "version") == 0) {
                        gpx_version = avp[1];
                }
                else if (strcmp(avp[0], "src") == 0) {
                        gpx_creator = avp[1];
                }
                else if (strcmp(avp[0], "xsi:schemaLocation") == 0) {
                        if (0 == strstr(xsi_schema_loc, avp[1])) {
                            xsi_schema_loc = xstrappend(xsi_schema_loc, " ");
                            xsi_schema_loc = xstrappend(xsi_schema_loc, avp[1]);
                        }
                }
                avp+=2;
        }
        gpx_tag_found = 1;
}

static void
tag_wpt(const char **attrv)
{
        const char **avp = &attrv[0];

        wpt_tmp = waypt_new();

        cur_tag = NULL;
        while (*avp) { 
                if (strcmp(avp[0], "lat") == 0) {
                        wpt_tmp->pos.latitude =
			    roadmap_math_from_floatstring (avp[1], MILLIONTHS);

                }
                else if (strcmp(avp[0], "lon") == 0) {
                        wpt_tmp->pos.longitude =
			    roadmap_math_from_floatstring(avp[1], MILLIONTHS);
                }
                avp+=2;
        }
        fs_ptr = &wpt_tmp->fs;
}

#if ROADMAP_UNNEEDED
static void
tag_cache_desc(const char ** attrv)
{
        const char **avp;

        cache_descr_is_html = 0;
        for (avp = &attrv[0]; *avp; avp+=2) {
                if (strcmp(avp[0], "html") == 0) {
                        if (strcmp(avp[1], "True") == 0) {
                                cache_descr_is_html = 1;
                        }
                }
        }
}

static void
tag_gs_cache(const char **attrv)
{
        const char **avp;

        cache_descr_is_html = 0;
        for (avp = &attrv[0]; *avp; avp+=2) {
                if (strcmp(avp[0], "id") == 0) {
                                wpt_tmp->gc_data.id = atoi(avp[1]);
                }
        }
}
#endif // ROADMAP_UNNEEDED

static void
start_something_else(const char *el, const char **attrv)
{
        const char **avp = attrv;
        char **avcp = NULL;
        int attr_count = 0;
        xml_tag *new_tag;
        fs_xml *fs_gpx;
       
        if ( !fs_ptr ) {
                return;
        }
        
        new_tag = (xml_tag *)xcalloc(sizeof(xml_tag),1);
        new_tag->tagname = xstrdup(el);
        
        /* count attributes */
        while (*avp) {
                attr_count++;
                avp++;
        }
        
        /* copy attributes */
        avp = attrv;
        new_tag->attributes = (char **)xcalloc(sizeof(char *),attr_count+1);
        avcp = new_tag->attributes;
        while (*avp) {
                *avcp = xstrdup(*avp);
                avcp++;
                avp++;
        }
        *avcp = NULL;
        
        if ( cur_tag ) {
                if ( cur_tag->child ) {
                        cur_tag = cur_tag->child;
                        while ( cur_tag->sibling ) {
                                cur_tag = cur_tag->sibling;
                        }
                        cur_tag->sibling = new_tag;
                        new_tag->parent = cur_tag->parent;
                }
                else {
                        cur_tag->child = new_tag;
                        new_tag->parent = cur_tag;
                }
        }
        else {
                fs_gpx = (fs_xml *)fs_chain_find( *fs_ptr, FS_GPX );
                
                if ( fs_gpx && fs_gpx->tag ) {
                        cur_tag = fs_gpx->tag;
                        while ( cur_tag->sibling ) {
                                cur_tag = cur_tag->sibling;
                        }
                        cur_tag->sibling = new_tag;
                        new_tag->parent = NULL;
                }
                else {
                        fs_gpx = fs_xml_alloc(FS_GPX);
                        fs_gpx->tag = new_tag;
                        fs_chain_add( fs_ptr, (format_specific_data *)fs_gpx );
                        new_tag->parent = NULL;
                }
        }
        cur_tag = new_tag;
}

static void
end_something_else()
{
        if ( cur_tag ) {
                cur_tag = cur_tag->parent;
        }
}

#if ROADMAP_UNNEEDED
static void
tag_log_wpt(const char **attrv)
{
        waypoint * lwp_tmp;
        const char **avp = &attrv[0];

        /* create a new waypoint */
        lwp_tmp = waypt_new();

        /* extract the lat/lon attributes */
        while (*avp) { 
                if (strcmp(avp[0], "lat") == 0) {
                        lwp_tmp->pos.latitude =
			    roadmap_math_from_floatstring(avp[1], MILLIONTHS);
                }
                else if (strcmp(avp[0], "lon") == 0) {
                        lwp_tmp->pos.longitude =
			    roadmap_math_from_floatstring(avp[1], MILLIONTHS);
                }
                avp+=2;
        }
        /* Make a new shortname.  Since this is a groundspeak extension, 
          we assume that GCBLAH is the current shortname format and that
          wpt_tmp refers to the currently parsed waypoint. Unfortunatley,
          we need to keep track of log_wpt counts so we don't collide with
          dupe shortnames.
        */

        if ((wpt_tmp->shortname) && (strlen(wpt_tmp->shortname) > 2)) {
                /* copy of the shortname */
                lwp_tmp->shortname = xcalloc(7, 1);
                sprintf(lwp_tmp->shortname, "%-4.4s%02d", 
                        &wpt_tmp->shortname[2], logpoint_ct++);

                waypt_add(cur_waypoint_list, lwp_tmp);
        } 
}
#endif // ROADMAP_UNNEEDED

static void
gpx_start(void *data, const char *el, const char **attr)
{
        char *e;
        char *ep;
        int passthrough;

        vmem_realloc(&current_tag, strlen(current_tag.mem) + 2 + strlen(el));
        e = current_tag.mem;
        ep = e + strlen(e);
        *ep++ = '/';
        strcpy(ep, el);

        
        /*
         * FIXME: Find out why a cdatastr[0] doesn't adequately reset the       
         * cdata handler.
         */
        memset(cdatastr.mem, 0, cdatastr.size);

        switch (get_tag(current_tag.mem, &passthrough)) {
        case tt_gpx:
                tag_gpx(attr);
                break;
        case tt_wpt:
                tag_wpt(attr);
                break;
#if ROADMAP_UNNEEDED
        case tt_wpt_link:
                if (0 == strcmp(attr[0], "href")) {
                        wpt_tmp->url = xstrdup(attr[1]);
                }
                break;
#endif
        case tt_rte:
                rte_tmp = route_head_alloc();
                fs_ptr = &rte_tmp->fs;
                break;
        case tt_rte_rtept:
                tag_wpt(attr);
                break; 
        case tt_trk:
                trk_tmp = route_head_alloc();
                fs_ptr = &trk_tmp->fs;
                break;
        case tt_trk_trkseg_trkpt:
                tag_wpt(attr);
                break;
        case tt_unknown:
                start_something_else(el, attr);
                return;
#if ROADMAP_UNNEEDED
        case tt_cache:
                tag_gs_cache(attr);
                break;
        case tt_cache_log_wpt:
                if (opt_logpoint)
                        tag_log_wpt(attr);
                break;
        case tt_cache_desc_long:
        case tt_cache_desc_short:
                tag_cache_desc(attr);
                break;
#endif
        default:
                break;
        }
        if (passthrough) {
                start_something_else(el, attr);
        }
}

#if ROADMAP_UNNEEDED
static struct
gs_type_mapping{
        geocache_type type;
        const char *name;
} gs_type_map[] = {
        { gt_traditional, "Traditional Cache" },
        { gt_multi, "Multi-cache" },
        { gt_virtual, "Virtual Cache" },
        { gt_event, "Event Cache" },
        { gt_webcam, "Webcam Cache" },
        { gt_suprise, "Unknown Cache" },
        { gt_earth, "Earthcache" },
        { gt_cito, "Cache In Trash Out Event" },
        { gt_letterbox, "Letterbox Hybrid" },
        { gt_locationless, "Locationless (Reverse) Cache" },

        { gt_benchmark, "Benchmark" }, /* Not Groundspeak; for GSAK  */
};

static struct
gs_container_mapping{
        geocache_container type;
        const char *name;
} gs_container_map[] = {
        { gc_other, "Unknown" },
        { gc_micro, "Micro" },
        { gc_regular, "Regular" },
        { gc_large, "Large" },
        { gc_small, "Small" },
        { gc_virtual, "Virtual" }
};

static geocache_type
gs_mktype(const char *tp)
{
        int i;
        int sz = sizeof(gs_type_map) / sizeof(gs_type_map[0]);

        for (i = 0; i < sz; i++) {
                if (0 == strcasecmp(tp, gs_type_map[i].name)) {
                        return gs_type_map[i].type;
                }
        }
        return gt_unknown;
}

#if NEEDED  // inverse of gs_mktype() 
static const char *
gs_get_cachetype(geocache_type tgc)
{
        int i;
        int sz = sizeof(gs_type_map) / sizeof(gs_type_map[0]);

        for (i = 0; i < sz; i++) {
                if (tgc == gs_type_map[i].type) {
                        return gs_type_map[i].name;
                }
        }
        return "Unknown";
}
#endif

static geocache_container
gs_mkcont(const char *tp)
{
        int i;
        int sz = sizeof(gs_container_map) / sizeof(gs_container_map[0]);

        for (i = 0; i < sz; i++) {
                if (0 == strcasecmp(tp, gs_container_map[i].name)) {
                        return gs_container_map[i].type;
                }
        }
        return gc_unknown;
}
#endif // ROADMAP_UNNEEDED


time_t 
xml_parse_time( const char *cdatastr ) 
{
        int off_hr = 0;
        int off_min = 0;
        int off_sign = 1;
        char *offsetstr = NULL;
        char *pointstr = NULL;
        struct tm tm;
        time_t rv = 0;
        char *timestr = xstrdup( cdatastr );
        
        offsetstr = strchr( timestr, 'Z' );
        if ( offsetstr ) {
                /* zulu time; offsets stay at defaults */
                *offsetstr = '\0';
        } else {
                offsetstr = strchr( timestr, '+' );
                if ( offsetstr ) {
                        /* positive offset; parse it */
                        *offsetstr = '\0';
                        sscanf( offsetstr+1, "%d:%d", &off_hr, &off_min );
                } else {
                        offsetstr = strchr( timestr, 'T' );
                        if ( offsetstr ) {
                                offsetstr = strchr( offsetstr, '-' );
                                if ( offsetstr ) {
                                        /* negative offset; parse it */
                                        *offsetstr = '\0';
                                        sscanf( offsetstr+1, "%d:%d", 
                                                        &off_hr, &off_min );
                                        off_sign = -1;
                                }
                        }
                }
        }
        
        pointstr = strchr( timestr, '.' );
        if ( pointstr ) {
                *pointstr = '\0';
        }
        
        sscanf(timestr, "%d-%d-%dT%d:%d:%d", 
                &tm.tm_year,
                &tm.tm_mon,
                &tm.tm_mday,
                &tm.tm_hour,
                &tm.tm_min,
                &tm.tm_sec);
        tm.tm_mon -= 1;
        tm.tm_year -= 1900;
        tm.tm_isdst = 0;
        
        rv = mkgmtime(&tm) - off_sign*off_hr*3600 - off_sign*off_min*60;
        
        xfree(timestr);
        
        return rv;
}

static void
gpx_end(void *data, const char *el)
{
        char *s = strrchr(current_tag.mem, '/');
        char *cdatastrp = cdatastr.mem;
        int passthrough;
#if ROADMAP_UNNEEDED
        float x;
        static time_t gc_log_date;
#endif

        if (strcmp(s + 1, el)) {
                fprintf(stderr, "Mismatched tag %s\n", el);
        }

        switch (get_tag(current_tag.mem, &passthrough)) {
        /*
         * First, the tags that are file-global.
         */
        case tt_time:
                file_time = xml_parse_time(cdatastrp);
                break;
        case tt_email:
                if (gpx_email) xfree(gpx_email);
                gpx_email = xstrdup(cdatastrp);
                break;
        case tt_author:
                if (gpx_author) xfree(gpx_author);
                gpx_author = xstrdup(cdatastrp);
                break;
        case tt_gpx:
                /* Could invoke release code here */
                break;
        /*
         * Waypoint-specific tags.
         */
#if ROADMAP_UNNEEDED
        case tt_wpt_url:
                wpt_tmp->url = xstrdup(cdatastrp);
                break;
        case tt_wpt_urlname:
        case tt_wpt_link_text:
                wpt_tmp->url_link_text = xstrdup(cdatastrp);
                break;
#endif // ROADMAP_UNNEEDED
        case tt_wpt:
                if (doing_no_waypoints) {
                    waypt_free(wpt_tmp);
                } else if (reading_weepoints) {
                    /* copy the minimal info from waypoint to weepoint,
                     * and discard the temporary waypoint
                     */
                    weepoint *weept = weept_new();
                    weept->pos = wpt_tmp->pos;
                    if (wpt_tmp->shortname) {
                        weept->name = strdup(wpt_tmp->shortname);
                    }
                    weept_add(cur_waypoint_list, weept);
                    waypt_free(wpt_tmp);
                } else {
                    waypt_add(cur_waypoint_list, wpt_tmp);
                }
                logpoint_ct = 0;
                cur_tag = NULL;
                wpt_tmp = NULL;
                break;
#if ROADMAP_UNNEEDED
        case tt_cache_name:
                if (wpt_tmp->notes != NULL) xfree(wpt_tmp->notes);
                wpt_tmp->notes = xstrdup(cdatastrp);
                break;
        case tt_cache_container:
                wpt_tmp->gc_data.container = gs_mkcont(cdatastrp);
                break;
        case tt_cache_type:
                wpt_tmp->gc_data.type = gs_mktype(cdatastrp);
                break;
        case tt_cache_difficulty:
                sscanf(cdatastrp, "%f", &x);
                wpt_tmp->gc_data.diff = x * 10;
                break;
        case tt_cache_hint:
                rtrim(cdatastrp);
                if (cdatastrp[0]) {
                        wpt_tmp->gc_data.hint = xstrdup(cdatastrp);
                }
                break;
        case tt_cache_desc_long:
                rtrim(cdatastrp);
                if (cdatastrp[0]) {
                        wpt_tmp->gc_data.desc_long.is_html = cache_descr_is_html;
                        wpt_tmp->gc_data.desc_long.utfstring = xstrdup(cdatastrp);
                }
                break;
        case tt_cache_desc_short:
                rtrim(cdatastrp);
                if (cdatastrp[0]) {
                        wpt_tmp->gc_data.desc_short.is_html = cache_descr_is_html;
                        wpt_tmp->gc_data.desc_short.utfstring = xstrdup(cdatastrp);
                }
                break;
        case tt_cache_terrain:
                sscanf(cdatastrp, "%f", &x);
                wpt_tmp->gc_data.terr = x * 10;
                break;
        case tt_cache_placer:
                wpt_tmp->gc_data.placer = xstrdup(cdatastrp);
                break;
        case tt_cache_log_date:
                gc_log_date = xml_parse_time( cdatastrp );
                break;
        /*
         * "Found it" logs follow the date according to the schema,
         * if this is the first "found it" for this waypt, just use the
         * last date we saw in this log.
         */
        case tt_cache_log_type:
                if ((0 == strcmp(cdatastrp, "Found it")) && 
                    (0 == wpt_tmp->gc_data.last_found)) {
                        wpt_tmp->gc_data.last_found  = gc_log_date;
                }
                gc_log_date = 0;
                break;
#endif // ROADMAP_UNNEEDED
        /*
         * Route-specific tags.
         */
        case tt_rte_name:
                rte_tmp->rte_name = xstrdup(cdatastrp);
                break;
        case tt_rte:
                if (doing_no_routes)
                    route_del(rte_tmp);
                else
                    route_add(cur_route_list, rte_tmp);
                rte_tmp = NULL;
                break;
        case tt_rte_rtept:
                route_add_wpt_tail(rte_tmp, wpt_tmp);
                wpt_tmp = NULL;
                break;
        case tt_rte_desc:
                rte_tmp->rte_desc = xstrdup(cdatastrp);
                break;
        case tt_rte_number:
                rte_tmp->rte_num = atoi(cdatastrp);
                break;
        /*
         * Track-specific tags.
         */
        case tt_trk_name:
                trk_tmp->rte_name = xstrdup(cdatastrp);
                break;
        case tt_trk:
                if (doing_no_tracks)
                    route_free(trk_tmp);
                else
                    route_add(cur_track_list, trk_tmp);
                trk_tmp = NULL;
                break;
        case tt_trk_trkseg_trkpt:
                route_add_wpt_tail(trk_tmp, wpt_tmp);
                wpt_tmp = NULL;
                break;
        case tt_trk_desc:
                trk_tmp->rte_desc = xstrdup(cdatastrp);
                break;
        case tt_trk_number:
                trk_tmp->rte_num = atoi(cdatastrp);
                break;
        case tt_trk_trkseg_trkpt_course:
		/* external is degrees, internal is in hundredths */
                wpt_tmp->course =
                        roadmap_math_from_floatstring(cdatastrp, HUNDREDTHS);
                break;
        case tt_trk_trkseg_trkpt_speed:
		/* external is m/sec, internal is cm/sec */
                wpt_tmp->speed =
                        roadmap_math_from_floatstring(cdatastrp, HUNDREDTHS);
                break;

        /*
         * Items that are actually in multiple categories.
         */
        case tt_wpt_ele:
        case tt_rte_rtept_ele:
        case tt_trk_trkseg_trkpt_ele:
		/* external is meters, internal is mm */
                wpt_tmp->altitude =
                        roadmap_math_from_floatstring(cdatastrp, THOUSANDTHS);
                break;
        case tt_wpt_name:
        case tt_rte_rtept_name:
        case tt_trk_trkseg_trkpt_name:
                wpt_tmp->shortname = xstrdup(cdatastrp);
                break;
#if ROADMAP_UNNEEDED
        case tt_wpt_sym:
        case tt_rte_rtept_sym:
        case tt_trk_trkseg_trkpt_sym:
                wpt_tmp->icon_descr = xstrdup(cdatastrp);
                break;
#endif
        case tt_wpt_time:
        case tt_trk_trkseg_trkpt_time:
        case tt_rte_rtept_time:
                wpt_tmp->creation_time = xml_parse_time( cdatastrp );
                break;
        case tt_wpt_cmt:
        case tt_rte_rtept_cmt:
        case tt_trk_trkseg_trkpt_cmt:
                wpt_tmp->description = xstrdup(cdatastrp);
                break;
        case tt_wpt_desc:
        case tt_trk_trkseg_trkpt_desc:
        case tt_rte_rtept_desc:
                if (wpt_tmp->notes != NULL) xfree(wpt_tmp->notes);
                wpt_tmp->notes = xstrdup(cdatastrp);
                break;
#if ROADMAP_UNNEEDED
        case tt_pdop:
                wpt_tmp->pdop = atof(cdatastrp);
                break;
        case tt_hdop:
                wpt_tmp->hdop = atof(cdatastrp);
                break;
        case tt_vdop:
                wpt_tmp->vdop = atof(cdatastrp);
                break;
        case tt_sat:
                wpt_tmp->sat = atof(cdatastrp);
                break;
        case tt_fix:
                wpt_tmp->fix = atoi(cdatastrp)-1;
                if ( wpt_tmp->fix < fix_2d) {
                        if (!strcasecmp(cdatastrp, "none"))
                                wpt_tmp->fix = fix_none;
                        else if (!strcasecmp(cdatastrp, "dgps"))
                                wpt_tmp->fix = fix_dgps;
                        else if (!strcasecmp(cdatastrp, "pps"))
                                wpt_tmp->fix = fix_pps;
                        else
                                wpt_tmp->fix = fix_unknown;
                }
                break;
#endif // ROADMAP_UNNEEDED
        case tt_unknown:
                end_something_else();
                *s = 0;
                return;
        default:
                break;
        }

        if (passthrough)
                end_something_else();

        *s = 0;
}


static void
gpx_cdata(void *dta, const XML_Char *s, int len)
{
        char *estr;
        int *cdatalen;
        char **cdata;
        xml_tag *tmp_tag;
        size_t slen = strlen(cdatastr.mem);

        vmem_realloc(&cdatastr,  1 + len + slen);
        estr = ((char *) cdatastr.mem) + slen;
        memcpy(estr, s, len);
        estr[len]  = 0;

        if (!cur_tag) 
                return;

        if ( cur_tag->child ) {
                tmp_tag = cur_tag->child;
                while ( tmp_tag->sibling ) {
                        tmp_tag = tmp_tag->sibling;
                }
                cdata = &(tmp_tag->parentcdata);
                cdatalen = &(tmp_tag->parentcdatalen);
        } else {
                cdata = &(cur_tag->cdata);
                cdatalen = &(cur_tag->cdatalen);
        }
        estr = *cdata;
        *cdata = xcalloc( *cdatalen + len + 1, 1);
        if ( estr ) {
                memcpy( *cdata, estr, *cdatalen);
                xfree( estr );
        }
        estr = *cdata + *cdatalen;
        memcpy( estr, s, len );
        *(estr+len) = '\0';
        *cdatalen += len;
}

static void
gpx_read_pre()
{

        file_time = 0;
        current_tag = vmem_alloc(1, 0);
        *((char *)current_tag.mem) = '\0';
        
        psr = XML_ParserCreate(NULL);
        if (!psr) {
                fatal(MYNAME ": Cannot create XML Parser\n");
        }

        /* recent gpsbabel code has this, plus different utf8_to_int()
         * processing
         * XML_SetUnknownEncodingHandler(psr, 
         *      cet_lib_expat_UnknownEncodingHandler, NULL);
         */

        cdatastr = vmem_alloc(1, 0);
        *((char *)cdatastr.mem) = '\0';

        if (!xsi_schema_loc) {
                xsi_schema_loc = xstrdup(DEFAULT_XSI_SCHEMA_LOC);
        }
        if (!xsi_schema_loc) {
                fatal("gpx: Unable to allocate %d bytes of memory.\n", (int) strlen(DEFAULT_XSI_SCHEMA_LOC) + 1);
        }

        XML_SetElementHandler(psr, gpx_start, gpx_end);
        XML_SetCharacterDataHandler(psr, gpx_cdata);

        wpt_tmp = NULL;
        rte_tmp = NULL;
        trk_tmp = NULL;
        cur_tag = NULL;
        fs_ptr = NULL;
        gpx_tag_found = 0;
}

static
void 
gpx_read_post(void) 
{
        vmem_free(&current_tag);
        vmem_free(&cdatastr);

        /* 
         * Old gpsbabel comment follows.  if we ever add ability to
         *  merge routes, we should watch this...
        *//*
         * Don't free schema_loc.  It really is important that we preserve
         * this across reads or else merges/copies of files with different 
         * schemas won't retain the headers.
         *
         */
        if ( xsi_schema_loc ) {         
                xfree(xsi_schema_loc);
                xsi_schema_loc = NULL;
        }
         
        if ( gpx_email ) {
                xfree(gpx_email);
                gpx_email = NULL;
        }
        if ( gpx_author ) {
                xfree(gpx_author);
                gpx_author = NULL;
        }
        XML_ParserFree(psr);
        psr = NULL;

}

int
gpx_read(FILE *ifile, queue_head *wq, int wee, queue_head *rq, queue_head *tq)
{
        int len;
        int done = 0;
        char *buf = xmalloc(MY_CBUF_SZ);
        int result = 1;
        int extra;

        doing_no_waypoints = (wq == NULL);
        doing_no_routes = (rq == NULL);
        doing_no_tracks = (tq == NULL);
        reading_weepoints = wee;

        cur_waypoint_list = wq;
        cur_route_list = rq;
        cur_track_list = tq;

        gpx_read_pre();

        while (!done) {
#define GEOCACHING_HACK 1
#if GEOCACHING_HACK
                /* 
                 * The majority of this block (in fact, all but the 
                 * call to XML_Parse) are a disgusting hack to 
                 * correct defective GPX files that Geocaching.com
                 * issues as pocket queries.   They contain escape
                 * characters as entities (&#x00-&#x1f) which makes
                 * them not validate which croaks expat and torments
                 * users.
                 *
                 * Look for '&' in the last maxentlength chars.   If 
                 * we find it, strip it, then read byte-at-a-time 
                 * until we find a non-entity.
                 */
                char *badchar;
                char *semi;
                int maxentlength = 8;
                len = fread(buf, 1, MY_CBUF_SZ - maxentlength, ifile);
                done = feof(ifile) || !len;
                buf[len] = '\0';
                badchar = buf+len-maxentlength;
                badchar = strchr( badchar, '&' );
                extra = maxentlength-1; /* for terminator */
                while ( badchar && len < MY_CBUF_SZ-1) {
                        semi = strchr( badchar, ';');
                        while ( extra && !semi ) {
                                len += fread( buf+len, 1, 1, ifile);
                                buf[len]='\0';
                                extra--;
                                if ( buf[len-1] == ';') 
                                        semi= buf+len-1;
                        }
                        badchar = strchr( badchar+1, '&' );
                } 
                {
                        char *hex="0123456789abcdef";
                        badchar = strstr( buf, "&#x" );
                        while ( badchar ) {
                                int val = 0;
                                char *hexit = badchar+3;
                                semi = strchr( badchar, ';' );
                                if ( semi ) {
                                        while (*hexit && *hexit != ';') {
                                                val *= 16;
                                                val += strchr( hex, *hexit )-hex;
                                                hexit++;
                                        }
                                        
                                        if ( val < 32 ) {
                                                warning( MYNAME ": Ignoring illegal character %s;\n\tConsider emailing %s at <%s>\n\tabout illegal characters in their GPX files.\n", badchar, gpx_author?gpx_author:"(unknown author)", gpx_email?gpx_email:"(unknown email address)" );
                                                memmove( badchar, semi+1, strlen(semi+1)+1 );
                                                len -= (semi-badchar)+1;
                                                badchar--;
                                        }
                                }
                                badchar = strstr( badchar+1, "&#x" );
                        } 
                }
#else
                len = fread(buf, 1, MY_CBUF_SZ, ifile);
                done = feof(ifile) || !len;
#endif
                result = XML_Parse(psr, buf, len, done);

                if (!result) {
                        warning(MYNAME ": XML parse error at %d: %s\n", 
                                (int) XML_GetCurrentLineNumber(psr),
                                XML_ErrorString(XML_GetErrorCode(psr)));
                        done = 1;
                }

                if (!gpx_tag_found) {
                        warning(MYNAME ": no GPX data in input\n");
                        result = 0;
                        done = 1;
                }
        }
        xfree(buf);

        gpx_read_post();

        return result;
}

static void
fprint_tag_and_attrs( FILE *ofd, char *prefix, char *suffix, xml_tag *tag, int indent )
{
        char **pa;
        while (indent--) fputc(' ', ofd);
        fprintf( ofd, "%s%s", prefix, tag->tagname );
        pa = tag->attributes;
        if ( pa ) {
                while ( *pa ) {
                        fprintf( ofd, " %s=\"%s\"", pa[0], pa[1] );
                        pa += 2;
                }
        }
        fprintf( ofd, "%s", suffix );
}

static void
fprint_xml_chain( FILE *ofd, xml_tag *tag, const waypoint *wpt, int indent) 
{
        char *tmp_ent;
        int i;
        while ( tag ) {
                i = indent;
                if ( !tag->cdata && !tag->child ) {
                        fprint_tag_and_attrs(ofd, "<", " />", tag, i );
                }
                else {
                        fprint_tag_and_attrs(ofd, "<", ">", tag, i );
                
                        if ( tag->cdata ) {
                                tmp_ent = xml_entitize( tag->cdata );
                                fprintf( ofd, "%s", tmp_ent );
                                xfree(tmp_ent);
                        }
                        if ( tag->child ) {
                                fprint_xml_chain(ofd, tag->child, wpt, i+1);
                                while (i--) fputc(' ', ofd);
                        }
#if ROADMAP_UNNEEDED
                        if ( wpt && wpt->gc_data.exported &&
                            strcmp(tag->tagname, "groundspeak:cache" ) == 0 ) {
                                xml_write_time( ofd, wpt->gc_data.exported, 
                                                "", "groundspeak:exported" );
                        }
#endif // ROADMAP_UNNEEDED
                        fprintf( ofd, "</%s>\n", tag->tagname);
                }
                if ( tag->parentcdata ) {
                        tmp_ent = xml_entitize(tag->parentcdata);
                        fprintf(ofd, "%s", tmp_ent );
                        xfree(tmp_ent);
                }
                tag = tag->sibling;     
        }
}       

void free_gpx_extras( xml_tag *tag )
{
        xml_tag *next = NULL;
        char **ap;
        
        while ( tag ) {
                if (tag->cdata) {
                        xfree(tag->cdata);
                }
                if (tag->child) {
                        free_gpx_extras(tag->child);
                }
                if (tag->parentcdata) {
                        xfree(tag->parentcdata);
                }
                if (tag->tagname) {
                        xfree(tag->tagname);
                }
                if (tag->attributes) {
                        ap = tag->attributes;

                        while (*ap)
                                xfree(*ap++);

                        xfree(tag->attributes);
                }
                
                next = tag->sibling;
                xfree(tag);
                tag = next;
        }
}

#if ROADMAP_UNNEEDED
/*
 * Handle the grossness of GPX 1.0 vs. 1.1 handling of linky links.
 */
static void
write_gpx_url( FILE *ofd, const waypoint *waypointp)
{
        char *tmp_ent;

        if (waypointp->url) {
                tmp_ent = xml_entitize(waypointp->url);
                if (gpx_wversion_num > 10) {
                        
                        fprintf(ofd, "  <link href=\"%s%s\">\n", 
                                urlbase ? urlbase : "", tmp_ent);
                        write_optional_xml_entity(ofd, "  ", "text", 
                                waypointp->url_link_text);
                        fprintf(ofd, "  </link>\n");
                } else {
                        fprintf(ofd, "  <url>%s%s</url>\n", 
                                urlbase ? urlbase : "", tmp_ent);
                        write_optional_xml_entity(ofd, "  ", "urlname", 
                                waypointp->url_link_text);
                }
                xfree(tmp_ent);
        }
}
#endif // ROADMAP_UNNEEDED

#if ROADMAP_UNNEEDED
/*
 * Write optional accuracy information for a given (way|track|route)point
 * to the output stream.  Done in one place since it's common for all three.
 * Order counts.
 */
static void
gpx_write_common_acc( FILE *ofd, const waypoint *waypointp, const char *indent)
{
        char *fix = NULL;

        switch (waypointp->fix) {
                case fix_2d:
                        fix = "2d";
                        break;
                case fix_3d:
                        fix = "3d";
                        break;
                case fix_dgps:
                        fix = "dgps";
                        break;
                case fix_pps:
                        fix = "pps";
                        break;
                case fix_none:
                        fix = "none";
                        break;
                /* GPX spec says omit if we don't know. */
                case fix_unknown:
                default:
                        break;
        }
        if (fix) {
                fprintf(ofd, "%s<fix>%s</fix>\n", indent, fix);
        }
        if (waypointp->sat > 0) {
                fprintf(ofd, "%s<sat>%d</sat>\n", indent, waypointp->sat);
        }
        if (waypointp->hdop) {
                fprintf(ofd, "%s<hdop>%f</hdop>\n", indent, waypointp->hdop);
        }
        if (waypointp->vdop) {
                fprintf(ofd, "%s<vdop>%f</vdop>\n", indent, waypointp->vdop);
        }
        if (waypointp->pdop) {
                fprintf(ofd, "%s<pdop>%f</pdop>\n", indent, waypointp->pdop);
        }
}
#endif

static void
gpx_write_common_position( FILE *ofd, const waypoint *waypointp,
        const char *indent)
{
        if (waypointp->altitude != unknown_alt) {
                fprintf(ofd, "%s<ele>%s</ele>\n", indent,
                        roadmap_math_to_floatstring
				(0, waypointp->altitude, THOUSANDTHS));
        }
        if (waypointp->creation_time) {
                xml_write_time(ofd, waypointp->creation_time, indent, "time");
        }
}

static void
gpx_write_common_description( FILE *ofd, const waypoint *waypointp,
        const char *indent, const char *oname)
{
        write_optional_xml_entity(ofd, indent, "name", oname);
        write_optional_xml_entity(ofd, indent, "cmt", waypointp->description);
        if (waypointp->notes && waypointp->notes[0])
                write_xml_entity(ofd, indent, "desc", waypointp->notes);
        else
                write_optional_xml_entity(ofd, indent, "desc", waypointp->description);
#if ROADMAP_UNNEEDED
        write_gpx_url(ofd, waypointp);
        write_optional_xml_entity(ofd, indent , "sym", waypointp->icon_descr);
#endif
}

static void
gpx_waypt_pr(const waypoint *waypointp)
{
        const char *oname;
        char *odesc;
        fs_xml *fs_gpx;
        char lon[32], lat[32];

        /*
         * Desperation time, try very hard to get a good shortname
         */
        odesc = waypointp->notes;
        if (!odesc) {
                odesc = waypointp->description;
        }
        if (!odesc) {
                odesc = waypointp->shortname;
        }

        oname = gpx_synthesize_shortnames ?
                                  mkshort(mkshort_handle, odesc) : 
                                  waypointp->shortname;

        fprintf(cb_file, "<wpt lat=\"%s\" lon=\"%s\">\n",
                roadmap_math_to_floatstring
			(lat, waypointp->pos.latitude, MILLIONTHS),
                roadmap_math_to_floatstring
			(lon, waypointp->pos.longitude, MILLIONTHS));

        gpx_write_common_position(cb_file, waypointp, "  ");
        gpx_write_common_description(cb_file, waypointp, "  ", oname);
#if ROADMAP_UNNEEDED
        gpx_write_common_acc(cb_file, waypointp, "  ");
#endif

        fs_gpx = (fs_xml *)fs_chain_find( waypointp->fs, FS_GPX );
        if ( fs_gpx ) {
                fprint_xml_chain(cb_file, fs_gpx->tag, waypointp, 2 );
        }
        fprintf(cb_file, "</wpt>\n");
}

static void
gpx_track_hdr(const route_head *rte)
{
        fs_xml *fs_gpx;

        fprintf(cb_file, "<trk>\n");
        write_optional_xml_entity(cb_file, "  ", "name", rte->rte_name);
        write_optional_xml_entity(cb_file, "  ", "desc", rte->rte_desc);
        if (rte->rte_num) {
                fprintf(cb_file, "<number>%d</number>\n", rte->rte_num);
        }
        fprintf(cb_file, "<trkseg>\n");

        fs_gpx = (fs_xml *)fs_chain_find( rte->fs, FS_GPX );
        if ( fs_gpx ) {
                fprint_xml_chain(cb_file, fs_gpx->tag, NULL, 1 );
        }
}

static void
gpx_trkpt_disp(const waypoint *waypointp)
{
        fs_xml *fs_gpx;
        char lon[32], lat[32];

        fprintf(cb_file,
                "<trkpt lat=\"%s\" lon=\"%s\">\n",
                    roadmap_math_to_floatstring
		    	(lat, waypointp->pos.latitude, MILLIONTHS),
                    roadmap_math_to_floatstring
			(lon, waypointp->pos.longitude, MILLIONTHS));

        gpx_write_common_position(cb_file, waypointp, "  ");

        /* These were accidentally removed from 1.1 */
        if (gpx_wversion_num == 10) {
                if (waypointp->course >= 0) {
                        fprintf(cb_file, "  <course>%s</course>\n", 
                                roadmap_math_to_floatstring(0,
				    waypointp->course, HUNDREDTHS));
                }
                if (waypointp->speed >= 0) {
                        fprintf(cb_file, "  <speed>%s</speed>\n", 
                                roadmap_math_to_floatstring(0,
				    waypointp->speed, HUNDREDTHS));
                }
        }

        /* GPX doesn't require a name on output, so if we made one up
         * on input, we might as well say nothing.
         */
        gpx_write_common_description(cb_file, waypointp, "  ", 
                waypointp->wpt_flags.shortname_is_synthetic ? 
                        NULL : waypointp->shortname);
#if ROADMAP_UNNEEDED
        gpx_write_common_acc(cb_file, waypointp, "  ");
#endif

        fs_gpx = (fs_xml *)fs_chain_find( waypointp->fs, FS_GPX );
        if ( fs_gpx ) {
                fprint_xml_chain(cb_file, fs_gpx->tag, waypointp, 2 );
        }

        fprintf(cb_file, "</trkpt>\n");
}

static void
gpx_track_tlr(const route_head *rte)
{
        fprintf(cb_file, "</trkseg>\n");
        fprintf(cb_file, "</trk>\n");
}

static void
gpx_route_hdr(const route_head *rte)
{
        fs_xml *fs_gpx;

        fprintf(cb_file, "<rte>\n");
        write_optional_xml_entity(cb_file, "  ", "name", rte->rte_name);
        write_optional_xml_entity(cb_file, "  ", "desc", rte->rte_desc);
        if (rte->rte_num) {
                fprintf(cb_file, "  <number>%d</number>\n", rte->rte_num);
        }

        fs_gpx = (fs_xml *)fs_chain_find( rte->fs, FS_GPX );
        if ( fs_gpx ) {
                fprint_xml_chain(cb_file, fs_gpx->tag, NULL, 2 );
        }
}

static void
gpx_rtept_disp(const waypoint *waypointp)
{
        fs_xml *fs_gpx;
        char lon[32], lat[32];

        fprintf(cb_file,
                "  <rtept lat=\"%s\" lon=\"%s\">\n",
                    roadmap_math_to_floatstring
			(lat, waypointp->pos.latitude, MILLIONTHS),
                    roadmap_math_to_floatstring
			(lon, waypointp->pos.longitude, MILLIONTHS));

        gpx_write_common_position(cb_file, waypointp, "    ");
        gpx_write_common_description(cb_file, waypointp, "    ",
                waypointp->wpt_flags.shortname_is_synthetic ? 
                        NULL : waypointp->shortname);
#if ROADMAP_UNNEEDED
        gpx_write_common_acc(cb_file, waypointp, "    ");
#endif

        fs_gpx = (fs_xml *)fs_chain_find( waypointp->fs, FS_GPX );
        if ( fs_gpx ) {
                fprint_xml_chain(cb_file, fs_gpx->tag, waypointp, 2 );
        }

        fprintf(cb_file, "  </rtept>\n");
}

static void
gpx_route_tlr(const route_head *rte)
{
        fprintf(cb_file, "</rte>\n");
}

static void
gpx_waypt_bound_calc(const waypoint *waypointp)
{
        waypt_add_to_bounds(&all_bounds, waypointp);
}

static void
gpx_write_bounds(FILE *ofd, queue_head *wq, queue_head *rq, queue_head *tq)
{
        char minlon[32], minlat[32];
        char maxlon[32], maxlat[32];
        waypt_init_bounds(&all_bounds);

        if (wq) waypt_iterator(wq, gpx_waypt_bound_calc);
        if (rq) route_iterator(rq, NULL, NULL, gpx_waypt_bound_calc);
        if (tq) route_iterator(tq, NULL, NULL, gpx_waypt_bound_calc);
        if (waypt_bounds_valid(&all_bounds)) {
               fprintf(ofd,
                  "<bounds minlat=\"%s\" minlon =\"%s\" "
                  "maxlat=\"%s\" maxlon=\"%s\" />\n",
                  roadmap_math_to_floatstring
			(minlat, all_bounds.min_lat, MILLIONTHS),
                  roadmap_math_to_floatstring
			(minlon, all_bounds.min_lon, MILLIONTHS),
                  roadmap_math_to_floatstring
			(maxlat, all_bounds.max_lat, MILLIONTHS),
                  roadmap_math_to_floatstring
			(maxlon, all_bounds.max_lon, MILLIONTHS));
        }
}


static void
gpx_write_preamble( FILE *ofd, queue_head *wq, queue_head *rq, queue_head *tq)
{
        time_t now = 0;
        int short_length;

        gpx_wversion_num = strtod(gpx_wversion, NULL) * 10;

        if (gpx_wversion_num <= 0) {
                fatal(MYNAME ": gpx version number of '%s' not valid.\n",
                gpx_wversion);
        }
        
        now = current_time();

        short_length = atoi(snlen);

        if (suppresswhite) {
                setshort_whitespace_ok(mkshort_handle, 0);
        }

        setshort_length(mkshort_handle, short_length);

        fprintf(ofd,
                "<?xml version=\"1.0\" encoding=\"%s\"?>\n",
                gpx_charset_name);
        fprintf(ofd,
                "<gpx\n version=\"%s\"\n", gpx_wversion);
        fprintf(ofd,
                "creator=\"" CREATOR_NAME_URL "\"\n");
        fprintf(ofd, 
                "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n");
        fprintf(ofd,
                "xmlns=\"http://www.topografix.com/GPX/%c/%c\"\n",
                gpx_wversion[0], gpx_wversion[2]);
        if (xsi_schema_loc) {
                fprintf(ofd,
                        "xsi:schemaLocation=\"%s\">\n", xsi_schema_loc);
        } else {
                fprintf(ofd,
                        "xsi:schemaLocation=" DEFAULT_XSI_SCHEMA_LOC_FMT">\n",
                        gpx_wversion[0], gpx_wversion[2],
                        gpx_wversion[0], gpx_wversion[2]);
        }

        if (gpx_wversion_num > 10) {    
                fprintf(ofd, "<metadata>\n");
        }
        xml_write_time( ofd, now, "", "time" );

        gpx_write_bounds(ofd, wq, rq, tq);

        if (gpx_wversion_num > 10) {    
                fprintf(ofd, "</metadata>\n");
        }
}

static void
gpx_write_postfix( FILE *ofd)
{

        fprintf(ofd, "</gpx>\n");
}

int
gpx_write( FILE *ofd, queue_head *wq, queue_head *rq, queue_head *tq)
{

        mkshort_handle = mkshort_new_handle();

        gpx_write_preamble(ofd, wq, rq, tq);

        cb_file = ofd;

        if (wq) waypt_iterator(wq, gpx_waypt_pr);
        if (rq) route_iterator(rq, 
                gpx_route_hdr, gpx_route_tlr, gpx_rtept_disp);
        if (tq) route_iterator(tq,
                gpx_track_hdr, gpx_track_tlr, gpx_trkpt_disp);

        cb_file = NULL;  /* safety */

        gpx_write_postfix(ofd);

        mkshort_del_handle(&mkshort_handle);

        return 1;
}


#if NEEDED
static void
gpx_exit(void)
{
        if ( xsi_schema_loc ) {
                xfree(xsi_schema_loc);
                xsi_schema_loc = NULL;
        }
}
#endif

#endif // ifndef ROADMAP_USES_EXPAT

