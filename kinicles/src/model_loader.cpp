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

#include "model_loader.h"
#include "model_renderer.h"

model_loader::model_loader(engine* e_) : e(e_) {
	f = e->get_file_io();
}

model_loader::~model_loader() {
}

bus_model* model_loader::load(const string& elements_filename,
							  const string& vertices_filename,
							  const string& tex_coords_filename,
							  const string& bone_weights_filename,
							  const string& bone_indices_filename,
							  const string& matrices_filename) {
	stringstream elements_buffer, vertices_buffer, tc_buffer, bweights_buffer, bindices_buffer, bm_buffer;
	if(!f->file_to_buffer(elements_filename, elements_buffer)) {
		a2e_error("couldn't open elements file: %s!", elements_filename);
		return nullptr;
	}
	if(!f->file_to_buffer(vertices_filename, vertices_buffer)) {
		a2e_error("couldn't open vertices file: %s!", vertices_filename);
		return nullptr;
	}
	if(!f->file_to_buffer(tex_coords_filename, tc_buffer)) {
		a2e_error("couldn't open tex coords file: %s!", tex_coords_filename);
		return nullptr;
	}
	if(!f->file_to_buffer(bone_weights_filename, bweights_buffer)) {
		a2e_error("couldn't open bone weights file: %s!", bone_weights_filename);
		return nullptr;
	}
	if(!f->file_to_buffer(bone_indices_filename, bindices_buffer)) {
		a2e_error("couldn't open bone indices file: %s!", bone_indices_filename);
		return nullptr;
	}
	if(!f->file_to_buffer(matrices_filename, bm_buffer)) {
		a2e_error("couldn't open matrices file: %s!", matrices_filename);
		return nullptr;
	}
	
	stringstream normals_buffer;
	if(!f->file_to_buffer(e->data_path("NI-Nrms.txt"), normals_buffer)) {
		a2e_error("couldn't open normals file: %s!", "NI-Nrms.txt");
		return nullptr;
	}
	
	//
	bus_model* bmodel = new bus_model();
	static const float model_scale = 1.0f/10.0f;
	
	// read elements
	while(!elements_buffer.eof() && elements_buffer.good()) {
		unsigned int i1, i2, i3;
		elements_buffer >> i1;
		elements_buffer >> i2;
		elements_buffer >> i3;
		bmodel->indices.push_back(index3(i1, i2, i3));
	}
	
	// read vertices
	while(!vertices_buffer.eof() && vertices_buffer.good()) {
		float x, y, z;
		vertices_buffer >> x;
		vertices_buffer >> y;
		vertices_buffer >> z;
		bmodel->vertices.push_back(float3(x, y, z));
	}
	
	// read tex coords
	while(!tc_buffer.eof() && tc_buffer.good()) {
		float u, v;
		tc_buffer >> u;
		tc_buffer >> v;
		bmodel->tex_coords.push_back(coord(u, v));
	}
	
	// read normals
	while(!normals_buffer.eof() && normals_buffer.good()) {
		float x, y, z;
		normals_buffer >> x;
		normals_buffer >> y;
		normals_buffer >> z;
		bmodel->normals.push_back(float3(x, y, z));
	}
	
	// read bone weights
	while(!bweights_buffer.eof() && bweights_buffer.good()) {
		float wx, wy, wz, ww;
		bweights_buffer >> wx;
		bweights_buffer >> wy;
		bweights_buffer >> wz;
		bweights_buffer >> ww;
		bmodel->bone_weights.push_back(float4(wx, wy, wz, ww));
	}
	
	// read bone indices
	while(!bindices_buffer.eof() && bindices_buffer.good()) {
		unsigned int ix, iy, iz, iw;
		bindices_buffer >> ix;
		bindices_buffer >> iy;
		bindices_buffer >> iz;
		bindices_buffer >> iw;
		bmodel->bone_indices.push_back(uint4(ix, iy, iz, iw));
	}
	
	// read inverse matrices
	size_t m_index = 0;
	while(!bm_buffer.eof() && bm_buffer.good()) {
		if(m_index >= PLAYERS_BONE_COUNT) {
			a2e_error("too many matrices!");
			return nullptr;
		}
		
		matrix4f m;
		for(size_t i = 0; i < 16; i++) {
			bm_buffer >> m.data[i];
		}
		
		bmodel->matrices[m_index] = m;
		bmodel->matrices[m_index][3] *= model_scale;
		bmodel->matrices[m_index][7] *= model_scale;
		bmodel->matrices[m_index][11] *= model_scale;
		bmodel->joint_positions[m_index].set(-m.data[3], -m.data[7], -m.data[11], 1.0f);
		m_index++;
	}
	if(m_index != PLAYERS_BONE_COUNT) {
		a2e_error("too few matrices!");
		return nullptr;
	}
	
	for(auto& vertex : bmodel->vertices) {
		vertex *= model_scale;
	}
	
	a2e_debug("model (%s, %s, %s) read: #indices: %u, #vertices: %u, #normals, %u, #tex coords: %u",
			  elements_filename, vertices_filename, tex_coords_filename,
			  bmodel->indices.size(), bmodel->vertices.size(), bmodel->normals.size(), bmodel->tex_coords.size());
	
	return bmodel;
}
