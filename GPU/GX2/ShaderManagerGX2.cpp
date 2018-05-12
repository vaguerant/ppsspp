// Copyright (c) 2015- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#include "ppsspp_config.h"

#include <map>
#include <wiiu/gx2/shaders.h>

#include "base/logging.h"
#include "math/lin/matrix4x4.h"
#include "math/math_util.h"
#include "math/dataconv.h"
#include "util/text/utf8.h"
#include "Common/Common.h"
#include "Core/Config.h"
#include "Core/Reporting.h"
#include "GPU/Math3D.h"
#include "GPU/GPUState.h"
#include "GPU/ge_constants.h"
#include "GPU/GX2/ShaderManagerGX2.h"
#include "GPU/GX2/FragmentShaderGeneratorGX2.h"
#include "GPU/GX2/VertexShaderGeneratorGX2.h"
#include "GPU/GX2/GX2Util.h"


GX2PShader::GX2PShader(FShaderID id) : GX2PixelShader(), id_(id) {
	GenerateFragmentShaderGX2(id, this);
}

std::string GX2PShader::GetShaderString(DebugShaderStringType type) const {
	switch (type) {
	case SHADER_STRING_SHORT_DESC:
		return FragmentShaderDesc(id_);
	case SHADER_STRING_SOURCE_CODE:
	default:
		return "N/A";
	}
}

GX2VShader::GX2VShader(VShaderID id) : GX2VertexShader(), id_(id) {
	GenerateVertexShaderGX2(id, this);
}

std::string GX2VShader::GetShaderString(DebugShaderStringType type) const {
	switch (type) {
	case SHADER_STRING_SHORT_DESC:
		return VertexShaderDesc(id_);
	case SHADER_STRING_SOURCE_CODE:
	default:
		return "N/A";
	}
}

ShaderManagerGX2::ShaderManagerGX2(GX2ContextState *context)
	: lastVShader_(nullptr), lastFShader_(nullptr) {
	memset(&ub_base, 0, sizeof(ub_base));
	memset(&ub_lights, 0, sizeof(ub_lights));
	memset(&ub_bones, 0, sizeof(ub_bones));

	INFO_LOG(G3D, "sizeof(ub_base): %d", (int)sizeof(ub_base));
	INFO_LOG(G3D, "sizeof(ub_lights): %d", (int)sizeof(ub_lights));
	INFO_LOG(G3D, "sizeof(ub_bones): %d", (int)sizeof(ub_bones));

	push_base = MEM2_alloc(sizeof(ub_base), GX2_UNIFORM_BLOCK_ALIGNMENT);
	push_lights = MEM2_alloc(sizeof(ub_lights), GX2_UNIFORM_BLOCK_ALIGNMENT);
	push_bones = MEM2_alloc(sizeof(ub_bones), GX2_UNIFORM_BLOCK_ALIGNMENT);
}

ShaderManagerGX2::~ShaderManagerGX2() {
	MEM2_free(push_base);
	MEM2_free(push_lights);
	MEM2_free(push_bones);
	ClearShaders();
}

void ShaderManagerGX2::Clear() {
	for (auto iter = fsCache_.begin(); iter != fsCache_.end(); ++iter) {
		delete iter->second;
	}
	for (auto iter = vsCache_.begin(); iter != vsCache_.end(); ++iter) {
		delete iter->second;
	}
	fsCache_.clear();
	vsCache_.clear();
	lastFSID_.set_invalid();
	lastVSID_.set_invalid();
	gstate_c.Dirty(DIRTY_VERTEXSHADER_STATE | DIRTY_FRAGMENTSHADER_STATE);
}

void ShaderManagerGX2::ClearShaders() {
	Clear();
	DirtyLastShader();
	gstate_c.Dirty(DIRTY_ALL_UNIFORMS);
}

void ShaderManagerGX2::DirtyLastShader() {
	lastFSID_.set_invalid();
	lastVSID_.set_invalid();
	lastVShader_ = nullptr;
	lastFShader_ = nullptr;
	gstate_c.Dirty(DIRTY_VERTEXSHADER_STATE | DIRTY_FRAGMENTSHADER_STATE);
}

uint64_t ShaderManagerGX2::UpdateUniforms() {
	uint64_t dirty = gstate_c.GetDirtyUniforms();
	if (dirty != 0) {
		if (dirty & DIRTY_BASE_UNIFORMS) {
			BaseUpdateUniforms(&ub_base, dirty, true);
			for(int i = 0; i < sizeof(ub_base) / 4; i++)
				((u32_le*)push_base)[i] = ((u32*)&ub_base)[i];
			GX2Invalidate(GX2_INVALIDATE_MODE_CPU_UNIFORM_BLOCK, push_base, sizeof(ub_base));
		}
		if (dirty & DIRTY_LIGHT_UNIFORMS) {
			LightUpdateUniforms(&ub_lights, dirty);
			for(int i = 0; i < sizeof(ub_lights) / 4; i++)
				((u32_le*)push_lights)[i] = ((u32*)&ub_lights)[i];
			GX2Invalidate(GX2_INVALIDATE_MODE_CPU_UNIFORM_BLOCK, push_lights, sizeof(ub_lights));
		}
		if (dirty & DIRTY_BONE_UNIFORMS) {
			BoneUpdateUniforms(&ub_bones, dirty);
			for(int i = 0; i < sizeof(ub_bones) / 4; i++)
				((u32_le*)push_bones)[i] = ((u32*)&ub_bones)[i];
			GX2Invalidate(GX2_INVALIDATE_MODE_CPU_UNIFORM_BLOCK, push_bones, sizeof(ub_bones));
		}
	}
	gstate_c.CleanUniforms();
	return dirty;
}

void ShaderManagerGX2::BindUniforms() {
	GX2SetVertexUniformBlock(1, sizeof(ub_base), push_base);
	GX2SetVertexUniformBlock(2, sizeof(ub_lights), push_lights);
	GX2SetVertexUniformBlock(3, sizeof(ub_bones), push_bones);
	GX2SetPixelUniformBlock(1, sizeof(ub_base), push_base);
}

void ShaderManagerGX2::GetShaders(int prim, u32 vertType, GX2VShader **vshader, GX2PShader **fshader, bool useHWTransform) {
	VShaderID VSID;
	FShaderID FSID;

	if (gstate_c.IsDirty(DIRTY_VERTEXSHADER_STATE)) {
		gstate_c.Clean(DIRTY_VERTEXSHADER_STATE);
		ComputeVertexShaderID(&VSID, vertType, useHWTransform);
	} else {
		VSID = lastVSID_;
	}

	if (gstate_c.IsDirty(DIRTY_FRAGMENTSHADER_STATE)) {
		gstate_c.Clean(DIRTY_FRAGMENTSHADER_STATE);
		ComputeFragmentShaderID(&FSID);
	} else {
		FSID = lastFSID_;
	}

	// Just update uniforms if this is the same shader as last time.
	if (lastVShader_ != nullptr && lastFShader_ != nullptr && VSID == lastVSID_ && FSID == lastFSID_) {
		*vshader = lastVShader_;
		*fshader = lastFShader_;
		// Already all set, no need to look up in shader maps.
		return;
	}

	VSCache::iterator vsIter = vsCache_.find(VSID);
	GX2VShader *vs;
	if (vsIter == vsCache_.end()) {
		// Vertex shader not in cache. Let's generate it.
		vs = new GX2VShader(VSID);
		vsCache_[VSID] = vs;
	} else {
		vs = vsIter->second;
	}
	lastVSID_ = VSID;

	FSCache::iterator fsIter = fsCache_.find(FSID);
	GX2PShader *fs;
	if (fsIter == fsCache_.end()) {
		// Fragment shader not in cache. Let's generate it.
		fs = new GX2PShader(FSID);
		fsCache_[FSID] = fs;
	} else {
		fs = fsIter->second;
	}

	lastFSID_ = FSID;

	lastVShader_ = vs;
	lastFShader_ = fs;

	*vshader = vs;
	*fshader = fs;
}

std::vector<std::string> ShaderManagerGX2::DebugGetShaderIDs(DebugShaderType type) {
	std::string id;
	std::vector<std::string> ids;
	switch (type) {
	case SHADER_TYPE_VERTEX:
	{
		for (auto iter : vsCache_) {
			iter.first.ToString(&id);
			ids.push_back(id);
		}
		break;
	}
	case SHADER_TYPE_FRAGMENT:
	{
		for (auto iter : fsCache_) {
			iter.first.ToString(&id);
			ids.push_back(id);
		}
		break;
	}
	default:
		break;
	}
	return ids;
}

std::string ShaderManagerGX2::DebugGetShaderString(std::string id, DebugShaderType type, DebugShaderStringType stringType) {
	ShaderID shaderId;
	shaderId.FromString(id);
	switch (type) {
	case SHADER_TYPE_VERTEX:
	{
		auto iter = vsCache_.find(VShaderID(shaderId));
		if (iter == vsCache_.end()) {
			return "";
		}
		return iter->second->GetShaderString(stringType);
	}

	case SHADER_TYPE_FRAGMENT:
	{
		auto iter = fsCache_.find(FShaderID(shaderId));
		if (iter == fsCache_.end()) {
			return "";
		}
		return iter->second->GetShaderString(stringType);
	}
	default:
		return "N/A";
	}
}
