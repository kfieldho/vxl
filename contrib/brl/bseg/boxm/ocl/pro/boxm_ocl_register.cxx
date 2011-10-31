#include "boxm_ocl_register.h"

#include <bprb/bprb_macros.h>
#include <bprb/bprb_batch_process_manager.h>
#include <bprb/bprb_func_process.h>

#include "boxm_ocl_processes.h"
#include <boxm/ocl/boxm_render_probe_manager_sptr.h>
#include <boxm/ocl/boxm_ocl_change_detection_manager_sptr.h>
#include <boxm/ocl/boxm_update_bit_scene_manager_sptr.h>
#include <boxm/basic/boxm_util_data_types.h>
#include <boxm/ocl/boxm_ocl_bit_scene.h>
void boxm_ocl_register::register_datatype()
{  
    REGISTER_DATATYPE( boxm_ocl_change_detection_manager_sptr );
    REGISTER_DATATYPE( boxm_update_bit_scene_manager_sptr );
    REGISTER_DATATYPE( boxm_render_probe_manager_sptr );
    REGISTER_DATATYPE( boxm_array_1d_float_sptr );
    REGISTER_DATATYPE( boxm_ocl_bit_scene_sptr );

}

void boxm_ocl_register::register_process()
{
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_ocl_refine_process,"boxmOclRefineProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_ocl_convert_boxm_to_ocl_process,"boxmOclConvertBoxmToOclProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_ocl_init_render_probe_process,"boxmOclInitRenderProbeProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_ocl_run_render_probe_process,"boxmOclRunRenderProbeProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_ocl_finish_render_probe_process,"boxmOclFinishRenderProbeProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_ocl_init_change_detection_process,"boxmOclInitChangeDetectionProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_ocl_run_change_detection_process,"boxmOclRunChangeDetectionProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_ocl_finish_change_detection_process,"boxmOclFinishChangeDetectionProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_init_update_bit_scene_process,"boxmInitUpdateBitSceneProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_update_bit_scene_process,"boxmUpdateBitSceneProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_finish_update_bit_scene_process,"boxmFinishUpdateBitSceneProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_render_update_bit_scene_process,"boxmRenderUpdateBitSceneProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_query_bit_scene_process,"boxmQueryBitSceneProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_ray_probe_process,"boxmRayProbeProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_generate_3d_point_process,"boxmGenerate3dPointProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_change_detection_bit_scene_process,"boxmChangeDetectionBitSceneProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_refine_bit_scene_process,"boxmRefineBitSceneProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_clean_aux_data_scene_process,"boxmCleanAuxDataSceneProcess");
    REG_PROCESS_FUNC_CONS(bprb_func_process, bprb_batch_process_manager, boxm_init_bit_scene_process,"boxmInitBitSceneProcess");
}