// This is brl/bseg/bvpl/kernels/pro/processes/bvpl_create_corner2d_kernel_process.cxx
#include <bprb/bprb_func_process.h>
//:
// \file
// \brief A class for creating 2d-corner kernel
// \author Isabel Restrepo
// \date September 15, 2009
// \verbatim
//  Modifications
//   <none yet>
// \endverbatim

#include <bprb/bprb_parameters.h>
#include <brdb/brdb_value.h>
#include <bvpl/kernels/bvpl_corner2d_kernel_factory.h>
#include <bvpl/kernels/bvpl_create_directions.h>


namespace bvpl_create_corner2d_kernel_process_globals
{
  const unsigned n_inputs_ = 0;
  const unsigned n_outputs_ = 1;
}


bool bvpl_create_corner2d_kernel_process_cons(bprb_func_process& pro)
{
  using namespace bvpl_create_corner2d_kernel_process_globals;

  //process takes 0 inputs and 1 output
  vcl_vector<vcl_string> input_types_(n_inputs_);
  vcl_vector<vcl_string> output_types_(n_outputs_);
  output_types_[0] ="bvpl_kernel_sptr";

  return pro.set_input_types(input_types_) && pro.set_output_types(output_types_);
}

bool bvpl_create_corner2d_kernel_process(bprb_func_process& pro)
{
  using namespace bvpl_create_corner2d_kernel_process_globals;

  if (pro.n_inputs() != n_inputs_)
  {
    vcl_cout << pro.name() << " The input number should be " << n_inputs_<< vcl_endl;
    return false;
  }

  //get inputs from params:
  unsigned int length = 5;
  pro.parameters()->get_value("length", length);
  unsigned int width = 5;
  pro.parameters()->get_value("width", width);
  unsigned int thickness = 5;
  pro.parameters()->get_value("thickness", thickness);

  float axis_x = 1.0f;
  pro.parameters()->get_value("axis_x", axis_x);
  float axis_y = 0.0f;
  pro.parameters()->get_value("axis_y", axis_y);
  float axis_z = 0.0f;
  pro.parameters()->get_value("axis_z", axis_z);
  float angle= 0.0f;
  pro.parameters()->get_value("angle", angle);

  vnl_float_3 axis(axis_x,axis_y, axis_z);

  //Create the factory and get the vector of kernels
  bvpl_corner2d_kernel_factory factory(length,width,thickness);

  factory.set_rotation_axis(axis);
  factory.set_angle(angle);

  bvpl_kernel_sptr kernel_sptr = new bvpl_kernel(factory.create());
  vcl_cout << "Creating corner kernel with axis " << kernel_sptr->axis() << " and angle " << kernel_sptr->angle() << vcl_endl
           << "length: " << length << ", width: " << width << ", thickness: " << thickness << vcl_endl;
  //kernel_sptr->print();
  pro.set_output_val<bvpl_kernel_sptr>(0, kernel_sptr);

  return true;
}

