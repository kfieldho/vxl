#if ATI
__constant sampler_t RowSampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;
#endif

#if NVIDIA
const sampler_t RowSampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;
#endif
//RAY_TRACE_OCL_SCENE_OPT
//uses int2 tree cells and uchar8 mixture cells
__kernel
void
rerender_ocl_scene_opt(__global float   * depth_image_view1,
                       __read_only image2d_t intensity_image_view2,
                       __global float16 * camera_view2, //last four floats are camera center
                       __global int4    * scene_dims,  // level of the root.
                       __global float4  * scene_origin,
                       __global float4  * block_dims,
                       __global int4    * block_ptrs,
                       __global int     * root_level,
                       __global int     * num_buffer,
                       __global int     * len_buffer,
                       __global int2    * tree_array,
                       __global float   * alpha_array,
                       __global float16 * persp_cam, // camera orign and SVD of inverse of camera matrix
                       __global uint4   * imgdims,   // dimensions of the image
                       __local  float16 * local_copy_cam,
                       __local  uint4   * local_copy_imgdims,
                       __local  float4  * local_camera_center_view2,
                       __global float   * in_image,
                       __global uint    * gl_rerender_image)    // input image and store vis_inf and pre_inf
{
  uchar llid = (uchar)(get_local_id(0) + get_local_size(0)*get_local_id(1));

  if (llid == 0 )
  {
    local_copy_cam[0]=persp_cam[0];  // conjugate transpose of U
    local_copy_cam[1]=persp_cam[1];  // V
    local_copy_cam[2]=persp_cam[2];  // Winv(first4) and ray_origin(last four)
    (*local_copy_imgdims)=(*imgdims);
    (*local_camera_center_view2)=(float4)((*camera_view2).scdef);
  }
  barrier(CLK_LOCAL_MEM_FENCE);

  // camera origin
  float4 ray_o=(float4)local_copy_cam[2].s4567;
  ray_o.w=1.0f;
  int rootlevel=(*root_level);

  //cell size of what?
  float cellsize=(float)(1<<rootlevel);
  cellsize=1/cellsize;
  short4 root = (short4)(0,0,0,rootlevel);
  short4 exit_face=(short4)-1;
  int curr_cell_ptr=-1;

  // get image coordinates
  int i=0,j=0;  map_work_space_2d(&i,&j);
  int lenbuffer =(*len_buffer);

  // rootlevel of the trees.

  // check to see if the thread corresponds to an actual pixel as in some cases #of threads will be more than the pixels.
  if (i>=(*local_copy_imgdims).z || j>=(*local_copy_imgdims).w  || depth_image_view1[j*get_global_size(0)+i]<0.0f) {
    gl_rerender_image[j*get_global_size(0)+i]=rgbaFloatToInt((float4)0.0f);
    in_image[j*get_global_size(0)+i]=(float)0.0f;
    return;
  }
  float4 origin=(*scene_origin);
  float data_return=0.0f;
  float tnear = 0.0f, tfar =0.0f;
  float4 ray_d = backproject(i,j,local_copy_cam[0],local_copy_cam[1],local_copy_cam[2],ray_o);

  float4 point3d=ray_o+depth_image_view1[j*get_global_size(0)+i]*ray_d;
  point3d.w=1;


  ray_d=(*local_camera_center_view2)-point3d;
  ray_d.w=0.0;
  ray_d=normalize(ray_d);
  ray_d.w=0.5f;
  ray_o=point3d;


  ////// scene origin
  float4 blockdims=(*block_dims);
  int4 scenedims=(int4)(*scene_dims).xyzw;
  scenedims.w=1;blockdims.w=1; // for safety purposes.

  float4 cell_min, cell_max;
  float4 entry_pt, exit_pt;

  ////// scene bounding box
  cell_min = origin;
  cell_max = blockdims*convert_float4(scenedims)+origin;
  int hit  = intersect_cell(ray_o, ray_d, cell_min, cell_max,&tnear, &tfar);
  if (!hit) {
    gl_rerender_image[j*get_global_size(0)+i]=rgbaFloatToInt((float4)(0.0,0.0,0.0,0.0));
    in_image[j*get_global_size(0)+i]=(float)0.0f;
    return;
  }

  //  just added 0.01 for now to get out of the surface cloud.
  tnear=tnear>0?tnear:0.01;

  entry_pt = ray_o + tnear*ray_d;

  int4 curr_block_index = convert_int4((entry_pt-origin)/blockdims);

  //// handling the border case where a ray pierces the max side
  curr_block_index=curr_block_index+(curr_block_index==scenedims);
  int global_count=0;

  while (!(any(curr_block_index<(int4)0) || any(curr_block_index>=(scenedims))))
  {
    // Ray tracing with in each block
    // 3-d index to 1-d index
    int4 block = block_ptrs[curr_block_index.z
                           +curr_block_index.y*scenedims.z
                           +curr_block_index.x*scenedims.y*scenedims.z];

    // tree offset is the root_ptr
    int root_ptr = block.x*lenbuffer+block.y;
    float4 local_ray_o= (ray_o-origin)/blockdims-convert_float4(curr_block_index);

    // canonincal bounding box of the tree
    float4 block_entry_pt=(entry_pt-origin)/blockdims-convert_float4(curr_block_index);
    short4 entry_loc_code = loc_code(block_entry_pt, rootlevel);
    short4 curr_loc_code=(short4)-1;

    // traverse to leaf cell that contains the entry point
    //curr_cell_ptr = traverse_force_woffset(tree_array, root_ptr, root, entry_loc_code,&curr_loc_code,&global_count,root_ptr);
    curr_cell_ptr = traverse_force_woffset_mod_opt(tree_array, root_ptr, root, entry_loc_code,&curr_loc_code,&global_count,lenbuffer,block.x,block.y);

    // this cell is the first pierced by the ray
    // follow the ray through the cells until no neighbors are found
    while (true)
    {
      //// current cell bounding box
      cell_bounding_box(curr_loc_code, rootlevel+1, &cell_min, &cell_max);

      // check to see how close tnear and tfar are
      int hit = intersect_cell(local_ray_o, ray_d, cell_min, cell_max,&tnear, &tfar);

      // special case whenray grazes edge or corner of cube
      if (fabs(tfar-tnear)<cellsize/10)
      {
        block_entry_pt=block_entry_pt+ray_d*(float4)(cellsize/2);;
        block_entry_pt.w=0.5;
        if (any(block_entry_pt>=(float4)1.0f)|| any(block_entry_pt<=(float4)0.0f))
          break;

        entry_loc_code = loc_code(block_entry_pt, rootlevel);
        //// traverse to leaf cell that contains the entry point
        curr_cell_ptr = traverse_woffset_mod_opt(tree_array, root_ptr, root, entry_loc_code,&curr_loc_code,&global_count,lenbuffer,block.x,block.y);
        cell_bounding_box(curr_loc_code, rootlevel+1, &cell_min, &cell_max);
        hit = intersect_cell(local_ray_o, ray_d, cell_min, cell_max,&tnear, &tfar);
        if (hit)
        {
          block_entry_pt=local_ray_o + tnear*ray_d;
          tnear=tnear>0?tnear:0;
        }
      }
      if (!hit)
        break;
      tnear=tnear>0?tnear:0;

      //int data_ptr =  block.x*lenbuffer+tree_array[curr_cell_ptr].z;
      ushort2 child_data = as_ushort2(tree_array[curr_cell_ptr].y);
      int data_ptr = block.x*lenbuffer + (int) child_data.x;

      //// distance must be multiplied by the dimension of the bounding box
      float d = (tfar-tnear)*(blockdims.x);
      step_cell_visibility(alpha_array,data_ptr,d,&data_return);

      // Added aliitle extra to the exit point
      exit_pt=local_ray_o + (tfar+cellsize/10)*ray_d;exit_pt.w=0.5;

      // if the ray pierces the volume surface then terminate the ray
      if (any(exit_pt>=(float4)1.0f)|| any(exit_pt<=(float4)0.0f))
        break;

      //// required exit location code
      short4 exit_loc_code = loc_code(exit_pt, rootlevel);
      curr_cell_ptr = traverse_force_woffset_mod_opt(tree_array, root_ptr, root,exit_loc_code, &curr_loc_code,&global_count,lenbuffer,block.x,block.y);

      block_entry_pt = local_ray_o + (tfar)*ray_d;
    }

    // finding the next block

    // block bounding box
    cell_min=blockdims*convert_float4(curr_block_index)+origin;
    cell_max=cell_min+blockdims;
    if (!intersect_cell(ray_o, ray_d, cell_min, cell_max,&tnear, &tfar))
    {
      // this means the ray has hit a special case
      // two special cases
      // (1) grazing the corner/edge and (2) grazing the side.

      // this is the first case
      if (tfar-tnear<blockdims.x/100)
      {
        entry_pt=entry_pt + blockdims.x/2 *ray_d;
        curr_block_index=convert_int4((entry_pt-origin)/blockdims);
        curr_block_index.w=0;
      }
    }
    else
    {
      entry_pt=ray_o + tfar *ray_d;
      ray_d.w=1;
      if (any(-1*(isless(fabs(entry_pt-cell_min),(float4)blockdims.x/100.0f)*isless(fabs(ray_d),(float4)1e-3))))
        break;
      if (any(-1*(isless(fabs(entry_pt-cell_max),(float4)blockdims.x/100.0f)*isless(fabs(ray_d),(float4)1e-3))))
        break;
      curr_block_index=convert_int4(floor((entry_pt+(blockdims.x/20.0f)*ray_d-origin)/blockdims));
      curr_block_index.w=0;
    }
  }
  float2 point2d=(float2)0.0f;
  float16 project_cam=(*camera_view2);
  if (!project(project_cam,point3d,&point2d))
  {
    gl_rerender_image[j*get_global_size(0)+i]=rgbaFloatToInt((float4)0.0f);
    in_image[j*get_global_size(0)+i]=(float)0;
    return;
  }
  if (point2d.x<0 || point2d.y<0 || point2d.x>=(*local_copy_imgdims).z || point2d.y>=(*local_copy_imgdims).w)
  {
    gl_rerender_image[j*get_global_size(0)+i]=rgbaFloatToInt((float4)0.0f);
    in_image[j*get_global_size(0)+i]=(float)0.0;
    return;
  }
#if 0
  float4 weighted_intensity=read_imagef(intensity_image_view2,RowSampler,point2d);
  weighted_intensity*=exp(-data_return);
  gl_rerender_image[j*get_global_size(0)+i]=rgbaFloatToInt((float4)weighted_intensity);
  in_image[j*get_global_size(0)+i]=(float)weighted_intensity.x;
#endif

#if 1
  if (exp(-data_return)>0.5)
    in_image[j*get_global_size(0)+i]=(float)point2d.x;
  else
    in_image[j*get_global_size(0)+i]=-1.0f;
#endif
}

#if 0
__kernel
void rerender_view(__read_only image2d_t in_image,     // input image which will be rendered on another view
                   __global float16* in_cam,       // has to be actual camera + camera center;
                   __global float16* novel_cam,    // novel inv  camera
                   __global float* depth_image,
                   __global uint4* imgdims,
                   __global uint * gl_rerender_image,
                   __local  float16 * local_copy_cam)
{
    uchar llid = (uchar)(get_local_id(0) + get_local_size(0)*get_local_id(1));
    // get image coordinates
    int i=0,j=0;
    map_work_space_2d(&i,&j);
    if (llid == 0 )
    {
        local_copy_cam[0]=novel_cam[0];  // conjugate transpose of U
        local_copy_cam[1]=novel_cam[1];  // V
        local_copy_cam[2]=novel_cam[2];  // Winv(first4) and ray_origin(last four)
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    if (i>=(*imgdims).z || j>=(*imgdims).w)
        return;

    float4 ray_o=(float4)local_copy_cam[2].s4567;
    ray_o.w=1.0f;

    float4 ray_d = backproject(i,j,local_copy_cam[0],
                               local_copy_cam[1],
                               local_copy_cam[2],ray_o);
    if (depth_image[j*get_global_size(0)+i]<0.0f)
    {
        gl_rerender_image[j*get_global_size(0)+i]=rgbaFloatToInt((float4)0.0f);
        return;
    }

    float4 point3d=ray_o+depth_image[j*get_global_size(0)+i]*ray_d;


    point3d.w=1;
    float2 point2d=0.0f;
    float16 project_cam=(*in_cam);
    if (!project(project_cam,point3d,&point2d))
    {
        gl_rerender_image[j*get_global_size(0)+i]=rgbaFloatToInt((float4)0.0f);
        return;
    }
    if (point2d.x<0 || point2d.y<0 || point2d.x>=(*imgdims).z || point2d.y>=(*imgdims).w)
    {
        gl_rerender_image[j*get_global_size(0)+i]=rgbaFloatToInt((float4)point2d.x/1280);
        return;
    }
    gl_rerender_image[j*get_global_size(0)+i]=rgbaFloatToInt((float4)read_imagef(in_image,RowSampler,point2d));
    return;
}
#endif // 0