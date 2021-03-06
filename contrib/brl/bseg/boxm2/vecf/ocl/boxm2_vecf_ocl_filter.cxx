#include "boxm2_vecf_ocl_filter.h"
//:
// \file
#include <bocl/bocl_cl.h>
#include <vgl/vgl_box_3d.h>
#include <vgl/vgl_vector_3d.h>
#include <vgl/algo/vgl_rotation_3d.h>
#include <vgl/vgl_intersection.h>
#include <boxm2/ocl/boxm2_opencl_cache.h>
#include <boxm2/boxm2_block_metadata.h>
#include <boxm2/boxm2_block.h>
#include <boxm2/boxm2_data_base.h>
#include <boxm2/boxm2_util.h>
#include <boxm2/ocl/boxm2_ocl_util.h>
#include <boct/boct_bit_tree.h>

#include <bocl/bocl_kernel.h>
#include <vcl_where_root_dir.h>
#include <vcl_algorithm.h>

typedef vnl_vector_fixed<unsigned char,16> uchar16;
bool boxm2_vecf_ocl_filter::get_scene_appearance( boxm2_scene_sptr scene,
          vcl_string&      options)
{
    vcl_vector<vcl_string> apps = scene->appearances();
    bool foundDataType = false;
    for (unsigned int i=0; i<apps.size(); ++i) {
        if ( apps[i] == boxm2_data_traits<BOXM2_MOG3_GREY>::prefix() )
        {
            app_type_ = apps[i];
            foundDataType = true;
            options=" -D MOG_TYPE_8 ";
        }
        else if ( apps[i] == boxm2_data_traits<BOXM2_MOG3_GREY_16>::prefix() )
        {
            app_type_ = apps[i];
            foundDataType = true;
            options=" -D MOG_TYPE_16 ";
        }
    }
    if (!foundDataType) {
        vcl_cerr<<"ERROR: boxm2_vecf_ocl_filter: unsupported appearance type!" << vcl_endl;
        return false;
    }
    //set apptype size
    apptypesize_ = (int) boxm2_data_info::datasize(app_type_);
    return true;
}


boxm2_vecf_ocl_filter::boxm2_vecf_ocl_filter(boxm2_scene_sptr& source_scene,
                                             boxm2_scene_sptr& temp_scene,
                                             boxm2_opencl_cache_sptr ocl_cache)
  : source_scene_(source_scene),
     temp_scene_(temp_scene),
     opencl_cache_(ocl_cache)
{
  device_=opencl_cache_->get_device();
  this->compile_filter_kernel();
  this->init_ocl_filter();
}

boxm2_vecf_ocl_filter::~boxm2_vecf_ocl_filter()
{
  delete filter_kern;
  opencl_cache_->clear_cache();
}

bool boxm2_vecf_ocl_filter::compile_filter_kernel()
{
  vcl_string options;
  // sets apptypesize_ and app_type
  get_scene_appearance(source_scene_, options);
  vcl_vector<vcl_string> src_paths;
  vcl_string source_dir = vcl_string(VCL_SOURCE_ROOT_DIR) + "/contrib/brl/bseg/boxm2/ocl/cl/";
  vcl_string vecf_source_dir = vcl_string(VCL_SOURCE_ROOT_DIR)+ "/contrib/brl/bseg/boxm2/vecf/ocl/cl/";
  src_paths.push_back(source_dir     + "scene_info.cl");
  src_paths.push_back(source_dir     + "bit/bit_tree_library_functions.cl");
  src_paths.push_back(vecf_source_dir + "filter_block_six_neighbors.cl");
  this->filter_kern = new bocl_kernel();
  return filter_kern->create_kernel(&device_->context(),device_->device_id(), src_paths, "filter_block_six_neighbors", options, "filter_scene");
}

bool boxm2_vecf_ocl_filter::init_ocl_filter()
{
    centerX = new bocl_mem(device_->context(), boct_bit_tree::centerX, sizeof(cl_float)*585, "centersX lookup buffer");
    centerY = new bocl_mem(device_->context(), boct_bit_tree::centerY, sizeof(cl_float)*585, "centersY lookup buffer");
    centerZ = new bocl_mem(device_->context(), boct_bit_tree::centerZ, sizeof(cl_float)*585, "centersZ lookup buffer");
    centerX->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);
    centerY->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);
    centerZ->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);
    // output buffer for debugging
    // bit lookup buffer
    cl_uchar lookup_arr[256];
    boxm2_ocl_util::set_bit_lookup(lookup_arr);
    lookup=new bocl_mem(device_->context(), lookup_arr, sizeof(cl_uchar)*256, "bit lookup buffer");
    lookup->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);
    int status = 0;
    queue = clCreateCommandQueue(device_->context(),*(device_->device_id()),CL_QUEUE_PROFILING_ENABLE,&status);
    ocl_depth = 0;
    blk_info_temp = 0;
    blk_info_source = 0;
    info_buffer = 0;
    blk_temp = 0;
    alpha_temp = 0;
    mog_temp = 0;
    info_buffer_source = 0;
    blk_source = 0;
    alpha_source = 0;
    mog_source = 0;
    return true;
}
bool boxm2_vecf_ocl_filter::filter(vcl_vector<float> const& weights, unsigned num_iterations)
{
  // the filter algorithm assumes that the scene is uniformly refined to some depth
  // (for now all cells are at root, i.e., depth = 0)
  // cell neighborhoods with variable size cells is not currently handled
  int depth = 0;
  ocl_depth = new bocl_mem(device_->context(), &(depth), sizeof(int), "  depth of octree " );
  ocl_depth->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR );
  float w_buff[8];
  for(unsigned i = 0; i<7; ++i)
    w_buff[i]=weights[i];
  w_buff[7]=0.0f;
  filter_weights = new bocl_mem(device_->context(), w_buff, sizeof(float)*8, "weights" );
  filter_weights->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR );

  int n = num_iterations;
  ocl_n_iter = new bocl_mem(device_->context(), &(n), sizeof(int), "  depth of octree " );
  ocl_n_iter->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR );

  vcl_size_t local_threads[1]={64};
  vcl_size_t global_threads[1]={1};
  // set up all the kernel arguments
   vcl_vector<boxm2_block_id> blocks_source = source_scene_->get_block_ids();
   vcl_vector<boxm2_block_id> blocks_temp = temp_scene_->get_block_ids();
   if(blocks_temp.size()!=1||blocks_source.size()!=1)
     return false;
   vcl_vector<boxm2_block_id>::iterator iter_blk_temp = blocks_temp.begin();
   vcl_vector<boxm2_block_id>::iterator iter_blk_source = blocks_source.begin();
     //Gather information about the temp and setup temp data buffers
   blk_temp       = opencl_cache_->get_block(temp_scene_, *iter_blk_temp);
   alpha_temp     = opencl_cache_->get_data<BOXM2_ALPHA>(temp_scene_, *iter_blk_temp,0,true);
   info_buffer = temp_scene_->get_blk_metadata(*iter_blk_temp);
   int alphaTypeSize = (int)boxm2_data_info::datasize(boxm2_data_traits<BOXM2_ALPHA>::prefix());
   info_buffer->data_buffer_length = (int) (alpha_temp->num_bytes()/alphaTypeSize);
   int data_size = info_buffer->data_buffer_length;
   blk_info_temp  = new bocl_mem(device_->context(), info_buffer, sizeof(boxm2_scene_info), " Scene Info" );
   blk_info_temp->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);

   if(app_type_ == "boxm2_mog3_grey") {
     mog_temp       = opencl_cache_->get_data<BOXM2_MOG3_GREY>(temp_scene_, *iter_blk_temp,0,true);
   }
   else if(app_type_ == "boxm2_mog3_grey_16") {
     mog_temp       = opencl_cache_->get_data<BOXM2_MOG3_GREY_16>(temp_scene_, *iter_blk_temp,0,true);
   }
   else {
     vcl_cout << "Unknown appearance type for temp_scene " << app_type_ << '\n';
     return false;
   }
   global_threads[0] = (unsigned) RoundUp(info_buffer->scene_dims[0]*info_buffer->scene_dims[1]*info_buffer->scene_dims[2],(int)local_threads[0]);
   info_buffer_source = source_scene_->get_blk_metadata(*iter_blk_source);
   blk_source       = opencl_cache_->get_block(source_scene_, *iter_blk_source);
   alpha_source     = opencl_cache_->get_data<BOXM2_ALPHA>(source_scene_, *iter_blk_source,0,false);
   info_buffer_source->data_buffer_length = (int) (alpha_source->num_bytes()/alphaTypeSize);
   data_size = info_buffer_source->data_buffer_length;
   float* output_buff= new float[data_size];
   output = new bocl_mem(device_->context(), output_buff, sizeof(float)*info_buffer_source->data_buffer_length, "output" );
   output->create_buffer(CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR );
   output->zero_gpu_buffer(queue);
   blk_info_source  = new bocl_mem(device_->context(), info_buffer_source, sizeof(boxm2_scene_info), " Scene Info" );
   blk_info_source->create_buffer(CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR);
   int app_type_size_;
   if(app_type_ == "boxm2_mog3_grey") {
     mog_source       = opencl_cache_->get_data<BOXM2_MOG3_GREY>(source_scene_, *iter_blk_source,0,false);
     app_type_size_ = (int)boxm2_data_info::datasize(boxm2_data_traits<BOXM2_MOG3_GREY>::prefix());
   }
   else if(app_type_ == "boxm2_mog3_grey_16") {
     mog_source       = opencl_cache_->get_data<BOXM2_MOG3_GREY_16>(source_scene_, *iter_blk_source,0,false);
     app_type_size_ = (int)boxm2_data_info::datasize(boxm2_data_traits<BOXM2_MOG3_GREY_16>::prefix());
   }
   else {
     vcl_cout << "Unknown appearance type for source_scene " << app_type_ << '\n';
     return false;
   }
   filter_kern->set_arg(centerX.ptr());
   filter_kern->set_arg(centerY.ptr());
   filter_kern->set_arg(centerZ.ptr());
   filter_kern->set_arg(filter_weights);
   filter_kern->set_arg(ocl_n_iter);
   filter_kern->set_arg(lookup.ptr());
   filter_kern->set_arg(blk_temp);
   filter_kern->set_arg(alpha_temp);
   filter_kern->set_arg(mog_temp);
   filter_kern->set_arg(blk_info_source);
   filter_kern->set_arg(blk_source);
   filter_kern->set_arg(alpha_source);
   filter_kern->set_arg(mog_source);
   filter_kern->set_arg(ocl_depth);
   filter_kern->set_arg(output.ptr());
   filter_kern->set_local_arg(16*local_threads[0]*sizeof(unsigned char)); // local trees temp
   filter_kern->set_local_arg(16*local_threads[0]*sizeof(unsigned char)); // local trees source
   filter_kern->set_local_arg(16*local_threads[0]*sizeof(unsigned char)); // neighbor trees
   if(!filter_kern->execute(queue, 1, local_threads, global_threads))
     {
       vcl_cout<<"Kernel Failed to Execute "<<vcl_endl;
       return false;
     }
   int status = clFinish(queue);
   bool good_kern = check_val(status, CL_SUCCESS, "FILTER KERNEL FAILED: " + error_to_string(status));
   if(!good_kern)
     return false;
   mog_source->read_to_buffer(queue);
   alpha_source->read_to_buffer(queue);
   mog_temp->read_to_buffer(queue);
   alpha_temp->read_to_buffer(queue);
   vcl_memcpy(alpha_source->cpu_buffer(),alpha_temp->cpu_buffer(),data_size * sizeof(float));
   vcl_memcpy(mog_source->cpu_buffer(),mog_temp->cpu_buffer(),data_size * app_type_size_);
   mog_source->write_to_buffer(queue);
   alpha_source->write_to_buffer(queue);
   output->read_to_buffer(queue);//for debug
   status = clFinish(queue);
   bool good_read = check_val(status, CL_SUCCESS, "READ FROM GPU FAILED: " + error_to_string(status));
   if(!good_read)
     return false;
   filter_kern->clear_args();
   int count = 0;
   for (int i=0;i<data_size;i++){
           if(output_buff[i]!=0){
                   vcl_cout<<output_buff[i]<<" ";
                   count++;
           }
           if(count > 1000)
                   break;

   }
   vcl_cout << "Output: " << output_buff[0] <<' ' << output_buff[1] <<' ' << output_buff[2]
            << ' ' << output_buff[3] <<' ' << output_buff[4] <<' ' << output_buff[5] << '\n';

   boxm2_lru_cache::instance()->write_to_disk(source_scene_);
   // look into properly releasing all data
   ocl_depth->release_memory();
   blk_info_source->release_memory();
   blk_info_source->cpu_buffer();
   delete info_buffer_source;
   blk_info_temp->release_memory();
   delete info_buffer;
   delete output_buff;
}
