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

#include "particle_mesh.h"
#include "fft.h"

extern void particle_debug_draw(particle_system* ps, rtt::fbo* fbo_1, rtt::fbo* fbo_2);

particle_manager_mesh::particle_manager_mesh() : particle_manager_cl(::e) {
	cl->use_kernel("PARTICLE MESH INIT");
	init_range_local = cl->compute_local_kernel_range(1);
	cl->use_kernel("PARTICLE MESH RESPAWN");
	respawn_range_local = cl->compute_local_kernel_range(1);
	cl->use_kernel("PARTICLE MESH COMPUTE");
	compute_range_local = cl->compute_local_kernel_range(1);
	compute_range_local.set(128);
	
	local_lcm = core::lcm(core::lcm(init_range_local[0], respawn_range_local[0]), compute_range_local[0]);
	
	mesh_volume_tex = t->add_texture(e->data_path("mesh_volume.png"), texture_object::TF_LINEAR, e->get_anisotropic(), GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
	cl_mesh_volume_tex = ocl->create_ogl_image2d_buffer(opencl::BT_READ, mesh_volume_tex->tex());
	
	// create motion blur buffers
	const rtt::fbo* sce_fbo = sce->_get_scene_buffer();
	active_mb_buffer = 0;
	particle_render_buffer = r->add_buffer(sce_fbo->width, sce_fbo->height, GL_TEXTURE_2D, texture_object::TF_LINEAR, sce_fbo->anti_aliasing[0], GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, 1, rtt::DT_NONE);
	motion_blur_buffers[0] = r->add_buffer(sce_fbo->width, sce_fbo->height, GL_TEXTURE_2D, texture_object::TF_LINEAR, sce_fbo->anti_aliasing[0], GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, 1, rtt::DT_NONE);
	motion_blur_buffers[1] = r->add_buffer(sce_fbo->width, sce_fbo->height, GL_TEXTURE_2D, texture_object::TF_LINEAR, sce_fbo->anti_aliasing[0], GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, 1, rtt::DT_NONE);
	r->start_draw(motion_blur_buffers[0]);
	r->clear();
	r->stop_draw();
	r->start_draw(motion_blur_buffers[1]);
	r->clear();
	r->stop_draw();
}

particle_manager_mesh::~particle_manager_mesh() {
	ocl->delete_buffer(cl_mesh_volume_tex);
	r->delete_buffer(particle_render_buffer);
	r->delete_buffer(motion_blur_buffers[0]);
	r->delete_buffer(motion_blur_buffers[1]);
}

void particle_manager_mesh::reset_particle_system(particle_system* ps) {
	particle_system::internal_particle_data* pdata = ps->get_internal_particle_data();
	
	if(pdata->particle_count != (ps->get_living_time() * ps->get_spawn_rate()) / 1000) {
		reset_particle_count(ps);
	}
	
	mesh_aux_data* aux_data = (mesh_aux_data*)ps->get_aux_data();
	if(aux_data->ocl_aux_2 != nullptr) cl->delete_buffer(aux_data->ocl_aux_2);
	aux_data->ocl_aux_2 = cl->create_buffer(opencl::BT_READ_WRITE, pdata->particle_count * sizeof(float16), nullptr);
	
	cl->acquire_gl_object(pdata->ocl_pos_time_buffer);
	cl->acquire_gl_object(pdata->ocl_dir_buffer);
	
	cl->use_kernel("PARTICLE MESH INIT");
	unsigned int arg_num = 0;
	cl->set_kernel_argument(arg_num++, (float)ps->get_living_time());
	cl->set_kernel_argument(arg_num++, (unsigned int)pdata->particle_count);
	cl->set_kernel_argument(arg_num++, (float4)ps->get_position_offset());
	cl->set_kernel_argument(arg_num++, kernel_seed);
	cl->set_kernel_argument(arg_num++, (float)pdata->spawn_rate_ts);
	cl->set_kernel_argument(arg_num++, pdata->ocl_pos_time_buffer);
	cl->set_kernel_argument(arg_num++, pdata->ocl_dir_buffer);
	cl->set_kernel_argument(arg_num++, aux_data->ocl_aux_2);
	cl->set_kernel_range(pdata->ocl_range_global, cl::NDRange(256));
	cl->run_kernel();
	
	cl->release_gl_object(pdata->ocl_pos_time_buffer);
	cl->release_gl_object(pdata->ocl_dir_buffer);
	
	pdata->step_timer = SDL_GetTicks() - 1000;
	pdata->reinit_timer = SDL_GetTicks();
}

void particle_manager_mesh::run_particle_system(particle_system* ps) {
	particle_system::internal_particle_data* pdata = ps->get_internal_particle_data();
	bool updated = false;
	
	// mesh compute
	if(!updated) {
		// only acquire gl objects if the respawn kernel hasn't acquired them already
		cl->acquire_gl_object(pdata->ocl_pos_time_buffer);
		cl->acquire_gl_object(pdata->ocl_dir_buffer);
	}
	
	float4 camera_pos = float4(-*e->get_position());
	camera_pos.w = 1.0f;
	
	// update positions
	cl->use_kernel("PARTICLE MESH COMPUTE");
	unsigned int arg_num = 0;
	cl->set_kernel_argument(arg_num++, (float)(SDL_GetTicks() - pdata->step_timer));
	cl->set_kernel_argument(arg_num++, (float)ps->get_living_time());
	cl->set_kernel_argument(arg_num++, (unsigned int)pdata->particle_count);
	cl->set_kernel_argument(arg_num++, camera_pos);
	cl->set_kernel_argument(arg_num++, ((mesh_aux_data*)ps->get_aux_data())->mesh_pos_tex);
	cl->set_kernel_argument(arg_num++, ((mesh_aux_data*)ps->get_aux_data())->mesh_normal_tex);
	cl->set_kernel_argument(arg_num++, ((mesh_aux_data*)ps->get_aux_data())->mesh_border_tex);
	cl->set_kernel_argument(arg_num++, ((mesh_aux_data*)ps->get_aux_data())->mesh_push_tex);
	cl->set_kernel_argument(arg_num++, cl_mesh_volume_tex);
	cl->set_kernel_argument(arg_num++, pdata->ocl_pos_time_buffer);
	cl->set_kernel_argument(arg_num++, pdata->ocl_dir_buffer);
	cl->set_kernel_argument(arg_num++, ((mesh_aux_data*)ps->get_aux_data())->ocl_aux_2);
	cl->set_kernel_argument(arg_num++, f->get_spectrum_buffer());
	size_t line = f->get_active_spectrum_line();
	cl->set_kernel_argument(arg_num++, (unsigned int)(line == 0 ? FFT_CL_BUFFER_HEIGHT-1 : line-1));
	cl->set_kernel_range(pdata->ocl_range_global, compute_range_local);
	cl->run_kernel();
	
	pdata->step_timer = SDL_GetTicks();
	updated = true;
	
	// don't wait for another update when using reentrant sorting (and it's not complete yet)
	if(ps->is_reentrant_sorting() && !pdata->reentrant_complete) {
		sort_particle_system(ps);
	}
	
	if(updated) {
		// only start sorting when there is an update
		if(ps->is_sorting() &&
		   // sort here when not using reentrant sorting or when reentrant sorting has completed
		   (!ps->is_reentrant_sorting() || (ps->is_reentrant_sorting() && pdata->reentrant_complete))) {
			sort_particle_system(ps);
		}
		
		// release everything that has been acquired before
		cl->release_gl_object(pdata->ocl_pos_time_buffer);
		cl->release_gl_object(pdata->ocl_dir_buffer);
	}
}

void particle_manager_mesh::draw(const rtt::fbo* frame_buffer) {
#define PARTICLE_MOTION_BLUR 1
#if PARTICLE_MOTION_BLUR
	// stop drawing into main buffer
	rtt::fbo* main_buffer = (rtt::fbo*)r->get_current_buffer();
	r->stop_draw();
	
	// only draw into particle render buffer
	r->start_draw(particle_render_buffer);
	r->clear();
#endif
	for(const auto& psystem : particle_systems) {
		if(psystem->is_visible()) draw_particle_system(psystem, frame_buffer);
	}
#if PARTICLE_MOTION_BLUR
	r->stop_draw();
	
	// apply motion blur
	motion_blur_post_process(particle_render_buffer);
	
	// restart main buffer drawing/rendering and blit motion blurred buffer into main buffer
	r->start_draw(main_buffer);
	r->start_2d_draw();
	glEnable(GL_BLEND);
	glDepthMask(GL_FALSE);
	egfx->set_blend_mode(gfx::BLEND_MODE::ADD);
	egfx->draw_textured_passthrough_rectangle(gfx::rect(0, 0, main_buffer->width, main_buffer->height),
											  coord(0.0f, 0.0f),
											  coord(1.0f, 1.0f),
											  motion_blur_buffers[active_mb_buffer]->tex_id[0]);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	r->stop_2d_draw();
#endif
	
	// debug draw
	if(!particle_systems.empty()) {
		const auto& ps = *particle_systems.cbegin();
		particle_debug_draw(ps,
							((particle_manager_mesh::mesh_aux_data*)ps->get_aux_data())->pos_normal_fbo,
							((particle_manager_mesh::mesh_aux_data*)ps->get_aux_data())->border_fbo);
	}
}

void particle_manager_mesh::draw_particle_system(particle_system* ps, const rtt::fbo* frame_buffer) {
	// prep matrices
	matrix4f mvm(*e->get_modelview_matrix());
	matrix4f pm(*e->get_projection_matrix());
	matrix4f mvpm(mvm * pm);
	
	// draw
	particle_system::internal_particle_data* pdata = ps->get_internal_particle_data();
	
	glEnable(GL_BLEND);
	g->set_blend_mode(ps->get_blend_mode());
	glDepthMask(GL_FALSE);
	
	// point -> gs: quad
	gl3shader particle_draw = s->get_gl3shader("PARTICLE_DRAW_OPENCL");
	const auto ltype = ps->get_lighting_type();
	string shd_option = "#";
	switch(ltype) {
		case particle_system::LIGHTING_TYPE::NONE: shd_option = "#"; break;
		case particle_system::LIGHTING_TYPE::POINT: shd_option = "lit"; break;
		case particle_system::LIGHTING_TYPE::POINT_PP: shd_option = "lit_pp"; break;
	}
	particle_draw->use(shd_option);
	particle_draw->uniform("mvm", mvm);
	particle_draw->uniform("mvpm", mvpm);
	particle_draw->uniform("position", ps->get_position());
	
	const float2 near_far_plane = e->get_near_far_plane();
	const float2 projection_ab = float2(near_far_plane.y / (near_far_plane.y - near_far_plane.x),
										(-near_far_plane.y * near_far_plane.x) / (near_far_plane.y - near_far_plane.x));
	particle_draw->uniform("projection_ab", projection_ab);
	particle_draw->texture("depth_buffer", frame_buffer->depth_buffer);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	
	if(ltype != particle_system::LIGHTING_TYPE::NONE) {
		// note: max lights is currently 4
		struct __attribute__((packed)) light_info {
			float4 position;
			float4 color;
		} lights_data[A2E_MAX_PARTICLE_LIGHTS];
		const auto& lights = ps->get_lights();
		const size_t actual_lights = std::min(lights.size(), (size_t)A2E_MAX_PARTICLE_LIGHTS);
		for(size_t i = 0; i < actual_lights; i++) {
			lights_data[i].position = float4(lights[i]->get_position(),
											 lights[i]->get_sqr_radius());
			lights_data[i].color = float4(lights[i]->get_color(),
										  lights[i]->get_inv_sqr_radius());
		}
		for(size_t i = actual_lights; i < A2E_MAX_PARTICLE_LIGHTS; i++) {
			lights_data[i].position = float4(0.0f);
			lights_data[i].color = float4(0.0f);
		}
		// update and set ubo
		const GLuint lights_ubo = ps->get_lights_ubo();
		glBindBuffer(GL_UNIFORM_BUFFER, lights_ubo); // will be unbound automatically
		glBufferSubData(GL_UNIFORM_BUFFER, 0,
						(sizeof(float4) * 2) * A2E_MAX_PARTICLE_LIGHTS,
						&lights_data[0]);
		particle_draw->block("light_buffer", lights_ubo);
	}
	
	particle_draw->uniform("color", ps->get_color());
	particle_draw->uniform("size", ps->get_size());
	particle_draw->texture("particle_tex", ps->get_texture());
	particle_draw->attribute_array("in_vertex", pdata->ocl_gl_pos_time_vbo, 4);
	particle_draw->attribute_array("in_aux", pdata->ocl_gl_dir_vbo, 4);
	
	if(!ps->is_sorting() ||
	   (ps->is_sorting() && !ps->is_reentrant_sorting()) ||
	   (ps->is_reentrant_sorting() && ps->is_render_intermediate_sorted_buffer())) {
		// std: use active indices buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pdata->particle_indices_vbo[pdata->particle_indices_swap]);
	}
	else if(ps->is_reentrant_sorting() && !ps->is_render_intermediate_sorted_buffer()) {
		// use previously sorted indices buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pdata->particle_indices_vbo[1 - pdata->particle_indices_swap]);
	}
	else {
		assert(false && "invalid particle system state");
	}
	
	glDrawElements(GL_POINTS, (GLsizei)pdata->particle_count, GL_UNSIGNED_INT, nullptr);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	particle_draw->disable();
	
	glDepthMask(GL_TRUE);
	g->set_blend_mode(gfx::BLEND_MODE::DEFAULT);
	glDisable(GL_BLEND);
}

void particle_manager_mesh::motion_blur_post_process(rtt::fbo* scene_buffer) {
	active_mb_buffer = 1 - active_mb_buffer;
	
	// draw new frame over old frame and apply effect
	r->start_draw(motion_blur_buffers[active_mb_buffer]);
	r->start_2d_draw();
	
	gl3shader mb_shd = s->get_gl3shader("MOTION BLUR");
	mb_shd->use();
	mb_shd->texture("new_frame", scene_buffer->tex_id[0]);
	mb_shd->texture("old_frame", motion_blur_buffers[1 - active_mb_buffer]->tex_id[0]);
	egfx->draw_fullscreen_triangle();
	mb_shd->disable();
	
	r->stop_2d_draw();
	r->stop_draw();
}
