#!/bin/sh

set -e

# the shapefiles have the same name as their directory, and no suffix
# is needed.
shapefiles=$(for d in ne_110m* ; do echo $d/$d ; done)

# gather attribution and version info
for s in $shapefiles
do
    b=$(basename $s)
    sources="$sources$b: $(cat $s.VERSION.txt); "
done

# create the world basemap in 8 slices -- otherwise there are too many
# "squares", and buildmap consumes way too much memory on a small
# machine.
# the 8107?  map identifiers aren't special, except that they appear
# in app_a02.txt
for map in $(seq 0 7)
do
    echo
    echo Making basemap usc8107$map.rdm...

    case $map in
    0) w=-180; e=-90; s=-90; n=0;;
    1) w=-90;  e=0;   s=-90; n=0;;
    2) w=0;    e=90;  s=-90; n=0;;
    3) w=90;   e=180; s=-90; n=0;;
    4) w=-180; e=-90; s=0;   n=90;;
    5) w=-90;  e=0;   s=0;   n=90;;
    6) w=0;    e=90;  s=0;   n=90;;
    7) w=90;   e=180; s=0;   n=90;;
    esac

    # run all the shapefiles through the converter together
    for s in $shapefiles
    do
	shpdump $s
    done | tee foo | ./shp2osm.awk $w $s $e $n "$sources" >tmp.osm  

    ../buildmap_osm -c ../default/All -i tmp.osm -o usc8107$map.rdm
done

../buildus --path=.. --maps=.

#rm -f tmp.osm
