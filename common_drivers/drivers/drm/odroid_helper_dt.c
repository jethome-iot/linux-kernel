#include <linux/of.h>

#include <video/of_display_timing.h>
#include <video/videomode.h>

#include "odroid_helper.h"

int load_odroid_display_mode_from_dt(struct drm_connector *connector,
		struct device_node *node)
{
	struct display_timings *disp;
	struct device_node *timings_np;
	int i;
	int count = 0;

	if (!node)
		return 0;

	timings_np = of_get_child_by_name(node, "display-timings");
	if (!timings_np)
		return 0;

	disp = of_get_display_timings(node);
	if (!disp)
		return 0;

	for (i = 0; i < disp->num_timings; i++) {
		struct drm_display_mode mode;
		int ret;

		ret = of_get_drm_display_mode(node, &mode, NULL, i);
		if (ret)
			continue;

		mode.type = DRM_MODE_TYPE_USERDEF;
		mode.hskew = 0;
		mode.vscan = 0;

		ret = append_replace_drm_display_mode(connector, &mode);
		if (ret == 0)
			count++;
	}

	return count;
}
