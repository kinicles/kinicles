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

#include "ni_handler.h"
#include "model_renderer.h"
#include "mesh_border.h"
#include "mesh_push.h"

static xn::Context openni_context;
static xn::ScriptNode script_node;
static xn::DepthGenerator depth_generator;
static xn::UserGenerator user_generator;
static xn::Player openni_player;
static const mesh_border* mb = nullptr;
static const mesh_push* mp = nullptr;

XnBool g_bNeedPose = FALSE;
XnChar g_strPose[20] = "";
XnBool g_bDrawBackground = TRUE;
XnBool g_bDrawPixels = TRUE;
XnBool g_bDrawSkeleton = TRUE;
XnBool g_bPrintID = TRUE;
XnBool g_bPrintState = TRUE;

#define GL_WIN_SIZE_X 720
#define GL_WIN_SIZE_Y 480

XnBool g_bPause = false;
XnBool g_bRecord = false;

XnBool g_bQuit = false;

#define SAMPLE_XML_PATH "players-sample-user.xml"

#define CHECK_RC(nRetVal, what)										\
if(nRetVal != XN_STATUS_OK) {										\
	a2e_error("%s failed: %s", what, xnGetStatusString(nRetVal));	\
	return nRetVal;													\
}

float ni_handler::skeleton_scale = 1.0f/100.0f;
map<XnUserID, ni_handler::player_data*> ni_handler::players;
const XnSkeletonJoint ni_handler::user_joint_positions[PLAYER_JOINT_POSITION_COUNT] = {
	XN_SKEL_HEAD,
	XN_SKEL_NECK,
	XN_SKEL_TORSO,
	XN_SKEL_LEFT_SHOULDER,
	XN_SKEL_LEFT_ELBOW,
	XN_SKEL_LEFT_HAND,
	XN_SKEL_RIGHT_SHOULDER,
	XN_SKEL_RIGHT_ELBOW,
	XN_SKEL_RIGHT_HAND,
	XN_SKEL_LEFT_HIP,
	XN_SKEL_LEFT_KNEE,
	XN_SKEL_LEFT_FOOT,
	XN_SKEL_RIGHT_HIP,
	XN_SKEL_RIGHT_KNEE,
	XN_SKEL_RIGHT_FOOT,
	XN_SKEL_WAIST
};
map<XnSkeletonJoint, size_t> ni_handler::user_joint_map;

void ni_handler::destroy() {
	script_node.Release();
	depth_generator.Release();
	user_generator.Release();
	openni_player.Release();
	openni_context.Release();
}

int ni_handler::init(const string& recording_file, const mesh_border* mb_, const mesh_push* mp_) {
	XnStatus nRetVal = XN_STATUS_OK;
	mb = mb_;
	mp = mp_;
	
	if(!recording_file.empty()) {
		nRetVal = openni_context.Init();
		CHECK_RC(nRetVal, "Init");
		nRetVal = openni_context.OpenFileRecording(e->data_path(recording_file).c_str(), openni_player);
		if(nRetVal != XN_STATUS_OK) {
			a2e_error("Can't open recording %s: %s", e->data_path(recording_file).c_str(), xnGetStatusString(nRetVal));
			return 1;
		}
	}
	else {
		xn::EnumerationErrors errors;
		nRetVal = openni_context.InitFromXmlFile(e->data_path(SAMPLE_XML_PATH).c_str(), script_node, &errors);
		if(nRetVal == XN_STATUS_NO_NODE_PRESENT) {
			XnChar strError[1024];
			errors.ToString(strError, 1024);
			a2e_error("%s", strError);
			return (nRetVal);
		}
		else if(nRetVal != XN_STATUS_OK) {
			a2e_error("Open failed: %s", xnGetStatusString(nRetVal));
			return (nRetVal);
		}
	}
	
	nRetVal = openni_context.FindExistingNode(XN_NODE_TYPE_DEPTH, depth_generator);
	if(nRetVal != XN_STATUS_OK) {
		a2e_debug("No depth generator found. Using a default one...");
		xn::MockDepthGenerator mockDepth;
		nRetVal = mockDepth.Create(openni_context);
		CHECK_RC(nRetVal, "Create mock depth");
		
		// set some defaults
		XnMapOutputMode defaultMode;
		defaultMode.nXRes = 320;
		defaultMode.nYRes = 240;
		defaultMode.nFPS = 30;
		nRetVal = mockDepth.SetMapOutputMode(defaultMode);
		CHECK_RC(nRetVal, "set default mode");
		
		// set FOV
		XnFieldOfView fov;
		fov.fHFOV = 1.0225999419141749;
		fov.fVFOV = 0.79661567681716894;
		nRetVal = mockDepth.SetGeneralProperty(XN_PROP_FIELD_OF_VIEW, sizeof(fov), &fov);
		CHECK_RC(nRetVal, "set FOV");
		
		XnUInt32 nDataSize = defaultMode.nXRes * defaultMode.nYRes * sizeof(XnDepthPixel);
		XnDepthPixel* pData = (XnDepthPixel*)xnOSCallocAligned(nDataSize, 1, XN_DEFAULT_MEM_ALIGN);
		
		nRetVal = mockDepth.SetData(1, 0, nDataSize, pData);
		CHECK_RC(nRetVal, "set empty depth map");
		
		depth_generator = mockDepth;
	}
	depth_generator.GetMirrorCap().SetMirror(false);
	
	nRetVal = openni_context.FindExistingNode(XN_NODE_TYPE_USER, user_generator);
	if(nRetVal != XN_STATUS_OK) {
		nRetVal = user_generator.Create(openni_context);
		CHECK_RC(nRetVal, "Find user generator");
	}
	
	XnCallbackHandle hUserCallbacks, hCalibrationStart, hCalibrationComplete, hPoseDetected, hCalibrationInProgress, hPoseInProgress;
	if(!user_generator.IsCapabilitySupported(XN_CAPABILITY_SKELETON)) {
		a2e_error("Supplied user generator doesn't support skeleton");
		return 1;
	}
	nRetVal = user_generator.RegisterUserCallbacks(new_user, lost_user, nullptr, hUserCallbacks);
	CHECK_RC(nRetVal, "Register to user callbacks");
	nRetVal = user_generator.GetSkeletonCap().RegisterToCalibrationStart(calibration_start, nullptr, hCalibrationStart);
	CHECK_RC(nRetVal, "Register to calibration start");
	nRetVal = user_generator.GetSkeletonCap().RegisterToCalibrationComplete(calibration_complete, nullptr, hCalibrationComplete);
	CHECK_RC(nRetVal, "Register to calibration complete");
	
	if(user_generator.GetSkeletonCap().NeedPoseForCalibration()) {
		g_bNeedPose = TRUE;
		if(!user_generator.IsCapabilitySupported(XN_CAPABILITY_POSE_DETECTION)) {
			a2e_error("Pose required, but not supported");
			return 1;
		}
		nRetVal = user_generator.GetPoseDetectionCap().RegisterToPoseDetected(pose_detected, nullptr, hPoseDetected);
		CHECK_RC(nRetVal, "Register to Pose Detected");
	}
	
	user_generator.GetSkeletonCap().SetSkeletonProfile(XN_SKEL_PROFILE_ALL);
	
	nRetVal = user_generator.GetSkeletonCap().RegisterToCalibrationInProgress(calibration_in_progress, nullptr, hCalibrationInProgress);
	CHECK_RC(nRetVal, "Register to calibration in progress");
	
	nRetVal = user_generator.GetPoseDetectionCap().RegisterToPoseInProgress(pose_in_progress, nullptr, hPoseInProgress);
	CHECK_RC(nRetVal, "Register to pose in progress");
	
	nRetVal = openni_context.StartGeneratingAll();
	CHECK_RC(nRetVal, "StartGenerating");
	
	//
	size_t joint_num = 0;
	user_joint_map[XN_SKEL_HEAD] = joint_num++;
	user_joint_map[XN_SKEL_NECK] = joint_num++;
	user_joint_map[XN_SKEL_TORSO] = joint_num++;
	user_joint_map[XN_SKEL_LEFT_SHOULDER] = joint_num++;
	user_joint_map[XN_SKEL_LEFT_ELBOW] = joint_num++;
	user_joint_map[XN_SKEL_LEFT_HAND] = joint_num++;
	user_joint_map[XN_SKEL_RIGHT_SHOULDER] = joint_num++;
	user_joint_map[XN_SKEL_RIGHT_ELBOW] = joint_num++;
	user_joint_map[XN_SKEL_RIGHT_HAND] = joint_num++;
	user_joint_map[XN_SKEL_LEFT_HIP] = joint_num++;
	user_joint_map[XN_SKEL_LEFT_KNEE] = joint_num++;
	user_joint_map[XN_SKEL_LEFT_FOOT] = joint_num++;
	user_joint_map[XN_SKEL_RIGHT_HIP] = joint_num++;
	user_joint_map[XN_SKEL_RIGHT_KNEE] = joint_num++;
	user_joint_map[XN_SKEL_RIGHT_FOOT] = joint_num++;
	user_joint_map[XN_SKEL_WAIST] = joint_num++;
	
	return 0;
}

void ni_handler::run() {
	xn::SceneMetaData scene_md;
	xn::DepthMetaData depth_md;
	depth_generator.GetMetaData(depth_md);
	
	// Read next available data
	//openni_context.WaitOneUpdateAll(user_generator);
	openni_context.WaitNoneUpdateAll();
	
	for(const auto& player : players) {
		update_player_orientations(player.first);
		player.second->mr->rtt(player.first);
	}
}

//
static map<XnUInt32, pair<XnCalibrationStatus, XnPoseDetectionStatus>> m_Errors;
void XN_CALLBACK_TYPE ni_handler::calibration_in_progress(xn::SkeletonCapability& capability, XnUserID id, XnCalibrationStatus calibrationError, void* pCookie) {
	m_Errors[id].first = calibrationError;
}
void XN_CALLBACK_TYPE ni_handler::pose_in_progress(xn::PoseDetectionCapability& capability, const XnChar* strPose, XnUserID id, XnPoseDetectionStatus poseError, void* pCookie) {
	m_Errors[id].second = poseError;
}

// Callback: New user was detected
void XN_CALLBACK_TYPE ni_handler::new_user(xn::UserGenerator& generator, XnUserID nId, void* pCookie) {
	XnUInt32 epochTime = 0;
	xnOSGetEpochTime(&epochTime);
	a2e_debug("[%d] New User: %d", epochTime, nId);
	// New user found
	if(g_bNeedPose) {
		user_generator.GetPoseDetectionCap().StartPoseDetection(g_strPose, nId);
	}
	else {
		user_generator.GetSkeletonCap().RequestCalibration(nId, TRUE);
	}
}

// Callback: An existing user was lost
void XN_CALLBACK_TYPE ni_handler::lost_user(xn::UserGenerator& generator, XnUserID nId, void* pCookie) {
	XnUInt32 epochTime = 0;
	xnOSGetEpochTime(&epochTime);
	a2e_debug("[%d] Lost user: %d", epochTime, nId);
	
	// player lost
	remove_player(nId);
}

// Callback: Detected a pose
void XN_CALLBACK_TYPE ni_handler::pose_detected(xn::PoseDetectionCapability& capability, const XnChar* strPose, XnUserID nId, void* pCookie) {
	XnUInt32 epochTime = 0;
	xnOSGetEpochTime(&epochTime);
	a2e_debug("[%d] Pose %s detected for user %d", epochTime, strPose, nId);
	user_generator.GetPoseDetectionCap().StopPoseDetection(nId);
	user_generator.GetSkeletonCap().RequestCalibration(nId, TRUE);
}

// Callback: Started calibration
void XN_CALLBACK_TYPE ni_handler::calibration_start(xn::SkeletonCapability& capability, XnUserID nId, void* pCookie) {
	XnUInt32 epochTime = 0;
	xnOSGetEpochTime(&epochTime);
	a2e_debug("[%d] Calibration started for user %d", epochTime, nId);
}

// Callback: Finished calibration
void XN_CALLBACK_TYPE ni_handler::calibration_complete(xn::SkeletonCapability& capability, XnUserID nId, XnCalibrationStatus eStatus, void* pCookie) {
	XnUInt32 epochTime = 0;
	xnOSGetEpochTime(&epochTime);
	if(eStatus == XN_CALIBRATION_STATUS_OK) {
		// Calibration succeeded
		a2e_debug("[%d] Calibration complete, start tracking user %d", epochTime, nId);
		user_generator.GetSkeletonCap().StartTracking(nId);
		
		// new player
		add_player(nId);
	}
	else {
		// Calibration failed
		a2e_error("[%d] Calibration failed for user %d", epochTime, nId);
		if(g_bNeedPose) {
			user_generator.GetPoseDetectionCap().StartPoseDetection(g_strPose, nId);
		}
		else {
			user_generator.GetSkeletonCap().RequestCalibration(nId, TRUE);
		}
	}
}

void ni_handler::add_player(const XnUserID& id) {
	if(players.count(id) > 0) {
		a2e_error("a player with such an id (#%u) already exists!", id);
		return;
	}
	
	player_data* pd = new player_data();
	pd->mr = new model_renderer(mb->get_mesh_border_fbo(), mp->get_mesh_push_fbo(), mb->get_mesh_border_cl_object(), mp->get_mesh_push_cl_object());
	players[id] = pd;
}

void ni_handler::remove_player(const XnUserID& id) {
	if(players.count(id) == 0) {
		a2e_error("no player with such an id (#%u) exists!", id);
		return;
	}
	
	delete players[id]->mr;
	delete players[id];
	players.erase(id);
}

bool ni_handler::is_tracking(const unsigned int& user_id) {
	return user_generator.GetSkeletonCap().IsTracking(user_id);
}

float4 ni_handler::get_joint_position(const unsigned int& user_id, const XnSkeletonJoint& joint) {
	if(joint == XN_SKEL_WAIST) {
		//return players[user_id]->waist;
		return get_smooth_joint_position(user_id, joint);
	}
	
	XnSkeletonJointPosition pos;
	user_generator.GetSkeletonCap().GetSkeletonJointPosition(user_id,
															 joint,
															 pos);
	
	if(pos.fConfidence < 0.5f) return float4(0.0f, 0.0f, 0.0f, -1.0f);
	return float4(pos.position.X * skeleton_scale, pos.position.Y * skeleton_scale, pos.position.Z * skeleton_scale, 1.0f);
}

float4 ni_handler::get_smooth_joint_position(const unsigned int& user_id, const XnSkeletonJoint& joint) {
	if(players.count(user_id) == 0) return float4(0.0f);
	player_data* pdata = players[user_id];
	float4 smoothed_pos = pdata->cell_avg[user_joint_map[joint]] / float(PLAYER_JOINT_POSITION_CELLS);
	smoothed_pos.w = 1.0f;
	return smoothed_pos;
} 

matrix4f ni_handler::get_joint_orientation(const unsigned int& user_id, const XnSkeletonJoint& joint) {
	return players[user_id]->matrices[user_joint_map[joint]];
}

void ni_handler::update_player_orientations(const XnUserID& user_id) {
	player_data* pdata = players[user_id];
	array<float4, PLAYER_JOINT_POSITION_COUNT> joint_positions;
	for(size_t i = 0; i < PLAYER_JOINT_POSITION_COUNT; i++) {
		joint_positions[i] = get_joint_position(user_id, user_joint_positions[i]);
	}
	
	float4 waist((joint_positions[user_joint_map[XN_SKEL_LEFT_HIP]] +
				  joint_positions[user_joint_map[XN_SKEL_RIGHT_HIP]]) * 0.5f);
	// waist is only valid if both hip points are valid
	if(joint_positions[user_joint_map[XN_SKEL_LEFT_HIP]].w < 1.0f ||
	   joint_positions[user_joint_map[XN_SKEL_RIGHT_HIP]].w < 1.0f) {
		waist.w = -1.0f;
	}
	else waist.w = 1.0f;
	joint_positions[user_joint_map[XN_SKEL_WAIST]] = waist;
	
	// filter
	for(size_t i = 0; i < PLAYER_JOINT_POSITION_COUNT; i++) {
		if(joint_positions[i].w < 1.0f) {
			// use last valid position and continue
			joint_positions[i] = pdata->cell_avg[i] / float(PLAYER_JOINT_POSITION_CELLS);
			continue;
		}
		
		// average position
		const size_t cur_idx = pdata->cell_indices[i];
		pdata->cell_indices[i] = (pdata->cell_indices[i] + 1) % PLAYER_JOINT_POSITION_CELLS;
		
		pdata->cell_avg[i] -= pdata->cells[i][cur_idx];
		pdata->cell_avg[i] += joint_positions[i];
		pdata->cells[i][cur_idx] = joint_positions[i];
		joint_positions[i] = pdata->cell_avg[i] / float(PLAYER_JOINT_POSITION_CELLS);
		joint_positions[i].w = 1.0f;
	}
	
	// if either hip position is invalid, don't go on
	if(joint_positions[user_joint_map[XN_SKEL_LEFT_HIP]].w < 1.0f ||
	   joint_positions[user_joint_map[XN_SKEL_RIGHT_HIP]].w < 1.0f) {
		return;
	}
	
	// waist
	{
		float4 vec_x = (joint_positions[user_joint_map[XN_SKEL_TORSO]] - joint_positions[user_joint_map[XN_SKEL_WAIST]]).normalize();
		float4 vec_y = (joint_positions[user_joint_map[XN_SKEL_RIGHT_HIP]] - joint_positions[user_joint_map[XN_SKEL_WAIST]]).normalize();
		float4 vec_z = (vec_x ^ vec_y).normalize();
		
		pdata->matrices[user_joint_map[XN_SKEL_WAIST]] = matrix4f(vec_x.x, vec_x.y, vec_x.z, 0.0f,
																  vec_y.x, vec_y.y, vec_y.z, 0.0f,
																  vec_z.x, vec_z.y, vec_z.z, 0.0f,
																  0.0f, 0.0f, 0.0f, 1.0f);
	}
	
	// left hip
	{
		float4 vec_x = (joint_positions[user_joint_map[XN_SKEL_LEFT_KNEE]] - joint_positions[user_joint_map[XN_SKEL_LEFT_HIP]]).normalize();
		
		float4 hp = joint_positions[user_joint_map[XN_SKEL_LEFT_HIP]];
		float4 waist_vec_z = pdata->matrices[user_joint_map[XN_SKEL_WAIST]].column<2>();
		hp -= waist_vec_z;// * 0.2f;
		hp += joint_positions[user_joint_map[XN_SKEL_LEFT_FOOT]];
		hp *= 0.5f;
		
		float4 vec_z = (joint_positions[user_joint_map[XN_SKEL_LEFT_KNEE]] - hp).normalize();
		float4 vec_y = (vec_z ^ vec_x).normalize();
		vec_z = (vec_x ^ vec_y).normalize();
		
		pdata->matrices[user_joint_map[XN_SKEL_LEFT_HIP]] = matrix4f(vec_x.x, vec_x.y, vec_x.z, 0.0f,
																	 vec_y.x, vec_y.y, vec_y.z, 0.0f,
																	 vec_z.x, vec_z.y, vec_z.z, 0.0f,
																	 0.0f, 0.0f, 0.0f, 1.0f);
	}
	
	// right hip
	{
		float4 vec_x = (joint_positions[user_joint_map[XN_SKEL_RIGHT_KNEE]] - joint_positions[user_joint_map[XN_SKEL_RIGHT_HIP]]).normalize();
		
		float4 hp = joint_positions[user_joint_map[XN_SKEL_RIGHT_HIP]];
		float4 waist_vec_z = pdata->matrices[user_joint_map[XN_SKEL_WAIST]].column<2>();
		hp -= waist_vec_z;// * 0.2f;
		hp += joint_positions[user_joint_map[XN_SKEL_RIGHT_FOOT]];
		hp *= 0.5f;
		
		float4 vec_z = (joint_positions[user_joint_map[XN_SKEL_RIGHT_KNEE]] - hp).normalize();
		float4 vec_y = (vec_z ^ vec_x).normalize();
		vec_z = (vec_x ^ vec_y).normalize();
		
		pdata->matrices[user_joint_map[XN_SKEL_RIGHT_HIP]] = matrix4f(vec_x.x, vec_x.y, vec_x.z, 0.0f,
																	  vec_y.x, vec_y.y, vec_y.z, 0.0f,
																	  vec_z.x, vec_z.y, vec_z.z, 0.0f,
																	  0.0f, 0.0f, 0.0f, 1.0f);
	}
	
	// left knee
	{
		float4 vec_x = (joint_positions[user_joint_map[XN_SKEL_LEFT_FOOT]] - joint_positions[user_joint_map[XN_SKEL_LEFT_KNEE]]).normalize();
		float4 vec_y = pdata->matrices[user_joint_map[XN_SKEL_LEFT_HIP]].column<1>();
		float4 vec_z = (vec_x ^ vec_y).normalize();
		
		pdata->matrices[user_joint_map[XN_SKEL_LEFT_KNEE]] = matrix4f(vec_x.x, vec_x.y, vec_x.z, 0.0f,
																	  vec_y.x, vec_y.y, vec_y.z, 0.0f,
																	  vec_z.x, vec_z.y, vec_z.z, 0.0f,
																	  0.0f, 0.0f, 0.0f, 1.0f);
	}
	
	// right knee
	{
		float4 vec_x = (joint_positions[user_joint_map[XN_SKEL_RIGHT_FOOT]] - joint_positions[user_joint_map[XN_SKEL_RIGHT_KNEE]]).normalize();
		float4 vec_y = pdata->matrices[user_joint_map[XN_SKEL_RIGHT_HIP]].column<1>();
		float4 vec_z = (vec_x ^ vec_y).normalize();
		
		pdata->matrices[user_joint_map[XN_SKEL_RIGHT_KNEE]] = matrix4f(vec_x.x, vec_x.y, vec_x.z, 0.0f,
																	   vec_y.x, vec_y.y, vec_y.z, 0.0f,
																	   vec_z.x, vec_z.y, vec_z.z, 0.0f,
																	   0.0f, 0.0f, 0.0f, 1.0f);
	}
	
	// torso
	{
		float4 vec_x = (joint_positions[user_joint_map[XN_SKEL_NECK]] - joint_positions[user_joint_map[XN_SKEL_TORSO]]).normalize();
		float4 vec_y = (joint_positions[user_joint_map[XN_SKEL_RIGHT_SHOULDER]] - joint_positions[user_joint_map[XN_SKEL_LEFT_SHOULDER]]).normalize();
		float4 vec_z = (vec_x ^ vec_y).normalize();
		
		pdata->matrices[user_joint_map[XN_SKEL_TORSO]] = matrix4f(vec_x.x, vec_x.y, vec_x.z, 0.0f,
																  vec_y.x, vec_y.y, vec_y.z, 0.0f,
																  vec_z.x, vec_z.y, vec_z.z, 0.0f,
																  0.0f, 0.0f, 0.0f, 1.0f);
	}
	
	// neck/head
	{
		float4 vec_x = (joint_positions[user_joint_map[XN_SKEL_HEAD]] - joint_positions[user_joint_map[XN_SKEL_NECK]]).normalize();
		float4 vec_y = pdata->matrices[user_joint_map[XN_SKEL_TORSO]].column<1>();
		float4 vec_z = (vec_x ^ vec_y).normalize();
		
		pdata->matrices[user_joint_map[XN_SKEL_NECK]] = matrix4f(vec_x.x, vec_x.y, vec_x.z, 0.0f,
																 vec_y.x, vec_y.y, vec_y.z, 0.0f,
																 vec_z.x, vec_z.y, vec_z.z, 0.0f,
																 0.0f, 0.0f, 0.0f, 1.0f);
		pdata->matrices[user_joint_map[XN_SKEL_HEAD]] = pdata->matrices[user_joint_map[XN_SKEL_NECK]];
	}
	
	// left shoulder
	{
		float4 vec_x = (joint_positions[user_joint_map[XN_SKEL_LEFT_ELBOW]] - joint_positions[user_joint_map[XN_SKEL_LEFT_SHOULDER]]).normalize();
		float4 vec_y = (pdata->matrices[user_joint_map[XN_SKEL_TORSO]].column<2>() ^ vec_x).normalize();
		float4 vec_z = (vec_x ^ vec_y).normalize();
		
		pdata->matrices[user_joint_map[XN_SKEL_LEFT_SHOULDER]] = matrix4f(vec_x.x, vec_x.y, vec_x.z, 0.0f,
																		  vec_y.x, vec_y.y, vec_y.z, 0.0f,
																		  vec_z.x, vec_z.y, vec_z.z, 0.0f,
																		  0.0f, 0.0f, 0.0f, 1.0f);
	}
	
	// right shoulder
	{
		float4 vec_x = (joint_positions[user_joint_map[XN_SKEL_RIGHT_ELBOW]] - joint_positions[user_joint_map[XN_SKEL_RIGHT_SHOULDER]]).normalize();
		float4 vec_y = (pdata->matrices[user_joint_map[XN_SKEL_TORSO]].column<2>() ^ vec_x).normalize();
		float4 vec_z = (vec_x ^ vec_y).normalize();
		
		pdata->matrices[user_joint_map[XN_SKEL_RIGHT_SHOULDER]] = matrix4f(vec_x.x, vec_x.y, vec_x.z, 0.0f,
																		  vec_y.x, vec_y.y, vec_y.z, 0.0f,
																		  vec_z.x, vec_z.y, vec_z.z, 0.0f,
																		  0.0f, 0.0f, 0.0f, 1.0f);
	}
	
	// left elbow
	{
		float4 vec_x = (joint_positions[user_joint_map[XN_SKEL_LEFT_HAND]] - joint_positions[user_joint_map[XN_SKEL_LEFT_ELBOW]]).normalize();
		
		float4 hp = joint_positions[user_joint_map[XN_SKEL_LEFT_SHOULDER]];
		float4 torso_vec_z = pdata->matrices[user_joint_map[XN_SKEL_TORSO]].column<2>();
		hp += torso_vec_z;// * 0.2f;
		hp += joint_positions[user_joint_map[XN_SKEL_LEFT_HAND]];
		hp *= 0.5f;
		
		float4 vec_z = (hp - joint_positions[user_joint_map[XN_SKEL_LEFT_ELBOW]]).normalize();
		float4 vec_y = (vec_z ^ vec_x).normalize();
		vec_z = (vec_x ^ vec_y).normalize();
		
		pdata->matrices[user_joint_map[XN_SKEL_LEFT_ELBOW]] = matrix4f(vec_x.x, vec_x.y, vec_x.z, 0.0f,
																	   vec_y.x, vec_y.y, vec_y.z, 0.0f,
																	   vec_z.x, vec_z.y, vec_z.z, 0.0f,
																	   0.0f, 0.0f, 0.0f, 1.0f);
	}
	
	// right elbow
	{
		float4 vec_x = (joint_positions[user_joint_map[XN_SKEL_RIGHT_HAND]] - joint_positions[user_joint_map[XN_SKEL_RIGHT_ELBOW]]).normalize();
		
		float4 hp = joint_positions[user_joint_map[XN_SKEL_RIGHT_SHOULDER]];
		float4 torso_vec_z = pdata->matrices[user_joint_map[XN_SKEL_TORSO]].column<2>();
		hp += torso_vec_z;// * 0.2f;
		hp += joint_positions[user_joint_map[XN_SKEL_RIGHT_HAND]];
		hp *= 0.5f;
		
		float4 vec_z = (hp - joint_positions[user_joint_map[XN_SKEL_RIGHT_ELBOW]]).normalize();
		float4 vec_y = (vec_z ^ vec_x).normalize();
		vec_z = (vec_x ^ vec_y).normalize();
		
		pdata->matrices[user_joint_map[XN_SKEL_RIGHT_ELBOW]] = matrix4f(vec_x.x, vec_x.y, vec_x.z, 0.0f,
																		vec_y.x, vec_y.y, vec_y.z, 0.0f,
																		vec_z.x, vec_z.y, vec_z.z, 0.0f,
																		0.0f, 0.0f, 0.0f, 1.0f);
	}
}

void ni_handler::reset_players() {
	// stop "generating" new users while we're modifying the player data
	user_generator.StopGenerating();
	
	//
	set<XnUserID> active_user_ids;
	for(const auto& player : players) {
		active_user_ids.emplace(player.first);
	}
	for(const auto& user_id : active_user_ids) {
		// reset player
		remove_player(user_id);
		add_player(user_id);
	}
	
	// make #PLAYER_JOINT_POSITION_CELLS run calls, so that the players openni data gets initialized correctly
	for(size_t i = 0; i < PLAYER_JOINT_POSITION_CELLS; i++) {
		run();
	}
	
	// start again
	user_generator.StartGenerating();
}
