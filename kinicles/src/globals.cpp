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

#include "globals.h"

engine* e = nullptr;
file_io* fio = nullptr;
event* eevt = nullptr;
gfx* egfx = nullptr;
texman* t = nullptr;
opencl* ocl = nullptr;
ext* exts = nullptr;
shader* s = nullptr;
scene* sce = nullptr;
camera* cam = nullptr;
rtt* r = nullptr;
particle_manager* pm = nullptr;
audio_handler* ah = nullptr;
fft* f = nullptr;
a2e_texture debug_tex;

ni_handler* ni = nullptr;
