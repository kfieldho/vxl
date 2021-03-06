// This is brl/bseg/boxm2/vecf/ocl/pro/processes/boxm2_vecf_ocl_cache_neighbor_info_process.cxx
//:
// \file
// \brief  Cache neighborhood info needed for filtering and transforming
//
// \author Daniel Crispell
// \date 12 June 2015

#include <bprb/bprb_func_process.h>
#include <boxm2/ocl/boxm2_opencl_cache.h>
#include <boxm2/boxm2_scene.h>
#include <boxm2/vecf/ocl/boxm2_vecf_ocl_store_nbrs.h>

namespace boxm2_vecf_ocl_cache_neighbor_info_process_globals
{
  const unsigned n_inputs_ = 2;
  const unsigned n_outputs_ = 0;
}

bool boxm2_vecf_ocl_cache_neighbor_info_process_cons(bprb_func_process& pro)
{
  using namespace boxm2_vecf_ocl_cache_neighbor_info_process_globals;

  //process takes 2 inputs
  vcl_vector<vcl_string> input_types_(n_inputs_);
  input_types_[0] = "boxm2_scene_sptr";     // source scene
  input_types_[1] = "boxm2_opencl_cache_sptr";
  // process has 0 outputs:
  vcl_vector<vcl_string>  output_types_(n_outputs_);
  return pro.set_input_types(input_types_) && pro.set_output_types(output_types_);
}

bool boxm2_vecf_ocl_cache_neighbor_info_process(bprb_func_process& pro)
{
  using namespace boxm2_vecf_ocl_cache_neighbor_info_process_globals;
  if ( pro.n_inputs() < n_inputs_ ){
    vcl_cout << pro.name() << ": The number of inputs should be " << n_inputs_<< vcl_endl;
    return false;
  }
  vcl_cout << "Getting the inputs" << vcl_endl;
  //get the inputs
  boxm2_scene_sptr scene = pro.get_input<boxm2_scene_sptr>(0);
  boxm2_opencl_cache_sptr cache = pro.get_input<boxm2_opencl_cache_sptr>(1);

  vcl_cout << "creating store_nbrs object" << vcl_endl;
  boxm2_vecf_ocl_store_nbrs store_nbrs(scene, cache);
  vcl_cout << "calling augment_1_blk" << vcl_endl;
  store_nbrs.augment_1_blk();
  vcl_cout << "Done. returning." << vcl_endl;

  return true;
}
