// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2026 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <drm/drm_probe_helper.h>

struct sim {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct gpio_desc *reset_gpio;
};

static inline struct sim *to_sim(struct drm_panel *panel)
{
	return container_of_const(panel, struct sim, panel);
}

static void sim_reset(struct sim *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	usleep_range(1000, 2000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
}

static int sim_on(struct sim *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_dcs_set_display_brightness_multi(&dsi_ctx, 0x00cc);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_WRITE_CONTROL_DISPLAY,
				     0x2c);
	mipi_dsi_dcs_write_seq_multi(&dsi_ctx, MIPI_DCS_WRITE_POWER_SAVE, 0x01);
	mipi_dsi_dcs_set_display_on_multi(&dsi_ctx);
	mipi_dsi_dcs_exit_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 100);

	return dsi_ctx.accum_err;
}

static int sim_off(struct sim *ctx)
{
	struct mipi_dsi_multi_context dsi_ctx = { .dsi = ctx->dsi };

	mipi_dsi_dcs_set_display_off_multi(&dsi_ctx);
	mipi_dsi_usleep_range(&dsi_ctx, 10000, 11000);
	mipi_dsi_dcs_enter_sleep_mode_multi(&dsi_ctx);
	mipi_dsi_msleep(&dsi_ctx, 120);

	return dsi_ctx.accum_err;
}

static int sim_prepare(struct drm_panel *panel)
{
	struct sim *ctx = to_sim(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	sim_reset(ctx);

	ret = sim_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	return 0;
}

static int sim_unprepare(struct drm_panel *panel)
{
	struct sim *ctx = to_sim(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	ret = sim_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	return 0;
}

static const struct drm_display_mode sim_mode = {
	.clock = (720 + 328 + 4 + 32) * (1520 + 6 + 2 + 8) * 60 / 1000,
	.hdisplay = 720,
	.hsync_start = 720 + 328,
	.hsync_end = 720 + 328 + 4,
	.htotal = 720 + 328 + 4 + 32,
	.vdisplay = 1520,
	.vsync_start = 1520 + 6,
	.vsync_end = 1520 + 6 + 2,
	.vtotal = 1520 + 6 + 2 + 8,
	.width_mm = 64,
	.height_mm = 135,
	.type = DRM_MODE_TYPE_DRIVER,
};

static int sim_get_modes(struct drm_panel *panel,
			 struct drm_connector *connector)
{
	return drm_connector_helper_get_modes_fixed(connector, &sim_mode);
}

static const struct drm_panel_funcs sim_panel_funcs = {
	.prepare = sim_prepare,
	.unprepare = sim_unprepare,
	.get_modes = sim_get_modes,
};

static int sim_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct sim *ctx;
	int ret;

	ctx = devm_drm_panel_alloc(dev, struct sim, panel,
				   &sim_panel_funcs,
				   DRM_MODE_CONNECTOR_DSI);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS |
			  MIPI_DSI_MODE_VIDEO_NO_HFP | MIPI_DSI_MODE_LPM;

	ctx->panel.prepare_prev_first = true;

	ret = drm_panel_of_backlight(&ctx->panel);
	if (ret)
		return dev_err_probe(dev, ret, "Failed to get backlight\n");

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		drm_panel_remove(&ctx->panel);
		return dev_err_probe(dev, ret, "Failed to attach to DSI host\n");
	}

	return 0;
}

static void sim_remove(struct mipi_dsi_device *dsi)
{
	struct sim *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id sim_of_match[] = {
	{ .compatible = "motorola,deen-nova-hd" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, sim_of_match);

static struct mipi_dsi_driver sim_driver = {
	.probe = sim_probe,
	.remove = sim_remove,
	.driver = {
		.name = "panel-sim",
		.of_match_table = sim_of_match,
	},
};
module_mipi_dsi_driver(sim_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for mipi_mot_vid_nova_hd_586");
MODULE_LICENSE("GPL");
