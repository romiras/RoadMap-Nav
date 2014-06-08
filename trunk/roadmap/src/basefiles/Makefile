
TOP = ..
include $(TOP)/options.mk

all: usc81077.rdm

usc81077.rdm: basemap.sh shp2osm.awk
	./basemap.sh

clean:
	rm -f usdir.rdm usc8107?.rdm tmp.osm

sourcelist:
	@find . -print | egrep -v 'rdm$$|osm$$'

depends:

