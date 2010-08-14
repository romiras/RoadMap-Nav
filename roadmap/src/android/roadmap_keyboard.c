/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright 2010 Danny Backx
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
 * @brief Provide a keyboard widget for RoadMap dialogs.
 * @ingroup android
 */

#include <math.h>
#include <stdlib.h>

#include "roadmap_keyboard.h"

#include "roadmap.h"


#define ROADMAP_KEYBOARD_ROWS      4
#define ROADMAP_KEYBOARD_COLUMNS  10
#define ROADMAP_KEYBOARD_KEYS (ROADMAP_KEYBOARD_ROWS*ROADMAP_KEYBOARD_COLUMNS)

typedef struct {

   char character;

//   GtkWidget       *button;
   RoadMapKeyboard  keyboard;

} RoadMapKey;


struct roadmap_keyboard_context {

//   GtkWidget *frame;
//   GtkWidget *focus;

   RoadMapKey keys[ROADMAP_KEYBOARD_KEYS];
};

/**
 * @brief unused on Android
 * @return always null
 */
RoadMapKeyboard roadmap_keyboard_new (void)
{
	return NULL;
}
