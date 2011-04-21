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
   int         i, ok;
   NavigateIteration   *p, *q;
   NavigateStatus   rev;

   if (! Algo) {
      roadmap_log (ROADMAP_WARNING, "navigate_route_recalc -> no algorithm");
      return -1;
   }

   /* Has the navigation algorithm run on this status yet ? */
   if (stp->first == NULL) {
      roadmap_log (ROADMAP_WARNING, "navigate_route_recalc -> no previous route");
      return -1;
   }

   // roadmap_log (ROADMAP_WARNING, "navigate_route_recalc");
   roadmap_log (ROADMAP_WARNING, "navigate_route_recalc from %d (%d %d) to %d (%d %d)", stp->first->segment->line.line_id, stp->first->segment->from_pos.longitude, stp->first->segment->from_pos.latitude, stp->last->segment->line.line_id, stp->last->segment->to_pos.longitude, stp->last->segment->to_pos.latitude);


   /* Delete nodes in the old route */
   p = stp->first;
   if (p->next)
      for (p = p->next; p; p = q) {
        q = p->next;
        if (p != stp->last)
	   free(p);
      }

   stp->first->next = stp->first->prev = NULL;
   stp->last->next = stp->last->prev = NULL;
   stp->current = stp->first;

   stp->first->cost.distance = 0;
   stp->last->cost.distance = 0;
   stp->first->cost.time = 0;
   stp->last->cost.time = 0;


   /* Prepare for running the algorithm from both directions */
   rev.first = stp->last;
   rev.last = stp->first;

   stp->iteration = 1;
   ok = 0;
   while (stp->iteration <= Algo->max_iterations) {
      Algo->step_fn(Algo, stp);
      if (Algo->end_fn(stp)) {
         ok = 1;
         break;
      }
      if (Algo->both_ways) {
         Algo->step_fn(Algo, &rev);
         if (Algo->end_fn(stp)) {
            ok = 1;
            break;
         }
      }
      stp->iteration++;
   }

   /* Hook up the last entry */
   stp->current->next = stp->last;
   stp->last->prev = stp->current;

   if (! ok)
      roadmap_log (ROADMAP_WARNING, "navigate_route_recalc : max #iterations reached (%d).",
            Algo->max_iterations);
   else {
      roadmap_log (ROADMAP_WARNING,
         "navigate_route_recalc, %d iterations, %d hops\n\t%s: %.1f %s, %s: %.1f %s",
	 stp->iteration, navigate_route_numhops(stp),
	 roadmap_lang_get ("Length"),
	 stp->current->cost.distance / 1000.0,
	 roadmap_lang_get ("Km"),
	 roadmap_lang_get ("Time"),
	 stp->current->cost.time / 60.0,
	 roadmap_lang_get ("minutes"));
      int i;
      NavigateIteration *p;
      for (i=0, p=stp->first; p; p=p->next, i++)
         fprintf(stderr, "\t[%d] %d (%d %d -> %d %d) %.1f %.1f {%d} %p\n",
	    i, p->segment->line.line_id,
	    p->segment->from_pos.longitude, p->segment->from_pos.latitude,
	    p->segment->to_pos.longitude, p->segment->to_pos.latitude,
	    p->cost.distance / 1000.0, p->cost.time / 60.0,
	    roadmap_math_distance(&p->segment->from_pos, &stp->last->segment->to_pos),
	    p);
      fprintf(stderr, "First %p Last %p Current %p Last->Prev %p\n",
         stp->first, stp->last, stp->current, stp->last->prev);
   }

   return 0;
}

int navigate_route_numhops(NavigateStatus *stp)
{
   NavigateIteration *p;
   int i;

   for (i=0, p=stp->first; p; p = p->next)
      i++;
   return i;
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
   int         i, ok;
   NavigateIteration   *p;
   NavigateStatus   status, *stp = &status, rev;

   roadmap_log (ROADMAP_WARNING, "navigate_route_get_initial from %d (%d %d) to %d (%d %d)", from_line->line_id, from_pos.longitude, from_pos.latitude, to_line->line_id, to_pos.longitude, to_pos.latitude);

   if (! Algo)
      return (NavigateStatus) {NULL, NULL, NULL, 0, 0};

   /* navigate_route_setup(); */

   /* Set up the start info */
   stp->first = calloc(1, sizeof(struct NavigateIteration));
   stp->last = calloc(1, sizeof(struct NavigateIteration));

   stp->first->prev = status.first->next = NULL;
   stp->last->prev = status.last->next = NULL;
   stp->first->segment = calloc(1, sizeof(struct NavigateSegment));
   stp->last->segment = calloc(1, sizeof(struct NavigateSegment));

   stp->first->segment->line = *from_line;
   stp->first->segment->from_pos = from_pos;
   stp->first->segment->to_pos = from_pos;

   stp->last->segment->line = *to_line;
   stp->last->segment->from_pos = to_pos;
   stp->last->segment->to_pos = to_pos;

   stp->first->cost.distance = 0;
   stp->last->cost.distance = 0;
   stp->first->cost.time = 0;
   stp->last->cost.time = 0;

   stp->maxdist = roadmap_math_distance(&from_pos, &to_pos);

   stp->current = status.first;

   /*
    * Prepare for running the algorithm from both directions
    */
   rev.first = stp->last;
   rev.last = stp->first;

   stp->iteration = 1;
   ok = 0;
   while (stp->iteration <= Algo->max_iterations) {
      Algo->step_fn(Algo, stp);
      if (Algo->end_fn(stp)) {
         ok = 1;
         break;
      }
      if (Algo->both_ways) {
         Algo->step_fn(Algo, &rev);
         if (Algo->end_fn(stp)) {
            ok = 1;
            break;
         }
      }
      stp->iteration++;
   }

   if (ok) {
      roadmap_log (ROADMAP_WARNING, "navigate_route_get_initial, %d iterations", stp->iteration);

      /* Hook up the last entry */
      stp->current->next = stp->last;
      stp->last->prev = stp->current;
   } else {
      roadmap_log (ROADMAP_WARNING, "Max #iterations reached (%d).", Algo->max_iterations);
   }

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
