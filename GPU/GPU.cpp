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

#include "Common/GraphicsContext.h"
#include "Core/Core.h"

#include "GPU/GPU.h"
#include "GPU/GPUInterface.h"

#include "GPU/Null/NullGpu.h"
#include "GPU/Software/SoftGpu.h"

#if PPSSPP_PLATFORM(UWP)
#include "GPU/D3D11/GPU_D3D11.h"
#elif PPSSPP_PLATFORM(WIIU)
#include "GPU/GX2/GPU_GX2.h"
#else
#include "GPU/GLES/GPU_GLES.h"

#ifndef NO_VULKAN
#include "GPU/Vulkan/GPU_Vulkan.h"
#endif

#if defined(_WIN32)
#include "GPU/Directx9/GPU_DX9.h"
#include "GPU/D3D11/GPU_D3D11.h"
#endif

#endif

GPUStatistics gpuStats;
GPUInterface *gpu;
GPUDebugInterface *gpuDebug;

template <typename T>
static void SetGPU(T *obj) {
	gpu = obj;
	gpuDebug = obj;
}

#ifdef USE_CRT_DBG
#undef new
#endif

bool GPU_IsReady() {
	if (gpu)
		return gpu->IsReady();
	return false;
}

bool GPU_Init(GraphicsContext *ctx, Draw::DrawContext *draw) {
#if PPSSPP_PLATFORM(UWP)
	SetGPU(new GPU_D3D11(ctx, draw));
	return true;
#else
	switch (PSP_CoreParameter().gpuCore) {
	case GPUCORE_NULL:
		SetGPU(new NullGPU());
		break;
	case GPUCORE_GLES:
#if !PPSSPP_PLATFORM(WIIU)
		SetGPU(new GPU_GLES(ctx, draw));
		break;
#else
		return false;
#endif
	case GPUCORE_SOFTWARE:
		SetGPU(new SoftGPU(ctx, draw));
		break;
	case GPUCORE_DIRECTX9:
#if defined(_WIN32)
		SetGPU(new DIRECTX9_GPU(ctx, draw));
		break;
#else
		return false;
#endif
	case GPUCORE_DIRECTX11:
#if defined(_WIN32)
		SetGPU(new GPU_D3D11(ctx, draw));
		break;
#else
		return false;
#endif
	case GPUCORE_VULKAN:
#ifndef NO_VULKAN
		if (!ctx) {
			ERROR_LOG(G3D, "Unable to init Vulkan GPU backend, no context");
			break;
		}
		SetGPU(new GPU_Vulkan(ctx, draw));
#endif
		break;
	case GPUCORE_GX2:
#if PPSSPP_PLATFORM(WIIU)
		SetGPU(new GPU_GX2(ctx, draw));
		break;
#else
		return false;
#endif
	}

	return gpu != NULL;
#endif
}
#ifdef USE_CRT_DBG
#define new DBG_NEW
#endif

void GPU_Shutdown() {
	delete gpu;
	gpu = nullptr;
	gpuDebug = 0;
}
