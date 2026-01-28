#include <linux/fs.h>
#include <linux/gcd.h>
#include <video/display_timing.h>

#include "odroid_helper.h"

/*
 * Parse decimal string into fixed-point interger
 *
 * example:
 *    parse_string_to_fixed("12.345", 3) -> 12345
 *    parse_string_to_fixed("12.345", 6) -> 12345000
 */
static unsigned int parse_string_to_fixed(char *str, int digits)
{
	unsigned int ipart;
	unsigned int fpart = 0;
	unsigned int len;
	char buf[4] = { 0, };
	char *p;

	if (!str || digits == 0)
		return 0;

	p = memscan(str, '.', strlen(str));
	if (*p == '.') {
		*p++ = '\0';

		len = strnlen(p, digits);
		memcpy(buf, p, len);

		fpart = simple_strtoul(buf, NULL, 10);
		if (fpart) {
			for (; len < digits; len++)
				fpart *= 10;
		}
	}

	ipart = simple_strtoul(str, NULL, 10);
	if (ipart) {
		for (len = 0; len < digits; len++)
			ipart *= 10;
	}

	return ipart + fpart;
}

int drm_display_mode_from_modeline(
		char *modeline, struct drm_display_mode *mode)
{
	char *str;
	char *argv[32];
	int argc = 0;
	int pos = 0;

	/* Empty string */
	if (!modeline || strlen(modeline) == 0)
		return -EINVAL;

	/* skip spaces */
	while ((*modeline == ' ') || (*modeline == '\t'))
		modeline++;

	/* Maybe comment line */
	if (*modeline == '#')
		return -EINVAL;

	while ((str = strsep(&modeline, " ,\t")) != NULL)
		argv[argc++] = str;

	/* May start with 'Modeline' */
	if (!strcasecmp(argv[pos], "modeline"))
		pos++;

	/* Video timing name */
	if (argv[pos][0] == '"') {
		char *end;

		strncpy(mode->name, argv[pos] + 1, DRM_DISPLAY_MODE_LEN);

		end = strchr(mode->name, '"');
		if (end)
			*end = '\0';
	} else {
		strncpy(mode->name, argv[pos], DRM_DISPLAY_MODE_LEN);
	}

	mode->name[DRM_DISPLAY_MODE_LEN - 1] = '\0';

	pos++;

	/* The number of rest tokens must be 9 at least */
	if ((argc - pos) < 9)
		return -EINVAL;

	mode->type = DRM_MODE_TYPE_USERDEF;
	mode->status = MODE_OK;
	mode->hskew = 0;
	mode->vscan = 0;
	mode->flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC;

	/* Pixel clock */
	mode->clock = parse_string_to_fixed(argv[pos++], 3);

	/* Horizontal timing */
	mode->hdisplay = simple_strtoul(argv[pos++], NULL, 10);
	mode->hsync_start = simple_strtoul(argv[pos++], NULL, 10);
	mode->hsync_end = simple_strtoul(argv[pos++], NULL, 10);
	mode->htotal = simple_strtoul(argv[pos++], NULL, 10);

	/* Vertical timing */
	mode->vdisplay = simple_strtoul(argv[pos++], NULL, 10);
	mode->vsync_start = simple_strtoul(argv[pos++], NULL, 10);
	mode->vsync_end = simple_strtoul(argv[pos++], NULL, 10);
	mode->vtotal = simple_strtoul(argv[pos++], NULL, 10);

	/* H/V sync polarity & scan mode */
	while (pos < argc) {
		char *str = argv[pos++];

		if (!strcasecmp(str + 1, "hsync")) {
			mode->flags &= ~(DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PHSYNC);
			mode->flags |= (str[0] == '-')
				? DRM_MODE_FLAG_NHSYNC : DRM_MODE_FLAG_PHSYNC;
		}
		else if (!strcasecmp(str + 1, "vsync")) {
			mode->flags &= ~(DRM_MODE_FLAG_NVSYNC | DRM_MODE_FLAG_PVSYNC);
			mode->flags |= (str[0] == '-')
				? DRM_MODE_FLAG_NVSYNC : DRM_MODE_FLAG_PVSYNC;
		}
		else if (!strcasecmp(str, "interlaced"))
			mode->flags |= DRM_MODE_FLAG_INTERLACE;
		else if (!strcasecmp(str, "progressive"))
			mode->flags &= ~DRM_MODE_FLAG_INTERLACE;
	}

	return 0;
}

static int hdmi_timing_from_drm_display_mode(
		struct hdmi_timing *ht, struct drm_display_mode *mode)
{
	int div;

	ht->name = mode->name;
	ht->sname = NULL;

	ht->pixel_freq = mode->clock;

	ht->h_total = mode->htotal;
	ht->h_active = mode->hdisplay;
	ht->h_blank = mode->htotal - mode->hdisplay;
	ht->h_front = mode->hsync_start - mode->hdisplay;
	ht->h_sync = mode->hsync_end - mode->hsync_start;
	ht->h_back = mode->htotal - mode->hsync_end;

	ht->v_total = mode->vtotal;
	ht->v_active = mode->vdisplay;
	ht->v_blank = mode->vtotal - mode->vdisplay;
	ht->v_front = mode->vsync_start - mode->vdisplay;
	ht->v_sync = mode->vsync_end - mode->vsync_start;
	ht->v_back = mode->vtotal - mode->vsync_end;

	ht->h_pol = !!(mode->flags & DRM_MODE_FLAG_PHSYNC);
	ht->v_pol = !!(mode->flags & DRM_MODE_FLAG_PVSYNC);
	ht->pi_mode = !(mode->flags & DRM_MODE_FLAG_INTERLACE);

	div = gcd(ht->h_active, ht->v_active);
	ht->h_pict = ht->h_active / div;
	ht->v_pict = ht->v_active / div;

	// FIXME: not being used in HDMI driver
	ht->v_sync_ln = 1;
	ht->h_pixel = 1;
	ht->v_pixel = 1;
	ht->pixel_repetition_factor = 0;

	return 0;
}

int update_odroid_custom_hdmi_timing(struct drm_connector *connector, const char *modename)
{
	struct drm_display_mode *pmode, *pt;

	list_for_each_entry_safe(pmode, pt, &connector->modes, head) {
		if ((pmode->type & DRM_MODE_TYPE_USERDEF)
				&& !strcmp(pmode->name, modename)) {
			struct hdmi_timing *ht = odroid_custom_timing();
			if (ht) {
				struct hdmi_timing hdmi_timing = {
					.vic = HDMI_ODROID_CUSTOM_VIDEO,
					.name = NULL,
				};
				hdmi_timing_from_drm_display_mode(&hdmi_timing, pmode);
				memcpy(ht, &hdmi_timing, sizeof(struct hdmi_timing));
				return 0;
			}
		}
	}

	return -ENODEV;
}

int append_replace_drm_display_mode(struct drm_connector *connector,
		struct drm_display_mode *dm)
{
	struct drm_display_mode *pmode, *pt;
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, dm);
	if (!mode)
		return -ENOMEM;

	/* remove previous video mode with the same name */
	list_for_each_entry_safe(pmode, pt, &connector->probed_modes, head) {
		if (!strcmp(pmode->name, mode->name)) {
			list_del(&pmode->head);
			drm_mode_destroy(connector->dev, pmode);
			break;
		}
	}

	/* append new video mode */
	drm_mode_probed_add(connector, mode);
	drm_mode_debug_printmodeline(mode);

	return 0;
}

int odroid_set_preferred_mode(struct drm_connector *connector, const char *name)
{
	struct drm_display_mode *pmode, *pt;

	list_for_each_entry_safe(pmode, pt, &connector->probed_modes, head) {
		pmode->type &= ~DRM_MODE_TYPE_PREFERRED;
		if (!strcmp(pmode->name, name)) {
			pmode->type |= DRM_MODE_TYPE_PREFERRED;
		} else {
			if (odroid_force_single_display_mode()) {
				list_del(&pmode->head);
				drm_mode_destroy(connector->dev, pmode);
			}
		}
	}

	return -EINVAL;
}
