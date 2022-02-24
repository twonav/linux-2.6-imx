// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * max98390.c  --  MAX98390 ALSA Soc Audio driver
 *
 * Copyright (C) 2020 Maxim Integrated Products
 * Backported from original file https://elixir.bootlin.com/linux/v5.8-rc1/source/sound/soc/codecs/max98390.c
 *
 */

#include <linux/acpi.h>
#include <linux/cdev.h>
#include <linux/dmi.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/delay.h>


#include "max98390.h"

static struct reg_default max98390_reg_defaults[] = {
{0x200b,	0xf0},
{0x200c,	0x00},
{0x200d,	0x00},
{0x200e,	0x00},
{0x200f,	0x00},
{0x2010,	0x00},
{0x2011,	0x01},
{0x2012,	0x6c},
{0x2014,	0x00},
{0x2015,	0x00},
{0x2016,	0x00},
{0x2017,	0x75},
{0x2018,	0x8c},
{0x2019,	0x08},
{0x201a,	0x55},
{0x201b,	0x03},
{0x201c,	0x00},
{0x201d,	0x03},
{0x201e,	0x00},
{0x201f,	0xfc},
{0x2020,	0xff},
{0x2021,	0x10},
{0x2022,	0x10},
{0x2023,	0x00},
{0x2024,	0xc0},
{0x2025,	0x1c},
{0x2026,	0x44},
{0x2027,	0x08},
{0x202c,	0x00},
{0x202d,	0x00},
{0x202e,	0x00},
{0x202f,	0x00},
{0x2030,	0x00},
{0x2031,	0x00},
{0x2032,	0x00},
{0x2033,	0x00},
{0x2039,	0x0f},
{0x203a,	0x81},
{0x203b,	0x00},
{0x203c,	0x00},
{0x203d,	0x05},
{0x203e,	0x85},
{0x203f,	0x03},
{0x2040,	0xf7},
{0x2041,	0x1c},
{0x2042,	0x01},
{0x2043,	0x40},
{0x2044,	0x07},
{0x2045,	0x00},
{0x2046,	0x23},
{0x2047,	0x00},
{0x2048,	0x00},
{0x2049,	0x00},
{0x204a,	0x00},
{0x204b,	0x00},
{0x204c,	0x00},
{0x204d,	0x00},
{0x204e,	0x00},
{0x204f,	0x00},
{0x2050,	0x2c},
{0x2051,	0x00},
{0x2052,	0xff},
{0x2053,	0xff},
{0x2054,	0x00},
{0x2055,	0x00},
{0x2056,	0x00},
{0x2057,	0x00},
{0x2058,	0x00},
{0x2059,	0x00},
{0x205a,	0x00},
{0x205b,	0x00},
{0x205c,	0x00},
{0x205d,	0x00},
{0x205e,	0x00},
{0x205f,	0x1f},
{0x2060,	0x00},
{0x2061,	0x00},
{0x2062,	0x00},
{0x2063,	0x00},
{0x2064,	0x00},
{0x2065,	0x00},
{0x2066,	0x00},
{0x2067,	0x00},
{0x2068,	0x00},
{0x2069,	0x00},
{0x206a,	0x00},
{0x206b,	0x00},
{0x206c,	0x00},
{0x206d,	0x00},
{0x206e,	0x00},
{0x206f,	0x00},
{0x2070,	0x00},
{0x2071,	0x00},
{0x2072,	0x00},
{0x2073,	0x00},
{0x2074,	0x00},
{0x2075,	0x00},
{0x2076,	0x0e},
{0x2077,	0x84},
{0x2078,	0x07},
{0x2079,	0x07},
{0x207a,	0x01},
{0x207b,	0x00},
{0x207c,	0x46},
{0x207d,	0x2b},
{0x207e,	0x08},
{0x207f,	0x00},
{0x2080,	0x03},
{0x2081,	0x03},
{0x2082,	0x07},
{0x2083,	0x00},
{0x2084,	0x01},
{0x2101,	0xd0},
{0x2102,	0x95},
{0x2103,	0x0e},
{0x2105,	0x61},
{0x2106,	0xd4},
{0x2107,	0xe2},
{0x2109,	0xd0},
{0x210a,	0x95},
{0x210b,	0x0e},
{0x210d,	0x75},
{0x210e,	0xf4},
{0x210f,	0xe2},
{0x2111,	0xb4},
{0x2112,	0x4b},
{0x2113,	0x0d},
{0x2115,	0x0a},
{0x2116,	0x10},
{0x2117,	0x00},
{0x2119,	0x15},
{0x211a,	0x20},
{0x211b,	0x00},
{0x211d,	0x0a},
{0x211e,	0x10},
{0x211f,	0x00},
{0x2121,	0x75},
{0x2122,	0xf4},
{0x2123,	0xe2},
{0x2125,	0xb4},
{0x2126,	0x4b},
{0x2127,	0x0d},
{0x2129,	0xca},
{0x212a,	0xc0},
{0x212b,	0x0f},
{0x212d,	0x6b},
{0x212e,	0x7e},
{0x212f,	0xe0},
{0x2131,	0xca},
{0x2132,	0xc0},
{0x2133,	0x0f},
{0x2135,	0x65},
{0x2136,	0x7f},
{0x2137,	0xe0},
{0x2139,	0x8e},
{0x213a,	0x82},
{0x213b,	0x0f},
{0x213d,	0x00},
{0x213e,	0x00},
{0x213f,	0x10},
{0x2141,	0x00},
{0x2142,	0x00},
{0x2143,	0x00},
{0x2145,	0x00},
{0x2146,	0x00},
{0x2147,	0x00},
{0x2149,	0x00},
{0x214a,	0x00},
{0x214b,	0x00},
{0x214d,	0x00},
{0x214e,	0x00},
{0x214f,	0x00},
{0x2151,	0x00},
{0x2152,	0x00},
{0x2153,	0x10},
{0x2155,	0x00},
{0x2156,	0x00},
{0x2157,	0x00},
{0x2159,	0x00},
{0x215a,	0x00},
{0x215b,	0x00},
{0x215d,	0x00},
{0x215e,	0x00},
{0x215f,	0x00},
{0x2161,	0x00},
{0x2162,	0x00},
{0x2163,	0x00},
{0x2165,	0x00},
{0x2166,	0x00},
{0x2167,	0x10},
{0x2169,	0x02},
{0x216a,	0x71},
{0x216b,	0xe0},
{0x216d,	0xfd},
{0x216e,	0x91},
{0x216f,	0x0f},
{0x2171,	0x02},
{0x2172,	0x71},
{0x2173,	0xe0},
{0x2175,	0xfd},
{0x2176,	0x91},
{0x2177,	0x0f},
{0x2179,	0xe7},
{0x217a,	0x1b},
{0x217b,	0x10},
{0x217d,	0xf3},
{0x217e,	0x9a},
{0x217f,	0xe0},
{0x2181,	0xd3},
{0x2182,	0x5c},
{0x2183,	0x0f},
{0x2185,	0xf3},
{0x2186,	0x9a},
{0x2187,	0xe0},
{0x2189,	0xba},
{0x218a,	0x78},
{0x218b,	0x0f},
{0x218d,	0x70},
{0x218e,	0x9d},
{0x218f,	0x0f},
{0x2191,	0x0c},
{0x2192,	0x52},
{0x2193,	0xe3},
{0x2195,	0x97},
{0x2196,	0xbf},
{0x2197,	0x0d},
{0x2199,	0x0c},
{0x219a,	0x52},
{0x219b,	0xe3},
{0x219d,	0x07},
{0x219e,	0x5d},
{0x219f,	0x0d},
{0x21a1,	0x00},
{0x21a2,	0x00},
{0x21a3,	0x10},
{0x21a5,	0x00},
{0x21a6,	0x00},
{0x21a7,	0x00},
{0x21a9,	0x00},
{0x21aa,	0x00},
{0x21ab,	0x00},
{0x21ad,	0x00},
{0x21ae,	0x00},
{0x21af,	0x00},
{0x21b1,	0x00},
{0x21b2,	0x00},
{0x21b3,	0x00},
{0x21b5,	0x68},
{0x21b6,	0x9f},
{0x21b7,	0x11},
{0x21b9,	0xae},
{0x21ba,	0xcc},
{0x21bb,	0xe7},
{0x21bd,	0xd0},
{0x21be,	0x4a},
{0x21bf,	0x09},
{0x21c1,	0x33},
{0x21c2,	0x94},
{0x21c3,	0xea},
{0x21c5,	0xb4},
{0x21c6,	0x22},
{0x21c7,	0x08},
{0x21c9,	0xf7},
{0x21ca,	0x0c},
{0x21cb,	0x10},
{0x21cd,	0xbc},
{0x21ce,	0x06},
{0x21cf,	0xe1},
{0x21d1,	0x58},
{0x21d2,	0x01},
{0x21d3,	0x0f},
{0x21d5,	0x21},
{0x21d6,	0x02},
{0x21d7,	0xe1},
{0x21d9,	0xb4},
{0x21da,	0x09},
{0x21db,	0x0f},
{0x21dd,	0xd8},
{0x21de,	0x24},
{0x21df,	0x0f},
{0x21e1,	0x50},
{0x21e2,	0xb6},
{0x21e3,	0xe1},
{0x21e5,	0xd8},
{0x21e6,	0x24},
{0x21e7,	0x0f},
{0x21e9,	0x0b},
{0x21ea,	0xc2},
{0x21eb,	0xe1},
{0x21ed,	0x6b},
{0x21ee,	0x55},
{0x21ef,	0x0e},
{0x21f1,	0xde},
{0x21f2,	0x05},
{0x21f3,	0x00},
{0x21f5,	0xbb},
{0x21f6,	0x0b},
{0x21f7,	0x00},
{0x21f9,	0xde},
{0x21fa,	0x05},
{0x21fb,	0x00},
{0x21fd,	0x0b},
{0x21fe,	0xc2},
{0x21ff,	0xe1},
{0x2201,	0x6b},
{0x2202,	0x55},
{0x2203,	0x0e},
{0x2205,	0x8f},
{0x2206,	0x5d},
{0x2207,	0x0f},
{0x2209,	0xe2},
{0x220a,	0x44},
{0x220b,	0xe1},
{0x220d,	0x8f},
{0x220e,	0x5d},
{0x220f,	0x0f},
{0x2211,	0x91},
{0x2212,	0x67},
{0x2213,	0xe1},
{0x2215,	0xcd},
{0x2216,	0xdd},
{0x2217,	0x0e},
{0x2219,	0x58},
{0x221a,	0x11},
{0x221b,	0x00},
{0x221d,	0xaf},
{0x221e,	0x22},
{0x221f,	0x00},
{0x2221,	0x58},
{0x2222,	0x11},
{0x2223,	0x00},
{0x2225,	0x91},
{0x2226,	0x67},
{0x2227,	0xe1},
{0x2229,	0xcd},
{0x222a,	0xdd},
{0x222b,	0x0e},
{0x222d,	0x5f},
{0x222e,	0x98},
{0x222f,	0x0f},
{0x2231,	0x43},
{0x2232,	0xcf},
{0x2233,	0xe0},
{0x2235,	0x5f},
{0x2236,	0x98},
{0x2237,	0x0f},
{0x2239,	0xe2},
{0x223a,	0xd1},
{0x223b,	0xe0},
{0x223d,	0x5c},
{0x223e,	0x33},
{0x223f,	0x0f},
{0x2241,	0xee},
{0x2242,	0x84},
{0x2243,	0x0f},
{0x2245,	0x24},
{0x2246,	0xf6},
{0x2247,	0xe0},
{0x2249,	0xee},
{0x224a,	0x84},
{0x224b,	0x0f},
{0x224d,	0xd7},
{0x224e,	0xf9},
{0x224f,	0xe0},
{0x2251,	0x8f},
{0x2252,	0x0d},
{0x2253,	0x0f},
{0x2255,	0xe9},
{0x2256,	0x6d},
{0x2257,	0x0f},
{0x2259,	0x2e},
{0x225a,	0x24},
{0x225b,	0xe1},
{0x225d,	0xe9},
{0x225e,	0x6d},
{0x225f,	0x0f},
{0x2261,	0x64},
{0x2262,	0x29},
{0x2263,	0xe1},
{0x2265,	0x08},
{0x2266,	0xe1},
{0x2267,	0x0e},
{0x2269,	0xae},
{0x226a,	0x52},
{0x226b,	0x0f},
{0x226d,	0xa4},
{0x226e,	0x5a},
{0x226f,	0xe1},
{0x2271,	0xae},
{0x2272,	0x52},
{0x2273,	0x0f},
{0x2275,	0xfa},
{0x2276,	0x61},
{0x2277,	0xe1},
{0x2279,	0xb2},
{0x227a,	0xac},
{0x227b,	0x0e},
{0x227d,	0x82},
{0x227e,	0x32},
{0x227f,	0x0f},
{0x2281,	0xfd},
{0x2282,	0x9a},
{0x2283,	0xe1},
{0x2285,	0x82},
{0x2286,	0x32},
{0x2287,	0x0f},
{0x2289,	0x4d},
{0x228a,	0xa5},
{0x228b,	0xe1},
{0x228d,	0x53},
{0x228e,	0x6f},
{0x228f,	0x0e},
{0x2291,	0xe2},
{0x2292,	0x24},
{0x2293,	0x00},
{0x2295,	0x00},
{0x2296,	0x00},
{0x2297,	0x00},
{0x2299,	0x1e},
{0x229a,	0xdb},
{0x229b,	0xff},
{0x229d,	0x27},
{0x229e,	0x5d},
{0x229f,	0xe0},
{0x22a1,	0x3b},
{0x22a2,	0xb6},
{0x22a3,	0x0f},
{0x22a5,	0x53},
{0x22a6,	0xf7},
{0x22a7,	0x00},
{0x22a9,	0x00},
{0x22aa,	0x00},
{0x22ab,	0x00},
{0x22ad,	0xad},
{0x22ae,	0x08},
{0x22af,	0xff},
{0x22b1,	0x4a},
{0x22b2,	0xf1},
{0x22b3,	0xe2},
{0x22b5,	0x5a},
{0x22b6,	0x11},
{0x22b7,	0x0e},
{0x22b9,	0x1e},
{0x22ba,	0xdb},
{0x22bb,	0x0f},
{0x22bd,	0x27},
{0x22be,	0x5d},
{0x22bf,	0xe0},
{0x22c1,	0x1e},
{0x22c2,	0xdb},
{0x22c3,	0x0f},
{0x22c5,	0x27},
{0x22c6,	0x5d},
{0x22c7,	0xe0},
{0x22c9,	0x3b},
{0x22ca,	0xb6},
{0x22cb,	0x0f},
{0x22cd,	0xe8},
{0x22ce,	0x8d},
{0x22cf,	0x00},
{0x22d1,	0x00},
{0x22d2,	0x00},
{0x22d3,	0x00},
{0x22d5,	0x18},
{0x22d6,	0x72},
{0x22d7,	0xff},
{0x22d9,	0xce},
{0x22da,	0x25},
{0x22db,	0xe1},
{0x22dd,	0x2f},
{0x22de,	0xe4},
{0x22df,	0x0e},
{0x22e1,	0x45},
{0x22e2,	0xb2},
{0x22e3,	0x0e},
{0x22e5,	0x75},
{0x22e6,	0x9b},
{0x22e7,	0xe2},
{0x22e9,	0x45},
{0x22ea,	0xb2},
{0x22eb,	0x0e},
{0x22ed,	0xb0},
{0x22ee,	0xb6},
{0x22ef,	0xe2},
{0x22f1,	0xc5},
{0x22f2,	0x7f},
{0x22f3,	0x0d},
{0x22f5,	0x45},
{0x22f6,	0xb2},
{0x22f7,	0x0e},
{0x22f9,	0x75},
{0x22fa,	0x9b},
{0x22fb,	0xe2},
{0x22fd,	0x45},
{0x22fe,	0xb2},
{0x22ff,	0x0e},
{0x2301,	0xb0},
{0x2302,	0xb6},
{0x2303,	0xe2},
{0x2305,	0xc5},
{0x2306,	0x7f},
{0x2307,	0x0d},
{0x2309,	0x48},
{0x230a,	0x00},
{0x230b,	0x00},
{0x230d,	0x83},
{0x230e,	0x08},
{0x230f,	0x00},
{0x2311,	0x80},
{0x2312,	0x00},
{0x2313,	0x00},
{0x2315,	0x80},
{0x2316,	0x00},
{0x2317,	0x00},
{0x2380,	0x0b},
{0x2381,	0x5e},
{0x2382,	0x0b},
{0x2383,	0x5e},
{0x2384,	0x02},
{0x2385,	0x21},
{0x2386,	0x06},
{0x2387,	0x11},
{0x2388,	0x01},
{0x238e,	0xcc},
{0x238f,	0x0c},
{0x2390,	0xd7},
{0x2391,	0x03},
{0x2392,	0xe4},
{0x2393,	0x38},
{0x2394,	0x0c},
{0x2395,	0x64},
{0x2396,	0xb6},
{0x2397,	0x28},
{0x2398,	0x0b},
{0x2399,	0x00},
{0x239a,	0x99},
{0x239b,	0x0d},
{0x23a4,	0x03},
{0x23a5,	0x69},
{0x23a6,	0x15},
{0x23a7,	0x78},
{0x238b,	0x1b},
{0x23b5,	0x0e},
{0x23b6,	0x08},
{0x23b9,	0x00},
{0x23ba,	0x82},
{0x23e0,	0x76},
{0x23e1,	0x01},
{0x23ff,	0x01},
};

static int max98390_dsm_calibrate(struct snd_soc_component *component);

static int max98390_dai_set_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_component *component = codec_dai->component;
	struct max98390_priv *max98390 =
		snd_soc_component_get_drvdata(component);

	unsigned int mode;
	unsigned int format;
	unsigned int invert = 0;

	dev_dbg(component->dev, "%s: fmt 0x%08X\n", __func__, fmt);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		mode = MAX98390_PCM_MASTER_MODE_SLAVE;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		max98390->master = true;
		mode = MAX98390_PCM_MASTER_MODE_MASTER;
		break;
	default:
		dev_err(component->dev, "DAI clock mode unsupported\n");
		return -EINVAL;
	}

	regmap_update_bits(max98390->regmap,
		MAX98390_PCM_MASTER_MODE,
		MAX98390_PCM_MASTER_MODE_MASK,
		mode);

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_IB_NF:
		invert = MAX98390_PCM_MODE_CFG_PCM_BCLKEDGE;
		break;
	default:
		dev_err(component->dev, "DAI invert mode unsupported\n");
		return -EINVAL;
	}

	regmap_update_bits(max98390->regmap,
		MAX98390_PCM_MODE_CFG,
		MAX98390_PCM_MODE_CFG_PCM_BCLKEDGE,
		invert);

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		format = MAX98390_PCM_FORMAT_I2S;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		format = MAX98390_PCM_FORMAT_LJ;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		format = MAX98390_PCM_FORMAT_TDM_MODE1;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		format = MAX98390_PCM_FORMAT_TDM_MODE0;
		break;
	default:
		return -EINVAL;
	}

	regmap_update_bits(max98390->regmap,
		MAX98390_PCM_MODE_CFG,
		MAX98390_PCM_MODE_CFG_FORMAT_MASK,
		format << MAX98390_PCM_MODE_CFG_FORMAT_SHIFT);

	return 0;
}

static int max98390_get_bclk_sel(int bclk)
{
	int i;
	/* BCLKs per LRCLK */
	static int bclk_sel_table[] = {
		32, 48, 64, 96, 128, 192, 256, 320, 384, 512,
	};
	/* match BCLKs per LRCLK */
	for (i = 0; i < ARRAY_SIZE(bclk_sel_table); i++) {
		if (bclk_sel_table[i] == bclk) {
			return i + 2;
		}
	}
	return 0;
}

static int max98390_set_clock(struct snd_soc_component *component,
		struct snd_pcm_hw_params *params)
{
	struct max98390_priv *max98390 =
		snd_soc_component_get_drvdata(component);

	/* codec MCLK rate in master mode */
	static int rate_table[] = {
		5644800, 6000000, 6144000, 6500000,
		9600000, 11289600, 12000000, 12288000,
		13000000, 19200000,
	};

	/* BCLK/LRCLK ratio calculation */
	int blr_clk_ratio = params_channels(params)
		* snd_pcm_format_width(params_format(params));
	int value;
	if (max98390->master) {
		int i;
		/* match rate to closest value */
		for (i = 0; i < ARRAY_SIZE(rate_table); i++) {
			if (rate_table[i] >= max98390->sysclk)
				break;
		}
		if (i == ARRAY_SIZE(rate_table)) {
			dev_err(component->dev, "failed to find proper clock rate.\n");
			return -EINVAL;
		}

		regmap_update_bits(max98390->regmap,
			MAX98390_PCM_MASTER_MODE,
			MAX98390_PCM_MASTER_MODE_MCLK_MASK,
			i << MAX98390_PCM_MASTER_MODE_MCLK_RATE_SHIFT);
	}

	if (!max98390->tdm_mode) {
		/* BCLK configuration */
		value = max98390_get_bclk_sel(blr_clk_ratio);
		if (!value) {
			dev_err(component->dev, "format unsupported %d\n",
				params_format(params));
			return -EINVAL;
		}

		regmap_update_bits(max98390->regmap,
			MAX98390_PCM_CLK_SETUP,
			MAX98390_PCM_CLK_SETUP_BSEL_MASK,
			value);
	}
	return 0;
}

static int max98390_dai_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params,
		struct snd_soc_dai *dai)
{
	struct snd_soc_component *component =
		dai->component;

	struct max98390_priv *max98390 =
		snd_soc_component_get_drvdata(component);

	unsigned int sampling_rate;
	unsigned int chan_sz;

	/* pcm mode configuration */
	switch (snd_pcm_format_width(params_format(params))) {
	case 16:
		chan_sz = MAX98390_PCM_MODE_CFG_CHANSZ_16;
		break;
	case 24:
		chan_sz = MAX98390_PCM_MODE_CFG_CHANSZ_24;
		break;
	case 32:
		chan_sz = MAX98390_PCM_MODE_CFG_CHANSZ_32;
		break;
	default:
		dev_err(component->dev, "format unsupported %d\n",
			params_format(params));
		goto err;
	}

	regmap_update_bits(max98390->regmap,
		MAX98390_PCM_MODE_CFG,
		MAX98390_PCM_MODE_CFG_CHANSZ_MASK, chan_sz);

	dev_dbg(component->dev, "format supported %d",
		params_format(params));

	/* sampling rate configuration */
	switch (params_rate(params)) {
	case 8000:
		sampling_rate = MAX98390_PCM_SR_SET1_SR_8000;
		break;
	case 11025:
		sampling_rate = MAX98390_PCM_SR_SET1_SR_11025;
		break;
	case 12000:
		sampling_rate = MAX98390_PCM_SR_SET1_SR_12000;
		break;
	case 16000:
		sampling_rate = MAX98390_PCM_SR_SET1_SR_16000;
		break;
	case 22050:
		sampling_rate = MAX98390_PCM_SR_SET1_SR_22050;
		break;
	case 24000:
		sampling_rate = MAX98390_PCM_SR_SET1_SR_24000;
		break;
	case 32000:
		sampling_rate = MAX98390_PCM_SR_SET1_SR_32000;
		break;
	case 44100:
		sampling_rate = MAX98390_PCM_SR_SET1_SR_44100;
		break;
	case 48000:
		sampling_rate = MAX98390_PCM_SR_SET1_SR_48000;
		break;
	default:
		dev_err(component->dev, "rate %d not supported\n",
			params_rate(params));
		goto err;
	}

	/* set DAI_SR to correct LRCLK frequency */
	regmap_update_bits(max98390->regmap,
		MAX98390_PCM_SR_SETUP,
		MAX98390_PCM_SR_SET1_SR_MASK,
		sampling_rate);

	return max98390_set_clock(component, params);
err:
	return -EINVAL;
}

static int max98390_dai_tdm_slot(struct snd_soc_dai *dai,
		unsigned int tx_mask, unsigned int rx_mask,
		int slots, int slot_width)
{
	struct snd_soc_component *component = dai->component;
	struct max98390_priv *max98390 =
		snd_soc_component_get_drvdata(component);

	int bsel;
	unsigned int chan_sz;

	if (!tx_mask && !rx_mask && !slots && !slot_width)
		max98390->tdm_mode = false;
	else
		max98390->tdm_mode = true;

	dev_dbg(component->dev,
		"Tdm mode : %d\n", max98390->tdm_mode);

	/* BCLK configuration */
	bsel = max98390_get_bclk_sel(slots * slot_width);
	if (!bsel) {
		dev_err(component->dev, "BCLK %d not supported\n",
			slots * slot_width);
		return -EINVAL;
	}

	regmap_update_bits(max98390->regmap,
		MAX98390_PCM_CLK_SETUP,
		MAX98390_PCM_CLK_SETUP_BSEL_MASK,
		bsel);

	/* Channel size configuration */
	switch (slot_width) {
	case 16:
		chan_sz = MAX98390_PCM_MODE_CFG_CHANSZ_16;
		break;
	case 24:
		chan_sz = MAX98390_PCM_MODE_CFG_CHANSZ_24;
		break;
	case 32:
		chan_sz = MAX98390_PCM_MODE_CFG_CHANSZ_32;
		break;
	default:
		dev_err(component->dev, "format unsupported %d\n",
			slot_width);
		return -EINVAL;
	}

	regmap_update_bits(max98390->regmap,
		MAX98390_PCM_MODE_CFG,
		MAX98390_PCM_MODE_CFG_CHANSZ_MASK, chan_sz);

	/* Rx slot configuration */
	regmap_write(max98390->regmap,
		MAX98390_PCM_RX_EN_A,
		rx_mask & 0xFF);
	regmap_write(max98390->regmap,
		MAX98390_PCM_RX_EN_B,
		(rx_mask & 0xFF00) >> 8);

	/* Tx slot Hi-Z configuration */
	regmap_write(max98390->regmap,
		MAX98390_PCM_TX_HIZ_CTRL_A,
		~tx_mask & 0xFF);
	regmap_write(max98390->regmap,
		MAX98390_PCM_TX_HIZ_CTRL_B,
		(~tx_mask & 0xFF00) >> 8);

	return 0;
}

static int max98390_dai_set_sysclk(struct snd_soc_dai *dai,
		int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_component *component = dai->component;
	struct max98390_priv *max98390 =
		snd_soc_component_get_drvdata(component);

	max98390->sysclk = freq;
	return 0;
}

static const struct snd_soc_dai_ops max98390_dai_ops = {
	.set_sysclk = max98390_dai_set_sysclk,
	.set_fmt = max98390_dai_set_fmt,
	.hw_params = max98390_dai_hw_params,
	.set_tdm_slot = max98390_dai_tdm_slot,
};

static int max98390_dac_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec =
		snd_soc_dapm_to_codec(w->dapm);
	struct max98390_priv *max98390=
		snd_soc_codec_get_drvdata(codec);

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		regmap_update_bits(max98390->regmap,
			MAX98390_R203A_AMP_EN,
			MAX98390_AMP_EN_MASK, 1);
		regmap_update_bits(max98390->regmap,
			MAX98390_R23FF_GLOBAL_EN,
			MAX98390_GLOBAL_EN_MASK, 1);
		break;
	case SND_SOC_DAPM_POST_PMD:
		regmap_update_bits(max98390->regmap,
			MAX98390_R23FF_GLOBAL_EN,
			MAX98390_GLOBAL_EN_MASK, 0);
		regmap_update_bits(max98390->regmap,
			MAX98390_R203A_AMP_EN,
			MAX98390_AMP_EN_MASK, 0);
		break;
	}
	return 0;
}

static const char * const max98390_switch_text[] = {
	"Left", "Right", "LeftRight"};

static const char * const max98390_boost_voltage_text[] = {
	"6.5V", "6.625V", "6.75V", "6.875V", "7V", "7.125V", "7.25V", "7.375V",
	"7.5V", "7.625V", "7.75V", "7.875V", "8V", "8.125V", "8.25V", "8.375V",
	"8.5V", "8.625V", "8.75V", "8.875V", "9V", "9.125V", "9.25V", "9.375V",
	"9.5V", "9.625V", "9.75V", "9.875V", "10V"
};

static SOC_ENUM_SINGLE_DECL(max98390_boost_voltage,
		MAX98390_BOOST_CTRL0, 0,
		max98390_boost_voltage_text);

static DECLARE_TLV_DB_SCALE(max98390_spk_tlv, 300, 300, 0);
static DECLARE_TLV_DB_SCALE(max98390_digital_tlv, -8000, 50, 0);

static const char * const max98390_current_limit_text[] = {
	"0.00A", "0.50A", "1.00A", "1.05A", "1.10A", "1.15A", "1.20A", "1.25A",
	"1.30A", "1.35A", "1.40A", "1.45A", "1.50A", "1.55A", "1.60A", "1.65A",
	"1.70A", "1.75A", "1.80A", "1.85A", "1.90A", "1.95A", "2.00A", "2.05A",
	"2.10A", "2.15A", "2.20A", "2.25A", "2.30A", "2.35A", "2.40A", "2.45A",
	"2.50A", "2.55A", "2.60A", "2.65A", "2.70A", "2.75A", "2.80A", "2.85A",
	"2.90A", "2.95A", "3.00A", "3.05A", "3.10A", "3.15A", "3.20A", "3.25A",
	"3.30A", "3.35A", "3.40A", "3.45A", "3.50A", "3.55A", "3.60A", "3.65A",
	"3.70A", "3.75A", "3.80A", "3.85A", "3.90A", "3.95A", "4.00A", "4.05A",
	"4.10A"
};

static SOC_ENUM_SINGLE_DECL(max98390_current_limit,
		MAX98390_BOOST_CTRL1, 0,
		max98390_current_limit_text);

static int max98390_ref_rdc_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98390_priv *max98390 = snd_soc_codec_get_drvdata(codec);

	max98390->ref_rdc_value = ucontrol->value.integer.value[0];

	regmap_write(max98390->regmap, DSM_TPROT_RECIP_RDC_ROOM_BYTE0,
		max98390->ref_rdc_value & 0x000000ff);
	regmap_write(max98390->regmap, DSM_TPROT_RECIP_RDC_ROOM_BYTE1,
		(max98390->ref_rdc_value >> 8) & 0x000000ff);
	regmap_write(max98390->regmap, DSM_TPROT_RECIP_RDC_ROOM_BYTE2,
		(max98390->ref_rdc_value >> 16) & 0x000000ff);

	return 0;
}

static int max98390_ref_rdc_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98390_priv *max98390 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = max98390->ref_rdc_value;

	return 0;
}

static int max98390_ambient_temp_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98390_priv *max98390 = snd_soc_codec_get_drvdata(codec);

	max98390->ambient_temp_value = ucontrol->value.integer.value[0];

	regmap_write(max98390->regmap, DSM_TPROT_ROOM_TEMPERATURE_BYTE1,
		(max98390->ambient_temp_value >> 8) & 0x000000ff);
	regmap_write(max98390->regmap, DSM_TPROT_ROOM_TEMPERATURE_BYTE0,
		(max98390->ambient_temp_value) & 0x000000ff);

	return 0;
}

static int max98390_ambient_temp_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98390_priv *max98390 = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.integer.value[0] = max98390->ambient_temp_value;

	return 0;
}

static int max98390_adaptive_rdc_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_component *component = &codec->component;

	dev_warn(component->dev, "Put adaptive rdc not supported\n");

	return 0;
}

static int max98390_adaptive_rdc_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	int rdc, rdc0;
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct max98390_priv *max98390 = snd_soc_codec_get_drvdata(codec);

	regmap_read(max98390->regmap, THERMAL_RDC_RD_BACK_BYTE1, &rdc);
	regmap_read(max98390->regmap, THERMAL_RDC_RD_BACK_BYTE0, &rdc0);
	ucontrol->value.integer.value[0] = rdc0 | rdc << 8;

	return 0;
}

static int max98390_dsm_calib_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	/* Do nothing */
	return 0;
}

static int max98390_dsm_calib_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_soc_kcontrol_codec(kcontrol);
	struct snd_soc_component *component = &codec->component;

	max98390_dsm_calibrate(component);

	return 0;
}

static const struct snd_kcontrol_new max98390_snd_controls[] = {
	SOC_SINGLE_TLV("Digital Volume", DSM_VOL_CTRL,
		0, 184, 0,
		max98390_digital_tlv),
	SOC_SINGLE_TLV("Speaker Volume", MAX98390_R203D_SPK_GAIN,
		0, 6, 0,
		max98390_spk_tlv),
	SOC_SINGLE("Ramp Up Bypass Switch", MAX98390_R2039_AMP_DSP_CFG,
		MAX98390_AMP_DSP_CFG_RMP_UP_SHIFT, 1, 0),
	SOC_SINGLE("Ramp Down Bypass Switch", MAX98390_R2039_AMP_DSP_CFG,
		MAX98390_AMP_DSP_CFG_RMP_DN_SHIFT, 1, 0),
	SOC_SINGLE("Boost Clock Phase", MAX98390_BOOST_CTRL3,
		MAX98390_BOOST_CLK_PHASE_CFG_SHIFT, 3, 0),
	SOC_ENUM("Boost Output Voltage", max98390_boost_voltage),
	SOC_ENUM("Current Limit", max98390_current_limit),
	SOC_SINGLE_EXT("DSM Rdc", SND_SOC_NOPM, 0, 0xffffff, 0,
		max98390_ref_rdc_get, max98390_ref_rdc_put),
	SOC_SINGLE_EXT("DSM Ambient Temp", SND_SOC_NOPM, 0, 0xffff, 0,
		max98390_ambient_temp_get, max98390_ambient_temp_put),
	SOC_SINGLE_EXT("DSM Adaptive Rdc", SND_SOC_NOPM, 0, 0xffff, 0,
		max98390_adaptive_rdc_get, max98390_adaptive_rdc_put),
	SOC_SINGLE_EXT("DSM Calibration", SND_SOC_NOPM, 0, 1, 0,
		max98390_dsm_calib_get, max98390_dsm_calib_put),
};

static const struct soc_enum dai_sel_enum =
	SOC_ENUM_SINGLE(MAX98390_PCM_CH_SRC_1,
		MAX98390_PCM_RX_CH_SRC_SHIFT,
		3, max98390_switch_text);

static const struct snd_kcontrol_new max98390_dai_controls =
	SOC_DAPM_ENUM("DAI Sel", dai_sel_enum);

static const struct snd_soc_dapm_widget max98390_dapm_widgets[] = {
	SND_SOC_DAPM_DAC_E("Amp Enable", "HiFi Playback",
		SND_SOC_NOPM, 0, 0, max98390_dac_event,
		SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX("DAI Sel Mux", SND_SOC_NOPM, 0, 0,
		&max98390_dai_controls),
	SND_SOC_DAPM_OUTPUT("BE_OUT"),
};

static const struct snd_soc_dapm_route max98390_audio_map[] = {
	/* Plabyack */
	{"DAI Sel Mux", "Left", "Amp Enable"},
	{"DAI Sel Mux", "Right", "Amp Enable"},
	{"DAI Sel Mux", "LeftRight", "Amp Enable"},
	{"BE_OUT", NULL, "DAI Sel Mux"},
};

static bool max98390_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX98390_SOFTWARE_RESET ... MAX98390_INT_EN3:
	case MAX98390_IRQ_CTRL ... MAX98390_WDOG_CTRL:
	case MAX98390_MEAS_ADC_THERM_WARN_THRESH
		... MAX98390_BROWNOUT_INFINITE_HOLD:
	case MAX98390_BROWNOUT_LVL_HOLD ... DSMIG_DEBUZZER_THRESHOLD:
	case DSM_VOL_ENA ... MAX98390_R24FF_REV_ID:
		return true;
	default:
		return false;
	}
};

static bool max98390_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MAX98390_SOFTWARE_RESET ... MAX98390_INT_EN3:
	case MAX98390_MEAS_ADC_CH0_READ ... MAX98390_MEAS_ADC_CH2_READ:
	case MAX98390_PWR_GATE_STATUS ... MAX98390_BROWNOUT_STATUS:
	case MAX98390_BROWNOUT_LOWEST_STATUS:
	case MAX98390_ENV_TRACK_BOOST_VOUT_READ:
	case DSM_STBASS_HPF_B0_BYTE0 ... DSM_DEBUZZER_ATTACK_TIME_BYTE2:
	case THERMAL_RDC_RD_BACK_BYTE1 ... DSMIG_DEBUZZER_THRESHOLD:
	case DSM_THERMAL_GAIN ... DSM_WBDRC_GAIN:
		return true;
	default:
		return false;
	}
}

#define MAX98390_RATES SNDRV_PCM_RATE_8000_48000

#define MAX98390_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
	SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver max98390_dai[] = {
	{
		.name = "max98390-aif1",
		.playback = {
			.stream_name = "HiFi Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MAX98390_RATES,
			.formats = MAX98390_FORMATS,
		},
		.capture = {
			.stream_name = "HiFi Capture",
			.channels_min = 1,
			.channels_max = 2,
			.rates = MAX98390_RATES,
			.formats = MAX98390_FORMATS,
		},
		.ops = &max98390_dai_ops,
	}
};

static int max98390_dsm_init(struct snd_soc_component *component)
{
	int ret;
	int param_size, param_start_addr;
	char filename[128];
	const char *vendor, *product;
	struct max98390_priv *max98390 =
		snd_soc_component_get_drvdata(component);
	const struct firmware *fw;
	char *dsm_param;

	vendor = dmi_get_system_info(DMI_SYS_VENDOR);
	product = dmi_get_system_info(DMI_PRODUCT_NAME);

	if (vendor && product) {
		snprintf(filename, sizeof(filename), "dsm_param_%s_%s.bin",
			vendor, product);
	} else {
		sprintf(filename, "dsm_param.bin");
	}
	ret = request_firmware(&fw, filename, component->dev);
	if (ret) {
		ret = request_firmware(&fw, "dsm_param.bin", component->dev);
		if (ret)
			goto err;
	}

	dev_dbg(component->dev,
		"max98390: param fw size %zd\n",
		fw->size);
	if (fw->size < MAX98390_DSM_PARAM_MIN_SIZE) {
		dev_err(component->dev,
			"param fw is invalid.\n");
		ret = -EINVAL;
		goto err_alloc;
	}
	dsm_param = (char *)fw->data;
	param_start_addr = (dsm_param[0] & 0xff) | (dsm_param[1] & 0xff) << 8;
	param_size = (dsm_param[2] & 0xff) | (dsm_param[3] & 0xff) << 8;
	if (param_size > MAX98390_DSM_PARAM_MAX_SIZE ||
		param_start_addr < MAX98390_IRQ_CTRL ||
		fw->size < param_size + MAX98390_DSM_PAYLOAD_OFFSET) {
		dev_err(component->dev,
			"param fw is invalid.\n");
		ret = -EINVAL;
		goto err_alloc;
	}
	regmap_write(max98390->regmap, MAX98390_R203A_AMP_EN, 0x80);
	dsm_param += MAX98390_DSM_PAYLOAD_OFFSET;
	regmap_bulk_write(max98390->regmap, param_start_addr,
		dsm_param, param_size);
	regmap_write(max98390->regmap, MAX98390_R23E1_DSP_GLOBAL_EN, 0x01);

err_alloc:
	release_firmware(fw);
err:
	return ret;
}

static int max98390_dsm_calibrate(struct snd_soc_component *component)
{
	unsigned int rdc, rdc_cal_result, temp;
	unsigned int rdc_integer, rdc_factor;
	struct max98390_priv *max98390 =
		snd_soc_component_get_drvdata(component);

	regmap_write(max98390->regmap, MAX98390_R203A_AMP_EN, 0x81);
	regmap_write(max98390->regmap, MAX98390_R23FF_GLOBAL_EN, 0x01);

	regmap_read(max98390->regmap,
		THERMAL_RDC_RD_BACK_BYTE1, &rdc);
	regmap_read(max98390->regmap,
		THERMAL_RDC_RD_BACK_BYTE0, &rdc_cal_result);
	rdc_cal_result |= (rdc << 8) & 0x0000FFFF;
	if (rdc_cal_result)
		max98390->ref_rdc_value = 268435456U / rdc_cal_result;

	regmap_read(max98390->regmap, MAX98390_MEAS_ADC_CH2_READ, &temp);
	max98390->ambient_temp_value = temp * 52 - 1188;

	rdc_integer =  rdc_cal_result * 937  / 65536;
	rdc_factor = ((rdc_cal_result * 937 * 100) / 65536)
					- (rdc_integer * 100);

	dev_info(component->dev, "rdc resistance about %d.%02d ohm, reg=0x%X temp reg=0x%X\n",
		 rdc_integer, rdc_factor, rdc_cal_result, temp);

	regmap_write(max98390->regmap, MAX98390_R23FF_GLOBAL_EN, 0x00);
	regmap_write(max98390->regmap, MAX98390_R203A_AMP_EN, 0x80);

	return 0;
}

static void max98390_init_regs(struct snd_soc_component *component)
{
	struct max98390_priv *max98390 =
		snd_soc_component_get_drvdata(component);

	regmap_write(max98390->regmap, MAX98390_CLK_MON, 0x6f);
	regmap_write(max98390->regmap, MAX98390_DAT_MON, 0x00);
	regmap_write(max98390->regmap, MAX98390_PWR_GATE_CTL, 0x00);
	regmap_write(max98390->regmap, MAX98390_PCM_RX_EN_A, 0x03);
	regmap_write(max98390->regmap, MAX98390_ENV_TRACK_VOUT_HEADROOM, 0x0e);
	regmap_write(max98390->regmap, MAX98390_BOOST_BYPASS1, 0x46);
	regmap_write(max98390->regmap, MAX98390_FET_SCALING3, 0x03);

	/* voltage, current slot configuration */
	regmap_write(max98390->regmap,
		MAX98390_PCM_CH_SRC_2,
		(max98390->i_l_slot << 4 |
		max98390->v_l_slot)&0xFF);

	if (max98390->v_l_slot < 8) {
		regmap_update_bits(max98390->regmap,
			MAX98390_PCM_TX_HIZ_CTRL_A,
			1 << max98390->v_l_slot, 0);
		regmap_update_bits(max98390->regmap,
			MAX98390_PCM_TX_EN_A,
			1 << max98390->v_l_slot,
			1 << max98390->v_l_slot);
	} else {
		regmap_update_bits(max98390->regmap,
			MAX98390_PCM_TX_HIZ_CTRL_B,
			1 << (max98390->v_l_slot - 8), 0);
		regmap_update_bits(max98390->regmap,
			MAX98390_PCM_TX_EN_B,
			1 << (max98390->v_l_slot - 8),
			1 << (max98390->v_l_slot - 8));
	}

	if (max98390->i_l_slot < 8) {
		regmap_update_bits(max98390->regmap,
			MAX98390_PCM_TX_HIZ_CTRL_A,
			1 << max98390->i_l_slot, 0);
		regmap_update_bits(max98390->regmap,
			MAX98390_PCM_TX_EN_A,
			1 << max98390->i_l_slot,
			1 << max98390->i_l_slot);
	} else {
		regmap_update_bits(max98390->regmap,
			MAX98390_PCM_TX_HIZ_CTRL_B,
			1 << (max98390->i_l_slot - 8), 0);
		regmap_update_bits(max98390->regmap,
			MAX98390_PCM_TX_EN_B,
			1 << (max98390->i_l_slot - 8),
			1 << (max98390->i_l_slot - 8));
	}
}

static int max98390_probe(struct snd_soc_codec *codec)
{
	struct max98390_priv *max98390 =
		snd_soc_codec_get_drvdata(codec);

	regmap_write(max98390->regmap, MAX98390_SOFTWARE_RESET, 0x01);
	/* Sleep reset settle time */
	msleep(20);

	/* Amp init setting */
	max98390_init_regs(&codec->component);
	/* Update dsm bin param */
	max98390_dsm_init(&codec->component);

	/* Dsm Setting */
	if (max98390->ref_rdc_value) {
		regmap_write(max98390->regmap, DSM_TPROT_RECIP_RDC_ROOM_BYTE0,
			max98390->ref_rdc_value & 0x000000ff);
		regmap_write(max98390->regmap, DSM_TPROT_RECIP_RDC_ROOM_BYTE1,
			(max98390->ref_rdc_value >> 8) & 0x000000ff);
		regmap_write(max98390->regmap, DSM_TPROT_RECIP_RDC_ROOM_BYTE2,
			(max98390->ref_rdc_value >> 16) & 0x000000ff);
	}
	if (max98390->ambient_temp_value) {
		regmap_write(max98390->regmap, DSM_TPROT_ROOM_TEMPERATURE_BYTE1,
			(max98390->ambient_temp_value >> 8) & 0x000000ff);
		regmap_write(max98390->regmap, DSM_TPROT_ROOM_TEMPERATURE_BYTE0,
			(max98390->ambient_temp_value) & 0x000000ff);
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int max98390_suspend(struct device *dev)
{
	struct max98390_priv *max98390 = dev_get_drvdata(dev);

	dev_dbg(dev, "%s:Enter\n", __func__);

	regcache_cache_only(max98390->regmap, true);
	regcache_mark_dirty(max98390->regmap);

	return 0;
}

static int max98390_resume(struct device *dev)
{
	struct max98390_priv *max98390 = dev_get_drvdata(dev);

	dev_dbg(dev, "%s:Enter\n", __func__);

	regcache_cache_only(max98390->regmap, false);
	regcache_sync(max98390->regmap);

	return 0;
}
#endif

static const struct dev_pm_ops max98390_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(max98390_suspend, max98390_resume)
};

static const struct snd_soc_codec_driver soc_codec_dev_max98390 = {
	.probe			= max98390_probe,
	.controls		= max98390_snd_controls,
	.num_controls		= ARRAY_SIZE(max98390_snd_controls),
	.dapm_widgets		= max98390_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(max98390_dapm_widgets),
	.dapm_routes		= max98390_audio_map,
	.num_dapm_routes	= ARRAY_SIZE(max98390_audio_map),
	//.idle_bias_on		= 1,
	//.use_pmdown_time	= 1,
	//.endianness		= 1,
	//.non_legacy_dai_naming	= 1,
};

static const struct regmap_config max98390_regmap = {
	.reg_bits         = 16,
	.val_bits         = 8,
	.max_register     = MAX98390_R24FF_REV_ID,
	.reg_defaults     = max98390_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(max98390_reg_defaults),
	.readable_reg	  = max98390_readable_register,
	.volatile_reg	  = max98390_volatile_reg,
	.cache_type       = REGCACHE_RBTREE,
};

static void max98390_slot_config(struct i2c_client *i2c,
	struct max98390_priv *max98390)
{
	int value;
	struct device *dev = &i2c->dev;

	if (!device_property_read_u32(dev, "maxim,vmon-slot-no", &value))
		max98390->v_l_slot = value & 0xF;
	else
		max98390->v_l_slot = 0;

	if (!device_property_read_u32(dev, "maxim,imon-slot-no", &value))
		max98390->i_l_slot = value & 0xF;
	else
		max98390->i_l_slot = 1;
}

static int max98390_i2c_probe(struct i2c_client *i2c,
		const struct i2c_device_id *id)
{
	int ret = 0;
	int reg = 0;

	struct max98390_priv *max98390 = NULL;
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);

	ret = i2c_check_functionality(adapter,
		I2C_FUNC_SMBUS_BYTE
		| I2C_FUNC_SMBUS_BYTE_DATA);
	if (!ret) {
		dev_err(&i2c->dev, "I2C check functionality failed\n");
		return -ENXIO;
	}

	max98390 = devm_kzalloc(&i2c->dev, sizeof(*max98390), GFP_KERNEL);
	if (!max98390) {
		ret = -ENOMEM;
		return ret;
	}
	i2c_set_clientdata(i2c, max98390);

	ret = device_property_read_u32(&i2c->dev, "maxim,temperature_calib",
				       &max98390->ambient_temp_value);
	if (ret) {
		dev_info(&i2c->dev,
			 "no optional property 'temperature_calib' found, default:\n");
	}
	ret = device_property_read_u32(&i2c->dev, "maxim,r0_calib",
				       &max98390->ref_rdc_value);
	if (ret) {
		dev_info(&i2c->dev,
			 "no optional property 'r0_calib' found, default:\n");
	}

	dev_info(&i2c->dev,
		"%s: r0_calib: 0x%x,temperature_calib: 0x%x",
		__func__, max98390->ref_rdc_value,
		max98390->ambient_temp_value);

	/* voltage/current slot configuration */
	max98390_slot_config(i2c, max98390);

	/* regmap initialization */
	max98390->regmap = devm_regmap_init_i2c(i2c, &max98390_regmap);
	if (IS_ERR(max98390->regmap)) {
		ret = PTR_ERR(max98390->regmap);
		dev_err(&i2c->dev,
			"Failed to allocate regmap: %d\n", ret);
		return ret;
	}

	/* Check Revision ID */
	ret = regmap_read(max98390->regmap,
		MAX98390_R24FF_REV_ID, &reg);
	if (ret) {
		dev_err(&i2c->dev,
			"ret=%d, Failed to read: 0x%02X\n",
			ret, MAX98390_R24FF_REV_ID);
		return ret;
	}
	dev_info(&i2c->dev, "MAX98390 revisionID: 0x%02X\n", reg);

	ret = snd_soc_register_codec(&i2c->dev,
			&soc_codec_dev_max98390,
			max98390_dai, ARRAY_SIZE(max98390_dai));

	return ret;
}

static const struct i2c_device_id max98390_i2c_id[] = {
	{ "max98390", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, max98390_i2c_id);

#if defined(CONFIG_OF)
static const struct of_device_id max98390_of_match[] = {
	{ .compatible = "maxim,max98390", },
	{}
};
MODULE_DEVICE_TABLE(of, max98390_of_match);
#endif

#ifdef CONFIG_ACPI
static const struct acpi_device_id max98390_acpi_match[] = {
	{ "MX98390", 0 },
	{},
};
MODULE_DEVICE_TABLE(acpi, max98390_acpi_match);
#endif

static struct i2c_driver max98390_i2c_driver = {
	.driver = {
		.name = "max98390",
		.of_match_table = of_match_ptr(max98390_of_match),
		.acpi_match_table = ACPI_PTR(max98390_acpi_match),
		.pm = &max98390_pm,
	},
	.probe = max98390_i2c_probe,
	.id_table = max98390_i2c_id,
};

module_i2c_driver(max98390_i2c_driver)

MODULE_DESCRIPTION("ALSA SoC MAX98390 driver");
MODULE_AUTHOR("Steve Lee <steves.lee@maximintegrated.com>");
MODULE_LICENSE("GPL");
