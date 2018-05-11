
#include "GPU/GX2/GX2StaticShaders.h"
#include <wiiu/gx2/common.h>
#include <wiiu/gx2/shaders_asm.h>

// clang-format off
__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static u64 depalVCode [32] =
{
   CALL_FS NO_BARRIER,
   EXP_DONE(POS0,   _R1, _x, _y, _z, _1),
   EXP_DONE(PARAM0, _R2, _x, _y, _0, _0) NO_BARRIER
   END_OF_PROGRAM
};
// clang-format on

GX2VertexShader defVShaderGX2 = {
	{
		.sq_pgm_resources_vs.num_gprs = 3,
		.sq_pgm_resources_vs.stack_size = 1,
		.spi_vs_out_config.vs_export_count = 1,
		.num_spi_vs_out_id = 1,
		{
			{ .semantic_0 = 0x00, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF },
			{ .semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF },
			{ .semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF },
			{ .semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF },
			{ .semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF },
			{ .semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF },
			{ .semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF },
			{ .semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF },
			{ .semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF },
			{ .semantic_0 = 0xFF, .semantic_1 = 0xFF, .semantic_2 = 0xFF, .semantic_3 = 0xFF },
		},
		.sq_vtx_semantic_clear = ~0x3,
		.num_sq_vtx_semantic = 2,
		{ 0, 1, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
		.vgt_vertex_reuse_block_cntl.vtx_reuse_depth = 0xE,
		.vgt_hos_reuse_depth.reuse_depth = 0x10,
	}, /* regs */
	sizeof(depalVCode),
	(uint8_t *)&depalVCode,
	GX2_SHADER_MODE_UNIFORM_BLOCK,
};

// clang-format off
__attribute__((aligned(GX2_SHADER_ALIGNMENT))) static struct {
	u64 cf[32];
	u64 tex[1 * 2];
} quadPCode = {
	{
		TEX(32, 1) VALID_PIX,
		EXP_DONE(PIX0, _R0, _x, _y, _z, _w)
		END_OF_PROGRAM
	},
	{
		TEX_SAMPLE(_R0, _x, _y, _z, _w, _R0, _x, _y, _0, _0, _t0, _s0)
	}
};
// clang-format on

GX2PixelShader defPShaderGX2 = {
	{
		.sq_pgm_resources_ps.num_gprs = 2,
		.sq_pgm_exports_ps.export_mode = 0x2,
		.spi_ps_in_control_0.num_interp = 2,
		.spi_ps_in_control_0.persp_gradient_ena = 1,
		.spi_ps_in_control_0.baryc_sample_cntl = spi_baryc_cntl_centers_only,
		.num_spi_ps_input_cntl = 2,
		{ { .semantic = 0, .default_val = 1 }, { .semantic = 1, .default_val = 1 } },
		.cb_shader_mask.output0_enable = 0xF,
		.cb_shader_control.rt0_enable = TRUE,
		.db_shader_control.z_order = db_z_order_early_z_then_late_z,
	}, /* regs */
	sizeof(quadPCode),
	(uint8_t *)&quadPCode,
	GX2_SHADER_MODE_UNIFORM_BLOCK,
};

// TODO
// clang-format off
__attribute__((aligned(GX2_SHADER_ALIGNMENT))) static struct {
	u64 cf[32];
	u64 tex[1 * 2];
} stencilPCode = {
	{
		TEX(32, 1) VALID_PIX,
		EXP_DONE(PIX0, _R0, _x, _y, _z, _w)
		END_OF_PROGRAM
	},
	{
		TEX_SAMPLE(_R0, _x, _y, _z, _w, _R0, _x, _y, _0, _0, _t0, _s0)
	}
};
// clang-format on

GX2PixelShader stencilUploadPSshaderGX2 = {
	{
		.sq_pgm_resources_ps.num_gprs = 2,
		.sq_pgm_exports_ps.export_mode = 0x2,
		.spi_ps_in_control_0.num_interp = 2,
		.spi_ps_in_control_0.persp_gradient_ena = 1,
		.spi_ps_in_control_0.baryc_sample_cntl = spi_baryc_cntl_centers_only,
		.num_spi_ps_input_cntl = 2,
		{ { .semantic = 0, .default_val = 1 }, { .semantic = 1, .default_val = 1 } },
		.cb_shader_mask.output0_enable = 0xF,
		.cb_shader_control.rt0_enable = TRUE,
		.db_shader_control.z_order = db_z_order_early_z_then_late_z,
	}, /* regs */
	sizeof(stencilPCode),
	(uint8_t *)&stencilPCode,
	GX2_SHADER_MODE_UNIFORM_BLOCK,
};
