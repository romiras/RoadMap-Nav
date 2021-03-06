/* buildmap.h - General definition for the map builder tool.
 *
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
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

#ifndef _BUILDMAP__H_
#define _BUILDMAP__H_

#include "roadmap.h"
#include "roadmap_types.h"
#include "roadmap_db.h"

extern int switch_endian;
#define BUILDMAP_BLOCK  2048 /* memory allocation block. */
#define switch_endian_short(x) _switch_endian_short((unsigned short *)x)
#define switch_endian_int(x) _switch_endian_int((unsigned int *)x)

static inline void _switch_endian_int(unsigned int *i)
{
   unsigned char *b = (unsigned char *) i;
   *i = (unsigned int) (b[0]<<24 | b[1]<<16 | b[2]<<8 | b[3]);
}

static inline void _switch_endian_short(unsigned short *s)
{
   unsigned char *b = (unsigned char *) s;
   *s = (unsigned short) (b[0]<<8 | b[1]);
}

void buildmap_set_source (const char *name);
void buildmap_set_line   (int line);

void buildmap_fatal    (int column, char *format, ...);
void buildmap_error    (int column, char *format, ...);
void buildmap_progress (int done, int estimated);
void buildmap_info     (char *format, ...);
void buildmap_summary  (int verbose, char *format, ...);

int buildmap_get_error_count (void);
int buildmap_get_error_total (void);

typedef struct dictionary_volume *BuildMapDictionary;

BuildMapDictionary buildmap_dictionary_open (char *name);
RoadMapString buildmap_dictionary_add
             (BuildMapDictionary dictionary, const char *string, int length);

RoadMapString buildmap_dictionary_locate
                (BuildMapDictionary dictionary, const char *string);
char *buildmap_dictionary_get
         (BuildMapDictionary dictionary, RoadMapString index);

void  buildmap_dictionary_summary (void);
void  buildmap_dictionary_save    (void);
void  buildmap_dictionary_reset   (void);


struct buildmap_db_section {
   struct buildmap_db_section *parent;
   struct buildmap_db_section *first;   /* .. child. */
   struct buildmap_db_section *last;    /* .. child. */
   struct buildmap_db_section *next;    /* .. sibling. */
   struct roadmap_db_section  *head;
   int head_offset;
   int data;
};

typedef struct buildmap_db_section buildmap_db;


int buildmap_db_open (const char *path, const char *name);

buildmap_db *buildmap_db_add_section (buildmap_db *parent, const char *name);
int          buildmap_db_add_data (buildmap_db *section, int count, int size);
char        *buildmap_db_get_data (buildmap_db *section);

buildmap_db *buildmap_db_add_child (buildmap_db *parent,
                                    char *name,
                                    int count,
                                    int size);
void buildmap_db_close (void);

#endif // _BUILDMAP__H_

