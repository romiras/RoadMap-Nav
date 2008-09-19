/* roadmap_messagebox.cc - The C to C++ wrapper for the QT RoadMap message box.
 *
 * LICENSE:
 *
 *   (c) Copyright 2003 Latchesar Ionkov
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
 *   See roadmap_messagebox.h
 */
#include <qmessagebox.h>

extern "C" {
#include "roadmap_start.h"
#define __ROADMAP_MESSAGEBOX_NO_LANG
#include "roadmap_messagebox.h"
}

void roadmap_messagebox_hide(void *handle) {
   QMessageBox *mb = (QMessageBox *)handle;
   mb->~QMessageBox();
}

void *roadmap_messagebox(const char* title, const char* message) {
   QMessageBox *mb = 
    new QMessageBox ( title, message,
	QMessageBox::Information,
	QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton,
	(QWidget *)0, (const char *)0, FALSE);
   mb->show();
   return mb;
}

void *roadmap_messagebox_wait(const char* title, const char* message) {
   QMessageBox *mb = 
    new QMessageBox ( title, message,
	QMessageBox::Information,
	QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton,
	(QWidget *)0, (const char *)0, TRUE);
   mb->show();
   return mb;
}

void roadmap_messagebox_die(const char* title, const char* message) {
   QMessageBox::critical(0, title, message);
   exit(1);
}
