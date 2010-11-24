#!/bin/csh
set COUNTRY="nl"
set P=..
set MAPS="maps".$COUNTRY
set MAKEFILE=Makefile.$COUNTRY
set SPLITTER=../splitter-r161/splitter.jar
set MAXNODES=10000
#
set START=280
set DIVIDE=2
#
if (! -d $MAPS) then
  mkdir $MAPS
endif
#
set LIST=`ls iso-$COUNTRY-*.osm`
set INDEX=$START
#
foreach osm ($LIST)
	set num=`echo $osm | awk -F- '{print $3;}' | awk -F. '{print $1;}'`
	set AREA=areas.list.$COUNTRY.$num

	set LINE=`cat ../areas.list.$COUNTRY* | grep 0000$num":"`
	echo "num {" $num "} LINE {" $LINE "}"
	set c1=`echo $LINE | awk '{print $2;}'`
	set c2=`echo $LINE | awk '{print $4;}'`
	set x1=`echo $c1 | awk -F, '{print $1;}'`
	set y1=`echo $c1 | awk -F, '{print $2;}'`
	set x2=`echo $c2 | awk -F, '{print $1;}'`
	set y2=`echo $c2 | awk -F, '{print $2;}'`

#
# Simplified, assume DIVIDE=2
#
	set xm = `expr \( $x1 + $x2 \) / 2`
	set ym = `expr \( $y1 + $y2 \) / 2`

	echo 00000$INDEX":" $x1,$y1 to $xm,$ym >>$AREA
	echo " " >>$AREA
	set INDEX=`expr $INDEX + 1`
	echo 00000$INDEX":" $x1,$ym to $xm,$y2 >>$AREA
	echo " " >>$AREA
	set INDEX=`expr $INDEX + 1`
	echo 00000$INDEX":" $xm,$y1 to $x2,$ym >>$AREA
	echo " " >>$AREA
	set INDEX=`expr $INDEX + 1`
	echo 00000$INDEX":" $xm,$ym to $x2,$y2 >>$AREA
	echo " " >>$AREA
	set INDEX=`expr $INDEX + 1`

	java -Xmx2048M -jar $SPLITTER --split-file=$AREA $osm
end
#
rm template.args
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
	echo "	$P/buildmap_osm -c ../../default/All -m $MAPS -i iso-$COUNTRY-$i.osm -o iso-$COUNTRY-$i.rdm 2>&1 >bm-$COUNTRY-$i.out" >>$MAKEFILE
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
exit 0
