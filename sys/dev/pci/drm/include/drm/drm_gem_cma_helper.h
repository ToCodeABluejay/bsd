/* Public Domain */

#include <drm/drmP.h>
#include <drm/drm_gem.h>

void drm_gem_cma_free_object(struct drm_gem_object *);
int drm_gem_cma_dumb_create(struct drm_file *, struct drm_device *,
    struct drm_mode_create_dumb *);
int drm_gem_cma_dumb_map_offset(struct drm_file *, struct drm_device *,
    uint32_t, uint64_t *);
struct drm_gem_cma_object *drm_gem_cma_create(struct drm_device *,
    size_t);

extern const struct uvm_pagerops drm_gem_cma_uvm_ops;

struct drm_gem_cma_object {
	struct drm_gem_object	base;
	bus_dma_tag_t		dmat;
	bus_dmamap_t		dmamap;
	bus_dma_segment_t	dmasegs[1];
	size_t			dmasize;
	caddr_t			vaddr;
	struct sg_table		*sgt;
};

#define to_drm_gem_cma_obj(gem_obj) container_of(gem_obj, struct drm_gem_cma_object, base)
