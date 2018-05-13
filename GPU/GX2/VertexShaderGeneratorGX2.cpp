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

#include <wiiu/gx2/shaders_asm.h>
#undef ARRAY_SIZE

#include "GPU/Common/ShaderCommon.h"
#include "GPU/GX2/VertexShaderGeneratorGX2.h"
#include "GPU/Vulkan/VertexShaderGeneratorVulkan.h"
#include "GPU/GX2/GX2StaticShaders.h"
#include "GPU/ge_constants.h"

#include <wiiu/os/debug.h>

void GenerateVertexShaderGX2(const VShaderID &id, GX2VertexShader *vs) {
	// TODO;
	*vs = STVshaderGX2;

	if (id.Bit(VS_BIT_IS_THROUGH) && id.Bit(VS_BIT_HAS_COLOR)) {
		*vs = clearVShaderGX2;
	} else if (id.Bit(VS_BIT_HAS_COLOR) && id.Bit(VS_BIT_DO_TEXTURE)) {
		*vs = cTexVShaderGX2;
	}

#if 1
	DEBUG_STR(VertexShaderDesc(id).c_str());
	char glslcode[16384];
	GenerateVulkanGLSLVertexShader(id, glslcode);
	puts(glslcode);
#endif
}
