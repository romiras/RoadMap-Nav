
TOP = ..
include $(TOP)/options.mk

# this Makefile and directory just build a single tile, based
# on pre-loaded data from OSM.  it provides a sample map tile
# centered on Brighton, England.

all: qt19/3d/75/qt3d755007.rdm

qt19/3d/75/qt3d755007.rdm: qt19/3d/75/qt3d755007.osm.gz
	../buildmap_osm -c ../default/All --maps . --tileid=0x3d755007

clean:
	rm -f qt19/3d/75/qt3d755007.rdm qt19/3d/75/qt3d755007.ways

sourcelist:
	@find . -print | egrep -v 'rdm$$|ways$$'

depends:

