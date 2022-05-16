// Testing the GS LOD stuff
#include "defs.h"

#include <kernel.h>
#include <tamtypes.h>
#include <stdio.h>
#include <string.h>

#include <gif_tags.h>

#include <gs_gp.h>
#include <gs_psm.h>

#include <dma.h>
#include <dma_tags.h>

#include <draw.h>
#include <graph.h>

#include <packet.h>
#include <stdlib.h>
#include <fcntl.h>

const u32 WIDTH = 640;
const u32 HEIGHT = 480;
const s32 FRAME_PSM = GS_PSM_32;

int download_from_gs(u32 address, u32 format, const char *fname, int psm_size, int width, int height, u32 custtrx = 1);
bool download_one_pixel(u32 address, u32 format, u32* output);

void initialize_graphics(framebuffer_t *fb, zbuffer_t *zb)
{
	printf("Initializing graphics stuff\n");

	// Reset GIF
	(*(volatile u_int *)0x10003000) = 1;

	// Allocate framebuffer and zbuffer
	fb->width = WIDTH;
	fb->height = HEIGHT;
	fb->psm = FRAME_PSM;
	fb->mask = 0x00000000;

	fb->address = graph_vram_allocate(fb->width, fb->height, fb->psm, GRAPH_ALIGN_PAGE);

	zb->enable = 1;
	zb->zsm = GS_ZBUF_24;
	zb->address = graph_vram_allocate(fb->width, fb->height, zb->zsm, GRAPH_ALIGN_PAGE);
	zb->mask = 0;
	zb->method = ZTEST_METHOD_ALLPASS;

	graph_initialize(fb->address, fb->width, fb->height, fb->psm, 0, 0);
	// graph_set_mode(1, GRAPH_MODE_NTSC, 0, 0);

	dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0);
	dma_channel_fast_waits(DMA_CHANNEL_GIF);

	/*
		Init drawing stuff
	*/

	packet_t *packet = packet_init(50, PACKET_NORMAL);

	qword_t *q = packet->data;

	q = draw_setup_environment(q, 0, fb, zb);
	q = draw_primitive_xyoffset(q, 0, 0, 0);
	q = draw_disable_tests(q, 0, zb);
	q = draw_finish(q);

	dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
	draw_wait_finish();

	packet_free(packet);
	printf("Finished\n");
}

u32 texFFAddr;
u32 tex00Addr;
u32 texWhite[32*32] ALIGNED(16);
u32 texBlack[32*32] ALIGNED(16);

u32 draw_with_q(u32 Q, u32 L, u32 K)
{
	packet_t *packet = packet_init(100, PACKET_NORMAL);
	qword_t *q = packet->data;

	PACK_GIFTAG(q, GIF_SET_TAG(1, 1, GIF_PRE_ENABLE, GS_SET_PRIM(GS_PRIM_TRIANGLE, 0, 1, 0, 0, 0, 0, 0, 0), GIF_FLG_PACKED, 12),
				(u64)(GIF_REG_AD) | (GIF_REG_AD << 4) | (GIF_REG_AD << 8) | (GIF_REG_ST << 12) | (GIF_REG_RGBAQ << 16) | (GIF_REG_XYZ2 << 20) | (GIF_REG_ST << 24) | (GIF_REG_RGBAQ << 28) | ((u64)GIF_REG_XYZ2 << 32) | ((u64)GIF_REG_ST << 36) | ((u64)GIF_REG_RGBAQ << 40) | ((u64)GIF_REG_XYZ2 << 44));
	{
		q++;
		q->dw[0] = GS_SET_TEX1(0, 1, 0, 3, 0, L, K);
		q->dw[1] = GS_REG_TEX1;
		q++;
		q->dw[0] = (tex00Addr / 64) | (1 << 14);
		q->dw[1] = GS_REG_MIPTBP1;
		q++;
		q->dw[0] = GS_SET_TEX0(texFFAddr / 64, 1, GS_PSM_24, 5, 5, 0, 1, 0, 0, 0, 0, 0);
		q->dw[1] = GS_REG_TEX0;
		q++;
		// ST
		q->dw[0] = GIF_SET_ST(0, 0);
		q->dw[1] = Q;
		q++;
		// RGBAQ
		q->dw[0] = 0;
		q->dw[1] = 0;
		q++;
		// XYZ2
		q->dw[0] = (u64)((((0 << 4)) | (((u64)(0 << 4)) << 32)));
		q->dw[1] = (u64)(255);
		q++;
		// ST
		q->dw[0] = GIF_SET_ST(0x3f800000, 0x3f000000);
		q->dw[1] = Q;
		q++;
		// RGBAQ
		q->dw[0] = 0;
		q->dw[1] = 0;
		q++;
		// XYZ2
		q->dw[0] = (u64)((((0 << 4)) | (((u64)(64 << 4)) << 32)));
		q->dw[1] = (u64)(1);
		q++;
		// ST
		q->dw[0] = GIF_SET_ST(0x3f800000, 0x3f800000);
		q->dw[1] = Q;
		q++;
		// RGBAQ
		q->dw[0] = 0;
		q->dw[1] = 0;
		q++;
		// XYZ2
		q->dw[0] = (u64)((((64 << 4)) | (((u64)(0 << 4)) << 32)));
		q->dw[1] = (u64)(1);
		q++;
	}
	q = draw_finish(q);
	dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
	draw_wait_finish();

	packet_free(packet);
	return 1;
}

u32 g_L = 0;
u32 g_K;

u32 do_test(u32 Q) {
	draw_with_q(Q, g_L, g_K);
	u32 res;
	download_one_pixel(0, GS_PSM_32, &res);
	return res;
}

#define USE_FLOAT 0

#if USE_FLOAT
typedef u32(*MysteryFunction)(float);
#else
typedef u32(*MysteryFunction)(u32);
#endif

u32 CallMysteryFunction(MysteryFunction f, u32 value)
{
#if USE_FLOAT
    float fvalue;
    memcpy(&fvalue, &value, sizeof(fvalue));
    return f(fvalue);
#else
    return f(value);
#endif
}

struct Result {
    u32 input;
    u32 output;
};

Result SearchForLowestDifferent(MysteryFunction f, u32 upper, u32 upper_value, u32 lower, u32 lower_value)
{
    while (upper > lower + 1) {
        u32 mid = (upper + lower) / 2;
        u32 mid_value = CallMysteryFunction(f, mid);
        if (mid_value != lower_value) {
            upper = mid;
            upper_value = mid_value;
        } else {
            lower = mid;
            lower_value = mid_value;
        }
    }
    Result out;
    out.input = upper;
    out.output = upper_value;
    return out;
}

void FindAllBoundaries(MysteryFunction f) {
    u32 upper = 0x7fffffff; // FLT_MAX
    u32 lower = 0x00000000; // 0
    u32 upper_value = CallMysteryFunction(f, upper);
    u32 lower_value = CallMysteryFunction(f, lower);
    while (upper_value != lower_value) {
        Result res = SearchForLowestDifferent(f, upper, upper_value, lower, lower_value);
        printf("%08x - %08x: %08x\n", lower, res.input - 1, lower_value);
        lower = res.input;
        lower_value = res.output;
    }
    printf("%08x - %08x: %08x\n", lower, upper, lower_value);
}

int main(void)
{
	framebuffer_t fb;
	zbuffer_t zb;
	initialize_graphics(&fb, &zb);
	printf("Blah\n");

	packet_t *packet = packet_init(100, PACKET_NORMAL);

	qword_t *q = packet->data;

	// Clear the screen
	{
		PACK_GIFTAG(q, GIF_SET_TAG(1, 1, GIF_PRE_ENABLE, GIF_PRIM_SPRITE, GIF_FLG_PACKED, 5),
					GIF_REG_AD | (GIF_REG_RGBAQ << 4) | (GIF_REG_XYZ2 << 8) | (GIF_REG_XYZ2 << 12) | (GIF_REG_AD << 16));
		q++;
		q->dw[0] = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1);
		q->dw[1] = GS_REG_TEST;
		q++;
		// RGBAQ
		q->dw[0] = (u64)((0x33) | ((u64)0x33 << 32));
		q->dw[1] = (u64)((0x33) | ((u64)0 << 32));
		q++;
		// XYZ2
		q->dw[0] = (u64)((((0 << 4)) | (((u64)(0 << 4)) << 32)));
		q->dw[1] = (u64)(0);
		q++;
		// XYZ2
		q->dw[0] = (u64)((((640 << 4)) | (((u64)(480 << 4)) << 32)));
		q->dw[1] = (u64)(0);
		q++;
		q->dw[0] = 0;
		q->dw[1] = GS_REG_NOP;
		q++;
		q = draw_finish(q);

		dma_channel_send_normal(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);

		draw_wait_finish();
	}

	// Upload the textures
	texFFAddr = graph_vram_allocate(32, 32, GS_PSM_24, GRAPH_ALIGN_PAGE);
	tex00Addr = graph_vram_allocate(32, 32, GS_PSM_24, GRAPH_ALIGN_PAGE);
	printf("TexFF @ %08x, Tex00 @ %08x\n", texFFAddr, tex00Addr);

	q = packet->data;

	memset(texWhite, 0xFF, sizeof(texWhite));
	memset(texBlack, 0x00, sizeof(texBlack));
	FlushCache(0);

	q = draw_texture_transfer(q, texWhite, 32, 32, GS_PSM_32, texFFAddr, 64);
	q = draw_texture_transfer(q, texBlack, 32, 32, GS_PSM_32, tex00Addr, 64);
	q = draw_texture_flush(q);
	dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);

	for (int k = 0x7f8; k > -0x800; k -= 8) {
		printf("K = %d\n", k);
		g_K = k & 0xFFF;
		FindAllBoundaries(do_test);
	}
	printf("Sleeping\n");
	SleepThread();
}

bool download_one_pixel(u32 address, u32 format, u32* output)
{
	static union
	{
		u32 value_u32[4];
		u128 value;
	} enable_path3 ALIGNED(16) = {
		{VIF1_MSKPATH3(0), VIF1_NOP, VIF1_NOP, VIF1_NOP}
	};

	u32 res[64] ALIGNED(16);
	for (u32& r : res)
		r = 0xDEADBEEF;

	u32 prev_imr;
	u32 prev_chcr;
	u32 dmaChain[20 * 2] ALIGNED(16);

	u32 i = 0;
	dmaChain[i++] = VIF1_NOP;
	dmaChain[i++] = VIF1_MSKPATH3(0x8000);
	dmaChain[i++] = VIF1_FLUSHA;
	dmaChain[i++] = VIF1_DIRECT(6);

	u64* dma64 = (u64*)&dmaChain[i];
	i = 0;
	dma64[i++] = GIFTAG(5, 1, 0, 0, 0, 1);
	dma64[i++] = GIF_AD;
	dma64[i++] = GSBITBLTBUF_SET(address / 64, 1, format, 0, 0, GS_PSM_32);
	dma64[i++] = GSBITBLTBUF;
	dma64[i++] = GSTRXPOS_SET(0, 0, 0, 0, 0); // SSAX, SSAY, DSAX, DSAY, DIR
	dma64[i++] = GSTRXPOS;
	dma64[i++] = GSTRXREG_SET(8, 8); // RRW, RRH
	dma64[i++] = GSTRXREG;
	dma64[i++] = 0;
	dma64[i++] = GSFINISH;
	dma64[i++] = GSTRXDIR_SET(1);
	dma64[i++] = GSTRXDIR;

	prev_imr = GsPutIMR(GsGetIMR() | 0x0200);
	prev_chcr = *D1_CHCR;

	if ((*D1_CHCR & 0x0100) != 0)
		return false;

	*GS_CSR = CSR_FINISH;

	FlushCache(0);
	*D1_QWC = 0x7;
	*D1_MADR = (u32)dmaChain;
	*D1_CHCR = 0x101;

	asm __volatile__(" sync.l\n");

	// check if DMA is complete (STR=0)
	while (*D1_CHCR & 0x0100)
		;

	asm __volatile__("sync.l\n");

	// check if DMA is complete (STR=0)

	//printf("Waiting for DMA channel completion\n");
	while (*D1_CHCR & 0x0100)
		;
	//printf("Waiting for GS_CSR\n");
	while ((*GS_CSR & CSR_FINISH) == 0)
		;
	//printf("Waiting for the vif fifo to empty\n");
	// Wait for viffifo to become empty
	while ((*VIF1_STAT & (0x1f000000)))
		;

	// Reverse busdir and transfer image to host
	*VIF1_STAT = VIF1_STAT_FDR;
	*GS_BUSDIR = (u64)1;

	FlushCache(0);

	*D1_QWC = sizeof(res)/16;
	*D1_MADR = (u32)res;
	*D1_CHCR = 0x100;

	asm __volatile__(" sync.l\n");

	// Wait for viffifo to become empty
	while ((*VIF1_STAT & (0x1f000000)))
		;

	// check if DMA is complete (STR=0)
	while (*D1_CHCR & 0x0100)
		;

	*D1_CHCR = prev_chcr;

	asm __volatile__(" sync.l\n");

	*VIF1_STAT = 0;
	*GS_BUSDIR = (u64)0;
	// Put back prev imr and set finish event

	GsPutIMR(prev_imr);
	*GS_CSR = CSR_FINISH;
	// Enable path3 again
	*VIF1_FIFO = enable_path3.value;

	*output = res[0];
	return true;
}