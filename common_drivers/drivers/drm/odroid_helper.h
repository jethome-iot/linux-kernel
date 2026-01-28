#ifndef __ODROID_HELPER_H
#define __ODROID_HELPER_H

#include <drm/amlogic/meson_connector_dev.h>

extern struct hdmi_timing *odroid_custom_timing(void);
extern int drm_display_mode_from_modeline(
		char *modeline, struct drm_display_mode *mode);
extern int append_replace_drm_display_mode(
		struct drm_connector *connector,
		struct drm_display_mode *mode);

extern int update_odroid_custom_hdmi_timing(
		struct drm_connector *connector,
		const char *modename);

const char *odroid_preferred_display_mode(void);
extern int odroid_set_preferred_mode(
		struct drm_connector *connector,
		const char *name);
extern int odroid_force_single_display_mode(void);

extern int load_odroid_modelines(struct drm_connector *connector);
extern int load_odroid_modeline_from_commandline(struct drm_connector *connector);
extern int load_odroid_display_mode_from_dt(struct drm_connector *connector,
		struct device_node *node);
#endif
