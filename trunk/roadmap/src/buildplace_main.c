/* buildplace_main.c - The main function of the placename builder tool.
 *
 * LICENSE:
 *
 *   Copyright 2004 Stephen Woodbridge
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

#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <search.h>

#include "roadmap_types.h"
#include "roadmap_hash.h"
#include "roadmap_path.h"

#include "buildmap.h"
#include "buildmap_opt.h"

#include "buildmap_square.h"
#include "buildmap_point.h"
#include "buildmap_place.h"

#if ROADMAP_USE_SHAPEFILES

#include <shapefil.h>

/* shapefile column names */

#define F_NAME  "NAME"
#define F_STATE "STATE"
#define F_CC    "CC"
#define F_TYPE  "TYPE"

#endif				/* ROADMAP_USE_SHAPEFILES */


#define BUILDPLACE_FORMAT_SHAPE     1
#define BUILDPLACE_FORMAT_TXT       2

#define BUILDPLACE_MAX_DSG       1024
char *progname;
static char *BuildPlaceDSGStrings[BUILDPLACE_MAX_DSG] = { NULL };

static int BuildPlaceDSGlayer[BUILDPLACE_MAX_DSG];
static int BuildPlaceDSGCount = 0;

static char *BuildPlaceFormat = "TXT";

static char *BuildPlaceResult;
static char *BuildPlaceDSGFile = "./designations.txt";

struct opt_defs options[] = {
    {"format", "f", opt_string, "TXT",
     "Input files format (Text or ShapeFile)"},
    {"dsg", "d", opt_string, "./designations.txt",
     "The designations.txt file"},
    {"maps", "m", opt_string, "",
     "Location for the generated map files"},
    {"verbose", "v", opt_flag, "0",
     "Show more progress information"},
    {"debug", "d", opt_flag, "0",
     "Show debug information"},
    OPT_DEFS_END
};


#if ROADMAP_USE_SHAPEFILES
static RoadMapString
str2dict(BuildMapDictionary d, const char *string)
{

    if (!strlen(string)) {
	return buildmap_dictionary_add(d, "", 0);
    }

    return buildmap_dictionary_add(d, (char *) string, strlen(string));
}
#endif				/* ROADMAP_USE_SHAPEFILES */


static void
buildplace_save(const char *name)
{

    char db_name[128];


#ifdef WHY
    char *cursor;
    snprintf(db_name, 127, "usc%s", name);

    /* Remove the suffix if any was provided. */
    cursor = strrchr(db_name, '.');
    if (cursor != NULL) {
	*cursor = 0;
    }
#endif
    snprintf(db_name, 127, "usc%s.rdm", name);

    if (buildmap_db_open(BuildPlaceResult, db_name) < 0) {
	buildmap_fatal(0, "cannot create database %s", db_name);
    }

    buildmap_db_save();

    buildmap_db_close();
}

static void
buildplace_dsg_reset(void)
{
    int i;

    hdestroy();
    for (i = 0; i < BUILDPLACE_MAX_DSG; i++) {
	if (BuildPlaceDSGStrings[i])
	    free(BuildPlaceDSGStrings[i]);
	BuildPlaceDSGStrings[i] = NULL;
    }
    BuildPlaceDSGCount = 0;
}


static void
buildplace_dsg_initialize(void)
{
    int i;

    hcreate(BUILDPLACE_MAX_DSG);
    for (i = 0; i < BUILDPLACE_MAX_DSG; i++)
	BuildPlaceDSGlayer[i] = 0;
    BuildPlaceDSGCount = 0;
}


#if ROADMAP_USE_SHAPEFILES
static int
dsg2layer(const char *dsg)
{
    ENTRY e, *ep;

    e.key = (char *) dsg;
    ep = hsearch(e, FIND);
    return ep ? *(int *) ep->data : 0;
}
#endif


static void
buildplace_dsg_add(const char *dsg, int layer)
{
    ENTRY e, *ep;

    e.key = (char *) dsg;
    ep = hsearch(e, FIND);
    if (!ep) {
	if (BuildPlaceDSGCount + 1 > BUILDPLACE_MAX_DSG)
	    buildmap_fatal(0, "maximum designations has been exceeded");
	e.key = BuildPlaceDSGStrings[BuildPlaceDSGCount] = strdup(dsg);
	e.data = (void *) &BuildPlaceDSGlayer[BuildPlaceDSGCount];
	ep = hsearch(e, ENTER);
	if (!ep)
	    buildmap_fatal(0, "failed to add designation to hash");
	BuildPlaceDSGlayer[BuildPlaceDSGCount++] = layer;
    }
}


static void
buildplace_dsg_summary(void)
{

    fprintf(stderr,
	    "-- DSG hash statistics: %d DSG codes\n", BuildPlaceDSGCount);
}


static void
buildplace_shapefile_process(const char *source)
{

#if ROADMAP_USE_SHAPEFILES

    static BuildMapDictionary DictionaryName;

    char name[160];
    int irec;
    int record_count;
    int pname;
    int layer;
    int point;
    int lat, lon;

    int iNAME, iSTATE, iCC, iTYPE;

    DBFHandle hDBF;
    SHPHandle hSHP;
    SHPObject *shp;

    DictionaryName = buildmap_dictionary_open("placename");

    buildmap_set_source((char *) source);

    hDBF = DBFOpen(source, "rb");
    hSHP = SHPOpen(source, "rb");

    iNAME = DBFGetFieldIndex(hDBF, F_NAME);
    iSTATE = DBFGetFieldIndex(hDBF, F_STATE);
    iCC = DBFGetFieldIndex(hDBF, F_CC);
    iTYPE = DBFGetFieldIndex(hDBF, F_TYPE);

    record_count = DBFGetRecordCount(hDBF);

    for (irec = 0; irec < record_count; irec++) {

	strcpy(name, DBFReadStringAttribute(hDBF, irec, iCC));
	strcat(name, "/");
	strcat(name, DBFReadStringAttribute(hDBF, irec, iSTATE));
	strcat(name, "/");
	strcat(name, DBFReadStringAttribute(hDBF, irec, iNAME));

	pname = str2dict(DictionaryName, name);

	layer = dsg2layer(DBFReadStringAttribute(hDBF, irec, iTYPE));
	if (layer == 0)
	    continue;

	/* add the place */

	shp = SHPReadObject(hSHP, irec);

	lon = shp->padfX[0] * 1000000.0;
	lat = shp->padfY[0] * 1000000.0;

	SHPDestroyObject(shp);

	point = buildmap_point_add(lon, lat);

	buildmap_place_add(pname, layer, point);

	if ((irec & 0xff) == 0) {
	    buildmap_progress(irec, record_count);
	}

    }

    DBFClose(hDBF);
    SHPClose(hSHP);

#else

    fprintf(stderr,
	    "cannot process %s: built with no shapefile support.\n", source);
    exit(1);

#endif				/* ROADMAP_USE_SHAPEFILES */
}


static void
buildplace_txt_process(const char *source)
{

    static BuildMapDictionary DictionaryName;

    char *bufp = NULL;
    size_t buflen;
    ssize_t gotlen;
    char place[512];
    char *name;
    double lat, lon;
    int ilat, ilon;
    int lineno;
    int nameindex;
    int pname;
    int layer;
    int point;

    buildmap_set_source((char *) source);

    DictionaryName = buildmap_dictionary_open("placename");

    if (strcmp(source, "-") != 0)
	buildmap_fatal(0, "TXT format must come from stdin: use '-'");

    lineno = 0;
    while ((gotlen = getline(&bufp, &buflen, stdin)) != -1) {
	int n;
	lineno++;
	n = sscanf(bufp, "%lf\t%lf\t%s\t%n", &lat, &lon, place, &nameindex);
	if (n != 3)
	    buildmap_fatal(0, "bad text line format, line %d: %s", lineno,
			   bufp);

	name = &bufp[nameindex];
	buildmap_debug("got %f %f %s %s", lat, lon, place, name);

	pname = str2dict(DictionaryName, name);

	layer = dsg2layer(place);
	if (layer == 0)
	    continue;

	ilat = lat * 1000000.0;
	ilon = lon * 1000000.0;

	point = buildmap_point_add(ilon, ilat);

	buildmap_place_add(pname, layer, point);

	if ((lineno % 100) == 0) {
	    buildmap_progress(lineno, 0);
	}
    }

}


static void
buildplace_read_dsg(const char *dsgfile)
{

    FILE *file;
    char buff[2048];
    char *p;
    int c;
    int layer;

    buildplace_dsg_initialize();

    file = fopen(dsgfile, "rb");
    if (file == NULL) {
	buildmap_fatal(0, "cannot open file %s", dsgfile);
    }

    while (!feof(file)) {

	if (fgets(buff, 2048, file)) {
	    c = strspn(buff, " \t\r\n");
	    if (buff[c] == '#' || strlen(buff + c) == 0)
		continue;
	    // buff[c] == '\0'

	    layer = strtol(buff + c, &p, 10);
	    if (layer < 0 || layer > BUILDMAP_MAX_PLACE_LAYER)
		buildmap_fatal(0, "place layer is out of range");

	    while (isspace(*p))
		p++;		/* skip leading blanks */
	    c = strcspn(p, " \t\r\n");
	    p[c] = '\0';

	    buildplace_dsg_add(p, layer);
	}
    }

    fclose(file);
}



void
usage(char *progpath, const char *msg)
{

    char *prog = strrchr(progpath, '/');

    if (prog)
	prog++;
    else
	prog = progpath;

    if (msg)
	fprintf(stderr, "%s: %s\n", prog, msg);
    fprintf(stderr, "usage: %s [options] <FIPS code> <source>\n", prog);
    opt_desc(options, 1);
    exit(1);
}

int
main(int argc, char **argv)
{

    int verbose = 0, debug = 0;
    int error;
    char *source, *fips;

    progname = strrchr(argv[0], '/');

    if (progname)
	progname++;
    else
	progname = argv[0];

    // BuildPlaceResult = strdup(roadmap_path_preferred("maps")); /* default. */

    /* parse the options */
    error = opt_parse(options, &argc, argv, 0);
    if (error)
	usage(progname, opt_strerror(error));

    /* then, fetch the option values */
    error = opt_val("maps", &BuildPlaceResult) ||
	opt_val("format", &BuildPlaceFormat) ||
	opt_val("dsg", &BuildPlaceDSGFile) ||
	opt_val("verbose", &verbose) || opt_val("debug", &debug);
    if (error)
	usage(progname, opt_strerror(error));

    if (debug)
	buildmap_message_adjust_level(BUILDMAP_MESSAGE_DEBUG);
    else if (verbose)
	buildmap_message_adjust_level(BUILDMAP_MESSAGE_VERBOSE);

    if (argc != 3)
	usage(progname, "missing required arguments");

    fips = argv[1];
    source = argv[2];

    buildplace_read_dsg(BuildPlaceDSGFile);

    if (strcasecmp(BuildPlaceFormat, "TXT") == 0) {

	buildplace_txt_process(source);

    } else if (strcasecmp(BuildPlaceFormat, "SHAPE") == 0) {

	buildplace_shapefile_process(source);

    } else {
	fprintf(stderr, "%s: unsupported input format, must be TXT or SHAPE\n",
		BuildPlaceFormat);
	exit(1);
    }

    buildmap_db_sort();

    if (verbose) {
	roadmap_hash_summary();
	buildmap_db_summary();
	buildplace_dsg_summary();
    }

    buildplace_save(fips);

    buildmap_db_reset();
    buildplace_dsg_reset();
    roadmap_hash_reset();

    return 0;
}
