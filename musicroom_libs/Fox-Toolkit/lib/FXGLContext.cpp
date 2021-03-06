/********************************************************************************
*                                                                               *
*                     G L  R e n d e r i n g   C o n t e x t                    *
*                                                                               *
*********************************************************************************
* Copyright (C) 2000,2010 by Jeroen van der Zijp.   All Rights Reserved.        *
*********************************************************************************
* This library is free software; you can redistribute it and/or modify          *
* it under the terms of the GNU Lesser General Public License as published by   *
* the Free Software Foundation; either version 3 of the License, or             *
* (at your option) any later version.                                           *
*                                                                               *
* This library is distributed in the hope that it will be useful,               *
* but WITHOUT ANY WARRANTY; without even the implied warranty of                *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 *
* GNU Lesser General Public License for more details.                           *
*                                                                               *
* You should have received a copy of the GNU Lesser General Public License      *
* along with this program.  If not, see <http://www.gnu.org/licenses/>          *
********************************************************************************/
#include "xincs.h"
#include "fxver.h"
#include "fxdefs.h"
#include "FXHash.h"
#include "FXThread.h"
#include "FXStream.h"
#include "FXElement.h"
#include "FXString.h"
#include "FXSize.h"
#include "FXPoint.h"
#include "FXException.h"
#include "FXRectangle.h"
#include "FXSettings.h"
#include "FXRegistry.h"
#include "FXAccelTable.h"
#include "FXFont.h"
#include "FXVisual.h"
#include "FXGLVisual.h"
#include "FXEvent.h"
#include "FXWindow.h"
#include "FXApp.h"
#include "FXGLContext.h"


/*
  Notes:
  - Creates FXGLContext based on frame buffer properties described in the desired FXGLVisual.
  - When realized, match actual hardware against desired frame buffer properties and create a
    FXGLContext conformant with the best matching hardware configuration.  Note that we don't
    have a window yet, necessarily!
  - There will be three different ways to make a FXGLCanvas:

      1 Each FXGLCanvas has its own FXGLContext; The FXGLContext is owned by the FXGLCanvas and
        destroyed when the FXGLCanvas is.

      2 Each FXGLCanvas has its own FXGLContext, but it may share the display list and other
        GL objects with those of another FXGLContext.  Thus the other FXGLContext with which
        it shares must be passed in.

      3 The FXGLCanvas shares the FXGLContext with another FXGLCanvas.  This is probably the
        most efficient way as all the GL state information is preserved between the FXGLCanvas
        windows.
*/


using namespace FX;

/*******************************************************************************/

namespace FX {


// Object implementation
FXIMPLEMENT(FXGLContext,FXId,NULL,0)


// Make GL context
FXGLContext::FXGLContext():surface(NULL),visual(NULL),shared(NULL){
  FXTRACE((100,"FXGLContext::FXGLContext %p\n",this));
  }


// Make a GL context
FXGLContext::FXGLContext(FXApp *a,FXGLVisual *vis,FXGLContext* shr):FXId(a),surface(NULL),visual(vis),shared(shr){
  FXTRACE((100,"FXGLContext::FXGLContext %p\n",this));
  }


// Create GL context
void FXGLContext::create(){
#ifdef HAVE_GL_H
  if(!xid){
    if(getApp()->isInitialized()){
      FXTRACE((100,"%s::create %p\n",getClassName(),this));

      // Got to have a visual
      if(!visual){ fxerror("%s::create: trying to create context without a visual.\n",getClassName()); }

      // If sharing contexts for display lists, shared context must be created already
      if(shared && !shared->id()){ fxerror("%s::create: trying to create context before shared context has been created.\n",getClassName()); }

      // Initialize visual
      visual->create();

#if defined(WIN32)
      PIXELFORMATDESCRIPTOR pfd={sizeof(PIXELFORMATDESCRIPTOR),1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
      HWND hwnd=CreateWindow(TEXT("GLTEMP"),TEXT(""),0,0,0,0,0,(HWND)NULL,(HMENU)NULL,(HINSTANCE)getApp()->getDisplay(),NULL);
      HDC hdc=::GetDC(hwnd);
      SetPixelFormat(hdc,(FXint)(FXival)visual->id(),&pfd);
      xid=(FXID)wglCreateContext(hdc);
      if(!xid){
        throw FXWindowException("unable to create GL window.");
        }
      // I hope I didn't get this backward; the new context obviously has no
      // display lists yet, but the old one may have, as it has already been around
      // for a while.  If you see this fail and can't explain why, then that might
      // be what's going on.  Report this to jeroen@fox-toolkit.com
      if(shared && !wglShareLists((HGLRC)shared->id(),(HGLRC)xid)){
        throw FXWindowException("unable to share GL context.");
        }
      ::ReleaseDC(hwnd,hdc);
      DestroyWindow(hwnd);
#else
      XVisualInfo vi;
      vi.visual=(Visual*)visual->visual;
      vi.visualid=vi.visual->visualid;
      vi.screen=DefaultScreen((Display*)getApp()->getDisplay());
      vi.depth=visual->getDepth();
      vi.c_class=vi.visual->c_class;
      vi.red_mask=vi.visual->red_mask;
      vi.green_mask=vi.visual->green_mask;
      vi.blue_mask=vi.visual->blue_mask;
      vi.colormap_size=vi.visual->map_entries;
      vi.bits_per_rgb=vi.visual->bits_per_rgb;
      xid=(FXID)glXCreateContext((Display*)getApp()->getDisplay(),&vi,shared?(GLXContext)shared->id():NULL,true);
      if(!xid){
        throw FXWindowException("unable to create GL context.");
        }
#endif
      }
    }
#endif
  }


// Detach the GL context
void FXGLContext::detach(){
  visual->detach();
#ifdef HAVE_GL_H
  if(xid){
    FXTRACE((100,"FXGLContext::detach %p\n",this));
    surface=NULL;
    xid=0;
    }
#endif
  }


// Destroy the GL context
void FXGLContext::destroy(){
#ifdef HAVE_GL_H
  if(xid){
    if(getApp()->isInitialized()){
      FXTRACE((100,"FXGLContext::destroy %p\n",this));
#ifdef WIN32
      wglDeleteContext((HGLRC)xid);
#else
      glXDestroyContext((Display*)getApp()->getDisplay(),(GLXContext)xid);
#endif
      }
    surface=NULL;
    xid=0;
    }
#endif
  }


// Change visual
void FXGLContext::setVisual(FXGLVisual* vis){
  if(!vis){ fxerror("%s::setVisual: NULL visual\n",getClassName()); }
  if(xid){ fxerror("%s::setVisual: visual should be set before calling create()\n",getClassName()); }
  visual=vis;
  }


// Change share context
void FXGLContext::setShared(FXGLContext *ctx){
  if(xid){ fxerror("%s::setShared: sharing should be set before calling create()\n",getClassName()); }
  shared=ctx;
  }


//  Make the rendering context of drawable current
FXbool FXGLContext::begin(FXDrawable *draw){
#ifdef HAVE_GL_H
  if(xid && !surface){
#ifdef WIN32
    HDC hdc=(HDC)draw->GetDC();
    if(draw->getVisual()->colormap){
      SelectPalette(hdc,(HPALETTE)draw->getVisual()->colormap,false);
      RealizePalette(hdc);
      }
    if(wglMakeCurrent(hdc,(HGLRC)xid)){
      surface=draw;
      return true;
      }
#else
    if(glXMakeCurrent((Display*)getApp()->getDisplay(),draw->id(),(GLXContext)xid)){
      surface=draw;
      return true;
      }
#endif
    }
#endif
  return false;
  }


// Make the rendering context of drawable non-current
FXbool FXGLContext::end(){
#ifdef HAVE_GL_H
  if(xid && surface){
#ifdef WIN32
    HDC hdc=wglGetCurrentDC();
    if(wglMakeCurrent(NULL,NULL)!=0){
      surface->ReleaseDC(hdc);
      surface=NULL;
      return true;
      }
#else
    if(glXMakeCurrent((Display*)getApp()->getDisplay(),None,(GLXContext)NULL)){
      surface=NULL;
      return true;
      }
#endif
   }
#endif
  return false;
  }


// Used by GL to swap the buffers in double buffer mode, or flush a single buffer
void FXGLContext::swapBuffers(){
#ifdef HAVE_GL_H
  if(xid){
#ifdef WIN32
    // SwapBuffers(wglGetCurrentDC());
    // wglSwapLayerBuffers(wglGetCurrentDC(),WGL_SWAP_MAIN_PLANE);
    if(wglSwapLayerBuffers(wglGetCurrentDC(),WGL_SWAP_MAIN_PLANE)==false){
      SwapBuffers(wglGetCurrentDC());
      }
#else
    glXSwapBuffers((Display*)getApp()->getDisplay(),glXGetCurrentDrawable());
#endif
    }
#endif
  }


// Return true if THIS context is current
FXbool FXGLContext::isCurrent() const {
#ifdef HAVE_GL_H
  if(xid){
#ifdef WIN32
    return (FXID)wglGetCurrentContext()==xid;
#else
    return (FXID)glXGetCurrentContext()==xid;
#endif
    }
#endif
  return false;
  }


// Return true if thread has ANY current context
FXbool FXGLContext::hasCurrent(){
#ifdef HAVE_GL_H
#ifdef WIN32
  return wglGetCurrentContext()!=NULL;
#else
  return glXGetCurrentContext()!=NULL;
#endif
#else
  return false;
#endif
  }


// Has double buffering
FXbool FXGLContext::isDoubleBuffer() const {
  return visual->isDoubleBuffer();
  }


// Has stereo buffering
FXbool FXGLContext::isStereo() const {
  return visual->isStereo();
  }


// Save data
void FXGLContext::save(FXStream& store) const {
  FXId::save(store);
  store << visual;
  store << shared;
  }


// Load data
void FXGLContext::load(FXStream& store){
  FXId::load(store);
  store >> visual;
  store >> shared;
  }


// Close and release any resources
FXGLContext::~FXGLContext(){
  FXTRACE((100,"FXGLContext::~FXGLContext %p\n",this));
  destroy();
  surface=(FXDrawable*)-1L;
  visual=(FXGLVisual*)-1L;
  shared=(FXGLContext*)-1L;
  }


/*******************************************************************************/


#if defined(HAVE_XFT_H) && defined(HAVE_GL_H)

// Xft version
static FXbool glXUseXftFont(XftFont* font,int first,int count,int listBase){
  GLint swapbytes,lsbfirst,rowlength,skiprows,skippixels,alignment,list;
  GLfloat x0,y0,dx,dy;
  FT_Face face;
  FT_Error err;
  FXint i,size,x,y;
  FXuchar *glyph;
  FXbool result=false;

  // Save the current packing mode for bitmaps
  glGetIntegerv(GL_UNPACK_SWAP_BYTES,&swapbytes);
  glGetIntegerv(GL_UNPACK_LSB_FIRST,&lsbfirst);
  glGetIntegerv(GL_UNPACK_ROW_LENGTH,&rowlength);
  glGetIntegerv(GL_UNPACK_SKIP_ROWS,&skiprows);
  glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&skippixels);
  glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);

  // Set desired packing modes
  glPixelStorei(GL_UNPACK_SWAP_BYTES,GL_FALSE);
  glPixelStorei(GL_UNPACK_LSB_FIRST,GL_FALSE);
  glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
  glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
  glPixelStorei(GL_UNPACK_ALIGNMENT,1);

  // Get face info
  face=XftLockFace(font);

  // Render font glyphs; use FreeType to render to bitmap
  for(i=first; i<count; i++){
    list=listBase+i;

    // Load glyph
    err=FT_Load_Glyph(face,FT_Get_Char_Index(face,i),FT_LOAD_DEFAULT);
    if(err) goto x;

    // Render glyph
    err=FT_Render_Glyph(face->glyph,FT_RENDER_MODE_MONO);
    if(err) goto x;

    // Pitch may be negative, its the stride between rows
    size=FXABS(face->glyph->bitmap.pitch) * face->glyph->bitmap.rows;

    // Glyph coordinates; note info in freetype is 6-bit fixed point
    x0=-(face->glyph->metrics.horiBearingX>>6);
    y0=(face->glyph->metrics.height-face->glyph->metrics.horiBearingY)>>6;
    dx=face->glyph->metrics.horiAdvance>>6;
    dy=0;

    // Allocate glyph data
    if(!allocElms(glyph,size)) goto x;

    // Copy into OpenGL bitmap format; note OpenGL upside down
    for(y=0; y<face->glyph->bitmap.rows; y++){
      for(x=0; x<face->glyph->bitmap.pitch; x++){
        glyph[y*face->glyph->bitmap.pitch+x]=face->glyph->bitmap.buffer[(face->glyph->bitmap.rows-y-1)*face->glyph->bitmap.pitch+x];
        }
      }

    // Put bitmap into display list
    glNewList(list,GL_COMPILE);
    glBitmap(FXABS(face->glyph->bitmap.pitch)<<3,face->glyph->bitmap.rows,x0,y0,dx,dy,glyph);
    glEndList();

    // Free glyph data
    freeElms(glyph);
    }

  // Success
  result=true;

  // Restore packing modes
x:glPixelStorei(GL_UNPACK_SWAP_BYTES,swapbytes);
  glPixelStorei(GL_UNPACK_LSB_FIRST,lsbfirst);
  glPixelStorei(GL_UNPACK_ROW_LENGTH,rowlength);
  glPixelStorei(GL_UNPACK_SKIP_ROWS,skiprows);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS,skippixels);
  glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);

  // Unlock face
  XftUnlockFace(font);
  return result;
  }


#endif


// Create a display list of bitmaps from font glyphs in a font
FXbool glUseFXFont(FXFont* font,int first,int count,int list){
  FXbool result=false;
  if(!font || !font->id()){ fxerror("glUseFXFont: invalid font.\n"); }
  FXTRACE((100,"glUseFXFont: first=%d count=%d list=%d\n",first,count,list));
#ifdef HAVE_GL_H
#ifdef WIN32
  if(wglGetCurrentContext()){
    HDC hdc=wglGetCurrentDC();
    HFONT oldfont=(HFONT)SelectObject(hdc,(HFONT)font->id());
    // Replace wglUseFontBitmaps() with wglUseFontBitmapsW()
    // Change glCallLists() parameter:
    //   len=utf2ncs(sbuffer,text.text(),text.length());
    //   glCallLists(len,GL_UNSIGNED_SHORT,(GLushort*)sbuffer);
    // Figure out better values for "first" and "count".
    result=wglUseFontBitmaps(hdc,first,count,list);
    SelectObject(hdc,oldfont);
    }
#else
  if(glXGetCurrentContext()){
#ifdef HAVE_XFT_H                       // Using XFT
    result=glXUseXftFont((XftFont*)font->id(),first,count,list);
#else                                   // Using XLFD
    glXUseXFont((Font)font->id(),first,count,list);
    result=true;
#endif
    }
#endif
#endif
  return result;
  }

}
