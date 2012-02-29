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

#ifndef __KINICLES_NI_HANDLER_H__
#define __KINICLES_NI_HANDLER_H__

#include "globals.h"

#define PLAYER_JOINT_POSITION_COUNT 16
#define PLAYER_JOINT_POSITION_CELLS 5

class mesh_border;
class mesh_push;
class model_renderer;
class ni_handler {
public:
	ni_handler() = delete;
	~ni_handler() = delete;
	
	static int init(const string& recording_file, const mesh_border* mb, const mesh_push* mp);
	static void destroy();
	static void run();
	static void reset_players();
	
	static bool is_tracking(const unsigned int& user_id);
	static float4 get_joint_position(const unsigned int& user_id, const XnSkeletonJoint& joint);
	static float4 get_smooth_joint_position(const unsigned int& user_id, const XnSkeletonJoint& joint);
	static matrix4f get_joint_orientation(const unsigned int& user_id, const XnSkeletonJoint& joint);
	
	static const XnSkeletonJoint user_joint_positions[PLAYER_JOINT_POSITION_COUNT];
	
protected:
	static float skeleton_scale;
	
	struct player_data {
		model_renderer* mr;
		array<matrix4f, PLAYER_JOINT_POSITION_COUNT> matrices;
		
		// averaging
		array<size_t, PLAYER_JOINT_POSITION_COUNT> cell_indices;
		array<float4, PLAYER_JOINT_POSITION_COUNT> cell_avg;
		array<array<float4, PLAYER_JOINT_POSITION_CELLS>, PLAYER_JOINT_POSITION_COUNT> cells;
	};
	static map<XnUserID, player_data*> players;
	static map<XnSkeletonJoint, size_t> user_joint_map;
	static void update_player_orientations(const XnUserID& user_id);
	
	static void add_player(const XnUserID& id);
	static void remove_player(const XnUserID& id);

	// these must be static
	static void XN_CALLBACK_TYPE new_user(xn::UserGenerator& generator, XnUserID nId, void* pCookie);
	static void XN_CALLBACK_TYPE lost_user(xn::UserGenerator& generator, XnUserID nId, void* pCookie);
	static void XN_CALLBACK_TYPE pose_detected(xn::PoseDetectionCapability& capability, const XnChar* strPose, XnUserID nId, void* pCookie);
	static void XN_CALLBACK_TYPE calibration_start(xn::SkeletonCapability& capability, XnUserID nId, void* pCookie);
	static void XN_CALLBACK_TYPE calibration_complete(xn::SkeletonCapability& capability, XnUserID nId, XnCalibrationStatus eStatus, void* pCookie);
	
	static void XN_CALLBACK_TYPE calibration_in_progress(xn::SkeletonCapability& capability, XnUserID id, XnCalibrationStatus calibrationError, void* pCookie);
	static void XN_CALLBACK_TYPE pose_in_progress(xn::PoseDetectionCapability& capability, const XnChar* strPose, XnUserID id, XnPoseDetectionStatus poseError, void* pCookie);

};

#endif
