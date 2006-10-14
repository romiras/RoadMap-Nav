/* roadmap_factory.c - The menu/toolbar/shortcut factory for RoadMap.
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
 *
 * SYNOPSYS:
 *
 *   See roadmap_factory.h
 */

#include <string.h>

#include "roadmap.h"
#include "roadmap_config.h"
#include "roadmap_main.h"
#include "roadmap_preferences.h"
#include "roadmap_help.h"

#include "roadmap_factory.h"


static RoadMapConfigDescriptor RoadMapConfigGeneralToolbar =
                        ROADMAP_CONFIG_ITEM("General", "Toolbar");

static RoadMapConfigDescriptor RoadMapConfigGeneralIcons =
                        ROADMAP_CONFIG_ITEM("General", "Icons");


const char RoadMapFactorySeparator[] = "--separator--";
const char RoadMapFactoryHelpTopics[] = "--help-topics--";

static const RoadMapFactory *RoadMapFactoryBindings = NULL;


static void roadmap_factory_keyboard (char *key) {

   const RoadMapFactory *binding;

   if (RoadMapFactoryBindings == NULL) return;

   for (binding = RoadMapFactoryBindings; binding->name != NULL; ++binding) {

      if (strcasecmp (binding->name, key) == 0) {
         if (binding->callback != NULL) {
            (*binding->callback) ();
            break;
         }
      }
   }
}

static void roadmap_factory_add_help (void) {

   int ok;
   const char *label;
   RoadMapCallback callback;

   for (ok = roadmap_help_first_topic(&label, &callback);
        ok;
        ok = roadmap_help_next_topic(&label, &callback)) {

      roadmap_main_add_menu_item (label, label, callback);
   }
}


void roadmap_factory (const RoadMapFactory *menu,
                      const RoadMapFactory *toolbar,
                      const RoadMapFactory *shortcuts) {

   int use_toolbar =
            (strcasecmp (roadmap_config_get (&RoadMapConfigGeneralToolbar),
                         "yes") == 0);

   int use_icons =
            (strcasecmp (roadmap_config_get (&RoadMapConfigGeneralIcons),
                         "yes") == 0);


   while (menu->name != NULL) {

      if (menu->callback == NULL) {
         if (menu->name == RoadMapFactorySeparator) {
            roadmap_main_add_separator ();
         } else if (menu->name == RoadMapFactoryHelpTopics) {
            roadmap_factory_add_help ();
         } else {
            roadmap_main_add_menu (menu->name);
         }
      } else {
         roadmap_main_add_menu_item (menu->name, menu->tip, menu->callback);
      }

      menu += 1;
   }

   if (use_toolbar) {

      while (toolbar->name != NULL) {

         if (toolbar->callback == NULL) {
            if (toolbar->name == RoadMapFactorySeparator) {
               roadmap_main_add_tool_space ();
            }
         } else if (use_icons) {
            roadmap_main_add_tool
               (toolbar->name, toolbar->icon, toolbar->tip, toolbar->callback);
         } else {
            roadmap_main_add_tool
               (toolbar->name, NULL, toolbar->tip, toolbar->callback);
         }

         toolbar += 1;
      }
   }

   RoadMapFactoryBindings = shortcuts;

   roadmap_main_set_keyboard (roadmap_factory_keyboard);
}
