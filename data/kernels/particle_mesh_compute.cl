
#include "a2e_cl_global.h"

#define SPECTRUM_SIZE (SPECTRUM_WIDTH * SPECTRUM_HEIGHT)
#define FSPECTRUM_SIZE ((float)SPECTRUM_SIZE)
#define SPECTRUM_HALF_WIDTH (SPECTRUM_WIDTH >> 1)

inline bool is_null(const float3* vec) {
	return (dot(*vec, *vec) == 0.0f ? true : false);
}

__kernel AUTO_VEC_HINT void particle_compute(const float time_passed,
											 const float living_time,
											 const uint particle_count,
											 const float4 camera_pos,
											 read_only image2d_t mesh_pos_tex,
											 read_only image2d_t mesh_normal_tex,
											 read_only image2d_t mesh_border_tex,
											 read_only image2d_t mesh_push_tex,
											 read_only image2d_t mesh_volume_tex,
											 __global float4* pos_time_buffer,
											 __global float4* aux_buffer,
											 __global float16* aux_2_buffer,
											 __global const float* spectrum_buffer,
											 const uint spectrum_line) {
	const uint particle_num = get_global_id(0);
	const sampler_t point_repeat_sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_FILTER_NEAREST | CLK_ADDRESS_REPEAT;
	const sampler_t linear_repeat_sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_FILTER_LINEAR | CLK_ADDRESS_REPEAT;
	
	float4 pos_time = pos_time_buffer[particle_num];
	float4 aux = aux_buffer[particle_num];
	float16 aux_2 = aux_2_buffer[particle_num];
	const float3 old_pos = pos_time.xyz;
	const float rand_depth = aux_2.s0;
	float3 acceleration = aux_2.s123;
	const float2 rand_circle_dir = aux_2.s45;
	
	// advance time
	const float time_speed = 0.1f;
	float time_step = (time_passed * time_speed) / 1000.0f;
	
	// check direction state (mesh border)
	const float2 mb_tex_coord = aux.xy;
	const float3 border_dir = read_imagef(mesh_border_tex, point_repeat_sampler, mb_tex_coord).xyz;
	
	const float cdist = 0.005f;
	const float2 cp_1 = (float2)(0.25f, 0.79512f);
	
	const float border_state = (is_null(&border_dir) && !(fast_distance(cp_1, aux.xy) < cdist) ? 0.0f : 1.0f);
	
	// if current border != new state
	float ignore_acc = 0.0f;
	if(pos_time.w != border_state) {
		// flip u/x if moving from the mesh to the outside
		if(pos_time.w == 0.0f) {
			aux.x = 1.0f - fmod(aux.x, 1.0f);
			aux.xy += normalize(rand_circle_dir) * cdist;
			ignore_acc = 1.0f;
		}
		pos_time.w = border_state;
	}
	
	// "simulate"
	if(border_dir.x == 0.0f && border_dir.y == 0.0f) {
		// read current position
		float3 pos = read_imagef(mesh_pos_tex, point_repeat_sampler, aux.xy).xyz;
		const float3 normal = read_imagef(mesh_normal_tex, point_repeat_sampler, aux.xy).xyz;
		
		// advance particle uv coord
		const float2 push_dir = read_imagef(mesh_push_tex, linear_repeat_sampler, aux.xy).xy;
		const float flip_dir = (aux.x < 0.5f ? -1.0f : 1.0f);
		const float2 uv_step = time_step * (push_dir + flip_dir * (float2)(-push_dir.y, push_dir.x));
		aux.xy += uv_step;
		
		// get power spectrum at this point
		float power = 0.0f;
		float depth_scale = 0.0f;
		if(aux.x >= 0.0f && aux.y >= 0.0f) {
			const float2 mod_uv = aux.xy - floor(aux.xy);
			const float2 fspectrum_pos = floor(mod_uv * (float2)(SPECTRUM_WIDTH, SPECTRUM_HEIGHT));
			const int2 spectrum_pos = (int2)(fspectrum_pos.x, fspectrum_pos.y);
			const int y_offset = spectrum_pos.y * SPECTRUM_WIDTH;
			
			const int x_offset_0 = spectrum_pos.x;
			const int x_offset_1 = SPECTRUM_WIDTH - spectrum_pos.x;
			const int x_offset_2 = abs(SPECTRUM_HALF_WIDTH - spectrum_pos.x);
			const int x_offset_3 = SPECTRUM_WIDTH - x_offset_2;
			
			power = spectrum_buffer[(y_offset + x_offset_0) % SPECTRUM_SIZE];
			power += spectrum_buffer[(y_offset + x_offset_1) % SPECTRUM_SIZE];
			power += spectrum_buffer[(y_offset + x_offset_2) % SPECTRUM_SIZE];
			power += spectrum_buffer[(y_offset + x_offset_3) % SPECTRUM_SIZE];
			power /= 2.0f;
			
			power = clamp(power, 0.0f, 10.0f);
			
			// set particle color dependent on power
			aux.z = fmod(power, 1.0f);
			aux.w = fmod(clamp((power - 1.0f), 0.0f, 3.0f), 1.0f) + clamp(aux.w, 0.0f, 0.2f);
			
			power /= 2.0f;
			if(power > 2.5f) {
				depth_scale = clamp(log((power * power) / 10.0f), 0.0f, 10.0f);
			}
		}
		
		// mesh depth texture is flipped (on y)
		const float mesh_depth = read_imagef(mesh_volume_tex, linear_repeat_sampler, (float2)(aux.x, 1.0f - aux.y)).x;
		const float3 depth_vec = normal * (-mesh_depth) * 1.3f * 0.25f * depth_scale; // "depth"
		
		//
		const float ACC_EPSILON = 0.001f;
		const float3 null_vec = (float3)(ACC_EPSILON, ACC_EPSILON, ACC_EPSILON);
		const bool old_pos_null = all(islessequal(old_pos, null_vec));
		const bool pos_null = all(islessequal(pos, null_vec));
		float3 new_acc = (float3)(0.0f, 0.0f, 0.0f);
		if(!pos_null) {
			pos += -depth_vec; // position isn't 0, so depth vec can be applied safely
			if(!old_pos_null) {
				// only set acc vec if neither position is 0
				new_acc = pos - old_pos;
				
				//
				const float dist = length(new_acc);
				const float step_size = clamp(time_passed / 1000.0f, 0.0f, 1.0f);
				if(dist < 5.0f && ignore_acc < 1.0f) {
					const float _tweak_it = 250.0f;
					const float friction = 0.75f;
					acceleration += acceleration * step_size;
					acceleration += new_acc * step_size;
					acceleration *= friction;
					pos_time.xyz = old_pos + acceleration * step_size * _tweak_it;
					aux_2.s123 = acceleration;
				}
				else {
					// if distance is too big, just set the position and reset acceleration
					pos_time.xyz = pos; // comment/disable this to see if above simulation fails
					aux_2.s123 = (float3)(0.0f, 0.0f, 0.0f);
					acceleration = (float3)(0.0f, 0.0f, 0.0f);
				}
			}
			else {
				// else, set initial position
				pos_time.xyz = pos;
				aux_2.s123 = (float3)(0.0f, 0.0f, 0.0f);
			}
		}
	}
	else {
		// advance particle uv in mesh border color (xy) direction
		aux.xy += border_dir.xy * border_dir.z;
		pos_time.xyz = (float3)(0.0f, 0.0f, 0.0f);
	}
#if 0
	// display power spectrum for debugging purposes:
	const float spectrum_size = (float)(SPECTRUM_WIDTH * SPECTRUM_HEIGHT);
	const float fparticle_num = fmod((float)particle_num, spectrum_size);
	const float power = spectrum_buffer[(size_t)floor(fparticle_num)];
	aux.xy = (float2)(fmod(fparticle_num, SPECTRUM_WIDTH), fparticle_num / SPECTRUM_WIDTH);
	aux.x /= (float)SPECTRUM_WIDTH;
	aux.y /= (float)SPECTRUM_HEIGHT;
	aux.z = 0.0f;
	aux.w = clamp(power, 0.0f, 5.0f);
#endif

	pos_time_buffer[particle_num] = pos_time;
	aux_buffer[particle_num] = aux;
	aux_2_buffer[particle_num] = aux_2;
}
