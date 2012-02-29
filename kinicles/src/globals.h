/*
 *  Kinicles
 *  Copyright (C) 2011 - 2012 Florian Ziesche
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __KINICLES_GLOBALS_H__
#define __KINICLES_GLOBALS_H__

#include <a2e.h>
#include <random>

// this is necessary on windows/mingw when including the windows openni headers
#if defined(WIN_UNIXENV)
#define _MSC_VER 1600
#endif

#include <XnOpenNI.h>
#include <XnCodecIDs.h>
#include <XnCppWrapper.h>
#include <XnPropNames.h>

using namespace std;

#define APPLICATION_NAME "Kinicles"
#define MESH_RTT_TEX_SIZE 1024
#define MESH_RTT_TEX_FORMAT GL_RGBA16F
#define MESH_RTT_TEX_TYPE GL_HALF_FLOAT

// engine
extern engine* e;
extern file_io * fio;
extern event* eevt;
extern gfx* egfx;
extern texman* t;
extern opencl* ocl;
extern ext* exts;
extern shader* s;
extern scene* sce;
extern camera* cam;
extern rtt* r;
extern particle_manager* pm;
extern a2e_texture debug_tex;

// openni
class ni_handler;
extern ni_handler* ni;

// misc
struct bus_model;
extern bus_model* bmodel;
class audio_handler;
extern audio_handler* ah;
class fft;
extern fft* f;

#endif
