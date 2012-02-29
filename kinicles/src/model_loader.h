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

#ifndef __KINICLES_MODEL_LOADER_H__
#define __KINICLES_MODEL_LOADER_H__

#include "globals.h"

struct bus_model;
class model_loader {
public:
	model_loader(engine* e);
	~model_loader();
	
	bus_model* load(const string& elements_filename,
					const string& vertices_filename,
					const string& tex_coords_filename,
					const string& bone_weights_filename,
					const string& bone_indices_filename,
					const string& matrices_filename);
	
protected:
	engine* e;
	file_io* f;
	
};

#endif
