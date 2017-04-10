/*
 * Copyright 2013 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Ben Skeggs
 */
#define gf100_ram(p) container_of((p), struct gf100_ram, base)
#include "ram.h"
#include "ramfuc.h"

#include <core/option.h>
#include <subdev/bios.h>
#include <subdev/bios/pll.h>
#include <subdev/bios/M0205.h>
#include <subdev/bios/rammap.h>
#include <subdev/bios/timing.h>
#include <subdev/bios/perf.h>
#include <subdev/clk.h>
#include <subdev/clk/pll.h>
#include <subdev/clk/gf100.h>
#include <subdev/ltc.h>
#include <subdev/gpio.h>

struct gf100_ramfuc {
	struct ramfuc base;

	struct ramfuc_reg r_gpio[4];

	struct ramfuc_reg r_0x10fe20;
	struct ramfuc_reg r_0x10fe24;
	struct ramfuc_reg r_0x137320;
	struct ramfuc_reg r_0x137330;

	struct ramfuc_reg r_0x132000;
	struct ramfuc_reg r_0x132004;
	struct ramfuc_reg r_0x132100;

	struct ramfuc_reg r_0x137390;

	struct ramfuc_reg r_0x10f290[5];

	struct ramfuc_reg r_mr[9];

	struct ramfuc_reg r_0x10f910;
	struct ramfuc_reg r_0x10f914;

	struct ramfuc_reg r_0x100b0c;
	struct ramfuc_reg r_0x10f050;
	struct ramfuc_reg r_0x10f090;
	struct ramfuc_reg r_0x10f200;
	struct ramfuc_reg r_0x10f210;
	struct ramfuc_reg r_0x10f310;
	struct ramfuc_reg r_0x10f314;
	struct ramfuc_reg r_0x10f604;
	struct ramfuc_reg r_0x10f610;
	struct ramfuc_reg r_0x10f614;
	struct ramfuc_reg r_0x10f628;
	struct ramfuc_reg r_0x10f62c;
	struct ramfuc_reg r_0x10f630;
	struct ramfuc_reg r_0x10f634;
	struct ramfuc_reg r_0x10f640;
	struct ramfuc_reg r_0x10f644;
	struct ramfuc_reg r_0x10f760;
	struct ramfuc_reg r_0x10f764;
	struct ramfuc_reg r_0x10f768;
	struct ramfuc_reg r_0x10f800;
	struct ramfuc_reg r_0x10f808;
	struct ramfuc_reg r_0x10f824;
	struct ramfuc_reg r_0x10f830;
	struct ramfuc_reg r_0x10f870;
	struct ramfuc_reg r_0x10f988;
	struct ramfuc_reg r_0x10f98c;
	struct ramfuc_reg r_0x10f990;
	struct ramfuc_reg r_0x10f998;
	struct ramfuc_reg r_0x10f9b0;
	struct ramfuc_reg r_0x10f9b4;
	struct ramfuc_reg r_0x10fb04;
	struct ramfuc_reg r_0x10fb08;
	struct ramfuc_reg r_0x10fc20[8];
	struct ramfuc_reg r_0x137300;
	struct ramfuc_reg r_0x137310;
	struct ramfuc_reg r_0x137360;
	struct ramfuc_reg r_0x1373ec;
	struct ramfuc_reg r_0x1373f0;
	struct ramfuc_reg r_0x1373f8;

	struct ramfuc_reg r_0x61c140;
	struct ramfuc_reg r_0x611200;

	struct ramfuc_reg r_0x13d8f4;
};

struct gf100_ram {
	struct nvkm_ram base;
	struct gf100_ramfuc fuc;

	struct list_head cfg;
	struct nvbios_ramcfg diff;

	struct nvbios_pll refpll;
	struct nvbios_pll mempll;
};

#define T(t) cfg->timing_10_##t
static int
gf100_ram_timing_calc(struct gf100_ram *ram, u32 *timing)
{
	struct nvbios_ramcfg *cfg = &ram->base.target.bios;
	struct nvkm_subdev *subdev = &ram->base.fb->subdev;
	struct nvkm_device *device = subdev->device;
	u32 cur1, cur2, cur4;

	cur1 = nvkm_rd32(device, 0x10f294);
	cur2 = nvkm_rd32(device, 0x10f298);
	cur4 = nvkm_rd32(device, 0x10f2a0);

	/* XXX: (G)DDR3? */
	switch ((!T(CWL)) * ram->base.type) {
	case NVKM_RAM_TYPE_GDDR5:
		T(CWL) = (cur1 & 0x00000380) >> 7;
		break;
	}

	timing[0] = (T(RP) << 24 | T(RAS) << 17 | T(RFC) << 8 | T(RC));
	timing[1] = (cur1 & ~0x03ffc07f) |
		    (T(RCDWR) << 20) |
		    (T(RCDRD) << 14) |
		    (T(CWL) << 7) |
		    (T(CL));
	/* XXX: lower 8 bytes are two bits indicating "feature(s) X" */
	timing[2] = (cur2 & ~0x00ffffff) |
		    (T(WR) << 16) |
		    (T(WTR) << 8);
	timing[3] = (T(FAW)) << 9 |
		    (T(CKE)) << 5 |
		    (T(XPDLL));
	timing[4] = (cur4 & ~0x001f8000) |
		    (T(RRD) << 15);

	nvkm_debug(subdev, "Entry: 290: %08x %08x %08x %08x\n",
		   timing[0], timing[1], timing[2], timing[3]);
	nvkm_debug(subdev, "  2a0: %08x\n",
		   timing[4]);
	return 0;
}
#undef T

static void
gf100_ram_train(struct gf100_ramfuc *fuc, u32 magic)
{
	struct gf100_ram *ram = container_of(fuc, typeof(*ram), fuc);
	struct nvkm_fb *fb = ram->base.fb;
	struct nvkm_device *device = fb->subdev.device;
	u32 part = nvkm_rd32(device, 0x022438), i;
	u32 mask = nvkm_rd32(device, 0x022554);
	u32 addr = 0x110974;

	ram_wr32(fuc, 0x10f910, magic);
	ram_wr32(fuc, 0x10f914, magic);

	for (i = 0; (magic & 0x80000000) && i < part; addr += 0x1000, i++) {
		if (mask & (1 << i))
			continue;
		ram_wait(fuc, addr, 0x0000000f, 0x00000000, 500000);
	}
}

static void
gf100_ram_gpio(struct gf100_ramfuc *fuc, u8 tag, u32 val)
{
	struct nvkm_gpio *gpio = fuc->base.fb->subdev.device->gpio;
	struct dcb_gpio_func func;
	u32 reg, sh, gpio_val;
	int ret;

	if (nvkm_gpio_get(gpio, 0, tag, DCB_GPIO_UNUSED) != val) {
		ret = nvkm_gpio_find(gpio, 0, tag, DCB_GPIO_UNUSED, &func);
		if (ret)
			return;

		reg = func.line >> 3;
		sh = (func.line & 0x7) << 2;
		gpio_val = ram_rd32(fuc, gpio[reg]);
		if (gpio_val & (8 << sh))
			val = !val;
		if (!(func.log[1] & 1))
			val = !val;

		ram_mask(fuc, gpio[reg], (0x3 << sh), ((val | 0x2) << sh));
		ram_nsec(fuc, 20000);
	}
}

static void gf100_ram_unk13d8f4(struct gf100_ramfuc *fuc)
{
	ram_nuke(fuc, 0x13d8f4);
	ram_wr32(fuc, 0x13d8f4, 0x00000000);
}

static void
gf100_ram_mr(struct gf100_ramfuc *fuc, u32 idx, u32 val)
{
	if (ram_rd32(fuc, mr[idx]) != val) {
		ram_wr32(fuc, mr[idx], val);
		ram_nsec(fuc, 1000);
	}
}

int
gf100_ram_calc(struct nvkm_ram *base, u32 freq)
{
	struct gf100_ram *ram = gf100_ram(base);
	struct gf100_ramfuc *fuc = &ram->fuc;
	struct nvkm_subdev *subdev = &ram->base.fb->subdev;
	struct nvkm_device *device = subdev->device;
	struct nvkm_clk *clk = device->clk;
	struct nvkm_bios *bios = device->bios;
	struct nvkm_gpio *gpio = device->gpio;
	struct nvkm_ram_data *next;
	struct gf100_clk_info mclk;
	u8  ver, hdr, cnt, len, strap;
	u32 data;
	u32 timing[5];
	u16 script;
	int ref;
	int from;
	int i, ret;
	u32 unk604, unk610, unk614, unk808;

	next = &ram->base.target;
	next->freq = freq;
	ram->base.next = next;

	/* lookup memory config data relevant to the target frequency */
	data = nvbios_rammapEm(bios, freq / 1000, &ver, &hdr,
				      &cnt, &len, &next->bios);
	if (!data || ver != 0x10 || len < 0x0e) {
		nvkm_error(subdev, "invalid/missing rammap entry\n");
		return -EINVAL;
	}

	/* locate specific data set for the attached memory */
	strap = nvbios_ramcfg_index(subdev);
	if (strap >= cnt) {
		nvkm_error(subdev, "invalid ramcfg strap\n");
		return -EINVAL;
	}

	data = nvbios_rammapSp(bios, data, ver, hdr, cnt, len, strap,
				       &ver, &hdr, &next->bios);
	if (!data || ver != 0x10 || hdr < 0x0e) {
		nvkm_error(subdev, "invalid/missing ramcfg entry\n");
		return -EINVAL;
	}

	/* lookup memory timings, if bios says they're present */
	strap = nvbios_rd08(bios, data + 0x01);
	if (next->bios.ramcfg_timing != 0xff) {
		data = nvbios_timingEp(bios, next->bios.ramcfg_timing,
				       &ver, &hdr, &cnt, &len,
				       &next->bios);
		if (!data || ver != 0x10 || hdr < 0x17) {
			nvkm_error(subdev, "invalid/missing timing entry\n");
			return -EINVAL;
		}
	}

	ret = gf100_mpll_info(device->clk, freq, &mclk);
	if (ret < 0) {
		nvkm_error(subdev, "failed mclk calculation\n");
		return ret;
	}
	if (ram->diff.ramcfg_FBVDDQ)
		mclk.mdiv |= !(next->bios.rammap_10_04_08) << 27;

	gf100_ram_timing_calc(ram, timing);

	if (!mclk.coef) /* XXX */
		timing[2] |= 0x11;

	ret = ram_init(fuc, ram->base.fb);
	if (ret)
		return ret;

	/* Determine ram-specific MR values */
	for (i = 0; i < 9; i++)
		ram->base.mr[i] = ram_rd32(fuc, mr[i]);

	switch (ram->base.type) {
	case NVKM_RAM_TYPE_GDDR5:
		ret = nvkm_gddr5_calc(&ram->base, false);
		break;
	default:
		ret = -ENOSYS;
		break;
	}

	/* determine current mclk configuration */
	from = !!(ram_rd32(fuc, 0x1373f0) & 0x00000002); /*XXX: ok? */

	/* determine target mclk configuration */
	if (!(ram_rd32(fuc, 0x137300) & 0x00000100))
		ref = nvkm_clk_read(clk, nv_clk_src_sppll0);
	else
		ref = nvkm_clk_read(clk, nv_clk_src_sppll1);

	ram_mask(fuc, 0x137360, 0x00000002, 0x00000000);

	/* XXX: Powercycle PLL - Conditions?
	if ((ram_rd32(fuc, 0x132000) & 0x00000002) || 0) {
		ram_nuke(fuc, 0x132000);
		ram_mask(fuc, 0x132000, 0x00000002, 0x00000002);
		ram_mask(fuc, 0x132000, 0x00000002, 0x00000000);
	}
	*/

	if (mclk.coef) {
		ram_nuke(fuc, 0x10fe20);
		ram_mask(fuc, 0x10fe20, 0x00000002, 0x00000002);
		ram_mask(fuc, 0x10fe20, 0x00000002, 0x00000000);
	}

	ram_wr32(fuc, 0x132100, 0x00000001);

	if (!mclk.coef) {
		ram_mask(fuc, 0x137300, 0x00030103, 0x00000103); /* div-mode */
		if (ram->diff.ramcfg_FBVDDQ && !next->bios.ramcfg_FBVDDQ)
			ram_mask(fuc, 0x137310, 0x08000000, 0x08000000);
		ram_wr32(fuc, 0x10f988, 0x20010000);
		ram_wr32(fuc, 0x10f98c, 0x00000000);
		ram_wr32(fuc, 0x10f990, 0x20012001);
		ram_wr32(fuc, 0x10f998, 0x00010a00);
	}

	/* -------------- PLLs preconfigured, now real PLLs */
	ram_wr32(fuc, 0x100b0c, 0x00080012);

	ram_wait_vblank(fuc);
	ram_wr32(fuc, 0x611200, 0x00003300);
	ram_block(fuc);

	ram_mask(fuc, mr[1], 0x0000000c, next->bios.timing_10_ODT << 2);
	if (next->bios.timing_10_ODT == 2) {
		ram_mask(fuc, 0x10f830, 0x00200000, 0x00000000);
		gf100_ram_gpio(fuc, 0x2e, 1);
	}

	ram_mask(fuc, 0x10f200, 0x00000800, 0x00000000);
	if (next->bios.ramcfg_10_02_10) {
		ram_mask(fuc, 0x10f824, 0x00000600, 0x00000000);
	} else {
		ram_mask(fuc, 0x10f808, 0x40000000, 0x40000000);
		ram_mask(fuc, 0x10f824, 0x00000180, 0x00000000);
	}
	ram_wr32(fuc, 0x10f210, 0x00000000);
	ram_nsec(fuc, 1000);
	if (from && !mclk.coef)
		gf100_ram_train(fuc, 0x000c1001);
	ram_wr32(fuc, 0x10f310, 0x00000001);
	ram_nsec(fuc, 1000);
	ram_wr32(fuc, 0x10f090, 0x00000061);
	ram_wr32(fuc, 0x10f090, 0xc000007f);
	ram_nsec(fuc, 1000);

	gf100_ram_unk13d8f4(fuc);
	ram_mask(fuc, 0x1373ec, 0x00020000, !(next->bios.ramcfg_10_02_10) << 17);

	if (nvkm_gpio_get(gpio, 0, 0x18, DCB_GPIO_UNUSED) ==
				next->bios.ramcfg_FBVDDQ) {
		gf100_ram_gpio(fuc, 0x18, !next->bios.ramcfg_FBVDDQ);
		ram_nsec(fuc, 64000);
	}

	if (!mclk.coef) {
		/* XXX: 10f200, 10f808 unconfirmed PLL related */
		ram_mask(fuc, 0x10f808, 0x00080000, 0x00000000);
		ram_mask(fuc, 0x10f200, 0x00008000, 0x00008000);
		/* XXX: 10f830[24] trigger, clear conditions */
		/* Verify masks */
		ram_mask(fuc, 0x10f830, 0x41000000, 0x41000000);
		ram_mask(fuc, 0x10f830, 0x01000000, 0x00000000);
		gf100_ram_unk13d8f4(fuc);
		ram_mask(fuc, 0x132100, 0x00000100, 0x00000100);
		ram_mask(fuc, 0x137310, 0x80003f00, mclk.mdiv);

		ram_nuke(fuc, 0x10f050);
		ram_wr32(fuc, 0x10f050, 0xff000490);
		ram_mask(fuc, 0x1373ec, 0x00000f0f, next->bios.ramcfg_10_0b_0f * 0x101);
		/* XXX: : switch to PLL? Are you mental?
		 * ram_wr32(fuc, 0x1373f0, 0x3);
		 */
		ram_mask(fuc, 0x137310, 0x08000000, 0x00000000);
		ram_mask(fuc, 0x132100, 0x00000100, 0x00000000);

		/* XXX: 10f830 */
		ram_mask(fuc, 0x10f830, 0x40400007, 0x00000007);
		gf100_ram_unk13d8f4(fuc);
		ram_mask(fuc, 0x1373f0, 0x00000002, 0x00000000);
		/* 10f824 */
		ram_mask(fuc, 0x10f824, 0x00000023, 0x00000023);
		gf100_ram_unk13d8f4(fuc);

		/* XXX: PLL unknown feature X off, only when from PLL? */
		ram_mask(fuc, 0x132000, 0x00000100, 0x00000000);
	}

	/* XXX: 10f090: is this a mask? */
	ram_wr32(fuc, 0x10f090, 0x4000007e);
	ram_nsec(fuc, 2000);
	ram_wr32(fuc, 0x10f314, 0x00000001); /* PRECHARGE */
	ram_wr32(fuc, 0x10f210, 0x80000000); /* REFRESH_AUTO = 1 */

	/* --------------------- MR, Timing ----------------------------*/
	gf100_ram_mr(fuc, 3, ram->base.mr[3]);
	gf100_ram_mr(fuc, 0, ram->base.mr[0]);

	for (i = 0; i < 5; i++)
		ram_wr32(fuc, 0x10f290[i], timing[i]);

	ram_mask(fuc, 0x10f200, 0x00001000, (!next->bios.ramcfg_10_02_08) << 12);

	/* XXX: A lot of "chipset"/"ram"type" specific stuff...? */
	unk604  = ram_rd32(fuc, 0x10f604) & ~0xf1000300;
	unk610  = ram_rd32(fuc, 0x10f610) & ~0x40000100;
	unk614  = ram_rd32(fuc, 0x10f614) & ~0x40000100;
	unk808  = ram_rd32(fuc, 0x10f808) & ~0x32a00000;

	if ( next->bios.ramcfg_10_02_20)
		unk604 |= 0xf0000000;

	if (!next->bios.ramcfg_10_02_04) {
		unk604 |= 0x01000000;
		unk808 |= 0x32a00000;
	}

	if ( next->bios.ramcfg_10_02_02)
		unk610 |= 0x00000100;
	if ( next->bios.ramcfg_10_02_01)
		unk614 |= 0x00000100;

	if ( next->bios.timing_10_18 != 0xff) {
		unk610 &= ~0xf0000000;
		unk610 |= next->bios.timing_10_18 << 28;

		unk614 &= ~0xf0000000;
		unk614 |= next->bios.timing_10_18 << 28;
	}

	if ( next->bios.ramcfg_FBVDDQ) {
		unk610 |= 0x40000000;
		unk614 |= 0x40000000;
	} else {
		unk604 |= 0x00000300;
	}

	ram_mask(fuc, 0x10f604, 0xffffffff, unk604);
	ram_mask(fuc, 0x10f614, 0xffffffff, unk614);
	ram_mask(fuc, 0x10f610, 0xffffffff, unk610);
	ram_mask(fuc, 0x10f808, 0xffffffff, unk808);

	data = next->bios.ramcfg_10_02_01 * 0x55;
	if ((ram_rd32(fuc, 0x1373f8) & 0xff) != data) {
		gf100_ram_unk13d8f4(fuc);
		ram_mask(fuc, 0x1373f8, 0x000000ff, data);
	}

	/* XXX: 10f870 default value (when unset in BIOS?) Training val? */
	ram_wr32(fuc, 0x10f870, next->bios.ramcfg_10_0d_0f * 0x11111111);

	/* XXX: all MRs?  */
	gf100_ram_mr(fuc, 1, ram->base.mr[1]);
	gf100_ram_mr(fuc, 5, ram->base.mr[5] & ~0x004);
	gf100_ram_mr(fuc, 6, ram->base.mr[6]);
	gf100_ram_mr(fuc, 7, ram->base.mr[7]);
	gf100_ram_unk13d8f4(fuc);

	if (device->chipset == 0xce) { /* XXX: Criterion? */
		ram_wr32(fuc, 0x10f760, 0xff00ff00);
		ram_wr32(fuc, 0x10f764, 0x0000ff00);
		ram_wr32(fuc, 0x10f768, 0xff00ff00);
	}

	if (!next->bios.rammap_10_04_40) {
		ram_wr32(fuc, 0x10f644, next->bios.ramcfg_10_07_0f * 0x11111111);
		ram_wr32(fuc, 0x10f640, next->bios.ramcfg_10_07_f0 * 0x11111111);
	} else {
		/* XXX: double-check this condition */
		script = nvbios_perf_script_unk1c(bios);
		if (script)
			ram_init_run(fuc, bios, script, strap);

		gf100_ram_train(fuc, 0x800e1008);
		ram_nsec(fuc, 1000);
	}

	if (next->bios.timing_10_ODT != 2) {
		gf100_ram_unk13d8f4(fuc);
		gf100_ram_gpio(fuc, 0x2e, 0);
		ram_mask(fuc, 0x10f830, 0x00200000, 0x00200000);
	}
	if (next->bios.rammap_10_0d_01)
		ram_wr32(fuc, 0x10f628, next->bios.ramcfg_10_05_f0 * 0x11111111);

	if (!next->bios.ramcfg_FBVDDQ) { /* XXX: condition more complex... */
		ram_wr32(fuc, 0x10f62c, next->bios.ramcfg_10_05_0f * 0x11111111);
	}

	if (mclk.coef) {
		ram_wr32(fuc, 0x10f200, 0x00ce0000);
		ram_wr32(fuc, 0x61c140, 0x09a40000);

 		ram_nsec(fuc, 1000);
		ram_wr32(fuc, 0x10f800, 0x00001804);
		gf100_ram_unk13d8f4(fuc);

	}

	gf100_ram_train(fuc, 0x80021001);

	if (next->bios.rammap_10_0d_02) {
		ram_nsec(fuc, 1000);
		ram_wr32(fuc, 0x10f630, next->bios.ramcfg_10_06_f0 * 0x11111111);
		ram_wr32(fuc, 0x10f634, next->bios.ramcfg_10_06_0f * 0x11111111);

		for (i = 0; i < 8; i++)
			ram_wr32(fuc, 0x10fc20[i], next->bios.ramcfg_10_0a_0f);

		gf100_ram_unk13d8f4(fuc);
		ram_nsec(fuc, 1000);
	} else {
		gf100_ram_train(fuc, 0x80081001);
	}

	gf100_ram_mr(fuc, 5, ram->base.mr[5]);
	ram_nsec(fuc, 1000);

	/* XXX: Don't do this reset in the highest perflvl, why? */
	ram_mask(fuc, 0x10f830, 0x01000000, 0x01000000);
	ram_mask(fuc, 0x10f830, 0x01000000, 0x00000000);

	ram_unblock(fuc);
	gf100_ram_unk13d8f4(fuc);

	ram_wr32(fuc, 0x100b0c, 0x00080028);
	ram_wr32(fuc, 0x611200, 0x3330);

	if (mclk.coef) {
		ram_nsec(fuc, 100000);
		ram_wr32(fuc, 0x10f9b0, 0x05313f41);
		ram_wr32(fuc, 0x10f9b4, 0x00002f50);
 		gf100_ram_train(fuc, 0x010c1001);
 	}

	ram_mask(fuc, 0x10f200, 0x00000800, 0x00000800);
	if (next->bios.ramcfg_10_02_10) {
		ram_mask(fuc, 0x10f824, 0x00000180, 0x00000180);
		ram_mask(fuc, 0x10f808, 0x40000000, 0x00000000);
	} else {
		ram_mask(fuc, 0x10f824, 0x00000600, 0x00000600);
	}

	if (!mclk.coef)
		ram_mask(fuc, 0x132000, 0x00000001, 0x00000000);

	return 0;
}

int
gf100_ram_prog(struct nvkm_ram *base)
{
	struct gf100_ram *ram = gf100_ram(base);
	struct nvkm_device *device = ram->base.fb->subdev.device;
	ram_exec(&ram->fuc, nvkm_boolopt(device->cfgopt, "NvMemExec", true));
	return 0;
}

void
gf100_ram_tidy(struct nvkm_ram *base)
{
	struct gf100_ram *ram = gf100_ram(base);
	ram_exec(&ram->fuc, false);
}

void
gf100_ram_put(struct nvkm_ram *ram, struct nvkm_mem **pmem)
{
	struct nvkm_ltc *ltc = ram->fb->subdev.device->ltc;
	struct nvkm_mem *mem = *pmem;

	*pmem = NULL;
	if (unlikely(mem == NULL))
		return;

	mutex_lock(&ram->fb->subdev.mutex);
	if (mem->tag)
		nvkm_ltc_tags_free(ltc, &mem->tag);
	__nv50_ram_put(ram, mem);
	mutex_unlock(&ram->fb->subdev.mutex);

	kfree(mem);
}

int
gf100_ram_get(struct nvkm_ram *ram, u64 size, u32 align, u32 ncmin,
	      u32 memtype, struct nvkm_mem **pmem)
{
	struct nvkm_ltc *ltc = ram->fb->subdev.device->ltc;
	struct nvkm_mm *mm = &ram->vram;
	struct nvkm_mm_node **node, *r;
	struct nvkm_mem *mem;
	int type = (memtype & 0x0ff);
	int back = (memtype & 0x800);
	const bool comp = gf100_pte_storage_type_map[type] != type;
	int ret;

	size  >>= NVKM_RAM_MM_SHIFT;
	align >>= NVKM_RAM_MM_SHIFT;
	ncmin >>= NVKM_RAM_MM_SHIFT;
	if (!ncmin)
		ncmin = size;

	mem = kzalloc(sizeof(*mem), GFP_KERNEL);
	if (!mem)
		return -ENOMEM;

	mem->size = size;

	mutex_lock(&ram->fb->subdev.mutex);
	if (comp) {
		/* compression only works with lpages */
		if (align == (1 << (17 - NVKM_RAM_MM_SHIFT))) {
			int n = size >> 5;
			nvkm_ltc_tags_alloc(ltc, n, &mem->tag);
		}

		if (unlikely(!mem->tag))
			type = gf100_pte_storage_type_map[type];
	}
	mem->memtype = type;

	node = &mem->mem;
	do {
		if (back)
			ret = nvkm_mm_tail(mm, 0, 1, size, ncmin, align, &r);
		else
			ret = nvkm_mm_head(mm, 0, 1, size, ncmin, align, &r);
		if (ret) {
			mutex_unlock(&ram->fb->subdev.mutex);
			ram->func->put(ram, &mem);
			return ret;
		}

		*node = r;
		node = &r->next;
		size -= r->length;
	} while (size);
	mutex_unlock(&ram->fb->subdev.mutex);

	mem->offset = (u64)mem->mem->offset << NVKM_RAM_MM_SHIFT;
	*pmem = mem;
	return 0;
}

int
gf100_ram_train_init_0(struct nvkm_ram *ram, struct gt215_ram_train *train)
{
	struct nvkm_subdev *subdev = &ram->fb->subdev;
	struct nvkm_device *device = subdev->device;
	int i, j;

	static const u8  train0[] = {
		0x00, 0xff, 0x55, 0xaa, 0x33, 0xcc,
		0x00, 0xff, 0xff, 0x00, 0xff, 0x00,
	};

	static const u32 train1[] = {
		0x00000000, 0xffffffff,
		0x55555555, 0xaaaaaaaa,
		0x33333333, 0xcccccccc,
		0xf0f0f0f0, 0x0f0f0f0f,
		0x00ff00ff, 0xff00ff00,
		0x0000ffff, 0xffff0000,
	};

	if ((train->mask & 0x03c3) == 0x03c3) {
		for (i = 0; i < 0x30; i++) {
			for (j = 0; j < 8; j += 4) {
				nvkm_wr32(device, 0x10f968 + j, (i << 8));
				nvkm_wr32(device, 0x10f920 + j, 0x00000000 |
						train->type08.data[i] << 4 |
						train->type06.data[i]);
				nvkm_wr32(device, 0x10f918 + j,
						train->type00.data[i]);
				nvkm_wr32(device, 0x10f920 + j, 0x00000100 |
						train->type09.data[i] << 4 |
						train->type07.data[i]);
				nvkm_wr32(device, 0x10f918 + j,
						train->type01.data[i]);
			}
		}
	} else {
		nvkm_debug(subdev,
			"missing link training data, using defaults\n");

		for (i = 0; i < 0x30; i++) {
			for (j = 0; j < 8; j += 4) {
				nvkm_wr32(device, 0x10f968 + j, (i << 8));
				nvkm_wr32(device, 0x10f920 + j, 0x00000100 |
								train0[i % 12]);
				nvkm_wr32(device, 0x10f918 + j, train1[i % 12]);
				nvkm_wr32(device, 0x10f920 + j, 0x00000000 |
								train0[i % 12]);
				nvkm_wr32(device, 0x10f918 + j, train1[i % 12]);
			}
		}
	}

	if (train->mask & 0x10) {
		for (j = 0; j < 8; j += 4) {
			for (i = 0; i < 0x100; i++) {
				nvkm_wr32(device, 0x10f968 + j, i);
				nvkm_wr32(device, 0x10f900 + j,
						train->type04.data[i]);
			}
		}
	}

	return 0;
}

int
gf100_ram_train_init(struct nvkm_ram *ram)
{
	u8 ramcfg = nvbios_ramcfg_index(&ram->fb->subdev);
	struct gt215_ram_train *train;
	int ret, i;

	if (!(train = kzalloc(sizeof(*train), GFP_KERNEL)))
		return -ENOMEM;

	for (i = 0; i < 0x100; i++) {
		ret = gt215_ram_train_type(ram, i, ramcfg, train);
		if (ret && ret != -ENOENT)
			break;
	}

	switch (ram->type) {
 	case NVKM_RAM_TYPE_GDDR5:
		ret = gf100_ram_train_init_0(ram, train);
 		break;
 	default:
		ret = 0;
		break;
 	}

	kfree(train);
	return ret;
}

int
gf100_ram_init(struct nvkm_ram *base)
{
	struct nvkm_subdev *subdev = &base->fb->subdev;
	struct nvkm_device *device = subdev->device;

	/* XXX Why does the blob do this? */
	nvkm_mask(device, 0x137360, 0x00000002, 0x00000000);

	return gf100_ram_train_init(base);
}

u32
gf100_ram_probe_fbpa_amount(struct nvkm_device *device, int fbpa)
{
	return nvkm_rd32(device, 0x11020c + (fbpa * 0x1000));
}

u32
gf100_ram_probe_fbp_amount(const struct nvkm_ram_func *func, u32 fbpao,
			   struct nvkm_device *device, int fbp, int *pltcs)
{
	if (!(fbpao & BIT(fbp))) {
		*pltcs = 1;
		return func->probe_fbpa_amount(device, fbp);
	}
	return 0;
}

u32
gf100_ram_probe_fbp(const struct nvkm_ram_func *func,
		    struct nvkm_device *device, int fbp, int *pltcs)
{
	u32 fbpao = nvkm_rd32(device, 0x022554);
	return func->probe_fbp_amount(func, fbpao, device, fbp, pltcs);
}

int
gf100_ram_ctor(const struct nvkm_ram_func *func, struct nvkm_fb *fb,
	       struct nvkm_ram *ram)
{
	struct nvkm_subdev *subdev = &fb->subdev;
	struct nvkm_device *device = subdev->device;
	struct nvkm_bios *bios = device->bios;
	const u32 rsvd_head = ( 256 * 1024); /* vga memory */
	const u32 rsvd_tail = (1024 * 1024); /* vbios etc */
	enum nvkm_ram_type type = nvkm_fb_bios_memtype(bios);
	u32 fbps = nvkm_rd32(device, 0x022438);
	u64 total = 0, lcomm = ~0, lower, ubase, usize;
	int ret, fbp, ltcs, ltcn = 0;

	nvkm_debug(subdev, "%d FBP(s)\n", fbps);
	for (fbp = 0; fbp < fbps; fbp++) {
		u32 size = func->probe_fbp(func, device, fbp, &ltcs);
		if (size) {
			nvkm_debug(subdev, "FBP %d: %4d MiB, %d LTC(s)\n",
				   fbp, size, ltcs);
			lcomm  = min(lcomm, (u64)(size / ltcs) << 20);
			total += size << 20;
			ltcn  += ltcs;
		} else {
			nvkm_debug(subdev, "FBP %d: disabled\n", fbp);
		}
	}

	lower = lcomm * ltcn;
	ubase = lcomm + func->upper;
	usize = total - lower;

	nvkm_debug(subdev, "Lower: %4lld MiB @ %010llx\n", lower >> 20, 0ULL);
	nvkm_debug(subdev, "Upper: %4lld MiB @ %010llx\n", usize >> 20, ubase);
	nvkm_debug(subdev, "Total: %4lld MiB\n", total >> 20);

	ret = nvkm_ram_ctor(func, fb, type, total, 0, ram);
	if (ret)
		return ret;

	nvkm_mm_fini(&ram->vram);

	/* Some GPUs are in what's known as a "mixed memory" configuration.
	 *
	 * This is either where some FBPs have more memory than the others,
	 * or where LTCs have been disabled on a FBP.
	 */
	if (lower != total) {
		/* The common memory amount is addressed normally. */
		ret = nvkm_mm_init(&ram->vram, rsvd_head >> NVKM_RAM_MM_SHIFT,
				   (lower - rsvd_head) >> NVKM_RAM_MM_SHIFT, 1);
		if (ret)
			return ret;

		/* And the rest is much higher in the physical address
		 * space, and may not be usable for certain operations.
		 */
		ret = nvkm_mm_init(&ram->vram, ubase >> NVKM_RAM_MM_SHIFT,
				   (usize - rsvd_tail) >> NVKM_RAM_MM_SHIFT, 1);
		if (ret)
			return ret;
	} else {
		/* GPUs without mixed-memory are a lot nicer... */
		ret = nvkm_mm_init(&ram->vram, rsvd_head >> NVKM_RAM_MM_SHIFT,
				   (total - rsvd_head - rsvd_tail) >>
				   NVKM_RAM_MM_SHIFT, 1);
		if (ret)
			return ret;
	}

	ram->ranks = (nvkm_rd32(device, 0x10f200) & 0x00000004) ? 2 : 1;
	return 0;
}

static int
gf100_ram_ctor_data(struct gf100_ram *ram, u8 ramcfg, int i)
{
	struct nvkm_bios *bios = ram->base.fb->subdev.device->bios;
	struct nvkm_ram_data *cfg;
	struct nvbios_ramcfg *d = &ram->diff;
	struct nvbios_ramcfg *p, *n;
	u8  ver, hdr, cnt, len;
	u32 data;
	int ret;

	if (!(cfg = kmalloc(sizeof(*cfg), GFP_KERNEL)))
		return -ENOMEM;
	p = &list_last_entry(&ram->cfg, typeof(*cfg), head)->bios;
	n = &cfg->bios;

	/* memory config data for a range of target frequencies */
	data = nvbios_rammapEp(bios, i, &ver, &hdr, &cnt, &len, &cfg->bios);
	if (ret = -ENOENT, !data)
		goto done;
	if (ret = -ENOSYS, ver != 0x10 || hdr < 0x0e)
		goto done;

	/* ... and a portion specific to the attached memory */
	data = nvbios_rammapSp(bios, data, ver, hdr, cnt, len, ramcfg,
			       &ver, &hdr, &cfg->bios);
	if (ret = -EINVAL, !data)
		goto done;
	if (ret = -ENOSYS, ver != 0x10 || hdr < 0x0a)
		goto done;

	/* lookup memory timings, if bios says they're present */
	if (cfg->bios.ramcfg_timing != 0xff) {
		data = nvbios_timingEp(bios, cfg->bios.ramcfg_timing,
				       &ver, &hdr, &cnt, &len,
				       &cfg->bios);
		if (ret = -EINVAL, !data)
			goto done;
		if (ret = -ENOSYS, ver != 0x10 || hdr < 0x10)
			goto done;
	}

	list_add_tail(&cfg->head, &ram->cfg);
	if (ret = 0, i == 0)
		goto done;

	d->ramcfg_FBVDDQ |= p->ramcfg_FBVDDQ != n->ramcfg_FBVDDQ;
done:
	if (ret)
		kfree(cfg);
	return ret;
}

int
gf100_ram_new_(const struct nvkm_ram_func *func,
	       struct nvkm_fb *fb, struct nvkm_ram **pram)
{
	struct nvkm_subdev *subdev = &fb->subdev;
	struct nvkm_bios *bios = subdev->device->bios;
	struct gf100_ram *ram;
	int i, ret;
	u8  ramcfg = nvbios_ramcfg_index(subdev);

	if (!(ram = kzalloc(sizeof(*ram), GFP_KERNEL)))
		return -ENOMEM;
	*pram = &ram->base;

	ret = gf100_ram_ctor(func, fb, &ram->base);
	if (ret)
		return ret;

	/* parse bios data for all rammap table entries up-front, and
	 * build information on whether certain fields differ between
	 * any of the entries.
	 *
	 * the binary driver appears to completely ignore some fields
	 * when all entries contain the same value.  at first, it was
	 * hoped that these were mere optimisations and the bios init
	 * tables had configured as per the values here, but there is
	 * evidence now to suggest that this isn't the case and we do
	 * need to treat this condition as a "don't touch" indicator.
	 */
	INIT_LIST_HEAD(&ram->cfg);

	for (i = 0; !ret; i++) {
		ret = gf100_ram_ctor_data(ram, ramcfg, i);
		if (ret && ret != -ENOENT) {
			nvkm_error(subdev, "failed to parse ramcfg data\n");
			return ret;
		}
	}

	ret = nvbios_pll_parse(bios, 0x0c, &ram->refpll);
	if (ret) {
		nvkm_error(subdev, "mclk refpll data not found\n");
		return ret;
	}

	ret = nvbios_pll_parse(bios, 0x04, &ram->mempll);
	if (ret) {
		nvkm_error(subdev, "mclk pll data not found\n");
		return ret;
	}

	ram->fuc.r_0x10fe20 = ramfuc_reg(0x10fe20);
	ram->fuc.r_0x10fe24 = ramfuc_reg(0x10fe24);
	ram->fuc.r_0x137320 = ramfuc_reg(0x137320);
	ram->fuc.r_0x137330 = ramfuc_reg(0x137330);

	ram->fuc.r_0x132000 = ramfuc_reg(0x132000);
	ram->fuc.r_0x132004 = ramfuc_reg(0x132004);
	ram->fuc.r_0x132100 = ramfuc_reg(0x132100);

	ram->fuc.r_0x137390 = ramfuc_reg(0x137390);

	for (i = 0; i < 5; i++)
		ram->fuc.r_0x10f290[i] = ramfuc_reg(0x10f290 + (i * 4));

	ram->fuc.r_mr[0] = ramfuc_reg(0x10f300);
	for (i = 1; i < 9; i++)
		ram->fuc.r_mr[i] = ramfuc_reg(0x10f330 + ((i - 1) * 4));

	ram->fuc.r_0x10f910 = ramfuc_reg(0x10f910);
	ram->fuc.r_0x10f914 = ramfuc_reg(0x10f914);

	ram->fuc.r_0x100b0c = ramfuc_reg(0x100b0c);
	ram->fuc.r_0x10f050 = ramfuc_reg(0x10f050);
	ram->fuc.r_0x10f090 = ramfuc_reg(0x10f090);
	ram->fuc.r_0x10f200 = ramfuc_reg(0x10f200);
	ram->fuc.r_0x10f210 = ramfuc_reg(0x10f210);
	ram->fuc.r_0x10f310 = ramfuc_reg(0x10f310);
	ram->fuc.r_0x10f314 = ramfuc_reg(0x10f314);
	ram->fuc.r_0x10f604 = ramfuc_reg(0x10f604);
	ram->fuc.r_0x10f610 = ramfuc_reg(0x10f610);
	ram->fuc.r_0x10f614 = ramfuc_reg(0x10f614);
	ram->fuc.r_0x10f628 = ramfuc_reg(0x10f628);
	ram->fuc.r_0x10f62c = ramfuc_reg(0x10f62c);
	ram->fuc.r_0x10f630 = ramfuc_reg(0x10f630);
	ram->fuc.r_0x10f634 = ramfuc_reg(0x10f634);
	/* XXX: mask? */
	for (i = 0; i < 8; i++)
		ram->fuc.r_0x10fc20[i] = ramfuc_reg(0x10fc20 + (i * 0xc));
	ram->fuc.r_0x10f640 = ramfuc_reg(0x10f640);
	ram->fuc.r_0x10f644 = ramfuc_reg(0x10f644);
	ram->fuc.r_0x10f760 = ramfuc_reg(0x10f760);
	ram->fuc.r_0x10f764 = ramfuc_reg(0x10f764);
	ram->fuc.r_0x10f768 = ramfuc_reg(0x10f768);
	ram->fuc.r_0x10f800 = ramfuc_reg(0x10f800);
	ram->fuc.r_0x10f808 = ramfuc_reg(0x10f808);
	ram->fuc.r_0x10f824 = ramfuc_reg(0x10f824);
	ram->fuc.r_0x10f830 = ramfuc_reg(0x10f830);
	ram->fuc.r_0x10f870 = ramfuc_reg(0x10f870);
	ram->fuc.r_0x10f988 = ramfuc_reg(0x10f988);
	ram->fuc.r_0x10f98c = ramfuc_reg(0x10f98c);
	ram->fuc.r_0x10f990 = ramfuc_reg(0x10f990);
	ram->fuc.r_0x10f998 = ramfuc_reg(0x10f998);
	ram->fuc.r_0x10f9b0 = ramfuc_reg(0x10f9b0);
	ram->fuc.r_0x10f9b4 = ramfuc_reg(0x10f9b4);
	ram->fuc.r_0x10fb04 = ramfuc_reg(0x10fb04);
	ram->fuc.r_0x10fb08 = ramfuc_reg(0x10fb08);
	ram->fuc.r_0x137310 = ramfuc_reg(0x137300);
	ram->fuc.r_0x137310 = ramfuc_reg(0x137310);
	ram->fuc.r_0x137360 = ramfuc_reg(0x137360);
	ram->fuc.r_0x1373ec = ramfuc_reg(0x1373ec);
	ram->fuc.r_0x1373f0 = ramfuc_reg(0x1373f0);
	ram->fuc.r_0x1373f8 = ramfuc_reg(0x1373f8);

	ram->fuc.r_0x61c140 = ramfuc_reg(0x61c140);
	ram->fuc.r_0x611200 = ramfuc_reg(0x611200);

	ram->fuc.r_0x13d8f4 = ramfuc_reg(0x13d8f4);

	ram->fuc.r_gpio[0] = ramfuc_reg(0x00e104);
	ram->fuc.r_gpio[1] = ramfuc_reg(0x00e108);
	ram->fuc.r_gpio[2] = ramfuc_reg(0x00e280);
	ram->fuc.r_gpio[3] = ramfuc_reg(0x00e284);
	return 0;
}

static const struct nvkm_ram_func
gf100_ram = {
	.upper = 0x0200000000,
	.probe_fbp = gf100_ram_probe_fbp,
	.probe_fbp_amount = gf100_ram_probe_fbp_amount,
	.probe_fbpa_amount = gf100_ram_probe_fbpa_amount,
	.init = gf100_ram_init,
	.get = gf100_ram_get,
	.put = gf100_ram_put,
	.calc = gf100_ram_calc,
	.prog = gf100_ram_prog,
	.tidy = gf100_ram_tidy,
};

int
gf100_ram_new(struct nvkm_fb *fb, struct nvkm_ram **pram)
{
	return gf100_ram_new_(&gf100_ram, fb, pram);
}
