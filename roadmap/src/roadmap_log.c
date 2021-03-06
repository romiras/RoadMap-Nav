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
 * @brief roadmap_log.c - a module for managing uniform error & info messages.
 *
 * This module is used to control and manage the appearance of messages
 * printed by the roadmap program. The goals are (1) to produce a uniform
 * look, (2) have a central point of control for error management and
 * (3) have a centralized control for routing messages.
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "roadmap.h"
#include "roadmap_path.h"
#include "roadmap_file.h"

#define ROADMAP_LOG_STACK_SIZE 256

static const char *RoadMapLogStack[ROADMAP_LOG_STACK_SIZE];
static int         RoadMapLogStackCursor = 0;

static struct roadmap_message_descriptor {

   int   level;
   int   show_stack;
   int   save_to_file;
   int   do_exit;
   char *prefix;

   RoadMapLogRedirect redirect;

} RoadMapMessageHead [] = {
   {ROADMAP_MESSAGE_DEBUG,   0, 0, 0, "++", NULL},
   {ROADMAP_MESSAGE_INFO,    0, 0, 0, "--", NULL},
   {ROADMAP_MESSAGE_WARNING, 0, 1, 0, "==", NULL},
   {ROADMAP_MESSAGE_ERROR,   1, 1, 0, "**", NULL},
   {ROADMAP_MESSAGE_FATAL,   1, 1, 1, "##", NULL},
   {0,                       1, 1, 1, "??", NULL}
};


static void roadmap_log_noredirect (const char *message) {}


static struct roadmap_message_descriptor *roadmap_log_find (int level) {

   struct roadmap_message_descriptor *category;

   for (category = RoadMapMessageHead; category->level != 0; ++category) {
      if (category->level == level) break;
   }
   return category;
}


void roadmap_log_push (const char *description) {

#ifdef LOGDEBUG
   roadmap_log(ROADMAP_DEBUG, "push of %s", description);
#endif
   if (RoadMapLogStackCursor < ROADMAP_LOG_STACK_SIZE) {
      RoadMapLogStack[RoadMapLogStackCursor++] = description;
   }
}

void roadmap_log_pop (void) {

   if (RoadMapLogStackCursor > 0) {
      RoadMapLogStackCursor -= 1;
#ifdef LOGDEBUG
      roadmap_log(ROADMAP_DEBUG, "pop of %s",
         RoadMapLogStack[RoadMapLogStackCursor]);
#endif
   }
}

void roadmap_log_reset_stack (void) {

   RoadMapLogStackCursor = 0;
}


void roadmap_log_save_all (void) {
    
    int i;
    
    for (i = 0; RoadMapMessageHead[i].level > 0; ++i) {
        RoadMapMessageHead[i].save_to_file = 1;
    }
}


void roadmap_log_save_none (void) {
    
    int i;
    
    for (i = 0; RoadMapMessageHead[i].level > 0; ++i) {
        RoadMapMessageHead[i].save_to_file = 0;
    }
    roadmap_log_purge();
}


int  roadmap_log_enabled (int level, char *source, int line) {
   return (level >= roadmap_verbosity());
}


static void roadmap_log_one (struct roadmap_message_descriptor *category,
                             FILE *file,
                             char  saved,
                             const char *source,
                             int line,
                             const char *format,
                             va_list ap) {

   int i;

   fprintf (file, "%c%s %s, line %d: ", saved, category->prefix, source, line);
   vfprintf(file, format, ap);
   fprintf (file, "\n");
#ifdef ANDROID
   if (saved != ' ')
	   __android_log_vprint (ANDROID_LOG_ERROR, "RoadMap", format, ap);
#endif

   if (category->show_stack && RoadMapLogStackCursor > 0) {

      int indent = 8;

#ifndef ANDROID
      fprintf (file, "   Call stack:\n");
#else
      __android_log_print (ANDROID_LOG_ERROR, "RoadMap", "   Call stack:\n");
#endif

      for (i = 0; i < RoadMapLogStackCursor; ++i) {
#ifndef ANDROID
          fprintf (file, "%*.*s %s\n", indent, indent, "", RoadMapLogStack[i]);
#else
          __android_log_print (ANDROID_LOG_ERROR, "RoadMap",
			  "%*.*s %s\n", indent, indent, "", RoadMapLogStack[i]);
#endif
          indent += 3;
      }
   }
}

static void roadmap_redirect_one (struct roadmap_message_descriptor *category,
                                  const char *format,
                                  va_list ap) {

   char message[1024];

   if (category->redirect != NULL) {

      vsnprintf(message, sizeof(message)-1, format, ap);
      message[0] = toupper(message[0]);

      category->redirect (message);
   }
}


void roadmap_log (int level, const char *source,
			int line, const char *format, ...) {

   FILE *file;
   va_list ap;
   char saved;
   struct roadmap_message_descriptor *category;
   char **enabled;

   if (level < roadmap_verbosity()) return;

   enabled = roadmap_debug();

   if (enabled != NULL) {

      int i;
      char *debug;

      for (i = 0, debug = enabled[0]; debug != NULL; debug = enabled[++i]) {
         if (strcmp (debug, source) == 0) break;
      }

      if (debug == NULL) return;
   }

   category = roadmap_log_find (level);

   va_start(ap, format);

   saved = ' ';

   if (category->save_to_file) {

      file = roadmap_file_fopen (roadmap_path_user(), "postmortem" _TXT, "sa");

      if (file != NULL) {

         roadmap_log_one (category, file, ' ', source, line, format, ap);
         fclose (file);

         va_end(ap);
         va_start(ap, format);

         saved = 's';
      }
   }

   roadmap_log_one (category, stderr, saved, source, line, format, ap);

   va_end(ap);
   va_start(ap, format);

   roadmap_redirect_one (category, format, ap);

   /* for now, assume that if someone has put in a redirect for
    * fatal errors, that they'll also take care of terminating. 
    * (this is only necessary currently for gtk, which doesn't
    * really run its dialog synchronously, and if we exit here,
    * we'll never see it.)
    */
   if (category->do_exit && !category->redirect) {
         exit(1);
   }

   va_end(ap);

}


void roadmap_log_purge (void) {

    roadmap_file_remove (roadmap_path_user(), "postmortem" _TXT);
}


void roadmap_check_allocated_with_source_line
                (const char *source, int line, const void *allocated) {

    if (allocated == NULL) {
        roadmap_log (ROADMAP_MESSAGE_FATAL, source, line, "no more memory");
    }
}


RoadMapLogRedirect roadmap_log_redirect (int level,
                                         RoadMapLogRedirect redirect) {

   RoadMapLogRedirect old;
   struct roadmap_message_descriptor *category;


   category = roadmap_log_find (level);

   old = category->redirect;
   category->redirect = redirect;

   if (old == NULL) old = roadmap_log_noredirect;

   return old;
}


void roadmap_log_cancel_redirect (void) {

   struct roadmap_message_descriptor *category;

   for (category = RoadMapMessageHead; category->level != 0; ++category) {
      category->redirect = NULL;
   }
}

