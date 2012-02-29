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

#ifndef __KINICLES_MODEL_RENDERER_H__
#define __KINICLES_MODEL_RENDERER_H__

#include "globals.h"
#include "particle_mesh.h"

#define PLAYERS_BONE_COUNT 12
struct bus_model {
	GLuint indices_vbo;
	GLuint vertices_vbo;
	GLuint tex_coords_vbo;
	GLuint normals_vbo;
	GLuint binormals_vbo;
	GLuint tangents_vbo;
	GLuint weights_vbo;
	GLuint bone_indices_vbo;
	
	vector<index3> indices;
	vector<float3> vertices;
	vector<coord> tex_coords;
	vector<float3> normals;
	vector<float4> bone_weights;
	vector<uint4> bone_indices;
	array<matrix4f, PLAYERS_BONE_COUNT> matrices;
	array<float4, PLAYERS_BONE_COUNT> joint_positions;
	
	bus_model() : indices_vbo(0), vertices_vbo(0), tex_coords_vbo(0), normals_vbo(0), binormals_vbo(0), tangents_vbo(0), weights_vbo(0), bone_indices_vbo(0) {
	}
	~bus_model() {
		if(glIsBuffer(indices_vbo)) glDeleteBuffers(1, &indices_vbo);
		if(glIsBuffer(vertices_vbo)) glDeleteBuffers(1, &vertices_vbo);
		if(glIsBuffer(tex_coords_vbo)) glDeleteBuffers(1, &tex_coords_vbo);
		if(glIsBuffer(normals_vbo)) glDeleteBuffers(1, &normals_vbo);
		if(glIsBuffer(binormals_vbo)) glDeleteBuffers(1, &binormals_vbo);
		if(glIsBuffer(tangents_vbo)) glDeleteBuffers(1, &tangents_vbo);
		if(glIsBuffer(weights_vbo)) glDeleteBuffers(1, &weights_vbo);
		if(glIsBuffer(bone_indices_vbo)) glDeleteBuffers(1, &bone_indices_vbo);
	}
};

class model_renderer : public a2estatic {
public:
	model_renderer(const rtt::fbo* mesh_border_fbo, const rtt::fbo* mesh_push_fbo, const opencl::buffer_object* mb_cl_buffer, const opencl::buffer_object* mp_cl_buffer);
	virtual ~model_renderer();
	
	virtual void draw(const DRAW_MODE draw_mode);
	void rtt(const unsigned int& user_id);
	
	const rtt::fbo* _get_fbo(const size_t& idx) const {
		return mesh_fbo;
	}
	
protected:
	struct __attribute__((packed)) {
		matrix4f mat;
		matrix4f inv_mat;
	} bone_data[PLAYERS_BONE_COUNT];
	GLuint bones_ubo;
	
	index3** bindices;
	
	a2ematerial* dummy_mat;
	
	rtt::fbo* mesh_fbo;
	array<opencl::buffer_object*, 4> cl_fbos;
	
	a2e_texture particle_tex;
	particle_system* ps;
	particle_manager_mesh::mesh_aux_data aux_data;
	
	vector<light*> lights;
	
	float3 offset;
	
	virtual void draw_sub_object(const DRAW_MODE& draw_mode, const size_t& sub_object_num, const size_t& mask_id);
	
	void update_ubo(const unsigned int& user_id);
	
};

#endif
