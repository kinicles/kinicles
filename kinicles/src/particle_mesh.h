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

#ifndef __KINICLES_PARTICLE_MESH_H__
#define __KINICLES_PARTICLE_MESH_H__

#include "globals.h"
#include <particle/particle_cl.h>

class particle_manager_mesh : public particle_manager_cl {
public:
	particle_manager_mesh();
	virtual ~particle_manager_mesh();
	
	struct mesh_aux_data {
		opencl::buffer_object* mesh_pos_tex;
		opencl::buffer_object* mesh_normal_tex;
		opencl::buffer_object* mesh_border_tex;
		opencl::buffer_object* mesh_push_tex;
		rtt::fbo* pos_normal_fbo;
		rtt::fbo* border_fbo;
		rtt::fbo* push_fbo;
		
		opencl::buffer_object* ocl_aux_2;
	};
	
	virtual void draw(const rtt::fbo* frame_buffer);
	virtual void reset_particle_system(particle_system* ps);
	virtual void run_particle_system(particle_system* ps);
	virtual void draw_particle_system(particle_system* ps, const rtt::fbo* frame_buffer);
	
protected:
	a2e_texture mesh_volume_tex;
	opencl::buffer_object* cl_mesh_volume_tex;
	
	void motion_blur_post_process(rtt::fbo* scene_buffer);
	rtt::fbo* particle_render_buffer;
	rtt::fbo* motion_blur_buffers[2];
	size_t active_mb_buffer;
	
};

#endif
