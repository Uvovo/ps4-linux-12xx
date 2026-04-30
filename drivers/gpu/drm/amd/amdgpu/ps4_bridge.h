#ifndef __AMDGPU_PS4_BRIDGE_H__
#define __AMDGPU_PS4_BRIDGE_H__

struct drm_connector;
struct drm_display_mode;
struct drm_encoder;

int ps4_bridge_get_modes(struct drm_connector *connector);
enum drm_connector_status ps4_bridge_detect(struct drm_connector *connector,
					    bool force);
enum drm_mode_status ps4_bridge_mode_valid(struct drm_connector *connector,
					   const struct drm_display_mode *mode);
int ps4_bridge_register(struct drm_connector *connector,
			struct drm_encoder *encoder);

#endif
