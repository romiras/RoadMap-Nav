#!/bin/sh

# merge all the sourcefiles into one XML file.
# the output is designed to mimic the current OSM XML format closely,
# both in layout of individual elements, and in ordering of elements
# within the file.

west="$1"
south="$2"
east="$3"
north="$4"
sources="$5"

# use sed first to ensure spaces around some fields, to separate them
# from, or to replace, their punctuation.  makes the awk job easier.
# "awk" is gawk on my system.  don't know if that matters.
sed -e '/^Shape:[[:digit:]]/s/:/ /' -e 's/,/ /g' -e 's/[()]/ & /g' |
awk --assign sources="$sources" \
    --assign west="$west" \
    --assign south="$south" \
    --assign east="$east" \
    --assign north="$north" '

function emit_way(way_id)
{
    printf "    <way id=\"%s\">\n", way_id
    for (i = 0; i < wayindex; i++) {
	printf "	<nd ref=\"%s\"/>\n", waynode[i]
    }
    # printf "	<nd ref=\"%s\"/>\n", waynode[0]
    printf "	<tag k=\"boundary\" v=\"administrative\"/>\n"
    printf "	<tag k=\"admin_level\" v=\"2\"/>\n"
    printf "    </way>\n"
    printf "\n"
}

    BEGIN {
	    discard = 0
	    nodeid = 1;
	    way_id = 1;
	    printf "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	    printf "<osm version=\"0.6\" generator=\"roadmap shp2osm.awk\">\n"
	    printf "<note>Made with Natural Earth. Free vector and raster map data @ naturalearthdata.com</note>\n"
	    printf "<note>source files: " sources "</note>\n"
	}

    /^Shape [[:digit:]].*/ {
	    if (wayindex != 0) {
		emit_way(way_id++)
	    }
	    wayindex = 0

	}
    /^ *Bounds:/ {

	    lat=$4; lon=$3
	    if ( lon >= west  && lon < east &&
	         lat >= south && lat < north) {
		discard = 0
		wayindex = 0
	    } else {
		discard = 1
	    }
    }
    /^ *\+  *\(.*Ring/ && !discard {

	    if (wayindex != 0) {
		emit_way(way_id++)
	    }
	    wayindex = 0

	    lat=$4; lon=$3
	    printf "    <node id=\"%s\" lat=\"%s\" lon=\"%s\"/>\n", nodeid, lat, lon
	    waynode[wayindex++] = nodeid++;
	}

    /^ *\(/ && !discard {

	    lat = $3; lon = $2
	    printf "    <node id=\"%s\" lat=\"%s\" lon=\"%s\"/>\n", nodeid, lat, lon
	    waynode[wayindex++] = nodeid++;
	}

    END {
	    if (way_id == 1 && wayindex == 0) {
		print "Error: shp2osm.awk got no input data" > "/dev/stderr"
		exit 1
	    }
	    if (wayindex != 0)
		emit_way(way_id++)
	    printf "</osm>\n"
	}
'
