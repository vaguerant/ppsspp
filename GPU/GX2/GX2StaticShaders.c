
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
	.gx2rBuffer.flags = GX2R_RESOURCE_LOCKED_READ_ONLY,
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
	.gx2rBuffer.flags = GX2R_RESOURCE_LOCKED_READ_ONLY,
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
	.gx2rBuffer.flags = GX2R_RESOURCE_LOCKED_READ_ONLY,
};

// clang-format off
__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct {
	u64 cf[32];
	u64 alu[16];
} vsSTCode = {
	{
		CALL_FS NO_BARRIER,
		ALU(32, 16) KCACHE0(CB1, _0_15),
		EXP_DONE(POS0, _R1, _x, _y, _z, _w),
		EXP(PARAM0, _R2, _x, _y, _0, _0) NO_BARRIER,
		EXP_DONE(PARAM1, _R3, _r, _g, _b, _a) NO_BARRIER
		END_OF_PROGRAM
	},
	{
		ALU_MUL(__, _x, _R1, _w, KC0(3), _y),
		ALU_MUL(__, _y, _R1, _w, KC0(3), _x),
		ALU_MUL(__, _z, _R1, _w, KC0(3), _w),
		ALU_MUL(__, _w, _R1, _w, KC0(3), _z)
		ALU_LAST,
		ALU_MULADD(_R123, _x, _R1, _z, KC0(2), _y, ALU_SRC_PV, _x),
		ALU_MULADD(_R123, _y, _R1, _z, KC0(2), _x, ALU_SRC_PV, _y),
		ALU_MULADD(_R123, _z, _R1, _z, KC0(2), _w, ALU_SRC_PV, _z),
		ALU_MULADD(_R123, _w, _R1, _z, KC0(2), _z, ALU_SRC_PV, _w)
		ALU_LAST,
		ALU_MULADD(_R123, _x, _R1, _y, KC0(1), _y, ALU_SRC_PV, _x),
		ALU_MULADD(_R123, _y, _R1, _y, KC0(1), _x, ALU_SRC_PV, _y),
		ALU_MULADD(_R123, _z, _R1, _y, KC0(1), _w, ALU_SRC_PV, _z),
		ALU_MULADD(_R123, _w, _R1, _y, KC0(1), _z, ALU_SRC_PV, _w)
		ALU_LAST,
		ALU_MULADD(_R1, _x, _R1, _x, KC0(0), _x, ALU_SRC_PV, _y),
		ALU_MULADD(_R1, _y, _R1, _x, KC0(0), _y, ALU_SRC_PV, _x),
		ALU_MULADD(_R1, _z, _R1, _x, KC0(0), _z, ALU_SRC_PV, _w),
		ALU_MULADD(_R1, _w, _R1, _x, KC0(0), _w, ALU_SRC_PV, _z)
		ALU_LAST,
	}
};

__attribute__((aligned(GX2_SHADER_ALIGNMENT))) static struct {
	u64 cf[32];
	u64 alu[16];
	u64 tex[1 * 2];
} fsSTCode =
{
	{
		TEX(48, 1) VALID_PIX,
		ALU(32, 4),
		EXP_DONE(PIX0, _R0, _x, _y, _z, _w)
		END_OF_PROGRAM
	},
	{
		ALU_MUL(_R0, _x, _R0, _x, _R1, _x),
		ALU_MUL(_R0, _y, _R0, _y, _R1, _y),
		ALU_MUL(_R0, _z, _R0, _z, _R1, _z),
		ALU_MUL(_R0, _w, _R0, _w, _R1, _w)
		ALU_LAST
	},
	{
		TEX_SAMPLE(_R0, _x, _y, _z, _w, _R0, _x, _y, _0, _0, _t0, _s0)
	}
};
// clang-format on

GX2VertexShader STVshaderGX2 = {
	{
		.sq_pgm_resources_vs.num_gprs = 4,
		.sq_pgm_resources_vs.stack_size = 1,
		.spi_vs_out_config.vs_export_count = 1,
		.num_spi_vs_out_id = 1,
		{
			{ .semantic_0 = 0x00, .semantic_1 = 0x01, .semantic_2 = 0xFF, .semantic_3 = 0xFF },
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
		.sq_vtx_semantic_clear = ~0x7,
		.num_sq_vtx_semantic = 3,
		{
			0, 1, 2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		},
		.vgt_vertex_reuse_block_cntl.vtx_reuse_depth = 0xE,
		.vgt_hos_reuse_depth.reuse_depth = 0x10,
	}, /* regs */
	.size = sizeof(vsSTCode),
	.program = (u8 *)&vsSTCode,
	.mode = GX2_SHADER_MODE_UNIFORM_BLOCK,
	.gx2rBuffer.flags = GX2R_RESOURCE_LOCKED_READ_ONLY,
};

GX2PixelShader STPshaderGX2 = {
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
	.size = sizeof(fsSTCode),
	.program = (uint8_t *)&fsSTCode,
	.mode = GX2_SHADER_MODE_UNIFORM_BLOCK,
	.gx2rBuffer.flags = GX2R_RESOURCE_LOCKED_READ_ONLY,
};

// clang-format off
__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct {
	u64 cf[32];
	u64 alu[16];
} clearVCode = {
	{
		CALL_FS NO_BARRIER,
		ALU(32, 16) KCACHE0(CB1, _0_15),
		EXP_DONE(POS0, _R1, _x, _y, _z, _w),
		EXP_DONE(PARAM0, _R2, _r, _g, _b, _a) NO_BARRIER
		END_OF_PROGRAM
	},
	{
		ALU_MUL(__, _x, _R1, _x, KC0(4), _x),
		ALU_MUL(__, _y, _R1, _x, KC0(4), _y),
		ALU_MUL(__, _z, _R1, _x, KC0(4), _z),
		ALU_MUL(__, _w, _R1, _x, KC0(4), _w)
		ALU_LAST,
		ALU_MULADD(__, _x, _R1, _y, KC0(5), _x, ALU_SRC_PV, _x),
		ALU_MULADD(__, _y, _R1, _y, KC0(5), _y, ALU_SRC_PV, _y),
		ALU_MULADD(__, _z, _R1, _y, KC0(5), _z, ALU_SRC_PV, _z),
		ALU_MULADD(__, _w, _R1, _y, KC0(5), _w, ALU_SRC_PV, _w)
		ALU_LAST,
		ALU_MULADD(__, _x, _R1, _z, KC0(6), _x, ALU_SRC_PV, _x),
		ALU_MULADD(__, _y, _R1, _z, KC0(6), _y, ALU_SRC_PV, _y),
		ALU_MULADD(__, _z, _R1, _z, KC0(6), _z, ALU_SRC_PV, _z),
		ALU_MULADD(__, _w, _R1, _z, KC0(6), _w, ALU_SRC_PV, _w)
		ALU_LAST,
		ALU_ADD(_R1, _x, KC0(7), _x, ALU_SRC_PV, _x),
		ALU_ADD(_R1, _y, KC0(7), _y, ALU_SRC_PV, _y),
		ALU_ADD(_R1, _z, KC0(7), _z, ALU_SRC_PV, _z),
		ALU_ADD(_R1, _w, KC0(7), _w, ALU_SRC_PV, _w)
		ALU_LAST,
	}
};
// clang-format on
GX2VertexShader clearVShaderGX2 = {
	{
		.sq_pgm_resources_vs.num_gprs = 3,
		.sq_pgm_resources_vs.stack_size = 1,
		.spi_vs_out_config.vs_export_count = 0,
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
		{
			0, 2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		},
		.vgt_vertex_reuse_block_cntl.vtx_reuse_depth = 0xE,
		.vgt_hos_reuse_depth.reuse_depth = 0x10,
	}, /* regs */
	sizeof(clearVCode),
	(uint8_t *)&clearVCode,
	GX2_SHADER_MODE_UNIFORM_BLOCK,
	.gx2rBuffer.flags = GX2R_RESOURCE_LOCKED_READ_ONLY,
};

// clang-format off
__attribute__((aligned(GX2_SHADER_ALIGNMENT))) static struct {
	u64 cf[1];
} clearPCode =
{
	{
		EXP_DONE(PIX0, _R0, _r, _g, _b, _a)
		END_OF_PROGRAM
	},
};

// clang-format on

GX2PixelShader clearPShaderGX2 = {
	{
		.sq_pgm_resources_ps.num_gprs = 1,
		.sq_pgm_exports_ps.export_mode = 0x2,
		.spi_ps_in_control_0.num_interp = 1,
		.spi_ps_in_control_0.persp_gradient_ena = TRUE,
		.spi_ps_in_control_0.baryc_sample_cntl = spi_baryc_cntl_centers_only,
		.num_spi_ps_input_cntl = 1,
		{ { .semantic = 0, .default_val = 1 } },
		.cb_shader_mask.output0_enable = 0xF,
		.cb_shader_control.rt0_enable = TRUE,
		.db_shader_control.z_order = db_z_order_early_z_then_late_z,
	}, /* regs */
	.size = sizeof(clearPCode),
	(uint8_t *)&clearPCode,
	GX2_SHADER_MODE_UNIFORM_BLOCK,
	.gx2rBuffer.flags = GX2R_RESOURCE_LOCKED_READ_ONLY,
};

// clang-format off
__attribute__((aligned(GX2_SHADER_ALIGNMENT)))
static struct {
	u64 cf[32];
	u64 alu[16];
} cTexVCode = {
	{
		CALL_FS NO_BARRIER,
		ALU(32, 16) KCACHE0(CB1, _0_15),
		EXP_DONE(POS0, _R1, _x, _y, _z, _w),
		EXP(PARAM0, _R2, _x, _y, _0, _0) NO_BARRIER,
		EXP_DONE(PARAM1, _R3, _r, _g, _b, _a) NO_BARRIER
		END_OF_PROGRAM
	},
	{
		ALU_MUL(__, _x, _R1, _x, KC0(0), _x),
		ALU_MUL(__, _y, _R1, _x, KC0(0), _y),
		ALU_MUL(__, _z, _R1, _x, KC0(0), _z),
		ALU_MUL(__, _w, _R1, _x, KC0(0), _w)
		ALU_LAST,
		ALU_MULADD(__, _x, _R1, _y, KC0(1), _x, ALU_SRC_PV, _x),
		ALU_MULADD(__, _y, _R1, _y, KC0(1), _y, ALU_SRC_PV, _y),
		ALU_MULADD(__, _z, _R1, _y, KC0(1), _z, ALU_SRC_PV, _z),
		ALU_MULADD(__, _w, _R1, _y, KC0(1), _w, ALU_SRC_PV, _w)
		ALU_LAST,
		ALU_MULADD(__, _x, _R1, _z, KC0(2), _x, ALU_SRC_PV, _x),
		ALU_MULADD(__, _y, _R1, _z, KC0(2), _y, ALU_SRC_PV, _y),
		ALU_MULADD(__, _z, _R1, _z, KC0(2), _z, ALU_SRC_PV, _z),
		ALU_MULADD(__, _w, _R1, _z, KC0(2), _w, ALU_SRC_PV, _w)
		ALU_LAST,
		ALU_ADD(_R1, _x, KC0(3), _x, ALU_SRC_PV, _x),
		ALU_ADD(_R1, _y, KC0(3), _y, ALU_SRC_PV, _y),
		ALU_ADD(_R1, _z, KC0(3), _z, ALU_SRC_PV, _z),
		ALU_ADD(_R1, _w, KC0(3), _w, ALU_SRC_PV, _w)
		ALU_LAST,
	}
};
// clang-format on

GX2VertexShader cTexVShaderGX2 = {
	{
		.sq_pgm_resources_vs.num_gprs = 4,
		.sq_pgm_resources_vs.stack_size = 1,
		.spi_vs_out_config.vs_export_count = 1,
		.num_spi_vs_out_id = 1,
		{
			{ .semantic_0 = 0x00, .semantic_1 = 0x01, .semantic_2 = 0xFF, .semantic_3 = 0xFF },
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
		.sq_vtx_semantic_clear = ~0x7,
		.num_sq_vtx_semantic = 3,
		{
			0, 1, 2, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		},
		.vgt_vertex_reuse_block_cntl.vtx_reuse_depth = 0xE,
		.vgt_hos_reuse_depth.reuse_depth = 0x10,
	}, /* regs */
	sizeof(cTexVCode),
	(uint8_t *)&cTexVCode,
	GX2_SHADER_MODE_UNIFORM_BLOCK,
	.gx2rBuffer.flags = GX2R_RESOURCE_LOCKED_READ_ONLY,
};

// clang-format off
__attribute__((aligned(GX2_SHADER_ALIGNMENT))) static struct {
	u64 cf[32];
	u64 alu[16];
	u64 tex[1 * 2];
} cTexPCode =
{
	{
		TEX(48, 1) VALID_PIX,
		ALU(32, 4),
		EXP_DONE(PIX0, _R1, _r, _g, _b, _a)
		END_OF_PROGRAM
	},
	{
		ALU_ADD(_R1, _r, _R1, _r, _R0, _r),
		ALU_ADD(_R1, _g, _R1, _g, _R0, _g),
		ALU_ADD(_R1, _b, _R1, _b, _R0, _b),
		ALU_MOV(_R1, _a, _R1, _a)
		ALU_LAST
	},
	{
		TEX_SAMPLE(_R0, _r, _g, _b, __, _R0, _x, _y, _0, _0, _t0, _s0)
	}
};

// clang-format on

GX2PixelShader cTexPShaderGX2 = {
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
	sizeof(cTexPCode),
	(uint8_t *)&cTexPCode,
	GX2_SHADER_MODE_UNIFORM_BLOCK,
	.gx2rBuffer.flags = GX2R_RESOURCE_LOCKED_READ_ONLY,
};
