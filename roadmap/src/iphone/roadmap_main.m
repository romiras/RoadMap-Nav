/*
 * LICENSE:
 *
 *   Copyright 2002 Pascal F. Martin
 *   Copyright 2008 Morten Bek Ditlevsen
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
 * @brief The main function of the RoadMap application for iPhone.
 */

/**
 * @defgroup iPhone iPhone implementation of RoadMap
 */

#include <stdlib.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "roadmap.h"
#include "roadmap_path.h"
#include "roadmap_start.h"
#include "roadmap_config.h"
#include "roadmap_history.h"
#include "roadmap_iphonecanvas.h"
#include "roadmap_iphonemain.h"

#include "roadmap_main.h"
#include "roadmap_time.h"

#import <Foundation/NSDictionary.h>

extern NSString *kUIButtonBarButtonAction;
extern NSString *kUIButtonBarButtonInfo;
extern NSString *kUIButtonBarButtonInfoOffset;
extern NSString *kUIButtonBarButtonSelectedInfo;
extern NSString *kUIButtonBarButtonStyle;
extern NSString *kUIButtonBarButtonTag;
extern NSString *kUIButtonBarButtonTarget;
extern NSString *kUIButtonBarButtonTitle;
extern NSString *kUIButtonBarButtonTitleVerticalHeight;
extern NSString *kUIButtonBarButtonTitleWidth;
extern NSString *kUIButtonBarButtonType;


static RoadMapApp *TheApp;
static int RoadMapMainTabBarAdded = 0;
static int sArgc;
static char ** sArgv;

struct roadmap_main_io {
   NSFileHandle *fh;
   RoadMapIO io;
   RoadMapInput callback;
};

#define ROADMAP_MAX_IO 16
static struct roadmap_main_io RoadMapMainIo[ROADMAP_MAX_IO];

struct roadmap_main_timer {
   NSTimer *timer;
   RoadMapCallback callback;
};

#define ROADMAP_MAX_TIMER 16
static struct roadmap_main_timer RoadMapMainPeriodicTimer[ROADMAP_MAX_TIMER];

static RoadMapCallback idle_callback = NULL;
static char *RoadMapMainTitle = NULL;

static RoadMapKeyInput RoadMapMainInput = NULL;
static UIView      *RoadMapMainBox     = NULL;
static RoadMapCanvasView *RoadMapCanvasBox   = NULL;
//static UIView      *RoadMapMainMenuBar = NULL;
UIToolbar *RoadMapMainTabBar = NULL;
//static UIView      *RoadMapMainStatus  = NULL;
static NSArray     *RoadMapMainTabBarArray = NULL;
static int          buttonCount = 0;
static RoadMapCallback RoadMapMainTabBarCallbacks[6];


void roadmap_main_send_email(const char *subject, const char *contents, 
                             const char *att_path, const char *att_name)
{
    char text[4096];
    if (att_path)
        snprintf(text, 4096, "mailto:?subject=%s&body=%s&attachment=%s/%s", subject, contents, att_path, att_name);
    else
        snprintf(text, 4096, "mailto:?subject=%s&body=%s", subject, contents);
    CFStringRef string = CFStringCreateWithCString(NULL, text, kCFStringEncodingUTF8);
    CFStringRef urlString = CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault, string, NULL, NULL, kCFStringEncodingUTF8);
    [TheApp openURL: [NSURL URLWithString: (NSString *)urlString]];
}

static char *roadmap_main_toolbar_icon (const char *icon) {
    unsigned int i;

    if (icon == NULL)
        return NULL;

    const char *icon_file = roadmap_path_search_icon (icon);
    if (icon_file == NULL)
        return NULL;

    for (i = 0; i < strlen(icon_file); i++)
    {
        if (strncmp (icon_file + i, "resources", 9) == 0)
	{
            return (char *)(icon_file + i);
	}
    }
    return NULL;
}

void roadmap_main_toggle_full_screen (void) {
   /* Don't care abobut full screen on the iPhone */
}

void roadmap_main_new (const char *title, int width, int height) {
    [TheApp newWithTitle: title andWidth: width andHeight: height];
}

void roadmap_main_title(char *fmt, ...) {
   /* Don't care about titles on the iPhone */
}

void roadmap_main_set_keyboard (RoadMapKeyInput callback) {
   RoadMapMainInput = callback;
}

RoadMapMenu roadmap_main_new_menu (const char *title) {
   return NULL;
}


void roadmap_main_free_menu (RoadMapMenu menu) {
     NSLog (@"roadmap_main_free_menu\n");
}


void roadmap_main_add_menu (RoadMapMenu menu, const char *label) {
     NSLog (@"roadmap_main_add_menu label: %s\n", label);
/*
   UIView *menu_item;

   if (RoadMapMainMenuBar == NULL) {

      RoadMapMainMenuBar = gtk_menu_bar_new();

      gtk_box_pack_start
         (GTK_BOX(RoadMapMainBox), RoadMapMainMenuBar, FALSE, TRUE, 0);
   }

   menu_item = gtk_menu_item_new_with_label (label);
   gtk_menu_shell_append (GTK_MENU_SHELL(RoadMapMainMenuBar), menu_item);

   gtk_menu_item_set_submenu (GTK_MENU_ITEM(menu_item), (UIView *) menu);
*/
}


void roadmap_main_popup_menu (RoadMapMenu menu,
                              const RoadMapGuiPoint *position) {
     NSLog (@"roadmap_main_popup_menu\n");

 /*  if (menu != NULL) {
      gtk_menu_popup (GTK_MENU(menu),
                      NULL,
                      NULL,
                      NULL,
                      NULL,
                      0,
                      gtk_get_current_event_time());
   }
   */
}


void roadmap_main_add_menu_item (RoadMapMenu menu,
                                 const char *label,
                                 const char *tip,
                                 RoadMapCallback callback) {

     NSLog (@"roadmap_main_add_menu_item label: %s tip: %s\n", label, tip);
/*
   UIView *menu_item;

   if (label != NULL) {

      menu_item = gtk_menu_item_new_with_label (label);
      g_signal_connect (menu_item, "activate",
                        (GCallback)roadmap_main_activate,
                        callback);
   } else {
      menu_item = gtk_menu_item_new ();
   }
   gtk_menu_shell_append (GTK_MENU_SHELL(menu), menu_item);
   gtk_widget_show(menu_item);

   if (tip != NULL) {
      gtk_tooltips_set_tip (gtk_tooltips_new (), menu_item, tip, NULL);
   }
   */
}


void roadmap_main_add_separator (RoadMapMenu menu) {
   NSLog (@"roadmap_main_add_seperator\n");

   roadmap_main_add_menu_item (menu, NULL, NULL, NULL);
}


void roadmap_main_add_toolbar (const char *orientation) {
     NSLog (@"roadmap_main_add_toolbar orientation: %s\n", orientation);
     RoadMapMainTabBarAdded = 1;
     RoadMapMainTabBarArray = [[NSArray array] init];
}

void roadmap_main_add_tool (const char *label,
                            const char *icon,
                            const char *tip,
                            RoadMapCallback callback) {
    char *iconstr;
    NSString *nsicon, *nslabel;

//    NSLog (@"roadmap_main_add_tool label: %s icon: %s tip: %s\n", label, icon, tip);
    if (buttonCount >= 5)
    {
        NSLog (@"roadmap_main_add_tool only room for 5 buttons\n");
        return;
    }

    NSArray *tmp = RoadMapMainTabBarArray;
    nslabel = [[NSString alloc] initWithUTF8String:icon];
    iconstr = roadmap_main_toolbar_icon(icon);
    UIImage *image = NULL;
    if (iconstr)
    {
        nsicon = [[NSString alloc] initWithUTF8String:iconstr];
        image = [UIImage imageNamed: nsicon];
    }

    UIBarButtonItem *item = [UIBarButtonItem alloc];
    if (image == NULL)
       [item initWithTitle: nslabel style: UIBarButtonItemStylePlain target: TheApp action: @selector(buttonPressed:)];
    else
       [item initWithImage: image style: UIBarButtonItemStylePlain target: TheApp action: @selector(buttonPressed:)];
    item.tag = buttonCount;
    item.width = 64.0f;
    RoadMapMainTabBarCallbacks[buttonCount] = callback;

    RoadMapMainTabBarArray = [tmp arrayByAddingObject: item];
    buttonCount++;
}


void roadmap_main_add_tool_space (void) {
    /* Don't care about spaces on the iPhone... */
}

static unsigned long roadmap_main_busy_start;

void roadmap_main_set_cursor (RoadMapCursor newcursor) {
    /* Don't care about cursors on the iPhone... */
}

void roadmap_main_busy_check(void) {

   if (roadmap_main_busy_start == 0)
      return;

   if (roadmap_time_get_millis() - roadmap_main_busy_start > 1000) {
      roadmap_main_set_cursor (ROADMAP_CURSOR_WAIT);
   }
}

void roadmap_main_add_canvas (void) {
    //struct CGRect rect = [UIHardware fullScreenApplicationContentRect];
    struct CGRect rect = [RoadMapMainBox frame];
    rect.origin.x = 0.0f;
    rect.origin.y = 20.0f;
    if (RoadMapMainTabBarAdded)
       rect.size.height = 480.0f - 49.0f - 20.0f;
    else
       rect.size.height = 480.0f - 20.0f;

    RoadMapCanvasBox = [[RoadMapCanvasView alloc] initWithFrame: rect];
    [RoadMapMainBox addSubview: RoadMapCanvasBox];
}

void roadmap_main_add_status (void) {
   /* Status bar seems like a waste of space on the iPhone... */
}


void roadmap_main_show (void) {
    /* Since this is called after the toolbar is
       configured, we use this to add the finalized
       toolbar to the main view */
     if (RoadMapMainTabBarAdded)
     {
        RoadMapMainTabBar = [TheApp createButtonBar];
        [RoadMapMainBox addSubview: RoadMapMainTabBar]; 
     }
}

void roadmap_main_set_input (RoadMapIO *io, RoadMapInput callback) {
    [TheApp setInput: io andCallback: callback];
}

void roadmap_main_remove_input (RoadMapIO *io) {
    [TheApp removeInput: io];
}

void roadmap_main_set_periodic (int interval, RoadMapCallback callback) {
    [TheApp setPeriodic: (interval*0.001) andCallback: callback];
}


void roadmap_main_remove_periodic (RoadMapCallback callback) {
   [TheApp removePeriodic: callback];
}


void roadmap_main_set_status (const char *text) {
//   NSLog (@"roadmap_main_set_status text: %s\n", text);
}

void roadmap_main_set_idle_function (RoadMapCallback callback) {
    idle_callback = callback;
    [TheApp setPeriodic: 0.00 andCallback: callback];
}

void roadmap_main_remove_idle_function (void) {
   [TheApp removePeriodic: idle_callback];
   idle_callback = NULL;
}


int roadmap_main_flush (void) {
/*   if ([[NSNotificationCenter defaultCenter] isEmpty])

      return 0;

   
   NSLog (@"notificationcenter has notifications\n");
   return 1;
*/

//   NSLog (@"roadmap_main_flush\n");
   //[[NSRunLoop currentRunLoop] run];
   
   double resolution = 0.0001;

  

       NSDate* next = [NSDate dateWithTimeIntervalSinceNow:resolution];

       [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode

                   beforeDate:next];

  


/*
   while (gtk_events_pending ()) {
      if (gtk_main_iteration ()) {
         exit(0);  // gtk_main_quit() called 
      }
   }
   */
   return 0;
}


int roadmap_main_flush_synchronous (int deadline) {
//   NSLog (@"roadmap_main_flush_synchronous\n");

   long start_time, duration;

   start_time = roadmap_time_get_millis();

  /* while (gtk_events_pending ()) {
      if (gtk_main_iteration ()) {
         exit(0); 
      }
   }
   */
//   gdk_flush();

   duration = roadmap_time_get_millis() - start_time;

   if (duration > deadline) {

      roadmap_log (ROADMAP_DEBUG, "processing flush took %d", duration);

      return 0; /* Busy. */
   }

   return 1; /* Not so busy. */
}

void roadmap_main_exit (void) {

 /*  static int exit_done;

   if (!exit_done++) {
      roadmap_start_exit ();
*/
      // YYYY [TheApp terminate];
   exit(0);
}

int main (int argc, char **argv) {
    int i;
    int j = 0;
    int returnCode;
    sArgc = argc;
    sArgv = (char **)malloc(argc * (sizeof (char*)));
    for (i=0; i<argc; i++)
    {
        if (strcmp(argv[i], "--launchedFromSB") != 0) {
            sArgv[i] = strdup(argv[j]);
            j++;
        }
        else
           sArgc--;
    }
    putenv("HOME=/var/mobile/Library/");

    // Add paths to roadmap drivers and to flite
    putenv("PATH=/Applications/RoadMap.app/bin:/usr/bin");
    
    roadmap_option (sArgc, sArgv, 0, NULL);
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    returnCode = UIApplicationMain(sArgc, sArgv, @"RoadMapApp", @"RoadMapApp");
    [pool release];
    return returnCode;
}

@implementation RoadMapApp
@synthesize window, view;

-(RoadMapApp *) init
{
  self = [super init];
  return self;
}

- (UIToolbar *) createButtonBar {
    UIToolbar *buttonBar;
    buttonBar = [ [ UIToolbar alloc ]
       initWithFrame: CGRectMake(0.0f, 480.0f - 49.0f, 320.0f, 49.0f)];

    buttonBar.items = RoadMapMainTabBarArray;
    return buttonBar;
}

- (void)buttonPressed: (id) sender {
    int button = [ sender tag ];
    (RoadMapMainTabBarCallbacks[button] ) ();
}

-(void) newWithTitle: (const char *)title andWidth: (int) width andHeight: (int) height
{
    self.window = [[UIWindow alloc] initWithFrame: [[UIScreen mainScreen] bounds]];
    self.view = [[UIView alloc] initWithFrame: [[UIScreen mainScreen] bounds]];
    [self.window addSubview: self.view];
    [self.window makeKeyAndVisible];
    RoadMapMainBox = self.view;

    if (RoadMapMainTitle != NULL) {
        free(RoadMapMainTitle);
    }
    RoadMapMainTitle = strdup (title);
}

- (void) periodicCallback: (NSTimer *) timer
{
   int i;
   for (i = 0; i < ROADMAP_MAX_TIMER; ++i) {
      if (RoadMapMainPeriodicTimer[i].timer == timer) {
         (* RoadMapMainPeriodicTimer[i].callback) ();
         break;
      }
   }
}

-(void) setPeriodic: (float) interval andCallback: (RoadMapCallback) callback
{
   int index;
   struct roadmap_main_timer *timer = NULL;

   for (index = 0; index < ROADMAP_MAX_TIMER; ++index) {

      if (RoadMapMainPeriodicTimer[index].callback == callback) {
         return;
      }
      if (timer == NULL) {
         if (RoadMapMainPeriodicTimer[index].callback == NULL) {
            timer = RoadMapMainPeriodicTimer + index;
         }
      }
   }

   if (timer == NULL) {
      roadmap_log (ROADMAP_FATAL, "Timer table saturated");
   }

   timer->callback = callback;
   timer->timer = [NSTimer scheduledTimerWithTimeInterval: interval
                     target: self
                     selector: @selector(periodicCallback:)
                     userInfo: nil
                     repeats: YES];
}

-(void) removePeriodic: (RoadMapCallback) callback
{
   int index;

   for (index = 0; index < ROADMAP_MAX_TIMER; ++index) {

      if (RoadMapMainPeriodicTimer[index].callback == callback) {

         RoadMapMainPeriodicTimer[index].callback = NULL;
         [RoadMapMainPeriodicTimer[index].timer invalidate];
         return;
      }
   }
}

-(void) ioCallback: (id) notify
{
   NSFileHandle *fh = [notify object];
   int i;
   int fd = [fh fileDescriptor];

   for (i = 0; i < ROADMAP_MAX_IO; ++i) {
      if (RoadMapMainIo[i].io.os.file == fd) {
         (* RoadMapMainIo[i].callback) (&RoadMapMainIo[i].io);
            [RoadMapMainIo[i].fh waitForDataInBackgroundAndNotify];
         break;
      }
   }

}

-(void) setInput: (RoadMapIO*) io andCallback: (RoadMapInput) callback
{
    int i;
    int fd = io->os.file; /* All the same on UNIX. */
    NSFileHandle *fh = [[NSFileHandle alloc] initWithFileDescriptor: fd];

    for (i = 0; i < ROADMAP_MAX_IO; ++i) {
        if (RoadMapMainIo[i].io.subsystem == ROADMAP_IO_INVALID) {
            RoadMapMainIo[i].io = *io;
            RoadMapMainIo[i].callback = callback;
            RoadMapMainIo[i].fh = fh;
            [[NSNotificationCenter defaultCenter]
                addObserver: self
                selector:@selector(ioCallback:)
                name:NSFileHandleDataAvailableNotification
                object:fh];
            [fh waitForDataInBackgroundAndNotify];
            break;
        }
    }
}

-(void) removeInput: (RoadMapIO*) io
{
    int i;
    int fd = io->os.file; /* All the same on UNIX. */

    for (i = 0; i < ROADMAP_MAX_IO; ++i) {
        if (RoadMapMainIo[i].io.os.file == fd) {
            [[NSNotificationCenter defaultCenter]
                removeObserver: self
                name:NSFileHandleDataAvailableNotification
                object:RoadMapMainIo[i].fh];
            RoadMapMainIo[i].io.os.file = -1;
            RoadMapMainIo[i].io.subsystem = ROADMAP_IO_INVALID;
            break;
        }
    }
}

- (void) applicationDidFinishLaunching: (UIApplication *) application
{
    TheApp = self;
    int i;
    for (i = 0; i < ROADMAP_MAX_IO; ++i) {
        RoadMapMainIo[i].io.os.file = -1;
        RoadMapMainIo[i].io.subsystem = ROADMAP_IO_INVALID;
    }

    roadmap_start (sArgc, sArgv);
    
  //YYY  [self reportAppLaunchFinished];
}

- (void)applicationWillSuspend
{
 //  [self terminate];
//   printf("go to sleep\n");
}

- (void)applicationDidResume
{
  printf("I am alive!\n");

}

- (void)applicationWillTerminate
{
    static int exit_done;

    if (!exit_done++) {
      roadmap_start_exit ();
    }
    [self release];
}

@end

