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

#ifndef __KINICLES_AUDIO_HANDLER_H__
#define __KINICLES_AUDIO_HANDLER_H__

#include "globals.h"
#ifdef __APPLE__
#include <OpenAl/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

// half in samples
#define PLAY_BUFFER_SIZE 4096

class fft;
class audio_handler {
public:
	audio_handler(fft* f, const bool audio_playback);
	virtual ~audio_handler();
	
	class audio_capture : public thread_base {
	public:
		audio_capture(fft* f, const bool audio_playback);
		virtual ~audio_capture();
		virtual void run();
	protected:
		fft* f;
		const bool is_audio_playback;
		void fill_buffer(ALCvoid* data);
	};
	class audio_play : public thread_base {
	public:
		audio_play();
		virtual ~audio_play();
		virtual void run();
	protected:
		ALuint source;
	};
	
protected:
	fft* f;
	
	audio_capture* capture;
	audio_play* player;
	const bool is_audio_playback;
	
};

#endif
