// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/drm_atomic_helper.h>

#include "meson_fb.h"
#include "meson_vpu.h"

static const struct drm_format_info meson_formats[] = {
	{ .format = DRM_FORMAT_ABGR10101010,	.depth = 40, .num_planes = 1,
		.cpp = { 5, 0, 0 }, .hsub = 1, .vsub = 1, .has_alpha = true },
	{ .format = DRM_FORMAT_YUVX1010102,	.depth = 32, .num_planes = 1,
		.cpp = { 4, 0, 0 }, .hsub = 1, .vsub = 1, .is_yuv = true },
};

void am_meson_fb_destroy(struct drm_framebuffer *fb)
{
	struct am_meson_fb *meson_fb = to_am_meson_fb(fb);
	int i;

	for (i = 0; i < AM_MESON_GEM_OBJECT_NUM && meson_fb->bufp[i]; i++)
		drm_gem_object_put(&meson_fb->bufp[i]->base);
	drm_framebuffer_cleanup(fb);
	if (meson_fb->logo && meson_fb->logo->alloc_flag)
		am_meson_free_logo_memory();
	DRM_DEBUG("meson_fb=0x%p,\n", meson_fb);
	kfree(meson_fb);
}

int am_meson_fb_create_handle(struct drm_framebuffer *fb,
			      struct drm_file *file_priv,
	     unsigned int *handle)
{
	struct am_meson_fb *meson_fb = to_am_meson_fb(fb);

	if (meson_fb->bufp[0])
		return drm_gem_handle_create(file_priv,
				     &meson_fb->bufp[0]->base, handle);
	else
		return -EFAULT;
}

struct drm_framebuffer_funcs am_meson_fb_funcs = {
	.create_handle = am_meson_fb_create_handle, //must for fbdev emulate
	.destroy = am_meson_fb_destroy,
};

struct drm_framebuffer *
am_meson_fb_alloc(struct drm_device *dev,
		  struct drm_mode_fb_cmd2 *mode_cmd,
		  struct drm_gem_object *obj)
{
	struct am_meson_fb *meson_fb;
	struct am_meson_gem_object *meson_gem;
	int ret = 0;

	meson_fb = kzalloc(sizeof(*meson_fb), GFP_KERNEL);
	if (!meson_fb)
		return ERR_PTR(-ENOMEM);

	if (obj) {
		meson_gem = container_of(obj, struct am_meson_gem_object, base);
		meson_fb->bufp[0] = meson_gem;
	} else {
		meson_fb->bufp[0] = NULL;
	}
	drm_helper_mode_fill_fb_struct(dev, &meson_fb->base, mode_cmd);

	ret = drm_framebuffer_init(dev, &meson_fb->base,
				   &am_meson_fb_funcs);
	if (ret) {
		dev_err(dev->dev, "Failed to initialize framebuffer: %d\n",
			ret);
		goto err_free_fb;
	}
	DRM_DEBUG("meson_fb[id:%d,ref:%d]=0x%p,meson_fb->bufp[0]=0x%p\n",
		 meson_fb->base.base.id,
		 kref_read(&meson_fb->base.base.refcount),
		 meson_fb, meson_fb->bufp[0]);

	return &meson_fb->base;

err_free_fb:
	kfree(meson_fb);
	return ERR_PTR(ret);
}

struct drm_framebuffer *am_meson_fb_create(struct drm_device *dev,
					   struct drm_file *file_priv,
				     const struct drm_mode_fb_cmd2 *mode_cmd)
{
	struct am_meson_fb *meson_fb = 0;
	struct drm_gem_object *obj = 0;
	struct am_meson_gem_object *meson_gem;
	int ret, i;

	meson_fb = kzalloc(sizeof(*meson_fb), GFP_KERNEL);
	if (!meson_fb)
		return ERR_PTR(-ENOMEM);

	/* support multi handle .*/
	for (i = 0; i < AM_MESON_GEM_OBJECT_NUM && mode_cmd->handles[i]; i++) {
		obj = drm_gem_object_lookup(file_priv, mode_cmd->handles[i]);
		if (!obj) {
			dev_err(dev->dev, "Failed to lookup GEM handle\n");
			kfree(meson_fb);
			return ERR_PTR(-ENOMEM);
		}
		meson_gem = container_of(obj, struct am_meson_gem_object, base);
		meson_fb->bufp[i] = meson_gem;
	}

	drm_helper_mode_fill_fb_struct(dev, &meson_fb->base, mode_cmd);

	ret = drm_framebuffer_init(dev, &meson_fb->base, &am_meson_fb_funcs);
	if (ret) {
		dev_err(dev->dev,
			"Failed to initialize framebuffer: %d\n",
			ret);
		drm_gem_object_put(obj);
		kfree(meson_fb);
		return ERR_PTR(ret);
	}
	DRM_DEBUG("meson_fb[id:%d,ref:%d]=0x%px,meson_fb->bufp[%d-0]=0x%p\n",
		  meson_fb->base.base.id,
		  kref_read(&meson_fb->base.base.refcount),
		  meson_fb, i, meson_fb->bufp[0]);

	return &meson_fb->base;
}

struct drm_framebuffer *
am_meson_drm_framebuffer_init(struct drm_device *dev,
			      struct drm_mode_fb_cmd2 *mode_cmd,
			      struct drm_gem_object *obj)
{
	struct drm_framebuffer *fb;

	fb = am_meson_fb_alloc(dev, mode_cmd, obj);
	if (IS_ERR(fb))
		return NULL;

	return fb;
}

int am_meson_mode_rmfb(struct drm_device *dev, u32 fb_id,
		  struct drm_file *file_priv)
{
	struct drm_framebuffer *fb = NULL;
	struct drm_framebuffer *fbl = NULL;
	int found = 0;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EOPNOTSUPP;

	fb = drm_framebuffer_lookup(dev, file_priv, fb_id);
	if (!fb)
		return -ENOENT;

	mutex_lock(&file_priv->fbs_lock);
	list_for_each_entry(fbl, &file_priv->fbs, filp_head)
		if (fb == fbl)
			found = 1;
	if (!found) {
		mutex_unlock(&file_priv->fbs_lock);
		goto fail_unref;
	}

	list_del_init(&fb->filp_head);
	mutex_unlock(&file_priv->fbs_lock);

	/* drop the reference we picked up in framebuffer lookup */
	drm_framebuffer_put(fb);

	/*
	 * we now own the reference that was stored in the fbs list
	 *
	 * drm_framebuffer_remove may fail with -EINTR on pending signals,
	 * so run this in a separate stack as there's no way to correctly
	 * handle this after the fb is already removed from the lookup table.
	 */
	DRM_DEBUG("fb=%px, fbid=%d, ref=%d\n", fb,
		fb->base.id, drm_framebuffer_read_refcount(fb));

	drm_framebuffer_put(fb);

	return 0;

fail_unref:
	drm_framebuffer_put(fb);
	return -ENOENT;
}

int am_meson_mode_rmfb_ioctl(struct drm_device *dev,
			void *data, struct drm_file *file_priv)
{
	u32 *fb_id = data;

	return am_meson_mode_rmfb(dev, *fb_id, file_priv);
}

static const struct drm_format_info *
lookup_format_info(const struct drm_format_info formats[],
		   int num_formats, u32 format)
{
	int i;

	for (i = 0; i < num_formats; i++) {
		if (formats[i].format == format)
			return &formats[i];
	}

	return NULL;
}

const struct drm_format_info *
am_meson_get_format_info(const struct drm_mode_fb_cmd2 *cmd)
{
	return lookup_format_info(meson_formats,
				  ARRAY_SIZE(meson_formats),
				  cmd->pixel_format);
}

static int fb_plane_width(int width,
			  const struct drm_format_info *format, int plane)
{
	if (plane == 0)
		return width;

	return DIV_ROUND_UP(width, format->hsub);
}

static int fb_plane_height(int height,
			   const struct drm_format_info *format, int plane)
{
	if (plane == 0)
		return height;

	return DIV_ROUND_UP(height, format->vsub);
}

static int meson_framebuffer_check(struct drm_device *dev,
			     const struct drm_mode_fb_cmd2 *r)
{
	const struct drm_format_info *info;
	int i;

	/* check if the format is supported at all */
	if (!drm_get_format_info(dev, r)) {
		DRM_DEBUG_KMS("bad framebuffer format %d\n",
			      r->pixel_format);
		return -EINVAL;
	}

	if (r->width == 0) {
		DRM_DEBUG_KMS("bad framebuffer width %u\n", r->width);
		return -EINVAL;
	}

	if (r->height == 0) {
		DRM_DEBUG_KMS("bad framebuffer height %u\n", r->height);
		return -EINVAL;
	}

	/* now let the driver pick its own format info */
	info = drm_get_format_info(dev, r);

	for (i = 0; i < info->num_planes; i++) {
		unsigned int width = fb_plane_width(r->width, info, i);
		unsigned int height = fb_plane_height(r->height, info, i);
		unsigned int block_size = info->char_per_block[i];
		u64 min_pitch = drm_format_info_min_pitch(info, i, width);

		if (!block_size && r->modifier[i] == DRM_FORMAT_MOD_LINEAR) {
			DRM_DEBUG_KMS("Format requires non-linear modifier for plane %d\n", i);
			return -EINVAL;
		}

		if (!r->handles[i]) {
			DRM_DEBUG_KMS("no buffer object handle for plane %d\n", i);
			return -EINVAL;
		}

		if (min_pitch > UINT_MAX)
			return -ERANGE;

		if ((uint64_t)height * r->pitches[i] + r->offsets[i] > UINT_MAX)
			return -ERANGE;

		if (block_size && r->pitches[i] < min_pitch) {
			DRM_DEBUG_KMS("bad pitch %u for plane %d %lld\n",
				r->pitches[i], i, min_pitch);
			return -EINVAL;
		}

		if (r->modifier[i] && !(r->flags & DRM_MODE_FB_MODIFIERS)) {
			DRM_DEBUG_KMS("bad fb modifier %llu for plane %d\n",
				      r->modifier[i], i);
			return -EINVAL;
		}

		if (r->flags & DRM_MODE_FB_MODIFIERS &&
		    r->modifier[i] != r->modifier[0]) {
			DRM_DEBUG_KMS("bad fb modifier %llu for plane %d\n",
				      r->modifier[i], i);
			return -EINVAL;
		}

		/* modifier specific checks: */
		switch (r->modifier[i]) {
		case DRM_FORMAT_MOD_SAMSUNG_64_32_TILE:
			/* NOTE: the pitch restriction may be lifted later if it turns
			 * out that no hw has this restriction:
			 */
			if (r->pixel_format != DRM_FORMAT_NV12 ||
					width % 128 || height % 32 ||
					r->pitches[i] % 128) {
				DRM_DEBUG_KMS("bad modifier data for plane %d\n", i);
				return -EINVAL;
			}
			break;

		default:
			break;
		}
	}

	for (i = info->num_planes; i < 4; i++) {
		if (r->modifier[i]) {
			DRM_DEBUG_KMS("non-zero modifier for unused plane %d\n", i);
			return -EINVAL;
		}

		/* Pre-FB_MODIFIERS userspace didn't clear the structs properly. */
		if (!(r->flags & DRM_MODE_FB_MODIFIERS))
			continue;

		if (r->handles[i]) {
			DRM_DEBUG_KMS("buffer object handle for unused plane %d\n", i);
			return -EINVAL;
		}

		if (r->pitches[i]) {
			DRM_DEBUG_KMS("non-zero pitch for unused plane %d\n", i);
			return -EINVAL;
		}

		if (r->offsets[i]) {
			DRM_DEBUG_KMS("non-zero offset for unused plane %d\n", i);
			return -EINVAL;
		}
	}

	return 0;
}

static struct drm_framebuffer *
meson_internal_framebuffer_create(struct drm_device *dev,
				const struct drm_mode_fb_cmd2 *r,
				struct drm_file *file_priv)
{
	struct drm_mode_config *config = &dev->mode_config;
	struct drm_framebuffer *fb;
	int ret;

	if (r->flags & ~(DRM_MODE_FB_INTERLACED | DRM_MODE_FB_MODIFIERS)) {
		DRM_DEBUG_KMS("bad framebuffer flags 0x%08x\n", r->flags);
		return ERR_PTR(-EINVAL);
	}

	if (config->min_width > r->width || r->width > config->max_width) {
		DRM_DEBUG_KMS("bad framebuffer width %d, should be >= %d && <= %d\n",
			  r->width, config->min_width, config->max_width);
		return ERR_PTR(-EINVAL);
	}
	if (config->min_height > r->height || r->height > config->max_height) {
		DRM_DEBUG_KMS("bad framebuffer height %d, should be >= %d && <= %d\n",
			  r->height, config->min_height, config->max_height);
		return ERR_PTR(-EINVAL);
	}

	if (r->flags & DRM_MODE_FB_MODIFIERS &&
	    !dev->mode_config.allow_fb_modifiers) {
		DRM_DEBUG_KMS("driver does not support fb modifiers\n");
		return ERR_PTR(-EINVAL);
	}

	ret = meson_framebuffer_check(dev, r);
	if (ret)
		return ERR_PTR(ret);

	fb = dev->mode_config.funcs->fb_create(dev, file_priv, r);
	if (IS_ERR(fb)) {
		DRM_DEBUG_KMS("could not create framebuffer\n");
		return fb;
	}

	return fb;
}

static int meson_mode_addfb2(struct drm_device *dev,
		    void *data, struct drm_file *file_priv)
{
	struct drm_mode_fb_cmd2 *r = data;
	struct drm_framebuffer *fb;

	if (!drm_core_check_feature(dev, DRIVER_MODESET))
		return -EOPNOTSUPP;

	fb = meson_internal_framebuffer_create(dev, r, file_priv);
	if (IS_ERR(fb))
		return PTR_ERR(fb);

	DRM_DEBUG_KMS("[FB:%d]\n", fb->base.id);
	r->fb_id = fb->base.id;

	/* Transfer ownership to the filp for reaping on close */
	mutex_lock(&file_priv->fbs_lock);
	list_add(&fb->filp_head, &file_priv->fbs);
	mutex_unlock(&file_priv->fbs_lock);

	return 0;
}

int am_meson_mode_addfb2_ioctl(struct drm_device *dev,
			  void *data, struct drm_file *file_priv)
{
#ifdef __BIG_ENDIAN
	if (!dev->mode_config.quirk_addfb_prefer_host_byte_order) {
		/*
		 * Drivers must set the
		 * quirk_addfb_prefer_host_byte_order quirk to make
		 * the drm_mode_addfb() compat code work correctly on
		 * bigendian machines.
		 *
		 * If they don't they interpret pixel_format values
		 * incorrectly for bug compatibility, which in turn
		 * implies the ADDFB2 ioctl does not work correctly
		 * then.  So block it to make userspace fallback to
		 * ADDFB.
		 */
		DRM_DEBUG_KMS("addfb2 broken on bigendian");
		return -EOPNOTSUPP;
	}
#endif

	return meson_mode_addfb2(dev, data, file_priv);
}
