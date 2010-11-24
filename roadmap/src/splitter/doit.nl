#!/bin/csh
set COUNTRY="nl"
set OSMFILE="netherlands-2010.11.13.osm"
set P=..
set MAPS="maps".$COUNTRY
set MAKEFILE=Makefile.$COUNTRY
set SPLITTER=splitter-r123/splitter.jar
set SPLITTER=splitter-r161/splitter.jar
set MAXNODES=10000
#
# rm -f iso-$COUNTRY*.osm iso-$COUNTRY*.osm.gz $MAPS/iso-$COUNTRY-*.rdm $MAKEFILE bm-$COUNTRY-*.out areas.list.$COUNTRY
#
if (! -r $OSMFILE) then
  if (-r $OSMFILE.bz2) then
    bunzip2 <$OSMFILE.bz2 >$OSMFILE
  else
    echo "$OSMFILE doesn't exist"
    exit 1
  endif
endif
#
if (! -d $MAPS) then
  mkdir $MAPS
endif
#
java -Xmx2048M -jar $SPLITTER --mapid=001 --max-nodes=30000 --resolution=12 $OSMFILE
#
mv areas.list areas.list.$COUNTRY
mv template.args template.args.$COUNTRY
#
echo ".SUFFIXES=	.osm .rdm .gz" >$MAKEFILE
echo "" >>$MAKEFILE
echo "SHELL=/bin/sh" >>$MAKEFILE
echo "P="$P >>$MAKEFILE
echo "" >>$MAKEFILE
echo "all::" >>$MAKEFILE
echo "" >>$MAKEFILE
#
set ALL=""
foreach i (`seq -w 0 999`)
    if (-r 00000$i.osm.gz) then
	mv 00000$i.osm.gz iso-$COUNTRY-$i.osm.gz
	echo "" >>$MAKEFILE
	echo "$MAPS/iso-$COUNTRY-$i.rdm:	iso-$COUNTRY-$i.osm" >>$MAKEFILE
	echo "	$P/buildmap_osm -c ../default/All -m $MAPS -i iso-$COUNTRY-$i.osm -o iso-$COUNTRY-$i.rdm 2>&1 >bm-$COUNTRY-$i.out" >>$MAKEFILE
	echo "" >>$MAKEFILE
	echo "iso-$COUNTRY-$i.osm:	iso-$COUNTRY-$i.osm.gz" >>$MAKEFILE
	echo "	gunzip <iso-$COUNTRY-$i.osm.gz >iso-$COUNTRY-$i.osm" >>$MAKEFILE

	set ALL="$ALL $MAPS/iso-$COUNTRY-$i.rdm"
    endif
end
echo "" >>$MAKEFILE
echo "all::	$ALL" >>$MAKEFILE
#
make -j10 -k -f $MAKEFILE
#
$P/buildus -d .. -m $MAPS
#
exit 0
