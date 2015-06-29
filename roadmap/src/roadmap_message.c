/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright 2007 Paul G. Fox
 *   Copyright 2008 Danny Backx
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

/**
 * @file
 * @brief Manage screen signs.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

#include "roadmap.h"
#include "roadmap_time.h"
#include "roadmap_message.h"
#include "roadmap_display.h"


static char *RoadMapMessageParameters[128] = {NULL};

static int RoadMapMessagesUseTime;
static int RoadMapMessageChanged;

int roadmap_message_time_in_use(void) {

    return RoadMapMessagesUseTime;
}

int roadmap_message_changed(void) {
    if (!RoadMapMessageChanged) return 0;

    RoadMapMessageChanged = 0;
    return 1;
}

int roadmap_message_format (char *text, int length, const char *format) {

    char *f;
    char *p = text;
    char *end = text + length - 1;
    
    while (*format) {
        
        if (*format == '%') {
            
            format += 1;
            if (*format <= 0) {
                break;
            }
            
            f = RoadMapMessageParameters[(int)(*format)];
            if (*format == 'T') {
                RoadMapMessagesUseTime = 1;
            }
            format += 1;
            if (f != NULL) {
                while (*f && p < end) {
                    *(p++) = *(f++);
                }
            } else {
                format = strchr (format, '|');
                
                if (format == NULL) {
                    return 0; /* Cannot build the string. */
                }
                format += 1;
                p = text; /* Restart. */
            }

        } else if (*format == '|') {
            
            break; /* We completed this alternative successfully. */
            
        } else {

            *(p++) = *(format++);
        }
        
        if (p >= end) {
            break;
        }
    }

    *p = 0;

    return p > text;
}

/**
 * @brief set a numbered message to the given string
 *
 * This table outlines which message means what<table><tr>
 *	A	estimated time of arrival (not yet implemented).<tr>
 *	B	Direction of travel (bearing).<tr>
 *	C	the name of the city for the selected or current street.<tr>
 *	D	Distance to the destination (set only when a trip is active).<tr>
 *	E	Next sunset time (evening), undefined in night time.<tr>
 *	e	Next sunset time, always defined.<tr>
 *	F	the full name (number, name, city) of the selected or current street.<tr>
 *	H	Altitude.<tr>
 *	M	Next sunrise time (morning), undefined in daylight time.<tr>
 *	m	Next sunrise time, always defined.<tr>
 *	N	the name of the selected or current street.<tr>
 *	P	the name of the selected or current place.<tr>
 *	R	the name of the route or list containing the selected place.<tr>
 *	S	Speed.<tr>
 *	T	Current time, format HH:MM.<tr>
 *	W	Distance to the next waypoint (set only when a trip is active).<tr>
 *	X	Directions to be followed when the next waypoint (with directions) is reached.
 *		(set only when a trip is active).<tr>
 *	Y	Distance to the next waypoint which includes directions, unless the GPS is
 *		"at" that waypoint.  (set only when a trip is active).<tr>
 *	#	the street number range to the selected or current street block.<tr>
 *	s	Total number of satellites.<tr>
 *	v	Total number of available satellites.<tr>
 *	x	Distance from one side of the screen to the other.<tr>
 *	y	Distance from the top to the bottom of the screen.</table>
 *
 *	1	Angle (in vague english) to the next waypoint
 *	2	Angle (in vague english) to the one after that waypoint
 *
 * @param parameter indicates which message to set
 * @param format this and the next parameters are printf-style
 */
void roadmap_message_set (int parameter, const char *format, ...) {
    
    va_list ap;
    char    value[256];
    int changed = 0;
    
    if (parameter <= 0) {
        roadmap_log (ROADMAP_ERROR, "invalid parameter code %d", parameter);
        return;
    }
    
    va_start(ap, format);
    vsnprintf(value, sizeof(value), format, ap);
    va_end(ap);

    if ((RoadMapMessageParameters[parameter] == NULL) != (value[0] == 0)) {
	changed = 1;
    }
    
    if (RoadMapMessageParameters[parameter] != NULL) {
	if (strcmp(RoadMapMessageParameters[parameter], value) != 0) {
	    changed = 1;
	}
        free (RoadMapMessageParameters[parameter]);
    }
    if (value[0] == 0) {
        RoadMapMessageParameters[parameter] = NULL;
    } else {
        RoadMapMessageParameters[parameter] = strdup (value);
    }

    if (changed) RoadMapMessageChanged = 1;
}

char *roadmap_message_get (int parameter) {
    
    return RoadMapMessageParameters[parameter] ? 
    	    RoadMapMessageParameters[parameter] : "";
}


void roadmap_message_unset (int parameter) {
    
    if (parameter <= 0) {
        roadmap_log (ROADMAP_ERROR, "invalid parameter code %d", parameter);
        return;
    }
    
    if (RoadMapMessageParameters[parameter] != NULL) {
        free (RoadMapMessageParameters[parameter]);
        RoadMapMessageParameters[parameter] = NULL;
	RoadMapMessageChanged = 1;
    }
}
