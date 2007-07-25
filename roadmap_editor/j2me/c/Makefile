######################################################################
##
## Copyright (C) 2006,  Blekinge Institute of Technology
##
## Filename:      Makefile
## Author:        Simon Kagstrom <ska@bth.se>
## Description:   Generic Makefile
##
## $Id: Makefile,v 1.2 2007-06-12 06:56:53 eshabtai Exp $
##
######################################################################
SUBDIRS =
TARGET  = program

OBJS    = ../../src/roadmap_dbread.o \
	  ../../src/roadmap_line.o \
	  ../../src/roadmap_log.o \
	  ../../src/roadmap_line_route.o \
	  ../../src/roadmap_line_speed.o \
	  ../../src/roadmap_turns.o \
	  ../../src/roadmap_dictionary.o \
	  ../../src/roadmap_math.o \
	  ../../src/roadmap_point.o \
	  ../../src/roadmap_polygon.o \
	  ../../src/roadmap_shape.o \
	  ../../src/roadmap_square.o \
	  ../../src/roadmap_config.o \
	  ../../src/roadmap_state.o \
	  ../../src/roadmap_layer.o \
	  ../../src/roadmap_street.o \
	  ../../src/roadmap_county.o \
	  ../../src/roadmap_locator.o \
	  ../../src/roadmap_metadata.o \
	  ../../src/roadmap_skin.o \
	  ../../src/roadmap_plugin.o \
	  ../../src/roadmap_hash.o \
	  ../../src/roadmap_list.o \
	  ../../src/roadmap_option.o \
	  ../../src/roadmap_display.o \
	  ../../src/roadmap_message.o \
	  ../../src/roadmap_sprite.o \
	  ../../src/roadmap_label.o \
	  ../../src/roadmap_object.o \
	  ../../src/roadmap_string.o \
	  ../../src/roadmap_lang.o \
	  ../../src/roadmap_adjust.o \
	  ../../src/roadmap_trip.o \
	  ../../src/roadmap_screen.o \
	  ../../src/roadmap_factory.o \
	  ../../src/roadmap_navigate.o \
	  ../../src/roadmap_fuzzy.o \
	  ../../src/roadmap_start.o \
	  ../../src/roadmap_screen_obj.o \
	  ../../src/roadmap_gps.o \
	  ../../src/roadmap_nmea.o \
	  ../../src/roadmap_gpsj2me.o \
	  ../../src/roadmap_input.o \
	  ../../src/roadmap_io.o \
	  ../../src/roadmap_geocode.o \
	  ../../src/roadmap_history.o \
	  ./roadmap_path.o \
	  ./roadmap_file.o \
	  ./roadmap_canvas.o \
	  ./roadmap_time.o \
	  ./roadmap_serial.o \
	  ./roadmap_main.o \
	  ./roadmap_dialog.o \
	  ./roadmap_address.o \
	  ./roadmap_sound.o \
	  ../../src/navigate/navigate_main.o \
	  ../../src/navigate/navigate_instr.o \
	  ../../src/navigate/navigate_bar.o \
	  ../../src/navigate/navigate_zoom.o \
	  ../../src/navigate/navigate_plugin.o \
	  ../../src/navigate/navigate_traffic.o \
	  ../../src/navigate/navigate_graph.o \
	  ../../src/navigate/navigate_cost.o \
	  ../../src/navigate/navigate_route_astar.o \
	  ../../src/navigate/fib-1.1/fib.o \
	  ./stubs.o

LDLIBS  = -lc -lsoftfloat -lm
CFLAGS  = -DJ2ME -DJ2MEMAP
INCLUDES = -I../../src -I../include

CIBYL_MIPS2JAVA_OPTS ?=-I../include

include $(CIBYL_BASE)/build/Rules.mk