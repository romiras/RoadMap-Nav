
TOP = ..
include $(TOP)/options.mk

all: usc81070.rdm

usc81070.rdm: basemap.sh shp2osm.awk
	./basemap.sh

clean:
	rm -f usdir.rdm usc81070.rdm basefile.osm

sourcelist:
	@find . -print | egrep -v 'rdm$$|osm$$'

depends:

