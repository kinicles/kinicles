
#ifndef __PARTICLE_INIT_CLH__
#define __PARTICLE_INIT_CLH__

#include "a2e_cl_global.h"

float2 sincos_f2(const float val) {
	return (float2)(sin(val), cos(val));
}

void AUTO_VEC_HINT init_particles(float8* ret, uint* kernel_seed,
								  const uint type, const float spawn_rate_ts, const float living_time, const uint particle_count, const float velocity,
								  const float3 angle, const float3 extents, const float3 direction, const float3 position_offset) {
	uint particle_num = get_global_id(0);
	float ltime = spawn_rate_ts;

	float4 position = (float4)0.0f;
	float4 dir;
	// output buffers (position, direction, velocity, living_time)
	switch(type) {
		case 0: { // box emitter
			float3 rand = (float3)(sfrand_m1_1(kernel_seed),
								   sfrand_m1_1(kernel_seed),
								   sfrand_m1_1(kernel_seed));
			// [-0.5, 0.5] * extents
			position.xyz = 0.5f * rand.xyz * extents.xyz;
		}
		break;
		case 1: { // sphere emitter
			float theta = 2.0f * A2E_PI * sfrand_0_1(kernel_seed);
			float u = sfrand_m1_1(kernel_seed);
			float squ = sqrt(1.0f - u*u);
			position.xyz = (float3)(squ * cos(theta), squ * sin(theta), u);
			position.xyz *= extents.x * sqrt(sfrand_0_1(kernel_seed)); // uniform scale
		}
		break;
		case 2: // point emitter
		default:
			break;
	}
	
	position.xyz += position_offset;
	
	if(spawn_rate_ts != 0.0f) {
		float particle_batch_ts = floor(particle_num / spawn_rate_ts);
		ltime = living_time + particle_batch_ts * 40.0f + ((particle_num / spawn_rate_ts) - particle_batch_ts) * 40.0f; // 1000 ms / 25 iterations = 40 ms
	}
	
	// rotate direction
	float3 rangle = (float3)(sfrand_m1_1(kernel_seed),
							 sfrand_m1_1(kernel_seed),
							 sfrand_m1_1(kernel_seed)) * angle.xyz;
	float3 sina = sin(rangle);
	float3 cosa = cos(rangle);
	
	/*
	 ((1,0,0),(0,cos(x),sin(x)),(0,-sin(x),cos(x))) *
	 ((cos(y),0,-sin(y)),(0,1,0),(sin(y),0,cos(y))) *
	 ((cos(z),sin(z),0),(-sin(z),cos(z),0),(0,0,1))
	 */
	dir.x = dot((float3)(cosa.y * cosa.z, cosa.y * sina.z, -sina.y), direction.xyz);
	
	dir.y = dot((float3)(cosa.z * sina.x * sina.y - cosa.x * sina.z,
						 cosa.x * cosa.z + sina.x * sina.y * sina.z,
						 cosa.y * sina.x), direction.xyz);
	
	dir.z = dot((float3)(cosa.x * cosa.z * sina.y + sina.x * sina.z,
						 cosa.x * sina.y * sina.z - cosa.z * sina.x,
						 cosa.x * cosa.y), direction.xyz);
	
	dir *= velocity;
	
	*ret = (float8)(position.x, position.y, position.z, ltime, dir.x, dir.y, dir.z, 0.0f);
}

#endif
