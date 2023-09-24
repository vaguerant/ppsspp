
#define GX2_COMP_SEL
#include "WiiU/GX2GraphicsContext.h"
#include "WiiU/GX2ScreenShader.h"
#include "thin3d/thin3d.h"
#include "thin3d/thin3d_create.h"
#include "Core/System.h"
#include "base/NativeApp.h"
#include "input/input_state.h"

#include <wiiu/gx2.h>
#include <wiiu/vpad.h>
#include <wiiu/os/memory.h>
#include <wiiu/os/debug.h>

static bool swap_is_pending(void *start_time) {
	uint32_t swap_count, flip_count;
	OSTime last_flip, last_vsync;

	GX2GetSwapStatus(&swap_count, &flip_count, &last_flip, &last_vsync);

	return last_vsync < *(OSTime *)start_time;
}

bool GX2GraphicsContext::Init() {
	static const RenderMode render_mode_map[] = {
		{ 0 },                                         /* GX2_TV_SCAN_MODE_NONE  */
		{ 854, 480, GX2_TV_RENDER_MODE_WIDE_480P },    /* GX2_TV_SCAN_MODE_576I  */
		{ 854, 480, GX2_TV_RENDER_MODE_WIDE_480P },    /* GX2_TV_SCAN_MODE_480I  */
		{ 854, 480, GX2_TV_RENDER_MODE_WIDE_480P },    /* GX2_TV_SCAN_MODE_480P  */
		{ 1280, 720, GX2_TV_RENDER_MODE_WIDE_720P },   /* GX2_TV_SCAN_MODE_720P  */
		{ 0 },                                         /* GX2_TV_SCAN_MODE_unk   */
		{ 1920, 1080, GX2_TV_RENDER_MODE_WIDE_1080P }, /* GX2_TV_SCAN_MODE_1080I */
		{ 1920, 1080, GX2_TV_RENDER_MODE_WIDE_1080P }  /* GX2_TV_SCAN_MODE_1080P */
	};
	render_mode_ = render_mode_map[GX2GetSystemTVScanMode()];
//	render_mode_ = render_mode_map[GX2_TV_SCAN_MODE_480P];

	cmd_buffer_ = MEM2_alloc(0x400000, 0x40);
	u32 init_attributes[] = { GX2_INIT_CMD_BUF_BASE, (u32)cmd_buffer_, GX2_INIT_CMD_BUF_POOL_SIZE, 0x400000, GX2_INIT_ARGC, 0, GX2_INIT_ARGV, 0, GX2_INIT_END };
	GX2Init(init_attributes);
	u32 size = 0;
	u32 tmp = 0;
	GX2CalcTVSize(render_mode_.mode, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_BUFFERING_MODE_DOUBLE, &size, &tmp);

	tv_scan_buffer_ = MEMBucket_alloc(size, GX2_SCAN_BUFFER_ALIGNMENT);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, tv_scan_buffer_, size);
	GX2SetTVBuffer(tv_scan_buffer_, size, render_mode_.mode, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_BUFFERING_MODE_DOUBLE);

	GX2CalcDRCSize(GX2_DRC_RENDER_MODE_SINGLE, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_BUFFERING_MODE_DOUBLE, &size, &tmp);

	drc_scan_buffer_ = MEMBucket_alloc(size, GX2_SCAN_BUFFER_ALIGNMENT);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, drc_scan_buffer_, size);
	GX2SetDRCBuffer(drc_scan_buffer_, size, GX2_DRC_RENDER_MODE_SINGLE, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8, GX2_BUFFERING_MODE_DOUBLE);

	color_buffer_.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
	color_buffer_.surface.width = render_mode_.width;
	color_buffer_.surface.height = render_mode_.height;
	color_buffer_.surface.depth = 1;
	color_buffer_.surface.mipLevels = 1;
	color_buffer_.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
	color_buffer_.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
	color_buffer_.viewNumSlices = 1;

	GX2CalcSurfaceSizeAndAlignment(&color_buffer_.surface);
	GX2InitColorBufferRegs(&color_buffer_);

	color_buffer_.surface.image = MEM1_alloc(color_buffer_.surface.imageSize, color_buffer_.surface.alignment);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, color_buffer_.surface.image, color_buffer_.surface.imageSize);

	// Copy color buffer to a texture
	tv_texture_.surface = color_buffer_.surface;
	tv_texture_.surface.use = GX2_SURFACE_USE_TEXTURE;
	tv_texture_.viewFirstMip = 0;
	tv_texture_.viewNumMips = 1;
	tv_texture_.viewFirstSlice = 0;
	tv_texture_.viewNumSlices = 1;
	tv_texture_.compMap = 0x00010203;
	GX2InitTextureRegs(&tv_texture_);
	GX2InitSampler(&tv_sampler_, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);

	drc_color_buffer_.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
	drc_color_buffer_.surface.width = 854;
	drc_color_buffer_.surface.height = 480;
	drc_color_buffer_.surface.depth = 1;
	drc_color_buffer_.surface.mipLevels = 1;
	drc_color_buffer_.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
	drc_color_buffer_.surface.use = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
	drc_color_buffer_.viewNumSlices = 1;

	GX2CalcSurfaceSizeAndAlignment(&drc_color_buffer_.surface);
	GX2InitColorBufferRegs(&drc_color_buffer_);

	drc_color_buffer_.surface.image = MEM1_alloc(drc_color_buffer_.surface.imageSize, drc_color_buffer_.surface.alignment);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, drc_color_buffer_.surface.image, drc_color_buffer_.surface.imageSize);

	depth_buffer_.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
	depth_buffer_.surface.width = render_mode_.width;
	depth_buffer_.surface.height = render_mode_.height;
	depth_buffer_.surface.depth = 1;
	depth_buffer_.surface.mipLevels = 1;
	depth_buffer_.surface.format = GX2_SURFACE_FORMAT_FLOAT_D24_S8;
	depth_buffer_.surface.use = GX2_SURFACE_USE_DEPTH_BUFFER;
	depth_buffer_.viewNumSlices = 1;

	GX2CalcSurfaceSizeAndAlignment(&depth_buffer_.surface);
	GX2InitDepthBufferRegs(&depth_buffer_);

	depth_buffer_.surface.image = MEM1_alloc(depth_buffer_.surface.imageSize, depth_buffer_.surface.alignment);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU, depth_buffer_.surface.image, depth_buffer_.surface.imageSize);

	ctx_state_ = (GX2ContextState *)MEM2_alloc(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT);
	GX2SetupContextStateEx(ctx_state_, GX2_TRUE);
	drc_ctx_state_ = (GX2ContextState *)MEM2_alloc(sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT);
	GX2SetupContextStateEx(drc_ctx_state_, GX2_TRUE);

	GX2SetContextState(ctx_state_);
	GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
	GX2SetColorBuffer(&color_buffer_, GX2_RENDER_TARGET_0);
	GX2SetDepthBuffer(&depth_buffer_);
	GX2SetViewport(0.0f, 0.0f, color_buffer_.surface.width, color_buffer_.surface.height, 0.0f, 1.0f);
	GX2SetScissor(0, 0, color_buffer_.surface.width, color_buffer_.surface.height);
	GX2SetDepthOnlyControl(GX2_DISABLE, GX2_DISABLE, GX2_COMPARE_FUNC_ALWAYS);
	GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, GX2_DISABLE, GX2_ENABLE);
	GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD, GX2_ENABLE, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD);
	GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, GX2_DISABLE, GX2_DISABLE);

	GX2ClearColor(&color_buffer_, 0.0f, 0.0f, 0.0f, 1.0f);
//	SwapBuffers();

	GX2SetTVEnable(GX2_ENABLE);
	GX2SetDRCEnable(GX2_ENABLE);

	draw_ = Draw::T3DCreateGX2Context(ctx_state_, &color_buffer_, &depth_buffer_);
	SetGPUBackend(GPUBackend::GX2);
	GX2SetSwapInterval(0);

	uint32_t fetch_shader_buf_size = GX2CalcFetchShaderSizeEx(
		0,
		GX2_FETCH_SHADER_TESSELLATION_NONE,	// No Tessellation
		GX2_TESSELLATION_MODE_DISCRETE		// ^^^^^^^^^^^^^^^
	);
	fetch_shader_buf_ = (uint8_t*)MEM2_alloc(
		fetch_shader_buf_size,
		0x100								// GX2_SHADER_PROGRAM_ALIGNMENT
	);

	GX2FetchShader* fetch_shader = &fetch_shader_;

	GX2InitFetchShaderEx(
		fetch_shader,
		fetch_shader_buf_,
		0,
		0,
		GX2_FETCH_SHADER_TESSELLATION_NONE,	// No Tessellation
		GX2_TESSELLATION_MODE_DISCRETE		// ^^^^^^^^^^^^^^^
	);

	// Make sure to flush CPU cache and invalidate GPU cache
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, fetch_shader->program, fetch_shader->size);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, screen_shader_VSH.program, screen_shader_VSH.size);
	GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, screen_shader_PSH.program, screen_shader_PSH.size);
	return draw_->CreatePresets();

	if (screen_shader_VSH.program == nullptr || uintptr_t(screen_shader_VSH.program) % 0x100 != 0)
	{
		OSReport("Run around in circles (vertex)\n");
		while (true) {}
	}

	if (screen_shader_PSH.program == nullptr || uintptr_t(screen_shader_PSH.program) % 0x100 != 0)
	{
		OSReport("Run around in circles (pixel)\n");
		while (true) {}
	}
}

void GX2GraphicsContext::Shutdown() {
	if (!draw_)
		return;
	delete draw_;
	draw_ = nullptr;
	GX2ClearColor(&color_buffer_, 0.0f, 0.0f, 0.0f, 1.0f);
	SwapBuffers();
	GX2DrawDone();
	GX2Shutdown();

	GX2SetTVEnable(GX2_DISABLE);
	GX2SetDRCEnable(GX2_DISABLE);

	MEM2_free(ctx_state_);
	ctx_state_ = nullptr;
	MEM2_free(drc_ctx_state_);
	drc_ctx_state_ = nullptr;
	MEM2_free(cmd_buffer_);
	cmd_buffer_ = nullptr;
	MEM1_free(color_buffer_.surface.image);
	color_buffer_ = {};
	MEMBucket_free(tv_scan_buffer_);
	tv_scan_buffer_ = nullptr;
	MEMBucket_free(drc_scan_buffer_);
	drc_scan_buffer_ = nullptr;
	MEM2_free(fetch_shader_buf_);
	fetch_shader_buf_ = nullptr;
}
#include "profiler/profiler.h"
void GX2GraphicsContext::SwapBuffers() {
	PROFILE_THIS_SCOPE("swap");

	GX2Invalidate(GX2_INVALIDATE_MODE_COLOR_BUFFER, color_buffer_.surface.image, color_buffer_.surface.imageSize);
	GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, tv_texture_.surface.image, tv_texture_.surface.imageSize);

	GX2SetContextState(drc_ctx_state_);

	GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);
	GX2SetVertexShader(&screen_shader_VSH);
	GX2SetPixelShader(&screen_shader_PSH);
	GX2SetFetchShader(&fetch_shader_);

	GX2SetPixelSampler(&tv_sampler_, 0);
	GX2SetPixelTexture(&tv_texture_, 0);

	// Set DRC color buffer, viewport and scissor
	GX2SetColorBuffer(&drc_color_buffer_, GX2_RENDER_TARGET_0);
	GX2SetViewport(0, 0, 854, 480, 0, 1);
	GX2SetScissor(0, 0, 854, 480);

	GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, 6, 0, 1);

	// Restore TV color buffer, viewport and scissor
	GX2SetColorBuffer(&color_buffer_, GX2_RENDER_TARGET_0);
	GX2SetViewport(0, 0, color_buffer_.surface.width, color_buffer_.surface.height, 0, 1);
	GX2SetScissor(0, 0, color_buffer_.surface.width, color_buffer_.surface.height);

	GX2DrawDone();
	GX2CopyColorBufferToScanBuffer(&color_buffer_, GX2_SCAN_TARGET_TV);
	GX2CopyColorBufferToScanBuffer(&drc_color_buffer_, GX2_SCAN_TARGET_DRC);
	GX2SwapScanBuffers();
	GX2Flush();
//	GX2WaitForVsync();
	GX2WaitForFlip();
	GX2SetContextState(ctx_state_);
	GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
}
