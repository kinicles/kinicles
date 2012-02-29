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

#include "fft.h"

#define FFT_THREADING 0

fft::fft(const bool random_spectrum_) : thread_base(), random_spectrum(random_spectrum_) {
#if FFT_THREADING
	// enable fftw threading
	if(fftwf_init_threads() == 0) {
		a2e_error("couldn't initialize fftw threading!");
	}
	fftwf_plan_with_nthreads(4);
#endif
	
	// create/init fftw data
	out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * FFT_BUFFER_SIZE);
	
	// 
	cl_spectrum_buffer = ocl->create_buffer(opencl::BT_READ | opencl::BT_BLOCK_ON_WRITE,
											FFT_CL_BUFFER_WIDTH * FFT_CL_BUFFER_HEIGHT * sizeof(float));
	spectrum_buffer_line = 0;
	
	//
	for(size_t i = 0; i < FFT_QUEUE_SIZE; i++) {
		AtomicSet(&queue_state[i], 0);
	}
	AtomicSet(&queue_index, 0);
	proc_index = 0;
	
	//
	AtomicSet(&cl_lock, 0);
	
	// start thread
	this->set_thread_delay(2);
	this->start();
}

fft::~fft() {
	// wait until queue is empty before we delete anything
	for(size_t i = 0; i < FFT_QUEUE_SIZE; i++) {
		while(AtomicGet(&queue_state[proc_index]) != 0) {
			SDL_Delay(1);
		}
	}
	ocl->delete_buffer(cl_spectrum_buffer);
	
	fftwf_free(out);
#if FFT_THREADING
	fftwf_cleanup_threads();
#endif
	fftwf_cleanup();
}

void fft::compute_fft(void* in, float* power_spectrum) {
	short int* sbuffer = (short int*)in;
	static float float_buffer[FFT_BUFFER_SIZE * 2];
	for(size_t i = 0; i < FFT_BUFFER_SIZE; i++) {
		float_buffer[i * 2] = ((float)sbuffer[i]) * (1.0f / 32768.0f);
		float_buffer[i * 2 + 1] = 0.0f;
	}

	fftwf_plan plan = fftwf_plan_dft_1d(FFT_BUFFER_SIZE, (fftwf_complex*)float_buffer, out, FFTW_FORWARD, FFTW_ESTIMATE);
	fftwf_execute(plan);

	for(size_t i = 0; i < FFT_BUFFER_SIZE / 2 ;i++) {
		power_spectrum[i] = out[i][0] * out[i][0] + out[i][1] * out[i][1];
	}
	
	fftwf_destroy_plan(plan);
}

void fft::queue_data(void* data) {
	// get index
	const size_t cur_idx = AtomicFetchThenIncrement(&queue_index) % FFT_QUEUE_SIZE;
	
	// check current queue state for this queue slot
	for(;;) {
		if(AtomicCAS(&queue_state[cur_idx], 0, 1)) {
			break;
		}
		
		// queue is full: empty it (-> ignore recorded samples) and return
		for(size_t i = 0; i < FFT_QUEUE_SIZE; i++) {
			AtomicSet(&queue_state[i], 0);
		}
		a2e_debug("emptied full queue!");
		return;
	}
	
	// copy data to queue
	memcpy(&buffer_queue[cur_idx][0], data, FFT_BUFFER_SIZE * sizeof(short int));
}

void fft::run() {
	if(AtomicGet(&queue_state[proc_index]) == 0) {
		// nothing to do, return
		return;
	}
	
	for(size_t pass = 0; pass < 16; pass++) { // max: 16 passes
		if(AtomicGet(&queue_state[proc_index]) == 0) {
			break;
		}
		
		float power_spectrum[FFT_BUFFER_SIZE / 2];
		array<float, FFT_CL_BUFFER_WIDTH> avg_buffer;
		static const size_t bucket_size = (FFT_BUFFER_SIZE / 2) / FFT_CL_BUFFER_WIDTH;
		static const size_t half_bucket_size = bucket_size / 2;
		static const float fhalf_bucket_size = float(half_bucket_size);
		
		if(random_spectrum) {
			for(size_t i = 0; i < FFT_CL_BUFFER_WIDTH; i++) {
				avg_buffer[i] = core::rand(0.0f, 2.0f);
			}
		}
		else {
			compute_fft(&buffer_queue[proc_index][0], power_spectrum);
			
			for(size_t i = 0; i < FFT_CL_BUFFER_WIDTH; i++) {
				avg_buffer[i] = 0.0f;
			}
			for(size_t i = 0; i < FFT_CL_BUFFER_WIDTH; i++) {
				for(size_t j = 0; j < half_bucket_size; j++) {
					avg_buffer[i] += fabs(power_spectrum[i*half_bucket_size + j]);
				}
				avg_buffer[i] /= fhalf_bucket_size;
				avg_buffer[i] = core::clamp(avg_buffer[i], 0.0f, 4.0f);
			}
		}
		
		ocl->write_buffer(cl_spectrum_buffer, &avg_buffer[0],
						  FFT_CL_BUFFER_WIDTH * spectrum_buffer_line * sizeof(float),
						  FFT_CL_BUFFER_WIDTH * sizeof(float));
		spectrum_buffer_line = (spectrum_buffer_line + 1) % FFT_CL_BUFFER_HEIGHT;
		
		// make slot available again, set next index
		AtomicSet(&queue_state[proc_index], 0);
		proc_index = (proc_index + 1) % FFT_QUEUE_SIZE;
	}
	
	// make sure that queue_index doesn't overflow
	if(AtomicGet(&queue_index) < FFT_QUEUE_SIZE) return;
	for(;;) {
		int cur_queue_idx = AtomicGet(&queue_index);
		if(AtomicCAS(&queue_index, cur_queue_idx, cur_queue_idx % FFT_QUEUE_SIZE)) {
			break;
		}
	}
}

void fft::write_lock() {
	for(;;) {
		if(AtomicCAS(&cl_lock, 0, 1)) {
			break;
		}
	}
}

void fft::write_unlock() {
	if(!AtomicCAS(&cl_lock, 1, 0)) {
		a2e_error("failed to unlock - this should have been 1!");
	}
}
