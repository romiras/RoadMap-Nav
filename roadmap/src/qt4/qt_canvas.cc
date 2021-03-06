/* qt_canvas.cc - A QT implementation for the RoadMap canvas
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
 *   See qt_canvas.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <qpolygon.h>
#include <qpainter.h>
#include "qt_canvas.h"
#include <qdebug.h>


RMapCanvas *roadMapCanvas = 0;
RoadMapCanvasMouseHandler phandler = 0;
RoadMapCanvasMouseHandler rhandler = 0;
RoadMapCanvasMouseHandler mhandler = 0;
RoadMapCanvasMouseHandler whandler = 0;
RoadMapCanvasConfigureHandler chandler = 0;

// Implementation of RMapCanvas class
RMapCanvas::RMapCanvas(QWidget* parent):QWidget(parent) {
   pixmap = 0;
   currentPen = 0;
   roadMapCanvas = this;
   basePen = createPen("stubPen");
   setPenThickness(2);

   /* turn off the default double-buffering */
   this->setAttribute(Qt::WA_PaintOnScreen);

   initColors();

   registerButtonPressedHandler(phandler);
   registerButtonReleasedHandler(rhandler);
   registerMouseMoveHandler(mhandler);
   registerMouseWheelHandler(whandler);

   registerConfigureHandler(chandler);
   //setBackgroundMode(Qt::NoBackground);
   //setAttribute(Qt::WA_NoBackgrond);
}

RMapCanvas::~RMapCanvas() {
   if (pixmap != 0) {
      delete pixmap;
      pixmap = 0;
   }

   // TODO: delete pens
}

RoadMapPen RMapCanvas::createPen(const char* name) {
   RoadMapPen p = pens[name];

   if (p == 0) {
      p = new roadmap_canvas_pen();
      QPen* pen = new QPen(Qt::SolidLine/*Qt::DotLine*/);
      p->pen = pen;
      p->font = new QFont("Arial",12);
      p->brush = new QBrush();
      p->fontcolor = new QColor("#000000");
      p->capitalize = 0;
      p->background = 0;
      p->buffersize = 0;
      p->buffercolor = new QColor("#ffffff");
      p->pen->setJoinStyle(Qt::RoundJoin);
      p->pen->setCapStyle(Qt::RoundCap);
      pens.insert(name, p);
   }

   currentPen = p;

   return p;
}

void RMapCanvas::selectPen(RoadMapPen p) {
   currentPen = p;
}

void RMapCanvas::setPenColor(const char* color) {
   if (currentPen != 0) {
     int opacity = currentPen->pen->color().alpha();
     currentPen->pen->setColor(getColor(color));
     currentPen->pen->color().setAlpha(opacity);
   }

}

void RMapCanvas::setPenLineStyle(const char* style) {
  if (currentPen != 0) {
    if (strcasecmp(style,"blank")==0) {
      currentPen->pen->setStyle((Qt::PenStyle)0);
    } else if (strcasecmp(style,"solid")==0) {
      currentPen->pen->setStyle((Qt::PenStyle)1);
    } else if (strcasecmp(style,"dashed")==0) {
      currentPen->pen->setStyle((Qt::PenStyle)2);
    } else if (strcasecmp(style,"dot")==0) {
      currentPen->pen->setStyle((Qt::PenStyle)3);
    } else if (strcasecmp(style,"dashdot")==0) {
      currentPen->pen->setStyle((Qt::PenStyle)4);
    } else if (strcasecmp(style,"dashdotdot")==0) {
      currentPen->pen->setStyle((Qt::PenStyle)5);
    }
  }
}

void RMapCanvas::setPenThickness(int thickness) {
   if (currentPen != 0) {
      currentPen->pen->setWidth(thickness);
   }
}

void RMapCanvas::setFontSize(int size) {
  if (currentPen != 0) {
    currentPen->font->setPointSize(size);
  }
}

int RMapCanvas::getPenThickness(RoadMapPen p) {
   if (p != 0) {
      return (int) p->pen->width();
   }
   return 1;
}

void RMapCanvas::setPenOpacity(int opacity) {

  if (currentPen != 0) {
    QColor c(currentPen->pen->color().rgb());
   
    c.setAlpha(opacity);
    currentPen->pen->setColor(c);

  }
}

void RMapCanvas::setPenLighter(int factor) {

  if (currentPen != 0) {
    QColor c(currentPen->pen->color().light(factor));

    currentPen->pen->setColor(c);
  }
}

void RMapCanvas::setPenDarker(int factor) {

  if (currentPen != 0) {
    QColor c(currentPen->pen->color().dark(factor));

    currentPen->pen->setColor(c);
  }
}

void RMapCanvas::setPenLineCapStyle(int cap) {
  if (currentPen != 0) {
    currentPen->pen->setCapStyle((Qt::PenCapStyle)cap);
  }
}

void RMapCanvas::setPenLineJoinStyle(int join) {
  if (currentPen != 0) {
    currentPen->pen->setJoinStyle((Qt::PenJoinStyle)join);
  }
}

void RMapCanvas::setBrushColor(const char *color) {
  if (currentPen != 0) {
    int opacity = currentPen->brush->color().alpha();
    QColor c(color);
    c.setAlpha(opacity);
    currentPen->brush->setColor(color);
    if (currentPen->brush->style()==Qt::NoBrush)
      currentPen->brush->setStyle(Qt::SolidPattern);
  }
}

void RMapCanvas::setBrushStyle(int style) {
  if (currentPen != 0) {
    currentPen->brush->setStyle((Qt::BrushStyle)style);
  }
}

void RMapCanvas::setBrushOpacity(int opacity) {
  if (currentPen != 0) {
    QColor c(currentPen->brush->color().rgb());
    c.setAlpha(opacity);
    currentPen->brush->setColor(c);
  }
}


void RMapCanvas::setFontName(const char *name) {
  if (currentPen != 0) {
    currentPen->font->setFamily(name);
    QFont f(*currentPen->font);
    if (!f.exactMatch()) {
       qWarning("font family: <%s> match not found",f.family().trimmed().toUtf8().constData());  
    }
  }
}

void RMapCanvas::setFontColor(const char *color) {
  if (currentPen != 0) {
    currentPen->fontcolor = new QColor(color);
  }
}

void RMapCanvas::setFontWeight(int weight) {
  if (currentPen != 0) {
    currentPen->font->setWeight(weight);
  }
}

void RMapCanvas::setFontSpacing(int spacing) {
  if (currentPen != 0) {
     currentPen->font->setStretch(spacing);
  }
}

void RMapCanvas::setFontItalic(int italic) {
  if (currentPen != 0) {
    currentPen->font->setItalic(italic);
  }
}

void RMapCanvas::setFontStrikeOut(int strikeout) {
  if (currentPen != 0) {
    currentPen->font->setStrikeOut(strikeout);
  }
}

void RMapCanvas::setFontUnderline(int underline) {
  if (currentPen != 0) {
    currentPen->font->setUnderline(underline);
  }
}

void RMapCanvas::setFontCapitalize(int capitalize) {
  if (currentPen != 0) {
    currentPen->capitalize = capitalize;
  }
}

void RMapCanvas::setBrushAsBackground(int background) {
  if (currentPen != 0) {
    currentPen->background = background;
  }
}

void RMapCanvas::setFontBufferColor(const char *color) {
  if (currentPen != 0) {
    currentPen->buffercolor = new QColor(color);
  }
}

void RMapCanvas::setFontBufferSize(int size) {
  if (currentPen != 0) {
    currentPen->buffersize = size;
  }
}

int RMapCanvas::getFontBufferSize(RoadMapPen pen) {
  return pen->buffersize;
}

void RMapCanvas::erase() {
   if (pixmap) {
      pixmap->fill(currentPen->pen->color());
   }
}

void RMapCanvas::setupPainterPen(QPainter &p) {

  p.setPen(*currentPen->pen);
  p.setFont(*currentPen->font);
// when i removed all of the ROADMAP_ADVANCED_STYLE ifdefs, i found
// this.  i'm going to enable the code, but since the ifdef was
// misspelled, the code may never have been tested.
#if 1 // defined(ROADMAP_ADVENCED_STYLE)
  if (currentPen->background) {
    p.setBackground(*currentPen->brush);
    p.setBackgroundMode(Qt::OpaqueMode);
  } else {
    p.setBrush(*currentPen->brush);
  }
#endif
}


void RMapCanvas::getTextExtents(const char* text, int* w, int* ascent,
   int* descent, int *can_tilt) {

   QFontMetrics fm(*currentPen->font);

   
   QRect r = fm.boundingRect(QString::fromUtf8(text));
   *w = r.width();
   *ascent = fm.ascent();
   *descent = fm.descent();
#ifdef QT_NO_ROTATE
   if (can_tilt) *can_tilt = 0;
#else
   if (can_tilt) *can_tilt = 1;
#endif

}

void RMapCanvas::drawString(RoadMapGuiPoint* position, 
      int corner, const char* text) {
   if (!pixmap) {
      return;
   }

   QPainter p(pixmap);
   if (currentPen != 0) {
     setupPainterPen(p);
   }
                
   int text_width;
   int text_ascent;
   int text_descent;
   int x, y;

   getTextExtents(text, &text_width, &text_ascent, &text_descent, NULL);

   x = position->x;
   y = position->y;
   if (corner & ROADMAP_CANVAS_RIGHT)
      x -= text_width;
   else if (corner & ROADMAP_CANVAS_CENTER_X)
      x -= text_width / 2;
 
   if (corner & ROADMAP_CANVAS_BOTTOM)
      y -= text_descent;
   else if (corner & ROADMAP_CANVAS_CENTER_Y)
      y = y - text_descent + ((text_descent + text_ascent) / 2);
   else /* TOP */
      y += text_ascent;

   if (currentPen != 0) {
     /* draw the buffer */
     if (currentPen->buffersize!=0) {
       QPen pen(p.pen());
       pen.setColor(*currentPen->buffercolor);
       p.setPen(pen);
       for (int i = x-currentPen->buffersize; 
            i <= x+currentPen->buffersize; i++)
         for (int j = y-currentPen->buffersize; 
              j <= y+currentPen->buffersize; j++)
            p.drawText(i, j, QString::fromUtf8(text));
     }
     QPen pen(p.pen());
     pen.setColor(*currentPen->fontcolor);
     p.setPen(pen);
   }

   p.drawText(x, y, QString::fromUtf8(text));
}

void RMapCanvas::drawStringAngle(RoadMapGuiPoint* position,
      int center, const char* text, int angle) {
#ifndef QT_NO_ROTATE
   if (!pixmap) {
      return;
   }

   QPainter p(pixmap);
   if (currentPen != 0) {
     setupPainterPen(p);
   }

   int text_width;
   int text_ascent;
   int text_descent;
   getTextExtents(text, &text_width, &text_ascent, &text_descent, NULL);

   p.translate(position->x,position->y);
   p.rotate((double)angle);

  if (currentPen != 0) {
     if (currentPen->buffersize!=0) {
       QPen pen(p.pen());
       pen.setColor(*currentPen->buffercolor);
       p.setPen(pen);
       for (int i = 0-currentPen->buffersize; i <= currentPen->buffersize; i++)
         for (int j = 0-currentPen->buffersize; j <= currentPen->buffersize; j++
)
           p.drawText((-text_width/2)+i, j-text_descent, QString::fromUtf8(text)
);
     }
     QPen pen(p.pen());
     pen.setColor(*currentPen->fontcolor);
     p.setPen(pen);
   }

   p.drawText(-text_width/2, -text_descent, QString::fromUtf8(text));
#endif
}

void RMapCanvas::drawMultiplePoints(int count, RoadMapGuiPoint* points) {
   QPainter p(pixmap);
   if (currentPen != 0) {
     setupPainterPen(p);
   }

   QPolygon pa(count);
   for(int n = 0; n < count; n++) {
      pa.setPoint(n, points[n].x, points[n].y);
   }

   p.drawPoints(pa);
}

void RMapCanvas::drawMultipleLines(int count, int* lines, 
      RoadMapGuiPoint* points, int fast_draw) {
   QPainter p(pixmap);
   if (currentPen != 0) {
     if (fast_draw) {
       basePen->pen->setColor(currentPen->pen->color());
       p.setPen(*basePen->pen);
     } else {
       setupPainterPen(p);
     }
   }

   for(int i = 0; i < count; i++) {
      int count_of_points = *lines;
      QPolygon pa(count_of_points);
      for(int n = 0; n < count_of_points; n++) {
         pa.setPoint(n, points[n].x, points[n].y);
      }

      p.drawPolyline(pa);

      lines++;
      points += count_of_points;
   }
}

void RMapCanvas::drawMultiplePolygons(int count, int* polygons, 
      RoadMapGuiPoint* points, int filled, int fast_draw) {

   QPainter p(pixmap);
   if (currentPen != 0) {
      if (filled && !fast_draw) {
        p.setPen(*currentPen->pen);
        if (currentPen->background) {
          p.setBackground(*currentPen->brush);
          p.setBackgroundMode(Qt::OpaqueMode);
        } else {
          if (currentPen->brush->style()==Qt::NoBrush) {
            p.setBrush(QColor(currentPen->pen->color()));
          } else {
            p.setBrush(*currentPen->brush);
          }
        }
      } else {
        p.setPen(*currentPen->pen);
      }
   }

   for(int i = 0; i < count; i++) {
      int count_of_points = *polygons;

      QPolygon pa(count_of_points);
      for(int n = 0; n < count_of_points; n++) {
         pa.setPoint(n, points[n].x, points[n].y);
      }

      p.drawPolygon(pa);

      polygons++;
      points += count_of_points;
   }
}

void RMapCanvas::drawMultipleCircles(int count, RoadMapGuiPoint* centers,
      int* radius, int filled, int fast_draw) {

   QPainter p(pixmap);
   if (currentPen != 0) {
      if (filled) {
         p.setPen(*currentPen->pen);
        if (currentPen->background) {
          p.setBackground(*currentPen->brush);
          p.setBackgroundMode(Qt::OpaqueMode);
        } else {
          if (currentPen->brush->style()==Qt::NoBrush) {
            p.setBrush(QColor(currentPen->pen->color()));
          } else {
            p.setBrush(*currentPen->brush);
          }
        }
      } else {
         p.setPen(*currentPen->pen);
      }

   }

   for(int i = 0; i < count; i++) {
      int r = radius[i];

      p.drawEllipse(centers[i].x - r, centers[i].y - r, 2*r, 2*r);
      if (filled) {
         p.drawChord(centers[i].x - r + 1,
            centers[i].y - r + 1,
            2 * r, 2 * r, 0, 16*360);
      }
   }
}

void RMapCanvas::registerButtonPressedHandler(RoadMapCanvasMouseHandler handler) {
  buttonPressedHandler = handler;
}

void RMapCanvas::registerButtonReleasedHandler(RoadMapCanvasMouseHandler handler) {
  buttonReleasedHandler = handler;
}

void RMapCanvas::registerMouseMoveHandler(RoadMapCanvasMouseHandler handler) {
  mouseMoveHandler = handler;
}

void RMapCanvas::registerMouseWheelHandler(RoadMapCanvasMouseHandler handler) {
  mouseWheelHandler = handler;
}


void RMapCanvas::registerConfigureHandler(RoadMapCanvasConfigureHandler handler) {
   configureHandler = handler;
}

int RMapCanvas::getHeight() {
   return height();
}

int RMapCanvas::getWidth() {
   return width();
}

void RMapCanvas::refresh(void) {
   update();
}

void RMapCanvas::mousePressEvent(QMouseEvent* ev) {

   int button;
   RoadMapGuiPoint pt;

   switch (ev->button()) {
      case Qt::LeftButton:  button = 1; break;
      case Qt::MidButton:   button = 2; break;
      case Qt::RightButton: button = 3; break;
      default:          button = 0; break;
   }
   pt.x = ev->x();
   pt.y = ev->y();

   if (buttonPressedHandler != 0) {
      buttonPressedHandler(button, &pt);
   }
}

void RMapCanvas::mouseReleaseEvent(QMouseEvent* ev) {

   int button;
   RoadMapGuiPoint pt;

   switch (ev->button()) {
      case Qt::LeftButton:  button = 1; break;
      case Qt::MidButton:   button = 2; break;
      case Qt::RightButton: button = 3; break;
      default:          button = 0; break;
   }
   pt.x = ev->x();
   pt.y = ev->y();

   if (buttonReleasedHandler != 0) {
      buttonReleasedHandler(button, &pt);
   }
}

void RMapCanvas::mouseMoveEvent(QMouseEvent* ev) {

   RoadMapGuiPoint pt;

   pt.x = ev->x();
   pt.y = ev->y();

   if (mouseMoveHandler != 0) {
      mouseMoveHandler(0, &pt);
   }
}

void RMapCanvas::wheelEvent (QWheelEvent* ev) {

   int direction;

   RoadMapGuiPoint pt;

   pt.x = ev->x();
   pt.y = ev->y();

   direction = ev->delta();
   direction = (direction > 0) ? 1 : ((direction < 0) ? -1 : 0);

   if (mouseWheelHandler != 0) {
      mouseWheelHandler(direction, &pt);
   }
}

void RMapCanvas::resizeEvent(QResizeEvent* ev) {
   configure();
}

void RMapCanvas::paintEvent(QPaintEvent* ev) {
  
   QRect target(0, 0, pixmap->width(), pixmap->height());
   QRect source(0, 0, pixmap->width(), pixmap->height()); 
   QPainter painter(this);
   painter.drawPixmap( target, *pixmap, source);
		
   //bitBlt(this, QPoint(0,0), pixmap, QRect(0, 0, pixmap->width(), pixmap->height()));
}

void RMapCanvas::configure() {
   if (pixmap != 0) {
      delete pixmap;
   }

   pixmap = new QPixmap(width(), height());

   if (configureHandler != 0) {
      configureHandler();
   }
}

QColor RMapCanvas::getColor(const char* color) {
   QColor *c = colors[color];

   if (c == 0) {
      c = new QColor(color);
      colors.insert(color, c);
   }

   return *c;
}

void RMapCanvas::initColors() {
#ifdef QWS4
// It seems that QPE does not have predefined named colors.
// Temporary fix is to hard-code some. Better solution
// is to read rgb.txt??

   colors.insert("black", new QColor(0, 0, 0));
   colors.insert("blue", new QColor(0, 0, 255));
   colors.insert("DarkGrey", new QColor(169, 169, 169));
   colors.insert("green", new QColor(0, 255, 0));
   colors.insert("grey", new QColor(190, 190, 190));
   colors.insert("IndianRed", new QColor(205, 92, 92));
   colors.insert("LightBlue", new QColor(173, 216, 230));
   colors.insert("LightSlateBlue", new QColor(132, 112, 255));
   colors.insert("LightYellow", new QColor(255, 255, 224));
   colors.insert("red", new QColor(255, 0, 0));
   colors.insert("white", new QColor(255, 255, 255));
   colors.insert("yellow", new QColor(255, 255, 0));
   colors.insert("DarkSeaGreen4", new QColor(105, 139, 105));
#endif
#ifdef QT4
   colors.insert("LightSlateBlue", new QColor(132, 112, 255));
   colors.insert("DarkSeaGreen4", new QColor(105, 139, 105));
#endif
}

