#include "odroid_helper.h"

#define DISPLAY_TIMING_FILENAME		"display-timings.txt"

static const char * const display_timings[] = {
	"/fat/"DISPLAY_TIMING_FILENAME,
	"/boot/"DISPLAY_TIMING_FILENAME,
	"/usr/lib/firmware/"DISPLAY_TIMING_FILENAME
};

int load_odroid_modelines(struct drm_connector *connector)
{
	struct drm_display_mode mode;
	struct file *file = NULL;
	unsigned long len = 4096;
	loff_t pos = 0;
	char *buf;
	char *str;
	int ret;
	int count = 0;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(display_timings); i++) {
		file = filp_open(display_timings[i], O_RDONLY, 0);
		if (!IS_ERR(file))
			break;
	}

	if (IS_ERR(file))
		return PTR_ERR(file);

	buf = kzalloc(len, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	ret = kernel_read(file, buf, len, &pos);
	if (ret <= 0)
		goto err_load_odroid_modelines;

	while ((str = strsep(&buf, "\n")) != NULL) {
		ret = drm_display_mode_from_modeline(str, &mode);
		if (ret)
			continue;

		ret = append_replace_drm_display_mode(connector, &mode);
		if (!ret)
			count++;
	}

err_load_odroid_modelines:
	filp_close(file, NULL);
	kfree(buf);

	return count;
}

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
