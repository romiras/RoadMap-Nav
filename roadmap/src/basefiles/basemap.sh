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

# run all the shapefiles through the converter together
for s in $shapefiles
do
    shpdump $s
done | ./shp2osm.awk "$sources" >basefile.osm  

../buildmap_osm -c ../default/All -i basefile.osm -o usc81070.rdm

../buildus --path=.. --maps=.


