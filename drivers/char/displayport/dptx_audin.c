// SPDX-License-Identifier: GPL-2.0-or-later OR MIT
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/of_platform.h>
#include <linux/kernel.h>

#include <sound/hdmi-codec.h>
#include <tcc_dptx_api.h>

#include "Dptx_v14.h"
#include "Dptx_dbg.h"

struct Dptx_Hmdi_Codec_Priv {
	u8 ucStream_Idx;
	struct tcc_dptx_audio_ops *pstAud_ops;
	struct platform_device *pstAud_dev;
	struct Dptx_Params *pstDptx;
};

struct Dptx_Hmdi_Codec_Priv stHmdi_Codec_Priv[PHY_INPUT_STREAM_MAX];


static int32_t dptx_audin_get_ops(struct Dptx_Hmdi_Codec_Priv *pstHmdi_Codec_Priv)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	struct device_node *pAud_nd;
	struct platform_device *pPlat_dev;

	pAud_nd = of_find_compatible_node(NULL, NULL, "telechips,dptx_audio");
	if (pAud_nd == NULL) {
		dptx_err("can't get dp audio compatiable node");

		goto return_funcs;
	}

	pPlat_dev = of_find_device_by_node(pAud_nd);
	if (pPlat_dev == NULL) {
		dptx_err("can't get dp audio device");

		goto return_funcs;
	}

	pstHmdi_Codec_Priv->pstAud_ops = (struct tcc_dptx_audio_ops *)pPlat_dev->dev.platform_data;

return_funcs:
	return iRetVal;
}

static int dptx_audin_hw_params(struct device *dev,
											void *data,
											struct hdmi_codec_daifmt *daifmt,
											struct hdmi_codec_params *params)
{
	uint8_t ucDp_Idx;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	struct Dptx_Hmdi_Codec_Priv *pstHmdi_Codec_Priv;
	struct tcc_audio_params staud_params;

	pstHmdi_Codec_Priv = (struct Dptx_Hmdi_Codec_Priv *)data;

	if (pstHmdi_Codec_Priv->pstAud_ops == NULL) {
		dptx_err("Audio ops isn't available");

		goto return_funcs;
	}

	if (daifmt->fmt != HDMI_I2S) {
		dptx_err("Invalid audio format %d", daifmt->fmt);

		goto return_funcs;
	}

	staud_params.sample_rate = params->sample_rate;
	staud_params.data_width = 16;//params->sample_width;
	staud_params.channels = params->channels;

	ucDp_Idx = pstHmdi_Codec_Priv->ucStream_Idx;

	iRetVal = pstHmdi_Codec_Priv->pstAud_ops->set_audio_params(ucDp_Idx, &staud_params);
	if (iRetVal != 0) {
		dptx_err("DP %d: failed to set codec params", ucDp_Idx);

		goto return_funcs;
	}

	iRetVal = pstHmdi_Codec_Priv->pstAud_ops->enable_audio(ucDp_Idx);
	if (iRetVal != 0) {
		dptx_err("DP %d: failed to set enable", ucDp_Idx);

		goto return_funcs;
	}

	dptx_dbg("DP %d", ucDp_Idx);
	dptx_dbg("Format %d", daifmt->fmt);
	dptx_dbg("Sample rate: %u", staud_params.sample_rate);
	dptx_dbg("Data width: %u", staud_params.data_width);
	dptx_dbg("Channels: %u", staud_params.channels);

return_funcs:
	return iRetVal;
}

static void dptx_audin_shutdown(struct device *dev, void *data)
{
	uint8_t ucDp_Idx;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	struct Dptx_Hmdi_Codec_Priv *pstHmdi_Codec_Priv;

	pstHmdi_Codec_Priv = (struct Dptx_Hmdi_Codec_Priv *)data;

	if (pstHmdi_Codec_Priv->pstAud_ops == NULL) {
		dptx_err("Audio ops isn't available");

		goto return_funcs;
	}

	ucDp_Idx = pstHmdi_Codec_Priv->ucStream_Idx;

	iRetVal = pstHmdi_Codec_Priv->pstAud_ops->disable_audio(ucDp_Idx);
	if (iRetVal != 0) {
		dptx_err("DP %d: failed to disable audio", ucDp_Idx);

		goto return_funcs;
	}

	dptx_dbg("DP %d : shut down", ucDp_Idx);

return_funcs:
	return;
}

static int dptx_audin_mute(struct device *dev, void *data, bool enable)
{
	uint8_t ucDp_Idx;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	struct Dptx_Hmdi_Codec_Priv *pstHmdi_Codec_Priv;

	pstHmdi_Codec_Priv = (struct Dptx_Hmdi_Codec_Priv *)data;

	if (pstHmdi_Codec_Priv->pstAud_ops == NULL) {
		dptx_err("Audio ops isn't available");

		goto return_funcs;
	}

	ucDp_Idx = pstHmdi_Codec_Priv->ucStream_Idx;

	iRetVal = pstHmdi_Codec_Priv->pstAud_ops->set_audio_mute(ucDp_Idx, enable);
	if (iRetVal != 0) {
		dptx_err("DP %d: failed to audio %s", ucDp_Idx, enable ? "mute":"unmute");

		goto return_funcs;
	}

	dptx_dbg("DP %d : audio %s", ucDp_Idx, enable ? "mute":"unmute");

return_funcs:
	return iRetVal;
}

static int dptx_audin_get_eld(struct device *dev,
												void *data,
												u8 *buf,
												size_t len)
{
	uint8_t ucDp_Idx;
	size_t ulEdid_max;
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	struct Dptx_Hmdi_Codec_Priv *pstHmdi_Codec_Priv;
	struct Dptx_Params *pstDptx;

	pstHmdi_Codec_Priv = (struct Dptx_Hmdi_Codec_Priv *)data;
	pstDptx = pstHmdi_Codec_Priv->pstDptx;

	if (pstHmdi_Codec_Priv->pstAud_ops == NULL) {
		dptx_err("Audio ops isn't available");

		goto return_funcs;
	}

	ucDp_Idx = pstHmdi_Codec_Priv->ucStream_Idx;

	ulEdid_max = (size_t)DPTX_EDID_BUFLEN;

	memcpy(buf, pstDptx->paucEdidBuf_Entry[ucDp_Idx], min(ulEdid_max, len));

	dptx_dbg("DP %d: edid buf len as %lu", ucDp_Idx, len);

return_funcs:
	return iRetVal;
}



static const struct hdmi_codec_ops hdmi_audio_codec_ops = {
	.hw_params = dptx_audin_hw_params,
	.audio_shutdown = dptx_audin_shutdown,
	.digital_mute = dptx_audin_mute,
	.get_eld = dptx_audin_get_eld,
};

struct hdmi_codec_pdata hdmi_codec_data = {
	.i2s = 1,
	.spdif = 0,
	.ops = &hdmi_audio_codec_ops,
	.max_i2s_channels = 8,
};

int32_t Dptx_AudIn_Register(struct Dptx_Params *pstDptx)
{
	int32_t iRetVal = DPTX_RETURN_NO_ERROR;
	uint8_t ucStream_Max = 0;
	uint8_t ucIdx;
	struct Dptx_Hmdi_Codec_Priv *pstHmdi_Codec_Priv;

	ucStream_Max = (uint8_t)PHY_INPUT_STREAM_MAX;

	(void)memset(&stHmdi_Codec_Priv[PHY_INPUT_STREAM_0], 0, sizeof(struct Dptx_Hmdi_Codec_Priv) * ucStream_Max);

	for (ucIdx = 0; ucIdx < pstDptx->ucNumOfPorts; ucIdx++) {
		pstHmdi_Codec_Priv = &stHmdi_Codec_Priv[ucIdx];

		(void)dptx_audin_get_ops(pstHmdi_Codec_Priv);

		pstHmdi_Codec_Priv->ucStream_Idx = ucIdx;
		pstHmdi_Codec_Priv->pstDptx = pstDptx;

		hdmi_codec_data.data = (void *)pstHmdi_Codec_Priv;

		pstHmdi_Codec_Priv->pstAud_dev = platform_device_register_data(pstDptx->dev,
														HDMI_CODEC_DRV_NAME,
														PLATFORM_DEVID_AUTO,
														 &hdmi_codec_data,
														 sizeof(hdmi_codec_data));
	}

	return iRetVal;
}


