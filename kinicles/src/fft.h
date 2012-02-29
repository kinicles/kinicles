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

#ifndef __KINICLES_FFT_H__
#define __KINICLES_FFT_H__

#include "globals.h"
#include "audio_handler.h"
#include <iostream>
#include <fftw3.h>
using namespace std;

#define FFT_BUFFER_SIZE (PLAY_BUFFER_SIZE/2)
#define FFT_CL_BUFFER_WIDTH 128
#define FFT_CL_BUFFER_HEIGHT 128
#define FFT_QUEUE_SIZE 64

extern float spectrum_sum;

class fft : public thread_base {
public:
	fft(const bool random_spectrum);
	virtual ~fft();

	void queue_data(void* data);
	virtual void run();
	
	void write_lock();
	void write_unlock();
	
	opencl::buffer_object* get_spectrum_buffer() {
		return cl_spectrum_buffer;
	}
	const size_t get_active_spectrum_line() const {
		return spectrum_buffer_line;
	}

protected:
	fftwf_complex* out;
	
	opencl::buffer_object* cl_spectrum_buffer;
	size_t spectrum_buffer_line;
	
	array<array<short int, FFT_BUFFER_SIZE>, FFT_QUEUE_SIZE> buffer_queue;
	atomic_t queue_state[FFT_QUEUE_SIZE];
	atomic_t queue_index;
	size_t proc_index;
	
	atomic_t cl_lock;
	
	void compute_fft(void* in, float* power_spectrum);
	
	bool random_spectrum;
	
};

#endif

