/*
 * LICENSE:
 *
 *   Copyright (c) 2008, 2009, 2011 by Danny Backx.
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
 * @brief entry points for route calculation, to become a user centric plugin mechanism 
 * @ingroup NavigatePlugin
 */

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "roadmap.h"
#include "roadmap_point.h"
#include "roadmap_math.h"
#include "navigate.h"
#include "navigate/navigate_simple.h"
#include "navigate_route.h"

static NavigateAlgorithm *Algo = NULL;
static int nAlgorithms = 0;
static int alloc = 0;

/**
 * @brief register an algorithm
 * @param algo the structure representing it
 */
void navigate_algorithm_register(NavigateAlgorithm *algo)
{
	if (nAlgorithms == alloc) {
		alloc += 4;
		Algo = (NavigateAlgorithm *)realloc((void *)Algo,
				sizeof(NavigateAlgorithm) * alloc);
	}
	Algo[nAlgorithms] = *algo;
	nAlgorithms++;
}

/**
 * @brief recalculate the route
 * @param stp pointer to current route
 * @return 0 for success
 */
int navigate_route_recalc (NavigateStatus *stp)
{
   roadmap_log (ROADMAP_WARNING, "navigate_route_recalc");
   return 0;
}

/**
 * @brief calculate the basic route.
 *
 * Called from navigate_calc_route and navigate_recalc_route.
 * Result will be placed in the "segments" parameter, number of steps in "size".
 *
 * @param from_line
 * @param from_pos
 * @param to_line
 * @param to_pos
 * @return track time
 */
NavigateStatus navigate_route_get_initial (PluginLine *from_line,
					   RoadMapPosition from_pos,
					   PluginLine *to_line,
					   RoadMapPosition to_pos)
{
	int			i, ok;
	NavigateIteration	*p;
	NavigateStatus	status, rev;

	roadmap_log (ROADMAP_WARNING, "navigate_route_get_initial");

	if (! Algo)
		return (NavigateStatus) {NULL, NULL, NULL, 0, 0};

	/* navigate_route_setup(); */

	/* Set up the start info */
	status.first = calloc(1, sizeof(struct NavigateIteration));
	status.last = calloc(1, sizeof(struct NavigateIteration));

	status.first->prev = status.first->next = NULL;
	status.last->prev = status.last->next = NULL;
	status.first->segment = calloc(1, sizeof(struct NavigateSegment));
	status.last->segment = calloc(1, sizeof(struct NavigateSegment));

	status.first->segment->line = *from_line;
	status.first->segment->from_pos = from_pos;
	status.first->segment->to_pos = from_pos;

	status.last->segment->line = *to_line;
	status.last->segment->from_pos = to_pos;
	status.last->segment->to_pos = to_pos;

	status.first->cost.distance = 0;
	status.last->cost.distance = 0;
	status.first->cost.time = 0;
	status.last->cost.time = 0;

	status.first->cost.distance = 0;
	status.first->cost.time = 0;

	status.maxdist = roadmap_math_distance(&from_pos, &to_pos);

	status.current = status.first;

	/*
	 * Prepare for running the algorithm from both directions
	 */
	rev.first = status.last;
	rev.last = status.first;

	status.iteration = 1;
	ok = 0;
	while (status.iteration <= Algo->max_iterations) {
		Algo->step_fn(Algo, &status);
		if (Algo->end_fn(&status)) {
			ok = 1;
			break;
		}
		if (Algo->both_ways) {
			Algo->step_fn(Algo, &rev);
			if (Algo->end_fn(&status)) {
				ok = 1;
				break;
			}
		}
		status.iteration++;
	}
	if (! ok)
		roadmap_log (ROADMAP_WARNING, "Max #iterations reached (%d).",
				Algo->max_iterations);
	else
	   roadmap_log (ROADMAP_WARNING, "navigate_route_get_initial, %d iterations", status.iteration);

	return status;
}

/**
 * @brief
 * @return
 */
int navigate_route_reload_data (void)
{
	return 0;
}

/**
 * @brief
 * @return
 */
int navigate_route_load_data (void)
{
   return 0;
}
