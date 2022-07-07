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

void scan_q(u32 Q);
void dma_send_vif1(u128* begin, u128* end);
void download_region(u32 address, u32 bw, u32 format, u32* output, u32 width, u32 height);

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

u32 texAddr;
u32 tex[1024] ALIGNED(16);

u32 decodeR(u32 r) {
	switch (r) {
		case 0x00: return 0;
		case 0x0f: return 1;
		case 0x1f: return 2;
		case 0x2f: return 3;
		case 0x3f: return 4;
		case 0x4f: return 5;
		case 0x5f: return 6;
		case 0x6f: return 7;
		case 0x7f: return 8;
		case 0x8f: return 9;
		case 0x9f: return 10;
		case 0xaf: return 11;
		case 0xbf: return 12;
		case 0xcf: return 13;
		case 0xdf: return 14;
		case 0xef: return 15;
		case 0xff: return 16;
	}
	printf("Bad R value!\n");
	return 0;
}

u32 decodePixel(u32 pixel) {
	u32 r = (pixel      ) & 0xff;
	u32 g = (pixel >>  8) & 0xff;
	u32 b = (pixel >> 16) & 0xff;
	u32 b_offset = b * 4 * 16;
	u32 g_offset;
	u32 r_offset;
	u32 r_rising;
	if ((b & 0x3f) == 0x3f && g < 0xfc) {
		// Green resets here, which is a bit annoying
		g_offset = 3 * 16;
		r_rising = 0;
	} else {
		g_offset = (g & 3) * 16;
		r_rising = !(g & 1);
	}
	if (r_rising) {
		r_offset = decodeR(r);
	} else {
		r_offset = 16 - decodeR(r);
	}
	if (r_offset >= 16)
		printf("Bad R Value!\n");
	return b_offset + g_offset + r_offset + 8;
}

extern u128 VUPointScanVIFTagBegin __attribute__((section(".vudata")));
extern u128 VUPointScanVIFTagEnd   __attribute__((section(".vudata")));

static u32 MakeQ(u32 base)
{
	// Float which is 2^16 less than 1.0, whose reciprocal will be 16 bits when cast to an integer with no adjustment necessary
	return (base << 9) | 0x37800000;
}

int main(void)
{
	framebuffer_t fb;
	zbuffer_t zb;
	initialize_graphics(&fb, &zb);
	printf("Blah\n");

	dma_send_vif1(&VUPointScanVIFTagBegin, &VUPointScanVIFTagEnd);

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

	for (u32 i = 0; i < 1024; i++) {
		u32 r = i & 1 ? 255 : 0;
		u32 g = i & 255;
		u32 b = i >> 2;
		tex[i] = r | (g << 8) | (b << 16);
	}

	// Upload the textures
	texAddr = graph_vram_allocate(1024, 1, GS_PSM_24, GRAPH_ALIGN_PAGE);
	printf("Tex @ %08x\n", texAddr);

	q = packet->data;

	FlushCache(0);

	q = draw_texture_transfer(q, tex, 1024, 1, GS_PSM_32, texAddr, 64);
	q = draw_texture_flush(q);
	dma_channel_send_chain(DMA_CHANNEL_GIF, packet->data, q - packet->data, 0, 0);
	static u32 data[32768] ALIGNED(16);
	scan_q(MakeQ(0));
	for (int i = 0; i < 16384; i++) {
		download_region(fb.address, fb.width / 64, GS_PSM_32, data, 128, 256);
		scan_q(MakeQ(i + 1)); // Start the next one while we process this one for a bit of parallelism
		u32 Q = MakeQ(i);
		FlushCache(0); // Make sure we actually see the newly downloaded data
		int low = 0;
		int high = 32767;
		// Remove values that were clamped to the texture edge, they don't tell us anything
		while (data[low] == 0) {
			low++;
		}
		while (data[high] == 0x00ffffff) {
			high--;
		}
		if (high <= low) {
			printf("??? Nothing but edge values for %04x\n", i);
			continue;
		}

		// We expect decodePixel(data[high]) to be about 0x3ff7
		u32 high_value = decodePixel(data[high]);
		// printf("GS: %04x / %08x = %04x\n", high, Q, high_value);
		// Expectation: GS calculates data[x] as (x * unk) >> 16 where unk is some 16-bit number (which is approximately 1/Q)
		// Assuming x is 0x3ff7, that unk could be up to 5 different values to make that equation true (since 0x3ff7 * 4 < (1<<16) but 0x3ff7 * 5 > (1<<16))
		// We'll find the lowest one here, then test it plus the 5 values above it for each loop

		// (high * unk) >> 16 == high_value
		// high * unk ~= high_value << 16
		// unk ~= (high_value << 16) / high
		u32 RCPGuess = (high_value << 16) / high;
		// If done with reals, the above equation would get the lowest value where (high * unk) >> 16 == high_value is true
		// But these are integers, so it instead will be *at most* that
		// keep incrementing until it works
		while (((RCPGuess * high) >> 16) != high_value)
			RCPGuess++;
		bool results[5] = {true, true, true, true, true};
		u32 possibilities[5] = {RCPGuess, RCPGuess + 1, RCPGuess + 2, RCPGuess + 3, RCPGuess + 4};
		// Thorough == true: Validate `Expectation` by going through every item from low to high
		// Thorough == false: Assume `Expectation` is true and exit as soon as we've narrowed results down to 1
		constexpr bool thorough = true;
		for (int j = high; j >= low; j--) { // higher values are more likely to give us interesting info, so start from the top
			for (int k = 0; k < 5; k++) {
				results[k] &= ((possibilities[k] * j) >> 16) == decodePixel(data[j]);
			}
			if (!thorough) {
				u32 sum = 0;
				for (int k = 0; k < 5; k++) {
					sum += results[k];
				}
				if (sum <= 1) {
					break;
				}
			}
		}
		u64 QInt = i | 0x4000; // or in the implicit leading 1 bit used by floating point
		s32 actual = 0x200000000000ull / QInt; // Actual reciprocal (in 17.15 fixed point)
		u32 sum = 0;
		for (int j = 0; j < 5; j++) {
			sum += results[j];
		}
		if (sum == 0) {
			printf("%08x (%04x) doesn't satisfy our expectations!\n", Q, i);
		} else if (sum == 1) {
			// Success!
			for (int j = 0; j < 5; j++) {
				if (results[j]) {
					s32 diff = (possibilities[j] << 15) - actual;
					float fdiff = static_cast<float>(diff) / static_cast<float>(1 << 15);
					printf("1 / %08x (%04x) = %04x (% 07.4f off)\n", Q, i, possibilities[j], fdiff);
				}
			}
		} else {
			printf("1 / %08x (%04x) could be any of the following:\n", Q, i);
			for (int j = 0; j < 5; j++) {
				if (results[j]) {
					s32 diff = (possibilities[j] << 15) - actual;
					float fdiff = static_cast<float>(diff) / static_cast<float>(1 << 15);
					printf("\t%04x (% 07.4f off)\n", possibilities[j], fdiff);
				}
			}
		}
	}
	printf("Sleeping\n");
	SleepThread();
}

static void compiler_memory_reorder_barrier()
{
	asm("":::"memory");
}

void dma_send_vif1(u128* begin, u128* end)
{
	// Wait for previous DMA to complete
	while (*D1_CHCR & 0x100)
		;
	*D1_QWC = end - begin;
	*D1_MADR = (u32)begin;
	*D1_CHCR = 0x101;
}

template <typename T>
T& uncached(T& t) {
	return *(T*)((u32)&t | 0x20000000);
}

void StartPointScan(u32 S, u32 T, u32 Q)
{
	static struct {
		u32 pad0[3] = {VIF1_NOP, VIF1_NOP, VIF1_NOP};
		u32 gifconfig_viftag = VIF1_DIRECT(4);
		u64 gifconfig_giftag[2] = {
			GIF_SET_TAG(3, 1, GIF_PRE_DISABLE, 0, GIF_FLG_PACKED, 1),
			GIF_REG_AD,
		};
		u64 gs_tex0[2] = {0, GS_REG_TEX0};
		u64 gs_tex1[2] = {GS_SET_TEX1(1, 0, 1, 1, 0, 0, 0), GS_REG_TEX1};
		u64 gs_clamp[2] = {GS_SET_CLAMP(1, 1, 0, 0, 0, 0), GS_REG_CLAMP};
		u32 unpack = VIF1_UNPACK4X32(2, 0);
		u64 unpack_giftag[2] = {
			GIF_SET_TAG(128, 1, GIF_PRE_ENABLE, GS_SET_PRIM(GS_PRIM_POINT, 0, 1, 0, 0, 0, 0, 0, 0), GIF_FLG_PACKED, 3),
			GIF_REG_ST | (GIF_REG_RGBAQ << 4) | (GIF_REG_XYZ2 << 8),
		};
		u32 S = 0;
		u32 T = 0;
		u32 Q = 0;
		u32 vu_pad = 0;
		u32 cmds[2] = {
			VIF1_MSCAL(0),
			VIF1_FLUSHA,
		};
		u32 pad1 = VIF1_NOP;
	} __attribute__((packed, aligned(16))) packet;

	uncached(packet).gs_tex0[0] = GS_SET_TEX0(texAddr / 64, 16, GS_PSM_24, 10, 0, 0, 1, 0, 0, 0, 0, 0);
	uncached(packet).S = S;
	uncached(packet).T = T;
	uncached(packet).Q = Q;
	compiler_memory_reorder_barrier();
	dma_send_vif1((u128*)&packet, (u128*)(&packet + 1));
}

void scan_q(u32 Q)
{
	u32 exp = Q >> 23;
	u32 ST = (exp - 14) << 23;
	StartPointScan(ST, ST, Q);
}

void download_region(u32 address, u32 bw, u32 format, u32* output, u32 width, u32 height)
{
	static struct {
		u32 header[4] = {
			VIF1_NOP,
			VIF1_MSKPATH3(0x8000),
			VIF1_FLUSHA,
			VIF1_DIRECT(6),
		};
		u64 giftag[2] = {GIF_SET_TAG(5, 1, GIF_PRE_DISABLE, 0, GIF_FLG_PACKED, 1), GIF_REG_AD};
		u64 bitbltbuf[2] = {0, GS_REG_BITBLTBUF};
		u64 trxpos[2] = {GS_SET_TRXPOS(0, 0, 0, 0, 0), GS_REG_TRXPOS};
		u64 trxreg[2] = {0, GS_REG_TRXREG};
		u64 finish[2] = {GS_SET_FINISH(0), GS_REG_FINISH};
		u64 trxdir[2] = {GS_SET_TRXDIR(1), GS_REG_TRXDIR};
	} __attribute__((packed, aligned(16))) packet;

	uncached(packet).bitbltbuf[0] = GS_SET_BITBLTBUF(address / 64, bw, format, 0, 0, GS_PSM_32);
	uncached(packet).trxreg[0] = GS_SET_TRXREG(width, height);
	compiler_memory_reorder_barrier();
	while (*D1_CHCR & 0x0100) // Wait for previous VIF1 DMA to complete
		;
	u32 prev_imr = GsPutIMR(GsGetIMR() | 0x0200);
	*D1_QWC = sizeof(packet) / 16;
	*D1_MADR = (u32)&packet;
	*D1_CHCR = 0x101;

	while (*D1_CHCR & 0x0100) // Wait for DMA to complete
		;
	while (!(*GS_CSR & CSR_FINISH)) // Wait for GS to complete
		;
	while (*VIF1_STAT & (0x1f000000)) // Wait for VIF1 fifo to empty
		;
	*VIF1_STAT = VIF1_STAT_FDR;
	*GS_BUSDIR = (u64)1;

	*D1_QWC = (sizeof(*output) * width * height + 15) / 16;
	*D1_MADR = (u32)output;
	*D1_CHCR = 0x100;

	while (*D1_CHCR & 0x0100) // Wait for DMA to complete
		;

	*VIF1_STAT = 0;
	*GS_BUSDIR = (u64)0;

	// Put back prev imr and set finish event
	GsPutIMR(prev_imr);
	*GS_CSR = CSR_FINISH;

	// Enable path3 again
	*VIF1_FIFO = MAKE_U128(VIF1_MSKPATH3(0), VIF1_NOP, VIF1_NOP, VIF1_NOP);

	return;
}
