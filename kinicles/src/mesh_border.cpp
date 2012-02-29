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

#include "mesh_border.h"

extern a2e_texture debug_tex;

mesh_border::mesh_border() {
	f = e->get_file_io();
	
	render_buffer = r->add_buffer(MESH_RTT_TEX_SIZE, MESH_RTT_TEX_SIZE, GL_TEXTURE_2D, texture_object::TF_POINT, rtt::TAA_NONE, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, MESH_RTT_TEX_FORMAT, GL_RGBA, MESH_RTT_TEX_TYPE, 1, rtt::DT_NONE);
	cl_buffer = ocl->create_ogl_image2d_buffer(opencl::BT_READ, render_buffer->tex_id[0]);
}

mesh_border::~mesh_border() {
	r->delete_buffer(render_buffer);
	ocl->delete_buffer(cl_buffer);
}

void mesh_border::load(const string& filename_1, const string& filename_2) {
	stringstream border_buffer[BORDER_BUFFER_COUNT];
	
	for(size_t border = 0; border < BORDER_BUFFER_COUNT; border++) {
		const string& filename = (border == 0 ? filename_1 : filename_2);
		if(!f->file_to_buffer(filename, border_buffer[border])) {
			a2e_error("couldn't open border file #%u: %s!", border, filename);
			return;
		}
		
		// read elements
		size_t line_count = 0;
		while(!border_buffer[border].eof() && border_buffer[border].good()) {
			float u1, v1, u2, v2, nx, ny, dist;
			border_buffer[border] >> u1;
			border_buffer[border] >> v1;
			border_buffer[border] >> u2;
			border_buffer[border] >> v2;
			border_buffer[border] >> nx;
			border_buffer[border] >> ny;
			border_buffer[border] >> dist;
			dist /= 10.0f;
			borders[border].vertices[0].push_back(float2(u1, v1 - 10.0f) / 10.0f);
			borders[border].vertices[0].push_back(float2(u2, v2 - 10.0f) / 10.0f);
			borders[border].vertices[1].push_back(float2(-u1, v1 - 10.0f) / 10.0f);
			borders[border].vertices[1].push_back(float2(-u2, v2 - 10.0f) / 10.0f);
			borders[border].normals[0].push_back(float4(nx, ny, dist+0.001f, 1.0f));
			borders[border].normals[0].push_back(float4(nx, ny, 0.001f, 1.0f));
			borders[border].normals[1].push_back(float4(-nx, ny, dist+0.001f, 1.0f));
			borders[border].normals[1].push_back(float4(-nx, ny, 0.001f, 1.0f));
			line_count++;
		}
	}
	
	//
	for(size_t j = 0; j < BORDER_BUFFER_COUNT; j++) {
		glGenBuffers(2, &border_gl_data[j].vertices_vbo[0]);
		glGenBuffers(2, &border_gl_data[j].normals_vbo[0]);
		
		for(size_t i = 0; i < 2; i++) {
			glBindBuffer(GL_ARRAY_BUFFER, border_gl_data[j].vertices_vbo[i]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float2) * borders[j].vertices[i].size(), &borders[j].vertices[i][0], GL_STATIC_DRAW);
			
			glBindBuffer(GL_ARRAY_BUFFER, border_gl_data[j].normals_vbo[i]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float4) * borders[j].normals[i].size(), &borders[j].normals[i][0], GL_STATIC_DRAW);
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void mesh_border::render() {
	r->start_draw(render_buffer);
	r->clear();
	r->start_2d_draw();
	
	//
	gl3shader simple_shd = s->get_gl3shader("SIMPLE");
	simple_shd->use("colored");
	
	for(ssize_t border = BORDER_BUFFER_COUNT-1; border >= 0; border--) {
		for(size_t j = 0; j < 2; j++) {
			simple_shd->uniform("mvpm", matrix4f().ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f) *
								matrix4f().translate((j == 0 ? -0.5f : 0.5f), 0.0f, 0.0f));
			
			for(size_t i = 0; i < 2; i++) {
				simple_shd->attribute_array("in_vertex", border_gl_data[border].vertices_vbo[i], 2);
				simple_shd->attribute_array("in_color", border_gl_data[border].normals_vbo[i], 4);
				
				glDisable(GL_CULL_FACE);
				glDrawArrays(GL_TRIANGLE_STRIP, 0, borders[border].vertices[i].size());
				glEnable(GL_CULL_FACE);
				glBindBuffer(GL_ARRAY_BUFFER, 0);
			}
		}
	}
	
	simple_shd->disable();
	
	r->stop_2d_draw();
	r->stop_draw();
}
