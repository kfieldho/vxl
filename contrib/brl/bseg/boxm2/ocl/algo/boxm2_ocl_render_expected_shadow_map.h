#ifndef boxm2_ocl_render_expected_shadow_map_h_
#define boxm2_ocl_render_expected_shadow_map_h_
//:
// \file
#include <bocl/bocl_device.h>
#include <bocl/bocl_kernel.h>

//boxm2 includes
#include <boxm2/boxm2_scene.h>
#include <boxm2/boxm2_block.h>
#include <boxm2/ocl/boxm2_opencl_cache.h>
#include <boxm2/io/boxm2_cache.h>

#include <vil/vil_image_view.h>

class boxm2_ocl_render_expected_shadow_map
{
public:
  static bool render(bocl_device_sptr device, boxm2_scene_sptr scene, boxm2_opencl_cache_sptr opencl_cache,
                     vpgl_camera_double_sptr cam, unsigned ni, unsigned nj, vcl_string ident, vil_image_view<float> &rendered_img);

private:

  static vcl_map<vcl_string,vcl_vector<bocl_kernel*> > kernels_;

  static void compile_kernel(bocl_device_sptr device,vcl_vector<bocl_kernel*> & vec_kernels, vcl_string opts);
};


#endif // boxm2_ocl_render_expected_shadow_map_h_
