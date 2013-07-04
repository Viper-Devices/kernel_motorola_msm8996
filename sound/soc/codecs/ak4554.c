/*
 * ak4554.c
 *
 * Copyright (C) 2013 Renesas Solutions Corp.
 * Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <sound/soc.h>

/*
 * ak4554 is very simple DA/AD converter which has no setting register.
 *
 * CAUTION
 *
 * ak4554 playback format is SND_SOC_DAIFMT_RIGHT_J,
 * and,   capture  format is SND_SOC_DAIFMT_LEFT_J
 * on same bit clock, LR clock.
 * But, this driver doesn't have snd_soc_dai_ops :: set_fmt
 *
 * CPU/Codec DAI image
 *
 * CPU-DAI1 (plaback only fmt = RIGHT_J) --+-- ak4554
 *					   |
 * CPU-DAI2 (capture only fmt = LEFT_J) ---+
 */

static struct snd_soc_dai_driver ak4554_dai = {
	.name = "ak4554-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 2,
		.channels_max = 2,
		.rates = SNDRV_PCM_RATE_8000_48000,
		.formats = SNDRV_PCM_FMTBIT_S16_LE,
	},
	.symmetric_rates = 1,
};

static struct snd_soc_codec_driver soc_codec_dev_ak4554 = {
};

static int ak4554_soc_probe(struct platform_device *pdev)
{
	return snd_soc_register_codec(&pdev->dev,
				      &soc_codec_dev_ak4554,
				      &ak4554_dai, 1);
}

static int ak4554_soc_remove(struct platform_device *pdev)
{
	snd_soc_unregister_codec(&pdev->dev);
	return 0;
}

static struct platform_driver ak4554_driver = {
	.driver = {
		.name = "ak4554-adc-dac",
		.owner = THIS_MODULE,
	},
	.probe	= ak4554_soc_probe,
	.remove	= ak4554_soc_remove,
};
module_platform_driver(ak4554_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SoC AK4554 driver");
MODULE_AUTHOR("Kuninori Morimoto <kuninori.morimoto.gx@renesas.com>");
