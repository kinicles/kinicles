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

#include "mesh_push.h"

extern a2e_texture debug_tex;

mesh_push::mesh_push() {
	f = e->get_file_io();
	
	render_buffer = r->add_buffer(MESH_RTT_TEX_SIZE, MESH_RTT_TEX_SIZE, GL_TEXTURE_2D, texture_object::TF_POINT, rtt::TAA_NONE, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, MESH_RTT_TEX_FORMAT, GL_RGBA, MESH_RTT_TEX_TYPE, 1, rtt::DT_NONE);
	cl_buffer = ocl->create_ogl_image2d_buffer(opencl::BT_READ, render_buffer->tex_id[0]);
}

mesh_push::~mesh_push() {
	r->delete_buffer(render_buffer);
	ocl->delete_buffer(cl_buffer);
}

void mesh_push::load(const string& filename) {
	for(size_t push = 0; push < PUSH_BUFFER_COUNT; push++) {
		stringstream buffer;
		if(!f->file_to_buffer(filename, buffer)) {
			a2e_error("couldn't open push file #%u: %s!", push, filename);
			return;
		}
		
		// read elements
		enum class READ_STATE { VERTICES, ELEMENTS, NONE } state = READ_STATE::NONE;
		while(!buffer.eof() && buffer.good()) {
			string token = "";
			buffer >> token;
			if(token == "vertex") state = READ_STATE::VERTICES;
			else if(token == "element") state = READ_STATE::ELEMENTS;
			else {
				buffer.putback(' ');
				for(auto iter = token.rbegin(); iter != token.rend(); iter++) {
					buffer.putback(*iter);
				}
			}
			
			switch(state) {
				case READ_STATE::VERTICES: {
					float u, v, nx, ny;
					buffer >> u;
					buffer >> v;
					buffer >> nx;
					buffer >> ny;
					push_buffers[push].vertices[0].push_back(float2(u, v - 10.0f) / 10.0f);
					push_buffers[push].normals[0].push_back(float4(nx, ny, 0.0f, 1.0f));
					push_buffers[push].vertices[1].push_back(float2(-u, v - 10.0f) / 10.0f);
					push_buffers[push].normals[1].push_back(float4(nx, ny, 0.0f, 1.0f));
				}
				break;
				case READ_STATE::ELEMENTS: {
					for(;;) {
						ssize_t elem = 0;
						buffer >> elem;
						if(elem == -1) {
							push_buffers[push].indices.push_back(0xFFFFFFFF);
							break;
						}
						push_buffers[push].indices.push_back((unsigned int)elem);
					}
				}
				break;
				default: break;
			}
		}
	}
	
	//
	for(size_t j = 0; j < PUSH_BUFFER_COUNT; j++) {
		glGenBuffers(PUSH_SUB_BUFFER_COUNT, &push_gl_data[j].vertices_vbo[0]);
		glGenBuffers(PUSH_SUB_BUFFER_COUNT, &push_gl_data[j].normals_vbo[0]);
		
		glGenBuffers(1, &push_gl_data[j].indices_vbo);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, push_gl_data[j].indices_vbo);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * push_buffers[j].indices.size(), &push_buffers[j].indices[0], GL_STATIC_DRAW);
		
		for(size_t i = 0; i < PUSH_SUB_BUFFER_COUNT; i++) {
			glBindBuffer(GL_ARRAY_BUFFER, push_gl_data[j].vertices_vbo[i]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float2) * push_buffers[j].vertices[i].size(), &push_buffers[j].vertices[i][0], GL_STATIC_DRAW);
			
			glBindBuffer(GL_ARRAY_BUFFER, push_gl_data[j].normals_vbo[i]);
			glBufferData(GL_ARRAY_BUFFER, sizeof(float4) * push_buffers[j].normals[i].size(), &push_buffers[j].normals[i][0], GL_STATIC_DRAW);
		}
	}
														 
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void mesh_push::render() {
	r->start_draw(render_buffer);
	r->clear();
	r->start_2d_draw();
	
	glPrimitiveRestartIndex(0xFFFFFFFF);
	glEnable(GL_PRIMITIVE_RESTART);
	
	//
	gl3shader simple_shd = s->get_gl3shader("SIMPLE");
	simple_shd->use("colored_mul_color");
	
	for(ssize_t push = PUSH_BUFFER_COUNT-1; push >= 0; push--) {
		for(size_t j = 0; j < 2; j++) {
			simple_shd->uniform("mvpm", matrix4f().ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f) *
								matrix4f().translate((j == 0 ? -0.5f : 0.5f), 0.0f, 0.0f));
			
			for(size_t i = 0; i < PUSH_SUB_BUFFER_COUNT; i++) {
				simple_shd->attribute_array("in_vertex", push_gl_data[push].vertices_vbo[i], 2);
				simple_shd->attribute_array("in_color", push_gl_data[push].normals_vbo[i], 4);
				const float flip_x = ((j == 0 && i == 0) || (j == 1 && i == 1) ? 1.0f : -1.0f);
				const float flip_y = (j == 0 ? 1.0f : -1.0f);
				simple_shd->uniform("in_color", float4(flip_x, flip_y, 1.0f, 1.0f));
				
				
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, push_gl_data[push].indices_vbo);
				glDrawElements(GL_TRIANGLE_STRIP, (GLsizei)push_buffers[push].indices.size(), GL_UNSIGNED_INT, nullptr);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
			}
		}
	}
	
	simple_shd->disable();
	
	glDisable(GL_PRIMITIVE_RESTART);
	
	r->stop_2d_draw();
	r->stop_draw();
}
