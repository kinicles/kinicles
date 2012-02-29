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

#ifndef __KINICLES_MESH_PUSH_H__
#define __KINICLES_MESH_PUSH_H__

#include "globals.h"

#define PUSH_BUFFER_COUNT 1
#define PUSH_SUB_BUFFER_COUNT 2

class mesh_push {
public:
	mesh_push();
	~mesh_push();
	
	void load(const string& filename);
	void render();
	
	rtt::fbo* get_mesh_push_fbo() const {
		return render_buffer;
	}
	
	opencl::buffer_object* get_mesh_push_cl_object() const {
		return cl_buffer;
	}
	
protected:
	file_io* f;
	
	struct {
		array<vector<float2>, PUSH_SUB_BUFFER_COUNT> vertices;
		array<vector<float4>, PUSH_SUB_BUFFER_COUNT> normals;
		vector<unsigned int> indices;
	} push_buffers[PUSH_BUFFER_COUNT];
	
	struct {
		GLuint vertices_vbo[PUSH_SUB_BUFFER_COUNT];
		GLuint normals_vbo[PUSH_SUB_BUFFER_COUNT];
		GLuint indices_vbo;
	} push_gl_data[PUSH_BUFFER_COUNT];
	
	rtt::fbo* render_buffer;
	opencl::buffer_object* cl_buffer;
	
};

#endif
