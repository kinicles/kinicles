
#include "a2e_cl_global.h"
#include "a2e_cl_particle_init.h"

// this is used for both the initial particle spawning and the particle respawning

#ifdef A2E_PARTICLE_INIT
__kernel AUTO_VEC_HINT void particle_init
#else
__kernel AUTO_VEC_HINT void particle_respawn
#endif
				(const uint type, const float living_time, const uint particle_count, const float velocity, const float4 angle, const float4 extents, const float4 direction,
				 const float4 position_offset, const uint seed,
#ifdef A2E_PARTICLE_INIT
				 const float spawn_rate_ts,
#else
				 const float4 gravity,
#endif
				 __global float4* pos_time_buffer, __global float4* dir_buffer) {
	const uint particle_num = get_global_id(0);
	
	// init this kernel seed (this _must_ by different for each particle, also offset particle number by seed, so we don't multiply 16807 by small numbers)
	uint kernel_seed = seed ^ mul24(seed - particle_num, 16807u);
	
#ifndef A2E_PARTICLE_INIT
	float4 pos_time = pos_time_buffer[particle_num];
	//float4 dir_vel = dir_buffer[particle_num];
	float particle_time = pos_time.w;
	if(particle_time > 0.0f) return;
	
	float spawn_rate_ts = 0.0f;
#endif
	
	float8 ret;
	init_particles(&ret, &kernel_seed, type, spawn_rate_ts, living_time, particle_count, velocity, angle.xyz, extents.xyz, direction.xyz, position_offset.xyz);
	float4 position = ret.s0123;
	float4 dir = ret.s4567;
	
#ifndef A2E_PARTICLE_INIT
	float tpassed = -particle_time;
	float time_step = tpassed / 1000.0f;
	float3 acceleration = gravity.xyz * time_step;
	dir.xyz += acceleration;
	position += dir * time_step;
	
	particle_time += living_time;
	position = (float4)(position.x, position.y, position.z, particle_time);
	dir = (float4)(dir.x, dir.y, dir.z, 0.0f);
#endif

	pos_time_buffer[particle_num] = position;
	dir_buffer[particle_num] = dir;
}
