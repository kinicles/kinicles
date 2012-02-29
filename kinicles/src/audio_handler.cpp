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

#include "audio_handler.h"
#include "fft.h"

static const ALint SAMPLE_RATE = 44100;
static const size_t CAPTURE_BUFFER_SIZE = 1024*1024*4;

static ALCdevice* play_device = nullptr;
static ALCdevice* capture_device = nullptr;
static ALCcontext* context = nullptr;

static const ALCuint audio_format = AL_FORMAT_MONO16;

//
char capture_buffer[CAPTURE_BUFFER_SIZE]; // ring buffer
size_t capture_offset = 0;
size_t read_offset = 0;
size_t read_end_offset = 0;

//
#define PLAY_BUFFER_COUNT 32
array<ALuint, PLAY_BUFFER_COUNT> play_buffers;
set<ALuint> available_buffers;
set<ALuint> empty_buffers;
vector<ALuint> queued_buffers;
static SDL_SpinLock buffer_lock;

//
#define AL_ERROR_CHECK() { \
	int error = alGetError(); \
	if(error != AL_NO_ERROR) { \
		a2e_error("OpenAL Error in line #%u: %X", __LINE__, error); \
	} \
}

audio_handler::audio_handler(fft* f_, const bool audio_playback) : f(f_), is_audio_playback(audio_playback) {
	capture_device = alcCaptureOpenDevice(nullptr, SAMPLE_RATE, audio_format, CAPTURE_BUFFER_SIZE);
	if(capture_device == nullptr) {
		a2e_error("failed to open capture device!");
	}
	AL_ERROR_CHECK();
	
	play_device = alcOpenDevice(nullptr);
	AL_ERROR_CHECK();
	context = alcCreateContext(play_device, nullptr);
	AL_ERROR_CHECK();
	if(play_device == nullptr || context == nullptr) {
		a2e_error("failed to create play device/context!");
	}
	alcMakeContextCurrent(context);
	AL_ERROR_CHECK();
	
	alGenBuffers(PLAY_BUFFER_COUNT, &play_buffers[0]);
	AL_ERROR_CHECK();
	for(size_t i = 0; i < PLAY_BUFFER_COUNT; i++) {
		empty_buffers.insert(play_buffers[i]);
	}
	
	alcCaptureStart(capture_device);
	AL_ERROR_CHECK();
	
	capture = new audio_capture(f, is_audio_playback);
	if(is_audio_playback) {
		player = new audio_play();
	}
	else player = nullptr;
}

audio_handler::~audio_handler() {
	delete capture;
	if(player != nullptr) delete player;
	
	alcCaptureStop(capture_device);
	alcCaptureCloseDevice(capture_device);
	
	alDeleteBuffers(PLAY_BUFFER_COUNT, &play_buffers[0]);
	alcDestroyContext(context);
	alcCloseDevice(play_device);
}

///////////////////////////////////////////////////////////////////////////////
// audio_capture

audio_handler::audio_capture::audio_capture(fft* f_, const bool audio_playback)
: thread_base(), f(f_), is_audio_playback(audio_playback) {
	this->set_thread_delay(10);
	this->start();
}

audio_handler::audio_capture::~audio_capture() {
}

void audio_handler::audio_capture::run() {
	// check if any samples were captured
	ALint captured_samples = 0;
	alcGetIntegerv(capture_device, ALC_CAPTURE_SAMPLES, (ALCsizei)sizeof(ALint), &captured_samples);
	AL_ERROR_CHECK();
	
	// no need to copy samples when there aren't enough to fill a buffer
	if(captured_samples >= PLAY_BUFFER_SIZE) {
		// copy captured samples to capture buffer
		const size_t samples_size = captured_samples * 2;
		if(capture_offset+samples_size >= CAPTURE_BUFFER_SIZE) {
			if(read_end_offset != 0) {
				a2e_error("overflow: capture buffer to far ahead of player!");
			}
			read_end_offset = capture_offset;
			capture_offset = 0;
		}
		alcCaptureSamples(capture_device, (ALCvoid*)(capture_buffer+capture_offset), captured_samples);
		capture_offset += samples_size;
		AL_ERROR_CHECK();
	}
	
	//
	for(;;) {
		if(read_end_offset == 0) {
			const size_t available_size = capture_offset - read_offset;
			if(available_size >= PLAY_BUFFER_SIZE) {
				fill_buffer((ALCvoid*)(capture_buffer+read_offset));
				read_offset += PLAY_BUFFER_SIZE;
			}
			else break;
		}
		else {
			// still completely reading at the end of the buffer?
			if(read_end_offset - read_offset >= PLAY_BUFFER_SIZE) {
				fill_buffer((ALCvoid*)(capture_buffer+read_offset));
				read_offset += PLAY_BUFFER_SIZE;
			}
			// else: partially at the end and the beginning
			else {
				const size_t available_size_end = read_end_offset - read_offset;
				const size_t available_size_start = capture_offset;
				const size_t available_size = available_size_end + available_size_start;
				if(available_size >= PLAY_BUFFER_SIZE) {
					// copy both parts to an intermediate buffer first, then copy to al device
					char play_buffer[PLAY_BUFFER_SIZE];
					memcpy(play_buffer, capture_buffer+read_offset, available_size_end);
					memcpy(play_buffer+available_size_end, capture_buffer, PLAY_BUFFER_SIZE-available_size_end);
					fill_buffer((ALCvoid*)play_buffer);
					read_end_offset = 0;
					read_offset = PLAY_BUFFER_SIZE - available_size_end;
				}
				else break;
			}
		}
	}
}

void audio_handler::audio_capture::fill_buffer(ALCvoid* data) {
	f->queue_data(data);
	
	// the subsequent code is only necessary for audio playback
	if(!is_audio_playback) return;
	
	if(empty_buffers.empty()) {
		a2e_error("underflow: no buffer available that can be filled - skipping data!");
		return;
	}
	
	SDL_AtomicLock(&buffer_lock);
	const ALuint empty_buffer = *empty_buffers.begin();
	empty_buffers.erase(empty_buffer);
	SDL_AtomicUnlock(&buffer_lock);
	
	alBufferData(empty_buffer, audio_format, data, PLAY_BUFFER_SIZE, SAMPLE_RATE);
	
	// remove from empty buffers and add it to the available buffers
	SDL_AtomicLock(&buffer_lock);
	available_buffers.insert(empty_buffer);
	SDL_AtomicUnlock(&buffer_lock);
}

///////////////////////////////////////////////////////////////////////////////
// audio_play

audio_handler::audio_play::audio_play() : thread_base() {
	alGenSources(1, &source);
	AL_ERROR_CHECK();
	
	alSourcef(source, AL_PITCH, 1.0f);
	alSourcef(source, AL_GAIN, 1.0f);
	alSource3f(source, AL_POSITION, 0, 0, 0);
	alSource3f(source, AL_VELOCITY, 0, 0, 0);
	alSourcei(source, AL_LOOPING, AL_FALSE);
	AL_ERROR_CHECK();
	
	this->set_thread_delay(10);
	this->start();
}

audio_handler::audio_play::~audio_play() {
	alDeleteSources(1, &source);
}

void audio_handler::audio_play::run() {
	// check if buffers can be unqueued
	ALint processed_buffers = 0;
	alGetSourcei(source, AL_BUFFERS_PROCESSED, &processed_buffers);
	AL_ERROR_CHECK();
	if(processed_buffers > 0) {
		if(processed_buffers > (int)queued_buffers.size()) {
			a2e_error("processed buffers > queued buffers"); // this shouldn't happen
			return;
		}
		
		alSourceUnqueueBuffers(source, processed_buffers, &queued_buffers[0]);
		SDL_AtomicLock(&buffer_lock);
		vector<ALuint> unqueue_buffers(queued_buffers.begin(), queued_buffers.begin()+processed_buffers);
		empty_buffers.insert(unqueue_buffers.begin(), unqueue_buffers.end());
		SDL_AtomicUnlock(&buffer_lock);
		
		AL_ERROR_CHECK();
		queued_buffers.erase(queued_buffers.begin(), queued_buffers.begin()+processed_buffers);
	}
	
	// queue new, available buffers
	if(!available_buffers.empty()) {
		SDL_AtomicLock(&buffer_lock);
		vector<ALuint> queue_buffers;
		queue_buffers.assign(available_buffers.begin(), available_buffers.end());
		available_buffers.clear();
		SDL_AtomicUnlock(&buffer_lock);
		
		alSourceQueueBuffers(source, queue_buffers.size(), &queue_buffers[0]);
		AL_ERROR_CHECK();
		queued_buffers.insert(queued_buffers.end(), queue_buffers.begin(), queue_buffers.end());
	}
	
	// check state of source and start playing if isn't already
	if(queued_buffers.size() > 0) {
		ALint source_state = 0;
		alGetSourcei(source, AL_SOURCE_STATE, &source_state);
		AL_ERROR_CHECK();
		if(source_state == AL_INITIAL ||
		   source_state == AL_STOPPED) {
			alSourcePlay(source);
			AL_ERROR_CHECK();
		}
	}
}
