//:
// \file
#include "boxm_ocl_change_detection_manager.h"
#include <vcl_where_root_dir.h>
#include <boxm/ocl/boxm_ocl_utils.h>
#include <vcl_cstdio.h>
#include <vul/vul_timer.h>
#include <boxm/boxm_block.h>
#include <boxm/boxm_scene.h>
#include <boxm/util/boxm_utils.h>
#include <boxm/basic/boxm_block_vis_graph_iterator.h>
#include <vil/vil_save.h>
#include <vil/vil_convert.h>

//: Initializes CPU side input buffers
// Put tree structure and data into arrays
bool boxm_ocl_change_detection_manager::init_ray_trace(boxm_ocl_scene *scene,
                                                       vpgl_camera_double_sptr cam,
                                                       vil_image_view<float> &obs,
                                                       vil_image_view<float> &cd)
{
  scene_ = scene;
  cam_ = cam;
  obs_img_ =obs;
  output_img_=cd;

//   Code for Pass_0
  vcl_string source_dir = vcl_string(VCL_SOURCE_ROOT_DIR) + "/contrib/brl/bseg/boxm/ocl/cl/";
  if (!this->load_kernel_source(source_dir+"loc_code_library_functions.cl") ||
      !this->append_process_kernels(source_dir+"cell_utils.cl") ||
      !this->append_process_kernels(source_dir+"octree_library_functions.cl") ||
      !this->append_process_kernels(source_dir+"backproject.cl")||
      !this->append_process_kernels(source_dir+"statistics_library_functions.cl")||
      !this->append_process_kernels(source_dir+"expected_functor.cl")||
      !this->append_process_kernels(source_dir+"ray_bundle_library_functions.cl")||
      !this->append_process_kernels(source_dir+"change_detection_ocl_scene.cl")) {
    vcl_cerr << "Error: boxm_ocl_change_detection_manager : failed to load kernel source (helper functions)\n";
    return false;
  }

  return !build_kernel_program(program_);
}

bool boxm_ocl_change_detection_manager::init_ray_trace(boxm_ocl_scene *scene,
                                                       unsigned ni,
                                                       unsigned nj)
{
  scene_ = scene;
  cam_=new vpgl_proj_camera<double>();
  obs_img_.set_size(ni,nj);
  output_img_.set_size(ni,nj);

  // Code for Pass_0
  vcl_string source_dir = vcl_string(VCL_SOURCE_ROOT_DIR) + "/contrib/brl/bseg/boxm/ocl/cl/";
  if (!this->load_kernel_source(source_dir+"loc_code_library_functions.cl") ||
      !this->append_process_kernels(source_dir+"cell_utils.cl") ||
      !this->append_process_kernels(source_dir+"octree_library_functions.cl") ||
      !this->append_process_kernels(source_dir+"backproject.cl")||
      !this->append_process_kernels(source_dir+"statistics_library_functions.cl")||
      !this->append_process_kernels(source_dir+"expected_functor.cl")||
      !this->append_process_kernels(source_dir+"ray_bundle_library_functions.cl")||
      !this->append_process_kernels(source_dir+"change_detection_ocl_scene.cl")) {
    vcl_cerr << "Error: boxm_ocl_change_detection_manager : failed to load kernel source (helper functions)\n";
    return false;
  }

  return !build_kernel_program(program_);
}

bool boxm_ocl_change_detection_manager::start()
{
  bool good=true;
  good = good && this->set_scene_data()
              && this->set_all_blocks()
              && this->set_scene_data_buffers()
              && this->set_tree_buffers()

              && this->set_input_view()
              && this->set_input_view_buffers();

  this->set_kernel();
  this->set_commandqueue();
  this->set_workspace();
  this->set_args();
  return good;
}

bool boxm_ocl_change_detection_manager::change_detection(vpgl_camera_double_sptr cam,
                                                         vil_image_view_base_sptr img,
                                                         vil_image_view_base_sptr& output)
{
    if (vpgl_proj_camera<double>* pcam = dynamic_cast<vpgl_proj_camera<double> *> (cam.ptr()))
    {
        //load image from file
        vil_image_view<float> floatimg(img->ni(), img->nj(), 1);
        if (vil_image_view<vxl_byte> *img_byte = dynamic_cast<vil_image_view<vxl_byte>*>(img.ptr()))
            vil_convert_stretch_range_limited(*img_byte ,floatimg, vxl_byte(0), vxl_byte(255), 0.0f, 1.0f);
        else {
            vcl_cerr << "Failed to load image\n";
            return 0;
        }
        //run the opencl update business

        this->set_input_image(floatimg);
        this->write_image_buffer();
        this->set_persp_camera(pcam);
        this->write_persp_camera_buffers();
        this->set_image_dims_buffers();
        this->run();
        this->read_output_image();
        output=this->get_output_image();
        vil_image_view<float> * output_float=static_cast<vil_image_view<float> *>(output.ptr());
        vil_image_view<vxl_byte> * byteimg=new vil_image_view<vxl_byte>(img->ni(), img->nj(), 1);
        vil_convert_stretch_range_limited<float>(*output_float,*byteimg,0.0f,1.0f,0,255);

        output=byteimg;
    }
    return false;
}

bool boxm_ocl_change_detection_manager::finish()
{
  return
      this->release_commandqueue()
   && this->release_kernel()
   && this->release_foreground_pdf_buffers()
   && this->clean_foreground_pdf()
   && this->release_input_image_buffers()
   && this->clean_input_image()
   && this->release_persp_camera_buffers()
   && this->clean_persp_camera()
   && this->release_scene_data_buffers()
   && this->clean_scene_data()
   && this->release_tree_buffers()
   && this->clean_tree();
}

//: update the tree
bool boxm_ocl_change_detection_manager::set_kernel()
{
  cl_int status = CL_SUCCESS;
  kernel_ = clCreateKernel(program_,"change_detection_ocl_scene",&status);
  if (!check_val(status,CL_SUCCESS,error_to_string(status))) {
    return false;
  }
  return true;
}


bool boxm_ocl_change_detection_manager::release_kernel()
{
  if (kernel_)
  {
    cl_int status = clReleaseKernel(kernel_);
    if (!check_val(status,CL_SUCCESS,error_to_string(status)))
      return false;
  }
  return true;
}


bool boxm_ocl_change_detection_manager::set_args()
{
  if (!kernel_)
    return false;
  cl_int status = CL_SUCCESS;
  int i=0;
  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&scene_dims_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (scene_dims_buf_)"))
    return SDK_FAILURE;
  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&scene_origin_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (scene_orign_buf_)"))
      return SDK_FAILURE;
  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&block_dims_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (block_dims_buf_)"))
    return SDK_FAILURE;
  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&block_ptrs_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (block_ptrs_buf_)"))
    return SDK_FAILURE;
  // root level buffer
  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&root_level_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (root_level_buf_)"))
    return SDK_FAILURE;
  // the length of buffer
  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&numbuffer_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (lenbuffer_buf_)"))
    return SDK_FAILURE;
  // the length of buffer
  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&lenbuffer_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (lenbuffer_buf_)"))
    return SDK_FAILURE;
  // the tree buffer
  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&cells_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (cells_buf_)"))
    return SDK_FAILURE;
  // data buffer
  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&cell_data_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (cell_data_buf_)"))
    return SDK_FAILURE;
  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&cell_alpha_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (cell_data_buf_)"))
      return SDK_FAILURE;

  // camera buffer
  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&persp_cam_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (data)"))
    return SDK_FAILURE;
  // roi dimensions
  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&img_dims_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (Img dimensions)"))
    return SDK_FAILURE;
  //// output image buffer
  status = clSetKernelArg(kernel_,i++,3*sizeof(cl_float16),0);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (local cam)"))
    return SDK_FAILURE;
  status = clSetKernelArg(kernel_,i++,sizeof(cl_uint4),0);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (local image dimensions)"))
    return SDK_FAILURE;
  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&int_image_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (int_image_buf_)"))
    return SDK_FAILURE;

  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&image_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (input_image)"))
    return SDK_FAILURE;

  image_gl_buf_ = clCreateBuffer(this->context_,
                                 CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                 wni_*wnj_*sizeof(cl_uint),
                                 image_gl_,&status);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (gl_image)"))
      return SDK_FAILURE;


  status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&image_gl_buf_);
  if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (gl_image)"))
    return SDK_FAILURE;
  //status = clSetKernelArg(kernel_,i++,sizeof(cl_mem),(void *)&foreground_pdf_buf_);
  //if (!check_val(status,CL_SUCCESS,"clSetKernelArg failed. (foreground_pdf)"))
  //  return SDK_FAILURE;

  return SDK_SUCCESS;
}


bool boxm_ocl_change_detection_manager::set_commandqueue()
{
  cl_int status = CL_SUCCESS;
  command_queue_ = clCreateCommandQueue(this->context(),this->devices()[0],CL_QUEUE_PROFILING_ENABLE,&status);
  if (!check_val(status,CL_SUCCESS,"Falied in command queue creation" + error_to_string(status)))
    return false;

  return true;
}


bool boxm_ocl_change_detection_manager::release_commandqueue()
{
  if (command_queue_)
  {
    cl_int status = clReleaseCommandQueue(command_queue_);
    if (!check_val(status,CL_SUCCESS,"clReleaseCommandQueue failed."))
      return false;
  }
  return true;
}


bool boxm_ocl_change_detection_manager::set_workspace()
{
  cl_int status = CL_SUCCESS;

  // check the local memeory
  cl_ulong used_local_memory;
  status = clGetKernelWorkGroupInfo(this->kernel(),this->devices()[0],
                                    CL_KERNEL_LOCAL_MEM_SIZE,
                                    sizeof(cl_ulong),&used_local_memory,NULL);
  if (!check_val(status,CL_SUCCESS,"clGetKernelWorkGroupInfo CL_KERNEL_LOCAL_MEM_SIZE failed."))
    return 0;

  // determine the work group size
  cl_ulong kernel_work_group_size;
  status = clGetKernelWorkGroupInfo(this->kernel(),this->devices()[0],CL_KERNEL_WORK_GROUP_SIZE,
                                    sizeof(cl_ulong),&kernel_work_group_size,NULL);
  if (!check_val(status,CL_SUCCESS,"clGetKernelWorkGroupInfo CL_KERNEL_WORK_GROUP_SIZE, failed."))
    return 0;

  globalThreads[0]=this->wni_;
  globalThreads[1]=this->wnj_;

  localThreads[0]=this->bni_;
  localThreads[1]=this->bnj_;

  if (used_local_memory > this->total_local_memory())
  {
    vcl_cout << "Unsupported: Insufficient local memory on device.\n";
    return 0;
  }
  else
    return 1;
}


bool boxm_ocl_change_detection_manager::run()
{
  cl_int status = CL_SUCCESS;

  // set up a command queue

  cl_event ceEvent;
  status = clEnqueueNDRangeKernel(command_queue_,this->kernel_, 2,NULL,globalThreads,localThreads,0,NULL,&ceEvent);

  if (!check_val(status,CL_SUCCESS,"clEnqueueNDRangeKernel failed. "+error_to_string(status)))
    return SDK_FAILURE;

  status = clFinish(command_queue_);
  if (!check_val(status,CL_SUCCESS,"clFinish failed."+error_to_string(status)))
    return SDK_FAILURE;
  cl_ulong tstart,tend;
  status = clGetEventProfilingInfo(ceEvent,CL_PROFILING_COMMAND_END,sizeof(cl_ulong),&tend,0);
  status = clGetEventProfilingInfo(ceEvent,CL_PROFILING_COMMAND_START,sizeof(cl_ulong),&tstart,0);
  gpu_time_= 1e-6f * (tend - tstart); // convert nanoseconds to milliseconds
  vcl_cout<<"GPU time is "<<gpu_time_<<vcl_endl;
  return SDK_SUCCESS;
}


bool boxm_ocl_change_detection_manager::run_scene()
{
  bool good=true;
  vcl_string error_message="";
  vul_timer timer;
  good=good && set_scene_data();
  good=good && set_all_blocks();
  good=good && set_scene_data_buffers();
  good=good && set_tree_buffers();
  // run the raytracing
  good=good && set_input_view();
  good=good && set_input_view_buffers();
  this->set_kernel();
  this->set_args();
  this->set_commandqueue();
  this->set_workspace();

  this->run();

  good =good && release_tree_buffers();
  good =good && clean_tree();
  good=good && read_output_image();
  //this->print_image();
  good=good && release_input_view_buffers();
  //good=good && clean_input_view();
  good=good && release_scene_data_buffers();
  good=good && clean_scene_data();

  // release the command Queue

  this->release_kernel();
#ifdef DEBUG
  vcl_cout << "Timing Analysis\n"
           << "===============\n"
           <<"openCL Running time "<<gpu_time_<<" ms\n"
           << "Running block "<<total_gpu_time/1000<<"s\n"
           << "total block loading time = " << total_load_time << "s\n"
           << "total block processing time = " << total_raytrace_time << 's' << vcl_endl;
#endif
  return true;
}


bool boxm_ocl_change_detection_manager:: read_output_image()
{
  cl_event events[2];

  // Enqueue readBuffers
  int status = clEnqueueReadBuffer(command_queue_,image_buf_,CL_TRUE,
                                   0,this->wni_*this->wnj_*sizeof(cl_float4),
                                   image_,
                                   0,NULL,&events[0]);

  if (!check_val(status,CL_SUCCESS,"clEnqueueBuffer (image_)failed."))
    return false;

  // Wait for the read buffer to finish execution
  status = clWaitForEvents(1, &events[0]);
  if (!check_val(status,CL_SUCCESS,"clWaitForEvents failed."))
    return false;

  status = clReleaseEvent(events[0]);
  return check_val(status,CL_SUCCESS,"clReleaseEvent failed.")==1;
}

void boxm_ocl_change_detection_manager::save_image(vcl_string img_filename)
{
  vil_image_view<float> oimage(output_img_.ni(),output_img_.nj());

  for (unsigned i=0;i<output_img_.ni();i++)
    for (unsigned j=0;j<output_img_.nj();j++)
      oimage(i,j)=image_[(j*wni_+i)*4];

  vil_save(oimage,img_filename.c_str());
}

vil_image_view_base_sptr boxm_ocl_change_detection_manager::get_output_image()
{
  vil_image_view<float> * oimage=new vil_image_view<float>(output_img_.ni(),output_img_.nj());

  for (unsigned i=0;i<output_img_.ni();i++)
    for (unsigned j=0;j<output_img_.nj();j++)
      (*oimage)(i,j)=image_[(j*wni_+i)*4];

  return oimage;
}

bool boxm_ocl_change_detection_manager:: read_trees()
{
  cl_event events[2];

  // Enqueue readBuffers
  int status = clEnqueueReadBuffer(command_queue_,cells_buf_,CL_TRUE,
                                   0,cells_size_*sizeof(cl_int4),
                                   cells_,
                                   0,NULL,&events[0]);

  if (!check_val(status,CL_SUCCESS,"clEnqueueBuffer (cells )failed."))
    return false;
  status = clWaitForEvents(1, &events[0]);
  if (!check_val(status,CL_SUCCESS,"clWaitForEvents failed."))
    return false;

  status = clEnqueueReadBuffer(command_queue_,cell_data_buf_,CL_TRUE,
                               0,cell_data_size_*sizeof(cl_uchar8),
                               cell_mixture_,
                               0,NULL,&events[0]);

  if (!check_val(status,CL_SUCCESS,"clEnqueueBuffer (cell data )failed."))
    return false;
  status = clEnqueueReadBuffer(command_queue_,cell_alpha_buf_,CL_TRUE,
                               0,cell_data_size_*sizeof(cl_float),
                               cell_alpha_,
                               0,NULL,&events[0]);

  if (!check_val(status,CL_SUCCESS,"clEnqueueBuffer (cell data )failed."))
    return false;
  status = clWaitForEvents(1, &events[0]);
  if (!check_val(status,CL_SUCCESS,"clWaitForEvents failed."))
    return false;

  status = clWaitForEvents(1, &events[0]);
  if (!check_val(status,CL_SUCCESS,"clWaitForEvents failed."))
    return false;
  // Wait for the read buffer to finish execution

  status = clReleaseEvent(events[0]);
  return check_val(status,CL_SUCCESS,"clReleaseEvent failed.")==1;
}


void boxm_ocl_change_detection_manager::print_tree()
{
  vcl_cout << "Tree Input\n";
  if (cells_)
    for (unsigned i = 0; i<cells_size_*4; i+=4) {
      int data_ptr = 16*cells_[i+2];
      vcl_cout << "tree input[" << i/4 << "]("
               << cells_[i]   << ' '
               << cells_[i+1] << ' '
               << cells_[i+2] << ' '
               << cells_[i+3];
      if (data_ptr>0)
        vcl_cout << '[' << cell_mixture_[data_ptr] << ','
                 << cell_mixture_[data_ptr+1] << ','
                 << cell_mixture_[data_ptr+2] << ','
                 << cell_mixture_[data_ptr+3] << ','
                 << cell_mixture_[data_ptr+4] << ','
                 << cell_mixture_[data_ptr+5] << ','
                 << cell_mixture_[data_ptr+6] << ','
                 << cell_mixture_[data_ptr+7] <<']';

      vcl_cout << ")\n";
    }
}


void boxm_ocl_change_detection_manager::print_image()
{
  vcl_cout << "IMage Output\n";
  if (image_)
  {
    vcl_cout<<"Plane 0"<<vcl_endl;
    for (unsigned j=0;j<img_dims_[3];j++)
    {
      for (unsigned i=0;i<img_dims_[2];i++)
        vcl_cout<<image_[(j*img_dims_[2]+i)*4]<<' ';
      vcl_cout<<vcl_endl;
    }
    vcl_cout<<"Plane 1"<<vcl_endl;
    for (unsigned j=0;j<img_dims_[3];j++)
    {
      for (unsigned i=0;i<img_dims_[2];i++)
        vcl_cout<<image_[(j*img_dims_[2]+i)*4+1]<<' ';
      vcl_cout<<vcl_endl;
    }
    vcl_cout<<"Plane 2"<<vcl_endl;
    for (unsigned j=0;j<img_dims_[3];j++)
    {
      for (unsigned i=0;i<img_dims_[2];i++)
        vcl_cout<<image_[(j*img_dims_[2]+i)*4+2]<<' ';
      vcl_cout<<vcl_endl;
    }
    vcl_cout<<"Plane 3"<<vcl_endl;
    for (unsigned j=0;j<img_dims_[3];j++)
    {
      for (unsigned i=0;i<img_dims_[2];i++)
        vcl_cout<<image_[(j*img_dims_[2]+i)*4+3]<<' ';
      vcl_cout<<vcl_endl;
    }
  }
}


bool boxm_ocl_change_detection_manager::clean_update()
{
  return true;
}

//: builds kernel program from source (a vcl_string)
int boxm_ocl_change_detection_manager::build_kernel_program(cl_program & program)
{
  cl_int status = CL_SUCCESS;
  vcl_size_t sourceSize[] = { this->prog_.size() };
  if (!sourceSize[0]) return SDK_FAILURE;
  if (program) {
    status = clReleaseProgram(program);
    program = 0;
    if (!check_val(status,
      CL_SUCCESS,
      "clReleaseProgram failed."))
      return SDK_FAILURE;
  }
  const char * source = this->prog_.c_str();

  program = clCreateProgramWithSource(this->context_,
                                      1,
                                      &source,
                                      sourceSize,
                                      &status);
  if (!check_val(status,
                       CL_SUCCESS,
                       "clCreateProgramWithSource failed."))
    return SDK_FAILURE;

  // create a cl program executable for all the devices specified
  status = clBuildProgram(program,
                          1,
                          this->devices(),
                          "",
                          NULL,
                          NULL);
  if (!check_val(status,
                       CL_SUCCESS,
                       error_to_string(status)))
  {
    vcl_size_t len;
    char buffer[2048];
    clGetProgramBuildInfo(program, this->devices()[0],
                          CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
    vcl_printf("%s\n", buffer);
    return SDK_FAILURE;
  }
  else
    return SDK_SUCCESS;
}


bool boxm_ocl_change_detection_manager::set_scene_data()
{
  return set_scene_dims()
      && set_scene_origin()
      && set_block_dims()
      && set_block_ptrs()
      && set_root_level();
}


bool boxm_ocl_change_detection_manager::clean_scene_data()
{
  return clean_scene_dims()
      && clean_scene_origin()
      && clean_block_dims()
      && clean_block_ptrs()
      && clean_root_level();
}


bool boxm_ocl_change_detection_manager::set_scene_data_buffers()
{
  return set_scene_dims_buffers()
      && set_scene_origin_buffers()
      && set_block_dims_buffers()
      && set_block_ptrs_buffers()
      && set_root_level_buffers();
}


bool boxm_ocl_change_detection_manager::release_scene_data_buffers()
{
  return release_scene_dims_buffers()
      && release_scene_origin_buffers()
      && release_block_dims_buffers()
      && release_block_ptrs_buffers()
      && release_root_level_buffers();
}


bool boxm_ocl_change_detection_manager::set_root_level()
{
  if (scene_==NULL)
  {
    vcl_cout<<"Scene is Missing"<<vcl_endl;
    return false;
  }
  root_level_=scene_->max_level()-1;
  return true;
}


bool boxm_ocl_change_detection_manager::clean_root_level()
{
  root_level_=0;
  return true;
}


bool boxm_ocl_change_detection_manager::set_scene_origin()
{
  if (scene_==NULL)
  {
    vcl_cout<<"Scene is Missing"<<vcl_endl;
    return false;
  }
  vgl_point_3d<double> orig=scene_->origin();

  scene_origin_=(cl_float *)boxm_ocl_utils::alloc_aligned(1,sizeof(cl_float4),16);

  scene_origin_[0]=(float)orig.x();
  scene_origin_[1]=(float)orig.y();
  scene_origin_[2]=(float)orig.z();
  scene_origin_[3]=0.0f;

  return true;
}


bool boxm_ocl_change_detection_manager::set_scene_origin_buffers()
{
  cl_int status;
  scene_origin_buf_ = clCreateBuffer(this->context_,
                                     CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                     4*sizeof(cl_float),
                                     scene_origin_,&status);
  return check_val(status,CL_SUCCESS,"clCreateBuffer (scene_origin_) failed.")==1;
}


bool boxm_ocl_change_detection_manager::release_scene_origin_buffers()
{
  cl_int status;
  status = clReleaseMemObject(scene_origin_buf_);
  return check_val(status,CL_SUCCESS,"clReleaseMemObject failed (scene_origin_buf_).")==1;
}


bool boxm_ocl_change_detection_manager::clean_scene_origin()
{
  if (scene_origin_)
    boxm_ocl_utils::free_aligned(scene_origin_);
  return true;
}


bool boxm_ocl_change_detection_manager::set_scene_dims()
{
  if (scene_==NULL)
  {
    vcl_cout<<"Scene is Missing"<<vcl_endl;
    return false;
  }
  scene_->block_num(scene_x_,scene_y_,scene_z_);
  scene_dims_=(cl_int *)boxm_ocl_utils::alloc_aligned(1,sizeof(cl_int4),16);

  scene_dims_[0]=scene_x_;
  scene_dims_[1]=scene_y_;
  scene_dims_[2]=scene_z_;
  scene_dims_[3]=0;

  return true;
}


bool boxm_ocl_change_detection_manager::set_scene_dims_buffers()
{
  cl_int status;
  scene_dims_buf_ = clCreateBuffer(this->context_,
                                   CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                   sizeof(cl_int4),
                                   scene_dims_,&status);
  return check_val(status,CL_SUCCESS,"clCreateBuffer (scene_dims_) failed.")==1;
}


bool boxm_ocl_change_detection_manager::release_scene_dims_buffers()
{
  cl_int status;
  status = clReleaseMemObject(scene_dims_buf_);
  return check_val(status,CL_SUCCESS,"clReleaseMemObject failed (scene_dims_buf_).")==1;
}


bool boxm_ocl_change_detection_manager::clean_scene_dims()
{
  if (scene_dims_)
    boxm_ocl_utils::free_aligned(scene_dims_);
  return true;
}


bool boxm_ocl_change_detection_manager::set_block_dims()
{
  if (scene_==NULL)
  {
    vcl_cout<<"Scene is Missing"<<vcl_endl;
    return false;
  }
  double x,y,z;
  scene_->block_dim(x,y,z);

  block_dims_=(cl_float *)boxm_ocl_utils::alloc_aligned(1,sizeof(cl_float4),16);

  block_dims_[0]=(float)x;
  block_dims_[1]=(float)y;
  block_dims_[2]=(float)z;
  block_dims_[3]=0;

  return true;
}


bool boxm_ocl_change_detection_manager::set_block_dims_buffers()
{
  cl_int status;
  block_dims_buf_ = clCreateBuffer(this->context_,
                                   CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                   4*sizeof(cl_float),
                                   block_dims_,&status);
  return check_val(status,CL_SUCCESS,"clCreateBuffer (block_dims_) failed.")==1;
}


bool boxm_ocl_change_detection_manager::release_block_dims_buffers()
{
  cl_int status;
  status = clReleaseMemObject(block_dims_buf_);
  return check_val(status,CL_SUCCESS,"clReleaseMemObject failed (block_dims_buf_).")==1;
}


bool boxm_ocl_change_detection_manager::clean_block_dims()
{
  if (block_dims_)
    boxm_ocl_utils::free_aligned(block_dims_);
  return true;
}


bool boxm_ocl_change_detection_manager::set_block_ptrs()
{
  if (scene_==NULL)
  {
    vcl_cout<<"Scene is Missing"<<vcl_endl;
    return false;
  }
  scene_->block_num(scene_x_,scene_y_,scene_z_);
  int numblocks=scene_x_*scene_y_*scene_z_;
  vcl_cout<<"Block size "<<(float)numblocks*16/1024.0/1024.0<<"MB"<<vcl_endl;

  block_ptrs_=(cl_int*)boxm_ocl_utils::alloc_aligned(numblocks,sizeof(cl_int4),16);
  scene_->get_block_ptrs(block_ptrs_);
  return true;
}


bool boxm_ocl_change_detection_manager::set_block_ptrs_buffers()
{
  cl_int status;
  block_ptrs_buf_ = clCreateBuffer(this->context_,
                                   CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                   scene_x_*scene_y_*scene_z_*sizeof(cl_int4),
                                   block_ptrs_,&status);
  return check_val(status,CL_SUCCESS,"clCreateBuffer (block_ptrs_) failed.")==1;
}


bool boxm_ocl_change_detection_manager::release_block_ptrs_buffers()
{
  cl_int status;
  status = clReleaseMemObject(block_ptrs_buf_);
  return check_val(status,CL_SUCCESS,"clReleaseMemObject failed (block_ptrs_buf_).")==1;
}


bool boxm_ocl_change_detection_manager::clean_block_ptrs()
{
  if (block_ptrs_)
    boxm_ocl_utils::free_aligned(block_ptrs_);
  return true;
}


bool boxm_ocl_change_detection_manager::set_root_level_buffers()
{
  cl_int status;
  root_level_buf_ = clCreateBuffer(this->context_,
                                   CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                   sizeof(cl_uint),
                                   &root_level_,&status);
  return check_val(status,CL_SUCCESS,"clCreateBuffer (root level) failed.")==1;
}


bool boxm_ocl_change_detection_manager::release_root_level_buffers()
{
  cl_int status;
  status = clReleaseMemObject(root_level_buf_);
  if (!check_val(status,CL_SUCCESS,"clReleaseMemObject failed (root_level_buf_)."))
    return false;
  return true;
}


bool boxm_ocl_change_detection_manager::set_input_view()
{
  return set_persp_camera()
      && set_input_image();
}


bool boxm_ocl_change_detection_manager::clean_input_view()
{
  return clean_persp_camera()
      && clean_input_image();
}


bool boxm_ocl_change_detection_manager::set_input_view_buffers()
{
  return set_persp_camera_buffers()
      && set_input_image_buffers()
      && set_image_dims_buffers();
}


bool boxm_ocl_change_detection_manager::release_input_view_buffers()
{
  return release_persp_camera_buffers()
      && release_input_image_buffers();
}


bool boxm_ocl_change_detection_manager::set_all_blocks()
{
  if (!scene_)
    return false;

  cells_ = NULL;
  cell_alpha_=NULL;
  cell_mixture_ = NULL;
  scene_->tree_buffer_shape(numbuffer_,lenbuffer_);
  cells_size_=numbuffer_*lenbuffer_;
  cell_data_size_=numbuffer_*lenbuffer_;

  /******* debug print **************/
  int cellBytes = sizeof(cl_int2);
  int dataBytes = sizeof(cl_uchar8)+sizeof(cl_float);
  vcl_cout<<"Optimized sizes:\n"
          <<"    cells: "<<(float)cells_size_*cellBytes/1024.0f/1024.0f<<"MB\n"
          <<"    data:  "<<(float)cells_size_*dataBytes/1024.0f/1024.0f<<"MB"<<vcl_endl;
  /**********************************/

  //allocate and initialize tree cells
  cells_=(cl_int *)boxm_ocl_utils::alloc_aligned(cells_size_,sizeof(cl_int2),16);
  scene_->get_tree_cells(cells_);

  //allocate and initialize alphas
  cell_alpha_=(cl_float *)boxm_ocl_utils::alloc_aligned(cell_data_size_,sizeof(cl_float),16);
  scene_->get_alphas(cell_alpha_);

  //allocate and initialize mix components
  cell_mixture_ = (cl_uchar *) boxm_ocl_utils::alloc_aligned(cell_data_size_,sizeof(cl_uchar8),16);
  scene_->get_mixture(cell_mixture_);

  if (cells_== NULL|| cell_mixture_ == NULL || cell_alpha_==NULL) {
    vcl_cout << "Failed to allocate host memory. (tree input)\n";
    return false;
  }
  return true;;
}


bool boxm_ocl_change_detection_manager::clean_tree()
{
  if (cells_)
    boxm_ocl_utils::free_aligned(cells_);
  if (cell_mixture_)
    boxm_ocl_utils::free_aligned(cell_mixture_);
  if (cell_alpha_)
    boxm_ocl_utils::free_aligned(cell_alpha_);

  lenbuffer_=0;
  numbuffer_=0;
  return true;
}


bool boxm_ocl_change_detection_manager::set_tree_buffers()
{
  cl_int status;
  cells_buf_ = clCreateBuffer(this->context_,
                              CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                              cells_size_*sizeof(cl_int2),
                              cells_,&status);
  if (!check_val(status,CL_SUCCESS,"clCreateBuffer (tree) failed."))
    return false;

  cell_data_buf_ = clCreateBuffer(this->context_,
                                  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                  cell_data_size_*sizeof(cl_uchar8),
                                  cell_mixture_,&status);
 if (!check_val(status,CL_SUCCESS,"clCreateBuffer (cell data) failed."))
    return false;

  cell_alpha_buf_ = clCreateBuffer(this->context_,
                                   CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                   cell_data_size_*sizeof(cl_float),
                                   cell_alpha_,&status);
  if (!check_val(status,CL_SUCCESS,"clCreateBuffer (cell data) failed."))
    return false;

  numbuffer_buf_=clCreateBuffer(this->context_,
                                CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                sizeof(cl_uint),
                                &numbuffer_,&status);
  if (!check_val(status,CL_SUCCESS,"clCreateBuffer (numbuffer_) failed."))
    return false;

  lenbuffer_buf_=clCreateBuffer(this->context_,
                                CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                sizeof(cl_uint),
                                &lenbuffer_,&status);
  if (!check_val(status,CL_SUCCESS,"clCreateBuffer (lenbuffer_) failed."))
    return false;
  /******* debug print ***************/
  int cellBytes = cells_size_*sizeof(cl_int2);
  int alphaBytes = cell_data_size_*sizeof(cl_float);
  int mixBytes = cell_data_size_*sizeof(cl_uchar8);
  int blockBytes = scene_x_*scene_y_*scene_z_*sizeof(cl_int4);
  float MB = (cellBytes + alphaBytes + mixBytes + blockBytes)/1024.0f/1024.0f;
  vcl_cout<<"GPU Mem allocated:\n"
          <<"   cells: "<<cellBytes<<" bytes\n"
          <<"   alpha: "<<alphaBytes<<" bytes\n"
          <<"   mix  : "<<mixBytes<<" bytes\n"
          <<"   block: "<<blockBytes<<" bytes\n"
          <<"TOTAL: "<<MB<<"MB"<<vcl_endl;
  /************************************/
 return true;
}


bool boxm_ocl_change_detection_manager::release_tree_buffers()
{
  cl_int status;
  status = clReleaseMemObject(lenbuffer_buf_);
  if (!check_val(status,CL_SUCCESS,"clReleaseMemObject failed (lenbuffer_buf_)."))
    return false;
  status = clReleaseMemObject(numbuffer_buf_);
  if (!check_val(status,CL_SUCCESS,"clReleaseMemObject failed (numbuffer_buf_)."))
    return false;
  status = clReleaseMemObject(cells_buf_);
  if (!check_val(status,CL_SUCCESS,"clReleaseMemObject failed (cells_buf_)."))
    return false;
  status = clReleaseMemObject(cell_data_buf_);
  if (!check_val(status,CL_SUCCESS,"clReleaseMemObject failed (cell_data_buf_)."))
    return false;
  status = clReleaseMemObject(cell_alpha_buf_);
  return check_val(status,CL_SUCCESS,"clReleaseMemObject failed (cell_alpha_buf_).");
}


bool boxm_ocl_change_detection_manager::set_persp_camera(vpgl_proj_camera<double> * pcam)
{
  if (pcam)
  {
    vnl_svd<double>* svd=pcam->svd();
    vnl_matrix<double> Ut=svd->U().conjugate_transpose();
    vnl_matrix<double> V=svd->V();
    vnl_vector<double> Winv=svd->Winverse().diagonal();
    persp_cam_=(cl_float *)boxm_ocl_utils::alloc_aligned(3,sizeof(cl_float16),16);

    int cnt=0;
    for (unsigned i=0;i<Ut.rows();i++)
    {
      for (unsigned j=0;j<Ut.cols();j++)
        persp_cam_[cnt++]=(cl_float)Ut(i,j);

      persp_cam_[cnt++]=0;
    }

    for (unsigned i=0;i<V.rows();i++)
      for (unsigned j=0;j<V.cols();j++)
        persp_cam_[cnt++]=(cl_float)V(i,j);

    for (unsigned i=0;i<Winv.size();i++)
      persp_cam_[cnt++]=(cl_float)Winv(i);

    vgl_point_3d<double> cam_center=pcam->camera_center();
    persp_cam_[cnt++]=(cl_float)cam_center.x();
    persp_cam_[cnt++]=(cl_float)cam_center.y();
    persp_cam_[cnt++]=(cl_float)cam_center.z();
    return true;
  }
  else {
    vcl_cerr << "Error set_persp_camera() : Missing camera\n";
    return false;
  }
}


bool boxm_ocl_change_detection_manager::set_persp_camera()
{
  if (vpgl_proj_camera<double>* pcam =
      dynamic_cast<vpgl_proj_camera<double>*>(cam_.ptr()))
  {
    vnl_svd<double>* svd=pcam->svd();

    vnl_matrix<double> Ut=svd->U().conjugate_transpose();
    vnl_matrix<double> V=svd->V();
    vnl_vector<double> Winv=svd->Winverse().diagonal();

    persp_cam_=(cl_float *)boxm_ocl_utils::alloc_aligned(3,sizeof(cl_float16),16);

    int cnt=0;
    for (unsigned i=0;i<Ut.rows();i++)
    {
      for (unsigned j=0;j<Ut.cols();j++)
        persp_cam_[cnt++]=(cl_float)Ut(i,j);

      persp_cam_[cnt++]=0;
    }

    for (unsigned i=0;i<V.rows();i++)
      for (unsigned j=0;j<V.cols();j++)
        persp_cam_[cnt++]=(cl_float)V(i,j);

    for (unsigned i=0;i<Winv.size();i++)
      persp_cam_[cnt++]=(cl_float)Winv(i);

    vgl_point_3d<double> cam_center=pcam->camera_center();
    persp_cam_[cnt++]=(cl_float)cam_center.x();
    persp_cam_[cnt++]=(cl_float)cam_center.y();
    persp_cam_[cnt++]=(cl_float)cam_center.z();
    return true;
  }
  else {
    vcl_cerr << "Error set_persp_camera() : unsupported camera type\n";
    return false;
  }
}


bool boxm_ocl_change_detection_manager::clean_persp_camera()
{
  if (persp_cam_)
    boxm_ocl_utils::free_aligned(persp_cam_);
  return true;
}


bool boxm_ocl_change_detection_manager::set_persp_camera_buffers()
{
  cl_int status;
  persp_cam_buf_ = clCreateBuffer(this->context_,
                                  CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                  3*sizeof(cl_float16),
                                  persp_cam_,&status);
  return check_val(status,CL_SUCCESS,"clCreateBuffer (persp_cam_buf_) failed.")==1;
}


bool boxm_ocl_change_detection_manager::write_persp_camera_buffers()
{
  cl_int status;
  status=clEnqueueWriteBuffer(command_queue_,persp_cam_buf_,CL_FALSE, 0,3*sizeof(cl_float16), persp_cam_, 0, 0, 0);
  if (!check_val(status,CL_SUCCESS,"clEnqueueWriteBuffer (persp_cam_buf_) failed."))
    return false;
  clFinish(command_queue_);

  return true;
}


bool boxm_ocl_change_detection_manager::release_persp_camera_buffers()
{
  cl_int status;
  status = clReleaseMemObject(persp_cam_buf_);
  return check_val(status,CL_SUCCESS,"clReleaseMemObject failed (persp_cam_buf_).")==1;
}

bool boxm_ocl_change_detection_manager::set_input_image(vil_image_view<float>  obs)
{
  for (unsigned i=0;i<wni_*wnj_;i++)
  {
      image_[i*4]=0;
      image_[i*4+1]=0;
      image_[i*4+2]=0;
      image_[i*4+3]=0;

      int_image_[i]=0.0;
  }
  // pad the image
  for (unsigned i=0;i<obs.ni();i++)
  {
    for (unsigned j=0;j<obs.nj();j++)
    {
      int_image_[(j*wni_+i)]=obs(i,j);
    }
  }
  return true;
}

bool boxm_ocl_change_detection_manager::write_image_buffer()
{
  cl_int status;
  status=clEnqueueWriteBuffer(command_queue_,int_image_buf_,CL_TRUE, 0,wni_*wnj_*sizeof(cl_float), int_image_, 0, 0, 0);
  if (!check_val(status,CL_SUCCESS,"clEnqueueWriteBuffer (image_buf_) failed."))
    return false;
  status=clFinish(command_queue_);
  if (!check_val(status,CL_SUCCESS,"clFinish (writing) failed."))
    return false;
  status=clEnqueueWriteBuffer(command_queue_,image_buf_,CL_TRUE, 0,wni_*wnj_*sizeof(cl_float4),image_, 0, 0, 0);
  if (!check_val(status,CL_SUCCESS,"clEnqueueWriteBuffer (image_buf_) failed."))
    return false;
  status=clFinish(command_queue_);
  if (!check_val(status,CL_SUCCESS,"clFinish (writing) failed."))
    return false;

  status=clEnqueueWriteBuffer(command_queue_,img_dims_buf_,CL_TRUE, 0,sizeof(cl_uint4), img_dims_, 0, 0, 0);
  if (!check_val(status,CL_SUCCESS,"clEnqueueWriteBuffer (imd_dims_buf_) failed."))
    return false;
  status=clFinish(command_queue_);

  return check_val(status,CL_SUCCESS,"clFinish (writing) failed.")==1;
}


bool boxm_ocl_change_detection_manager::set_input_image()
{
  wni_=(cl_uint)RoundUp(output_img_.ni(),bni_);
  wnj_=(cl_uint)RoundUp(output_img_.nj(),bnj_);

  int_image_=(cl_float*)boxm_ocl_utils::alloc_aligned(wni_*wnj_,sizeof(cl_float),16);
  image_    =(cl_float*)boxm_ocl_utils::alloc_aligned(wni_*wnj_,sizeof(cl_float4),16);
  image_gl_ =(cl_uint*)boxm_ocl_utils::alloc_aligned(wni_*wnj_,sizeof(cl_uint),16);

  img_dims_=(cl_uint *)boxm_ocl_utils::alloc_aligned(1,sizeof(cl_uint4),16);

  // pad the image
  for (unsigned i=0;i<obs_img_.ni();i++)
  {
    for (unsigned j=0;j<obs_img_.nj();j++)
    {
      image_[(j*wni_+i)*4]=0;
      image_[(j*wni_+i)*4+1]=0;
      image_[(j*wni_+i)*4+2]=0;
      image_[(j*wni_+i)*4+3]=0;

      image_gl_[(j*wni_+i)]=0;
      int_image_[(j*wni_+i)]=obs_img_(i,j);
    }
  }

  img_dims_[0]=0;
  img_dims_[1]=0;
  img_dims_[2]=output_img_.ni();
  img_dims_[3]=output_img_.nj();

  if (image_==NULL || img_dims_==NULL)
  {
    vcl_cerr<<"Failed allocation of image or image dimensions\n";
    return false;
  }
  else
    return true;
}

bool boxm_ocl_change_detection_manager::clean_input_image()
{
  if (image_)
    boxm_ocl_utils::free_aligned(image_);
  if (img_dims_)
    boxm_ocl_utils::free_aligned(img_dims_);
  return true;
}


bool boxm_ocl_change_detection_manager::set_input_image_buffers()
{
  cl_int status;
  image_buf_ = clCreateBuffer(this->context_,
                              CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                              wni_*wnj_*sizeof(cl_float4),
                              image_,&status);
  if (!check_val(status,CL_SUCCESS,"clCreateBuffer (image_buf_) failed."))
    return false;
  int_image_buf_ = clCreateBuffer(this->context_,
                                  CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                  wni_*wnj_*sizeof(cl_float),
                                  int_image_,&status);
  return check_val(status,CL_SUCCESS,"clCreateBuffer (int_image_buf_) failed.")==1;
}


bool boxm_ocl_change_detection_manager::set_image_dims_buffers()
{
  cl_int status;

  img_dims_buf_ = clCreateBuffer(this->context_,
                                 CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                                 sizeof(cl_uint4),
                                 img_dims_,&status);
  return check_val(status,CL_SUCCESS,"clCreateBuffer (imd_dims_buf_) failed.")==1;
}


bool boxm_ocl_change_detection_manager::release_input_image_buffers()
{
  cl_int status;
  status = clReleaseMemObject(image_buf_);
  if (!check_val(status,CL_SUCCESS,"clReleaseMemObject failed (image_buf_)."))
    return false;

  status = clReleaseMemObject(img_dims_buf_);
  return check_val(status,CL_SUCCESS,"clReleaseMemObject failed (img_dims_buf_).")==1;
}


bool boxm_ocl_change_detection_manager::set_foreground_pdf(vcl_vector<float> & hist)
{
  foreground_pdf_=(cl_float *)boxm_ocl_utils::alloc_aligned(hist.size(),sizeof(cl_float),16);

  for (unsigned i=0;i<hist.size();i++)
    foreground_pdf_[i]=hist[i];

  return true;
}

bool boxm_ocl_change_detection_manager::set_foreground_pdf_buffers()
{
  cl_int status;

  foreground_pdf_buf_ = clCreateBuffer(this->context_,
                                       CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR,
                                       20*sizeof(cl_float),
                                       foreground_pdf_,&status);
  return check_val(status,CL_SUCCESS,"clCreateBuffer (foreground_pdf_) failed.")==1;
}

bool boxm_ocl_change_detection_manager::release_foreground_pdf_buffers()
{
  cl_int status;
  status = clReleaseMemObject(foreground_pdf_buf_);
  return check_val(status,CL_SUCCESS,"clReleaseMemObject failed (foreground_pdf_buf_).")==1;
}

bool boxm_ocl_change_detection_manager::clean_foreground_pdf()
{
  if (foreground_pdf_)
    boxm_ocl_utils::free_aligned(foreground_pdf_);

  return true;
}

//: Binary write multi_tracker scene to stream
void vsl_b_write(vsl_b_ostream& /*os*/, boxm_ocl_change_detection_manager const& /*multi_tracker*/)
{
}


//: Binary load boxm scene from stream.
void vsl_b_read(vsl_b_istream& /*is*/, boxm_ocl_change_detection_manager& /*multi_tracker*/)
{
}

//: Binary write boxm scene pointer to stream
void vsl_b_read(vsl_b_istream& /*is*/, boxm_ocl_change_detection_manager* /*ph*/)
{
}

//: Binary write boxm scene pointer to stream
void vsl_b_write(vsl_b_ostream& /*os*/, boxm_ocl_change_detection_manager* const& /*ph*/)
{
}