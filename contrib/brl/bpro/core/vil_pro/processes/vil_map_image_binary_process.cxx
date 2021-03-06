// This is brl/bpro/core/vil_pro/processes/vil_map_image_binary_process.cxx
#include <bprb/bprb_func_process.h>
//:
// \file

#include "../vil_math_functors.h"
#include <vil/vil_convert.h>
#include <bprb/bprb_parameters.h>
#include <vcl_iostream.h>
#include <vcl_string.h>
#include <vil/vil_image_view_base.h>
#include <vil/vil_transform.h>
#include <vil/vil_math.h>

//: Constructor
bool vil_map_image_binary_process_cons(bprb_func_process& pro)
{
  //input
  bool ok=false;
  vcl_vector<vcl_string> input_types;
  input_types.push_back("vil_image_view_base_sptr");
  input_types.push_back("vil_image_view_base_sptr");
  input_types.push_back("vcl_string");
  ok = pro.set_input_types(input_types);
  if (!ok) return ok;

  //output
  vcl_vector<vcl_string> output_types;
  output_types.push_back("vil_image_view_base_sptr");
  ok = pro.set_output_types(output_types);
  if (!ok) return ok;
  return true;
}


//: Execute the process
bool vil_map_image_binary_process(bprb_func_process& pro)
{
  // Sanity check
  if (pro.n_inputs()< 3) {
    vcl_cout << "vil_map_image_binary_process: The input number should be 3" << vcl_endl;
    return false;
  }

  // get the inputs
  unsigned i=0;
  //Retrieve image from input
  vil_image_view_base_sptr image1 = pro.get_input<vil_image_view_base_sptr>(i++);

  //Retrieve image from input
  vil_image_view_base_sptr image2 = pro.get_input<vil_image_view_base_sptr>(i++);

  //Retrieve map functor name
  vcl_string functor = pro.get_input<vcl_string>(i++);

  // convert image to float
  vil_image_view_base_sptr fimage1 = vil_convert_cast(float(), image1);
  vil_image_view<float>fimg1 = *fimage1;
  vil_image_view_base_sptr fimage2 = vil_convert_cast(float(), image2);
  vil_image_view<float>fimg2 = *fimage2;

  vil_image_view<float> dest(fimg1.ni(), fimg1.nj(), fimg1.nplanes());
  // apply the functor
  if (functor == "add"){
    vcl_cout << "in add\n";
    vil_math_add_functor af;
    vil_transform(fimg1, fimg2, dest, af);
  }
  else{
    vcl_cerr << "In vil_map_image_binary_process::execute() - unknown functor "
             << functor << '\n';
    return false;
  }

  //for now, only handle byte or float outputs
  vil_image_view_base_sptr map_image;
  if (image1->pixel_format()==VIL_PIXEL_FORMAT_BYTE)
  {
    vil_image_view<vxl_byte>* temp = new vil_image_view<vxl_byte>;
    vil_convert_stretch_range(dest, *temp);
    map_image = temp;
  }
  else //leave the image as float
    map_image = new vil_image_view<float>(dest);

  pro.set_output_val<vil_image_view_base_sptr>(0, map_image);
  return true;
}

