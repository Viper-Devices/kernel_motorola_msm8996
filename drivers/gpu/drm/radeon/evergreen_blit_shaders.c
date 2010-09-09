/*
 * Copyright 2010 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *     Alex Deucher <alexander.deucher@amd.com>
 */

#include <linux/types.h>
#include <linux/kernel.h>

/*
 * evergreen cards need to use the 3D engine to blit data which requires
 * quite a bit of hw state setup.  Rather than pull the whole 3D driver
 * (which normally generates the 3D state) into the DRM, we opt to use
 * statically generated state tables.  The regsiter state and shaders
 * were hand generated to support blitting functionality.  See the 3D
 * driver or documentation for descriptions of the registers and
 * shader instructions.
 */

const u32 evergreen_default_state[] =
{
	0xc0012800, /* CONTEXT_CONTROL */
	0x80000000,
	0x80000000,

	0xc0016900,
	0x0000023b,
	0x00000000, /* SQ_LDS_ALLOC_PS */

	0xc0066900,
	0x00000240,
	0x00000000, /* SQ_ESGS_RING_ITEMSIZE */
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,
	0x00000000,

	0xc0046900,
	0x00000247,
	0x00000000, /* SQ_GS_VERT_ITEMSIZE */
	0x00000000,
	0x00000000,
	0x00000000,

	0xc0026f00,
	0x00000000,
	0x00000000, /* SQ_VTX_BASE_VTX_LOC */
	0x00000000,

	0xc0026900,
	0x00000010,
	0x00000000, /* DB_Z_INFO */
	0x00000000, /* DB_STENCIL_INFO */


	0xc0016900,
	0x00000200,
	0x00000000, /* DB_DEPTH_CONTROL */

	0xc0066900,
	0x00000000,
	0x00000060, /* DB_RENDER_CONTROL */
	0x00000000, /* DB_COUNT_CONTROL */
	0x00000000, /* DB_DEPTH_VIEW */
	0x0000002a, /* DB_RENDER_OVERRIDE */
	0x00000000, /* DB_RENDER_OVERRIDE2 */
	0x00000000, /* DB_HTILE_DATA_BASE */

	0xc0026900,
	0x0000000a,
	0x00000000, /* DB_STENCIL_CLEAR */
	0x00000000, /* DB_DEPTH_CLEAR */

	0xc0016900,
	0x000002dc,
	0x0000aa00, /* DB_ALPHA_TO_MASK */

	0xc0016900,
	0x00000080,
	0x00000000, /* PA_SC_WINDOW_OFFSET */

	0xc00d6900,
	0x00000083,
	0x0000ffff, /* PA_SC_CLIPRECT_RULE */
	0x00000000, /* PA_SC_CLIPRECT_0_TL */
	0x20002000, /* PA_SC_CLIPRECT_0_BR */
	0x00000000,
	0x20002000,
	0x00000000,
	0x20002000,
	0x00000000,
	0x20002000,
	0xaaaaaaaa, /* PA_SC_EDGERULE */
	0x00000000, /* PA_SU_HARDWARE_SCREEN_OFFSET */
	0x0000000f, /* CB_TARGET_MASK */
	0x0000000f, /* CB_SHADER_MASK */

	0xc0226900,
	0x00000094,
	0x80000000, /* PA_SC_VPORT_SCISSOR_0_TL */
	0x20002000, /* PA_SC_VPORT_SCISSOR_0_BR */
	0x80000000,
	0x20002000,
	0x80000000,
	0x20002000,
	0x80000000,
	0x20002000,
	0x80000000,
	0x20002000,
	0x80000000,
	0x20002000,
	0x80000000,
	0x20002000,
	0x80000000,
	0x20002000,
	0x80000000,
	0x20002000,
	0x80000000,
	0x20002000,
	0x80000000,
	0x20002000,
	0x80000000,
	0x20002000,
	0x80000000,
	0x20002000,
	0x80000000,
	0x20002000,
	0x80000000,
	0x20002000,
	0x80000000,
	0x20002000,
	0x00000000, /* PA_SC_VPORT_ZMIN_0 */
	0x3f800000, /* PA_SC_VPORT_ZMAX_0 */

	0xc0016900,
	0x000000d4,
	0x00000000, /* SX_MISC */

	0xc0026900,
	0x00000292,
	0x00000000, /* PA_SC_MODE_CNTL_0 */
	0x00000000, /* PA_SC_MODE_CNTL_1 */

	0xc0106900,
	0x00000300,
	0x00000000, /* PA_SC_LINE_CNTL */
	0x00000000, /* PA_SC_AA_CONFIG */
	0x00000005, /* PA_SU_VTX_CNTL */
	0x3f800000, /* PA_CL_GB_VERT_CLIP_ADJ */
	0x3f800000, /* PA_CL_GB_VERT_DISC_ADJ */
	0x3f800000, /* PA_CL_GB_HORZ_CLIP_ADJ */
	0x3f800000, /* PA_CL_GB_HORZ_DISC_ADJ */
	0x00000000, /* PA_SC_AA_SAMPLE_LOCS_0 */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /* PA_SC_AA_SAMPLE_LOCS_7 */
	0xffffffff, /* PA_SC_AA_MASK */

	0xc00d6900,
	0x00000202,
	0x00cc0010, /* CB_COLOR_CONTROL */
	0x00000210, /* DB_SHADER_CONTROL */
	0x00010000, /* PA_CL_CLIP_CNTL */
	0x00000004, /* PA_SU_SC_MODE_CNTL */
	0x00000100, /* PA_CL_VTE_CNTL */
	0x00000000, /* PA_CL_VS_OUT_CNTL */
	0x00000000, /* PA_CL_NANINF_CNTL */
	0x00000000, /* PA_SU_LINE_STIPPLE_CNTL */
	0x00000000, /* PA_SU_LINE_STIPPLE_SCALE */
	0x00000000, /* PA_SU_PRIM_FILTER_CNTL */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /* SQ_DYN_GPR_RESOURCE_LIMIT_1 */

	0xc0066900,
	0x000002de,
	0x00000000, /* PA_SU_POLY_OFFSET_DB_FMT_CNTL */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */

	0xc0016900,
	0x00000229,
	0x00000000, /* SQ_PGM_START_FS */

	0xc0016900,
	0x0000022a,
	0x00000000, /* SQ_PGM_RESOURCES_FS */

	0xc0096900,
	0x00000100,
	0x00ffffff, /* VGT_MAX_VTX_INDX */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /* SX_ALPHA_TEST_CONTROL */
	0x00000000, /* CB_BLEND_RED */
	0x00000000, /* CB_BLEND_GREEN */
	0x00000000, /* CB_BLEND_BLUE */
	0x00000000, /* CB_BLEND_ALPHA */

	0xc0026900,
	0x000002a8,
	0x00000000, /* VGT_INSTANCE_STEP_RATE_0 */
	0x00000000, /*  */

	0xc0026900,
	0x000002ad,
	0x00000000, /* VGT_REUSE_OFF */
	0x00000000, /*  */

	0xc0116900,
	0x00000280,
	0x00000000, /* PA_SU_POINT_SIZE */
	0x00000000, /* PA_SU_POINT_MINMAX */
	0x00000008, /* PA_SU_LINE_CNTL */
	0x00000000, /* PA_SC_LINE_STIPPLE */
	0x00000000, /* VGT_OUTPUT_PATH_CNTL */
	0x00000000, /* VGT_HOS_CNTL */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /* VGT_GS_MODE */

	0xc0016900,
	0x000002a1,
	0x00000000, /* VGT_PRIMITIVEID_EN */

	0xc0016900,
	0x000002a5,
	0x00000000, /* VGT_MULTI_PRIM_IB_RESET_EN */

	0xc0016900,
	0x000002d5,
	0x00000000, /* VGT_SHADER_STAGES_EN */

	0xc0026900,
	0x000002e5,
	0x00000000, /* VGT_STRMOUT_CONFIG */
	0x00000000, /*  */

	0xc0016900,
	0x000001e0,
	0x00000000, /* CB_BLEND0_CONTROL */

	0xc0016900,
	0x000001b1,
	0x00000000, /* SPI_VS_OUT_CONFIG */

	0xc0016900,
	0x00000187,
	0x00000000, /* SPI_VS_OUT_ID_0 */

	0xc0016900,
	0x00000191,
	0x00000100, /* SPI_PS_INPUT_CNTL_0 */

	0xc00b6900,
	0x000001b3,
	0x20000001, /* SPI_PS_IN_CONTROL_0 */
	0x00000000, /* SPI_PS_IN_CONTROL_1 */
	0x00000000, /* SPI_INTERP_CONTROL_0 */
	0x00000000, /* SPI_INPUT_Z */
	0x00000000, /* SPI_FOG_CNTL */
	0x00100000, /* SPI_BARYC_CNTL */
	0x00000000, /* SPI_PS_IN_CONTROL_2 */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */
	0x00000000, /*  */

	0xc0036e00, /* SET_SAMPLER */
	0x00000000,
	0x00000012,
	0x00000000,
	0x00000000,
};

const u32 evergreen_vs[] =
{
	0x00000004,
	0x80800400,
	0x0000a03c,
	0x95000688,
	0x00004000,
	0x15200688,
	0x00000000,
	0x00000000,
	0x3c000000,
	0x67961001,
	0x00080000,
	0x00000000,
	0x1c000000,
	0x67961000,
	0x00000008,
	0x00000000,
};

const u32 evergreen_ps[] =
{
	0x00000003,
	0xa00c0000,
	0x00000008,
	0x80400000,
	0x00000000,
	0x95200688,
	0x00380400,
	0x00146b10,
	0x00380000,
	0x20146b10,
	0x00380400,
	0x40146b00,
	0x80380000,
	0x60146b00,
	0x00000000,
	0x00000000,
	0x00000010,
	0x000d1000,
	0xb0800000,
	0x00000000,
};

const u32 evergreen_ps_size = ARRAY_SIZE(evergreen_ps);
const u32 evergreen_vs_size = ARRAY_SIZE(evergreen_vs);
const u32 evergreen_default_size = ARRAY_SIZE(evergreen_default_state);
