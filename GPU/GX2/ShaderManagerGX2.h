// Copyright (c) 2017- PPSSPP Project.

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

#pragma once

#include <map>
#include <wiiu/gx2.h>
#include <wiiu/os/memory.h>

#include "base/basictypes.h"
#include "GPU/Common/ShaderCommon.h"
#include "GPU/Common/ShaderId.h"
#include "GPU/Common/ShaderUniforms.h"

#include "GPU/GX2/GX2StaticShaders.h"

class GX2PShader : public GX2PixelShader {
public:
	// TODO: fixme
	GX2PShader(FShaderID id, bool useHWTransform) : GX2PixelShader(defPShaderGX2), id_(id), useHWTransform_(useHWTransform) {}
	~GX2PShader() {
		if (program && program != defPShaderGX2.program) {
			MEM2_free(program);
		}
	}

	const std::string source() const { return "N/A"; }
	bool UseHWTransform() const { return useHWTransform_; }
	std::string GetShaderString(DebugShaderStringType type) const;

protected:
	bool useHWTransform_;
	FShaderID id_;
};

class GX2VShader : public GX2VertexShader {
public:
	// TODO: fixme
	GX2VShader(VShaderID id, bool useHWTransform) : GX2VertexShader(defVShaderGX2), id_(id), failed_(false), useHWTransform_(useHWTransform) {}
	~GX2VShader() {
		if (program && program != defVShaderGX2.program) {
			MEM2_free(program);
		}
	}

	const std::string source() const { return "N/A"; }
	const u8 *bytecode() const { return program; }
	bool UseHWTransform() const { return useHWTransform_; }
	std::string GetShaderString(DebugShaderStringType type) const;

protected:
	bool failed_;
	bool useHWTransform_;
	VShaderID id_;
};

class ShaderManagerGX2 : public ShaderManagerCommon {
public:
	ShaderManagerGX2(GX2ContextState *context);
	~ShaderManagerGX2();

	void GetShaders(int prim, u32 vertType, GX2VShader **vshader, GX2PShader **fshader, bool useHWTransform);
	void ClearShaders();
	void DirtyLastShader() override;

	int GetNumVertexShaders() const { return (int)vsCache_.size(); }
	int GetNumFragmentShaders() const { return (int)fsCache_.size(); }

	std::vector<std::string> DebugGetShaderIDs(DebugShaderType type);
	std::string DebugGetShaderString(std::string id, DebugShaderType type, DebugShaderStringType stringType);

	uint64_t UpdateUniforms();
	void BindUniforms();

	// TODO: Avoid copying these buffers if same as last draw, can still point to it assuming we're still in the same pushbuffer.
	// Applies dirty changes and copies the buffer.
	bool IsBaseDirty() { return true; }
	bool IsLightDirty() { return true; }
	bool IsBoneDirty() { return true; }

private:
	void Clear();

	typedef std::map<FShaderID, GX2PShader *> FSCache;
	FSCache fsCache_;

	typedef std::map<VShaderID, GX2VShader *> VSCache;
	VSCache vsCache_;

	// Uniform block scratchpad. These (the relevant ones) are copied to the current pushbuffer at draw time.
	UB_VS_FS_Base ub_base;
	UB_VS_Lights ub_lights;
	UB_VS_Bones ub_bones;

	// Not actual pushbuffers
	void *push_base;
	void *push_lights;
	void *push_bones;

	GX2PShader *lastFShader_;
	GX2VShader *lastVShader_;

	FShaderID lastFSID_;
	VShaderID lastVSID_;
};
