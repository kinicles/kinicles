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

#include "main.h"

// global vars, don't change these!
static bool done = false;
static bool ni_update = true;
static bool show_debug_texture = false;
static bool debug_players_reset = false;
static rtt::fbo* particle_debug_fbo = nullptr;

int main(int argc, char *argv[]) {
	// initialize the engine
	e = new engine(argv[0], (const char*)"../../data/");
	e->init();
	e->set_caption(APPLICATION_NAME);
	const xml::xml_doc& config_doc = e->get_config_doc();
	
	// init class pointers
	fio = e->get_file_io();
	eevt = e->get_event();
	egfx = e->get_gfx();
	t = e->get_texman();
	ocl = e->get_opencl();
	exts = e->get_ext();
	s = e->get_shader();
	r = e->get_rtt();
	f = new fft(config_doc.get<bool>("config.audio.fake_spectrum", false));
	ah = new audio_handler(f, config_doc.get<bool>("config.audio.playback", false));
	
	sce = new scene(e);
	cam = new camera(e);
	
	// for debugging purposes
	debug_tex = a2e_texture(new texture_object());
	debug_tex->width = e->get_width();
	debug_tex->height = e->get_height();
	
	// compile additional shaders
	const string ar_shaders[][2] = {
		{ "IR_GP_SKINNING", "inferred/gp_skinning.a2eshd" },
		{ "IR_MP_SKINNING", "inferred/mp_skinning.a2eshd" },
		{ "RTT_MESH", "misc/rtt_mesh.a2eshd" },
		{ "PARTICLE DEBUG", "particle/particle_debug.a2eshd" },
		{ "MOTION BLUR", "misc/motion_blur.a2eshd" },
	};
	for(size_t i = 0; i < A2E_ARRAY_LENGTH(ar_shaders); i++) {
		if(!s->add_a2e_shader(ar_shaders[i][0], ar_shaders[i][1])) {
			a2e_error("couldn't add a2e-shader \"%s\"!", ar_shaders[i][1]);
			done = true;
		}
	}
	
	// compile additional kernels
	const string ar_kernels[][4] = {
		// identifier, kernel name, file name, build options
		{ "PARTICLE MESH INIT", "particle_init", "particle_mesh_spawn.cl", " -DA2E_PARTICLE_INIT" },
		{ "PARTICLE MESH RESPAWN", "particle_respawn", "particle_mesh_spawn.cl", "" },
		{ "PARTICLE MESH COMPUTE", "particle_compute", "particle_mesh_compute.cl",
			" -DSPECTRUM_WIDTH="+size_t2string(FFT_CL_BUFFER_WIDTH)
			+" -DSPECTRUM_HEIGHT="+size_t2string(FFT_CL_BUFFER_HEIGHT) },
	};
	for(size_t i = 0; i < A2E_ARRAY_LENGTH(ar_kernels); i++) {
		bool success = ocl->add_kernel_file(ar_kernels[i][0],
											ocl->make_kernel_path(ar_kernels[i][2].c_str()),
											ar_kernels[i][1],
											ar_kernels[i][3].c_str()) != nullptr;
		if(!success) {
			a2e_error("couldn't add opencl kernel \"%s\"!", ar_kernels[i][2]);
			done = true;
		}
	}
	
	// initialize the camera
	cam->set_rotation_speed(300.0f);
	cam->set_cam_speed(5.0f);
	cam->set_mouse_input(false);
	cam->set_keyboard_input(true);
	cam->set_wasd_input(true);
	
	// get camera settings from the config
	const float3 cam_pos = config_doc.get<float3>("config.camera.position", float3(0.0f, -12.0f, -5.0f));
	const float2 cam_rot = config_doc.get<float2>("config.camera.rotation", float2(0.0f, 180.0f));
	cam->set_position(cam_pos);
	cam->set_rotation(cam_rot.x, cam_rot.y, 0.0f);
	
	// create the scene
	create_scene();
	
	// load model
	model_loader ml(e);
	bmodel = ml.load(e->data_path("NI-Elem.txt"),
					 e->data_path("NI-Vrts.txt"),
					 e->data_path("NI-Tex0.txt"),
					 e->data_path("NI-boneW.txt"),
					 e->data_path("NI-boneI.txt"),
					 e->data_path("NI-bindMatrix-CM.txt"));
	
	// render mesh border and mesh push
	mesh_border* mb = new mesh_border();
	mb->load(e->data_path("NI-Border1.txt"), e->data_path("NI-Border2.txt"));
	mb->render();
	
	mesh_push* mp = new mesh_push();
	mp->load(e->data_path("NI-Push1.txt"));
	mp->render();
	
	// init openni
	const string oni_file = (argc > 1 ? string(argv[1]) : "");
	const int init_ret = ni_handler::init(oni_file, mb, mp);
	if(init_ret != XN_STATUS_OK) {
		a2e_error("couldn't initialize openni: %u", init_ret);
		done = true;
	}
	
	// add event handlers
	event::handler key_handler_fnctr(&key_handler);
	eevt->add_event_handler(key_handler_fnctr, EVENT_TYPE::KEY_DOWN, EVENT_TYPE::KEY_UP, EVENT_TYPE::KEY_PRESSED);
	event::handler mouse_handler_fnctr(&mouse_handler);
	eevt->add_event_handler(mouse_handler_fnctr, EVENT_TYPE::MOUSE_RIGHT_CLICK);
	event::handler quit_handler_fnctr(&quit_handler);
	eevt->add_event_handler(quit_handler_fnctr, EVENT_TYPE::QUIT);
	
	// additional debug stuff
	const int2 buffer_size(1024);
	particle_debug_fbo = r->add_buffer(buffer_size.x, buffer_size.y, GL_TEXTURE_2D, texture_object::TF_POINT, rtt::TAA_NONE, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE, GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT, 1, rtt::DT_NONE);
	
	// main loop
	while(!done) {
		// event handling
		eevt->handle_events();
		
		// set caption (app name and fps count)
		if(e->is_new_fps_count()) {
			static stringstream caption;
			caption << APPLICATION_NAME << " | FPS: " << e->get_fps();
			caption << " | Cam: " << float3(-*e->get_position());
			caption << " " << cam->get_rotation();
			e->set_caption(caption.str().c_str());
			core::reset(&caption);
		}
		
		// render
		e->start_draw();
		
		if(ni_update) ni->run();
		
		cam->run();
		sce->draw();
		
		// draw debug texture
		if(show_debug_texture) {
			if(debug_tex->width > 0 && debug_tex->height > 0) {
				e->start_2d_draw();
				size_t draw_width = debug_tex->width, draw_height = debug_tex->height;
				float ratio = float(draw_width) / float(draw_height);
				float scale = 1.0f;
				if(ratio >= 1.0f && draw_width > e->get_width()) {
					scale = float(e->get_width()) / float(draw_width);
				}
				else if(ratio < 1.0f && draw_height > e->get_height()) {
					scale = float(e->get_height()) / float(draw_height);
				}
				draw_width *= scale;
				draw_height *= scale;
				egfx->draw_textured_color_rectangle(gfx::rect(0, 0,
															  (unsigned int)draw_width,
															  (unsigned int)draw_height),
													coord(0.0f, 1.0f), coord(1.0f, 0.0f),
													float4(1.0f, 1.0f, 1.0f, 0.0f),
													float4(0.0f, 0.0f, 0.0f, 1.0f),
													debug_tex->tex());
				e->stop_2d_draw();
			}
		}
		
		e->stop_draw();
		
		// for debugging purposes only: reset players (set if opencl kernels are reloaded)
		if(debug_players_reset) {
			debug_players_reset = false;
			ni_handler::reset_players();
		}
	}
	debug_tex->tex_num = 0;
	
	// cleanup
	eevt->remove_event_handler(key_handler_fnctr);
	eevt->remove_event_handler(mouse_handler_fnctr);
	eevt->remove_event_handler(quit_handler_fnctr);
	r->delete_buffer(particle_debug_fbo);
	delete ah;
	delete f;
	ni_handler::destroy();
	delete mb;
	delete mp;
	delete mat;
	for(const auto& model : models) {
		delete model;
	}
	models.clear();
	for(const auto& l : lights) {
		delete l;
	}
	lights.clear();
	
	if(bmodel != nullptr) delete bmodel;
	
	delete sce;
	delete cam;
	delete e;
	
	return 0;
}

bool key_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	// cast correctly
	if(type == EVENT_TYPE::KEY_DOWN) {
		const shared_ptr<key_down_event>& key_evt = (shared_ptr<key_down_event>&)obj;
		switch(key_evt->key) {
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:
				cam->set_cam_speed(0.2f);
				break;
			default:
				return false;
		}
	}
	else if(type == EVENT_TYPE::KEY_UP) {
		const shared_ptr<key_up_event>& key_evt = (shared_ptr<key_up_event>&)obj;
		switch(key_evt->key) {
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:
				cam->set_cam_speed(5.0f);
				break;
			default:
				return false;
		}
	}
	else { // EVENT_TYPE::KEY_PRESSED
		const shared_ptr<key_pressed_event>& key_evt = (shared_ptr<key_pressed_event>&)obj;
		switch(key_evt->key) {
			case SDLK_ESCAPE:
				done = true;
				break;
			/*case SDLK_q:
				show_debug_texture ^= true;
				break;
			case SDLK_1:
				debug_tex->tex_num = sce->_get_g_buffer(0)->tex_id[0];
				break;
			case SDLK_2:
				debug_tex->tex_num = sce->_get_g_buffer(1)->tex_id[0];
				break;
			case SDLK_3:
				debug_tex->tex_num = sce->_get_g_buffer(1)->tex_id[1];
				break;
			case SDLK_4:
				debug_tex->tex_num = sce->_get_l_buffer(0)->tex_id[0];
				break;
			case SDLK_5:
				debug_tex->tex_num = sce->_get_l_buffer(1)->tex_id[0];
				break;
			case SDLK_6:
				debug_tex->tex_num = sce->_get_fxaa_buffer()->tex_id[0];
				break;
			case SDLK_7:
				debug_tex->tex_num = sce->_get_scene_buffer()->tex_id[0];
				break;
			case SDLK_F18:
			case SDLK_9:
				e->reload_shaders();
				break;
			case SDLK_F19:
			case SDLK_0:
				e->reload_kernels();
				debug_players_reset = true;
				break;*/
			case SDLK_e:
				ni_update ^= true;
				break;
			default:
				return false;
		}
	}
	return true;
}

bool mouse_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	if(type == EVENT_TYPE::MOUSE_RIGHT_CLICK) {
		const bool cur_state = cam->get_mouse_input();
		cam->set_mouse_input(cur_state ^ true);
		e->set_cursor_visible(cur_state);
	}
	return true;
}

bool quit_handler(EVENT_TYPE type, shared_ptr<event_object> obj) {
	done = true;
	return true;
}

void create_scene() {
	// first: load the material and plane model
	mat = new a2ematerial(e);
	mat->load_material(e->data_path("white_plane.a2mtl"));
	
	a2estatic* model = sce->create_a2emodel<a2estatic>();
	model->load_model(e->data_path("plane.a2m"));
	model->set_material(mat);
	model->set_hard_scale(2.0f, 2.0f, 2.0f);
	model->set_position(0.0f, 0.0f, 0.0f);
	sce->add_model(model);
	models.push_back(model);
	
	// second: let there be light
	light* l = new light(e, 0.0f, 25.0f, 0.0f);
	l->set_radius(56.0f);
	l->set_color(1.0f, 1.0f, 1.0f);
	sce->add_light(l);
	lights.push_back(l);
	
	// third: add particle systems
	pm = new particle_manager(e);
	pm->set_manager(new particle_manager_mesh()); // set custom particle manager
	sce->add_particle_manager(pm);
}

void particle_debug_draw(particle_system* ps, rtt::fbo* pos_normal_fbo, rtt::fbo* border_fbo) {
	if(ps == nullptr ||
	   pos_normal_fbo == nullptr ||
	   border_fbo == nullptr) {
		return;
	}
	
	r->start_draw(particle_debug_fbo);
	r->clear();
	r->start_2d_draw();
	
	// draw
	particle_system::internal_particle_data* pdata = ps->get_internal_particle_data();
	
	glEnable(GL_BLEND);
	egfx->set_blend_mode(gfx::BLEND_MODE::PRE_MUL);
	glDepthMask(GL_FALSE);
	
	// point -> gs: quad
	gl3shader particle_draw = s->get_gl3shader("PARTICLE DEBUG");
	particle_draw->use();
	particle_draw->uniform("in_color", float4(0.0f, 0.0f, 1.0f, 1.0f));
	particle_draw->uniform("mvpm", matrix4f().ortho(0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f));
	
	particle_draw->attribute_array("in_vertex", pdata->ocl_gl_dir_vbo, 4);
	
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
	
	glDisable(GL_PROGRAM_POINT_SIZE);
	glPointSize(2.0f);
	glDrawElements(GL_POINTS, (GLsizei)pdata->particle_count, GL_UNSIGNED_INT, nullptr);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glEnable(GL_PROGRAM_POINT_SIZE);
	
	particle_draw->disable();
	
	glDepthMask(GL_TRUE);
	egfx->set_blend_mode(gfx::BLEND_MODE::DEFAULT);
	glDisable(GL_BLEND);
	
	r->stop_2d_draw();
	r->stop_draw();
	
	//debug_tex->tex_num = particle_debug_fbo->tex_id[0];
}
