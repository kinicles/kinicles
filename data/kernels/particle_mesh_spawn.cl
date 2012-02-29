
#include "a2e_cl_global.h"

// this is used for both the initial particle spawning and the particle respawning

#ifdef A2E_PARTICLE_INIT
__kernel AUTO_VEC_HINT void particle_init
#else
__kernel AUTO_VEC_HINT void particle_respawn
#endif
				(const float living_time, const uint particle_count,
				 const float4 position_offset, const uint seed,
#ifdef A2E_PARTICLE_INIT
				 const float spawn_rate_ts,
#endif
				 __global float4* pos_time_buffer, __global float4* aux_buffer, __global float16* aux_2_buffer)
{
	const uint particle_num = get_global_id(0);
	const sampler_t point_repeat_sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_FILTER_NEAREST | CLK_ADDRESS_REPEAT;
	
	// init this kernel seed (this _must_ by different for each particle, also offset particle number by seed, so we don't multiply 16807 by small numbers)
	uint kernel_seed = seed ^ mul24(seed - particle_num, 16807u);
	
	// uv state: position + direction
	float2 dir = (float2)(sfrand_m1_1(&kernel_seed), sfrand_m1_1(&kernel_seed));
	
	float4 aux = (float4)(sfrand_0_1(&kernel_seed),		// u
						  sfrand_0_1(&kernel_seed),		// v
						  sfrand_0_1(&kernel_seed),		// rand #0
						  sfrand_0_1(&kernel_seed));	// rand #1
	float16 aux_2 = (float16)(sfrand_0_1(&kernel_seed), 0.0f, 0.0f, 0.0f,
							  dir.x, dir.y, 0.0f, 0.0f,
							  0.0f, 0.0f, 0.0f, 0.0f,
							  0.0f, 0.0f, 0.0f, 0.0f);
	float4 position = (float4)(0.0f, 0.0f, 0.0f, 0.0f);

	pos_time_buffer[particle_num] = position;
	aux_buffer[particle_num] = aux;
	aux_2_buffer[particle_num] = aux_2;
}
