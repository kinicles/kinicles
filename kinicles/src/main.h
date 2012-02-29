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

#ifndef __KINICLES_MAIN_H__
#define __KINICLES_MAIN_H__

#include "globals.h"
#include "ni_handler.h"
#include "model_loader.h"
#include "model_renderer.h"
#include "particle_mesh.h"
#include "audio_handler.h"
#include "mesh_border.h"
#include "mesh_push.h"
#include "fft.h"
#include <particle/particle_system.h>

// scene
a2ematerial* mat;
vector<a2estatic*> models;
vector<light*> lights;
vector<particle_system*> particles;
a2e_texture particle_tex;
particle_system* ps;
bus_model* bmodel;

void particle_debug_draw(particle_system* ps, rtt::fbo* pos_normal_fbo, rtt::fbo* border_fbo);

// prototypes
void create_scene();
bool key_handler(EVENT_TYPE type, shared_ptr<event_object> obj);
bool mouse_handler(EVENT_TYPE type, shared_ptr<event_object> obj);
bool quit_handler(EVENT_TYPE type, shared_ptr<event_object> obj);

#endif
