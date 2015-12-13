#ifndef __NVKM_CLK_GF100_H__
#define __NVKM_CLK_GF100_H__
#include "priv.h"

struct gf100_clk_info {
	u32 freq;
	u32 ssel;
	u32 dsrc;
	u32 ddiv;
	u32 coef;
	u32 mdiv;
	u32 mscoef;
};

int gf100_mpll_info(struct nvkm_clk *base, u32 khz,
		struct gf100_clk_info *info);

#endif /* __NVKM_CLK_GF100_H__ */
