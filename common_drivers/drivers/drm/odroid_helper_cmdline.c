#include <linux/slab.h>
#include <linux/module.h>

#include "odroid_helper.h"

static struct drm_display_mode mode;

static int modeline_set(const char *val, const struct kernel_param *kp)
{
	char *param;

	param = kstrdup(val, GFP_KERNEL);
	if (!param)
		return -ENOMEM;

	drm_display_mode_from_modeline(param, &mode);

	kfree(param);

	return 0;
}

static int modeline_get(char *buffer, const struct kernel_param *kp)
{
	return 0;
}

static const struct kernel_param_ops modeline_ops = {
	.set = modeline_set,
	.get = modeline_get,
};

module_param_cb(modeline, &modeline_ops, NULL, 0644);
__MODULE_PARM_TYPE(modeline, "charp");

int load_odroid_modeline_from_commandline(struct drm_connector *connector)
{
	int ret = append_replace_drm_display_mode(connector,
			drm_mode_duplicate(connector->dev, &mode));

	return (ret == 0) ? 1 : 0;
}

static char modename[DRM_DISPLAY_MODE_LEN];
static int force = 0;

const char *odroid_preferred_display_mode(void)
{
	return modename;
}

int odroid_force_single_display_mode(void)
{
	return force;
}

static int displaymode_set(const char *val, const struct kernel_param *kp)
{
	char *str = kstrdup(val, GFP_KERNEL);
	char *p;

	p = memscan(str, ':', strlen(str));
	if (p) {
		*p++ = '\0';

		if (!strcmp(p, "force"))
			force = 1;
	}

	strncpy(modename, str, DRM_DISPLAY_MODE_LEN);

	kfree(str);

	return 0;
}

static int displaymode_get(char *buffer, const struct kernel_param *kp)
{
	return 0;
}

static const struct kernel_param_ops displaymode_ops = {
	.set = displaymode_set,
	.get = displaymode_get,
};

module_param_cb(displaymode, &displaymode_ops, NULL, 0644);
__MODULE_PARM_TYPE(displaymode, "charp");
