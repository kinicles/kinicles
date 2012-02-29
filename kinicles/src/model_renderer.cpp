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

#include "model_renderer.h"
#include "ni_handler.h"

static atomic_t color_cycle = { static_cast<int>(time(nullptr)) };

model_renderer::model_renderer(const rtt::fbo* mesh_border_fbo, const rtt::fbo* mesh_push_fbo, const opencl::buffer_object* mb_cl_buffer, const opencl::buffer_object* mp_cl_buffer)
: a2estatic(::e, ::s, ::sce) {
	unsigned int* index_count = new unsigned int[1];
	index_count[0] = (unsigned int)bmodel->indices.size();
	bindices = new index3*[1];
	bindices[0] = (index3*)&bmodel->indices[0];
	
	if(bmodel->indices_vbo == 0) {
		// model hasn't been loaded yet, do so now
		load_from_memory(1,
						 (unsigned int)bmodel->vertices.size(), (float3*)&bmodel->vertices[0],
						 (coord*)&bmodel->tex_coords[0],
						 (unsigned int*)index_count, bindices);
		
		//
		set_position(0.0f, 0.0f, 0.0f);
		
		// create additional vbos/ubo
		const unsigned int bvertex_count = (unsigned int)bmodel->vertices.size();
		
		glGenBuffers(1, &bmodel->weights_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, bmodel->weights_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float4) * bvertex_count, &bmodel->bone_weights[0], GL_STATIC_DRAW);
		glGenBuffers(1, &bmodel->bone_indices_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, bmodel->bone_indices_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(int4) * bvertex_count, &bmodel->bone_indices[0], GL_STATIC_DRAW);
		
		// update normals vbo (vbo has already been created)
		glBindBuffer(GL_ARRAY_BUFFER, vbo_normals_id);
		glBufferSubData(GL_ARRAY_BUFFER, 0, vertex_count * 3 * sizeof(float), &bmodel->normals[0]);
		
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		
		// set bus_model vbos
		bmodel->indices_vbo = vbo_indices_ids[0];
		bmodel->vertices_vbo = vbo_vertices_id;
		bmodel->tex_coords_vbo = vbo_tex_coords_id;
		bmodel->normals_vbo = vbo_normals_id;
		bmodel->binormals_vbo = vbo_binormals_id;
		bmodel->tangents_vbo = vbo_tangents_id;
		
		vbo_indices_ids[0] = 0;
		vbo_vertices_id = 0;
		vbo_tex_coords_id = 0;
		vbo_normals_id = 0;
		vbo_binormals_id = 0;
		vbo_tangents_id = 0;
	}
	
	// load a dummy material for now
	dummy_mat = new a2ematerial(e);
	dummy_mat->load_material(e->data_path("NI.a2mtl"));
	set_material(dummy_mat);
	
	// note: this will be updated dynamically
	glGenBuffers(1, &bones_ubo);
	glBindBuffer(GL_UNIFORM_BUFFER, bones_ubo);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(bone_data), nullptr, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	
	// create rtt buffers
	const int2 buffer_size(MESH_RTT_TEX_SIZE);
	mesh_fbo = r->add_buffer(buffer_size.x, buffer_size.y, GL_TEXTURE_2D, texture_object::TF_POINT, rtt::TAA_NONE, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_RGBA32F, GL_RGBA, GL_FLOAT, 2, rtt::DT_NONE);
	cl_fbos[0] = ocl->create_ogl_image2d_buffer(opencl::BT_READ, mesh_fbo->tex_id[0]);
	cl_fbos[1] = ocl->create_ogl_image2d_buffer(opencl::BT_READ, mesh_fbo->tex_id[1]);
	cl_fbos[2] = (opencl::buffer_object*)mb_cl_buffer;
	cl_fbos[3] = (opencl::buffer_object*)mp_cl_buffer;
	
	// add particle systems
	particle_tex = t->add_texture(e->data_path("particle2.png"), e->get_filtering(), e->get_anisotropic(), GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
	aux_data.mesh_pos_tex = cl_fbos[0];
	aux_data.mesh_normal_tex = cl_fbos[1];
	aux_data.mesh_border_tex = cl_fbos[2];
	aux_data.mesh_push_tex = cl_fbos[3];
	aux_data.pos_normal_fbo = mesh_fbo;
	aux_data.border_fbo = (rtt::fbo*)mesh_border_fbo;
	aux_data.push_fbo = (rtt::fbo*)mesh_push_fbo;
	aux_data.ocl_aux_2 = nullptr;
	ps = pm->add_particle_system(particle_system::EMITTER_TYPE::BOX,
								 particle_system::LIGHTING_TYPE::POINT_PP,
								 particle_tex,
								 1024*32,
								 1000,
								 0.0f,			// doesn't matter:	<from here>
								 float3(0.0f),
								 float3(0.0f),
								 float3(0.0f),
								 float3(0.0f),
								 float3(0.0f),
								 float3(0.0f),	//					<to here>
								 float4(1.0f, 1.0f, 1.0f, 1.0f),
								 float2(0.075f),
								 &aux_data);
	ps->set_sorting(false);
	
	// add a random player light
	static const float3 player_colors[] = {
		float3(1.0f, 0.0f, 0.0f),
		float3(0.0f, 1.0f, 0.0f),
		float3(0.0f, 0.0f, 1.0f),
		float3(1.0f, 1.0f, 0.0f),
		float3(0.0f, 0.4f, 0.8f),
	};
	static const size_t player_color_count = A2E_ARRAY_LENGTH(player_colors);
	const size_t light_num = AtomicFetchThenIncrement(&color_cycle) % player_color_count;
	for(size_t i = 0; i < 1; i++) {
		light* l = new light(e, 0.0f, 8.0f, 0.0f);
		l->set_radius(24.0f);
		l->set_color(player_colors[light_num]);
		sce->add_light(l);
		lights.push_back(l);
	}
	ps->set_lights(lights);
	
	// draw setup (only needs to be done once)
	draw_vertices_vbo = bmodel->vertices_vbo;
	draw_tex_coords_vbo = bmodel->tex_coords_vbo;
	draw_normals_vbo = bmodel->normals_vbo;
	draw_binormals_vbo = bmodel->binormals_vbo;
	draw_tangents_vbo = bmodel->tangents_vbo;
	draw_indices_vbo = bmodel->indices_vbo;
	draw_index_count = index_count[0] * 3;
	
	// add to scene
	sce->add_model(this);
}

model_renderer::~model_renderer() {
	// remove from scene
	sce->delete_model(this);
	
	// don't let a2estatic delete these
	this->vertices = nullptr;
	this->tex_coords = nullptr;
	this->indices = nullptr;
	delete [] bindices;
	
	pm->delete_particle_system(ps);
	if(aux_data.ocl_aux_2 != nullptr) ocl->delete_buffer(aux_data.ocl_aux_2);
	
	if(glIsBuffer(bones_ubo)) glDeleteBuffers(1, &bones_ubo);
	
	r->delete_buffer(mesh_fbo);
	// Note: cl_fbos[2] and cl_fbos[3] are handled/deleted in another class
	ocl->delete_buffer(cl_fbos[0]);
	ocl->delete_buffer(cl_fbos[1]);
	
	for(const auto& l : lights) {
		sce->delete_light(l);
		delete l;
	}
	
	delete dummy_mat;
}

void model_renderer::draw(const DRAW_MODE draw_mode) {
	// not needed any more
}

void model_renderer::draw_sub_object(const DRAW_MODE& draw_mode, const size_t& sub_object_num, const size_t& mask_id) {
	// not needed any more
}

void model_renderer::rtt(const unsigned int& user_id) {
	update_ubo(user_id);
	
	r->start_draw(mesh_fbo);
	r->start_2d_draw();
	r->clear();
	
	static const GLenum draw_buffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, draw_buffers);
	
	gl3shader shd = s->get_gl3shader("RTT_MESH");
	shd->use();
	
	shd->attribute_array("in_vertex", draw_vertices_vbo, 3);
	shd->attribute_array("in_normal", draw_normals_vbo, 3);
	shd->attribute_array("in_tex_coord", draw_tex_coords_vbo, 2);
	
	// additional ins/uniforms:
	shd->attribute_array("weights", bmodel->weights_vbo, 4);
	shd->attribute_array("bone_indices", bmodel->bone_indices_vbo, 4);
	shd->block("bone_data_buffer", bones_ubo);
	shd->uniform("offset", offset);
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, draw_indices_vbo);
	glDrawElements(GL_TRIANGLES, (GLsizei)draw_index_count, GL_UNSIGNED_INT, nullptr);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	shd->disable();
	r->stop_2d_draw();
	r->stop_draw();
}

void model_renderer::update_ubo(const unsigned int& user_id) {
	static const XnSkeletonJoint user_joints[PLAYERS_BONE_COUNT] = {
		XN_SKEL_HEAD,
		XN_SKEL_NECK,
		XN_SKEL_TORSO,
		XN_SKEL_WAIST,
		XN_SKEL_LEFT_SHOULDER,
		XN_SKEL_LEFT_ELBOW,
		XN_SKEL_RIGHT_SHOULDER,
		XN_SKEL_RIGHT_ELBOW,
		XN_SKEL_LEFT_HIP,
		XN_SKEL_LEFT_KNEE,
		XN_SKEL_RIGHT_HIP,
		XN_SKEL_RIGHT_KNEE
	};
	
	// get data and update ubo
	offset.set(0.0f, 0.0f, 0.0f);
	if(ni_handler::is_tracking(user_id)) {
		float min_y = std::numeric_limits<float>::max();
		for(size_t i = 0; i < PLAYERS_BONE_COUNT; i++) {
			const float4 joint_pos = ni_handler::get_smooth_joint_position(user_id, user_joints[i]);
			bone_data[i].mat = ni_handler::get_joint_orientation(user_id, user_joints[i]);
			bone_data[i].inv_mat = bmodel->matrices[i];
			
			if(joint_pos.w == 1.0f) {
				// confidence is high enough
				// copy position to orientation matrix
				bone_data[i].mat[12] = joint_pos.x;
				bone_data[i].mat[13] = joint_pos.y;
				bone_data[i].mat[14] = joint_pos.z;
				min_y = std::min(min_y, joint_pos.y);
			}
		}
		
		// offset on y
		offset.y = -min_y;
		offset.y += 5.5f;
	}
	else {
		// user is not tracked, fill with dummy data
		for(size_t i = 0; i < PLAYERS_BONE_COUNT; i++) {
			bone_data[i].mat.identity();
			bone_data[i].inv_mat.identity();
		}
	}
	
	glBindBuffer(GL_UNIFORM_BUFFER, bones_ubo);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(bone_data), &bone_data[0]);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
	
	// update particle lights
	const float4 torso_pos = ni_handler::get_smooth_joint_position(user_id, XN_SKEL_TORSO);
	for(const auto& l : lights) {
		l->set_position(torso_pos.x, l->get_position().y, torso_pos.z);
	}
}
