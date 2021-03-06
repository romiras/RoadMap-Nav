/*
 * LICENSE:
 *
 *   Copyright 2006 Ehud Shabtai
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
 * @brief roadmap_turns.h - Manage the turn restrictions table.
 */

#ifndef _ROADMAP_TURNS__H_
#define _ROADMAP_TURNS__H_

#include "roadmap_types.h"
#include "roadmap_dbread.h"

int  roadmap_turns_in_square (int square, int *first, int *last);
int  roadmap_turns_of_line   (int line, int begin, int end,
                                        int *first, int *last);
int roadmap_turns_find_restriction (int node, int from_line, int to_line);

extern roadmap_db_handler RoadMapTurnsHandler;

#endif // _ROADMAP_TURNS__H_
