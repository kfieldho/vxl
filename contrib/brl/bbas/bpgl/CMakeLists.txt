# Brown view sphere directory
# brl/bbas/bpgl/CMakeLists.txt

INCLUDE_DIRECTORIES( ${BRL_INCLUDE_DIR}/bbas )
INCLUDE_DIRECTORIES( ${GEL_INCLUDE_DIR} )

SET(bpgl_sources
bpgl_comp_rational_camera.h         bpgl_comp_rational_camera.txx
bpgl_lens_distortion.h              bpgl_lens_distortion.txx
bpgl_radial_distortion.h            bpgl_radial_distortion.txx
bpgl_poly_radial_distortion.h       bpgl_poly_radial_distortion.txx
bpgl_reg_fundamental_matrix.h       bpgl_reg_fundamental_matrix.txx
bpgl_rolling_shutter_camera.h
bpgl_segmented_rolling_shutter_camera.h bpgl_segmented_rolling_shutter_camera.txx
bpgl_camera_estimator.h bpgl_camera_estimator.txx bpgl_camera_estimator_amoeba.h
   )

AUX_SOURCE_DIRECTORY(Templates bpgl_sources)

ADD_LIBRARY(bpgl ${bpgl_sources})
TARGET_LINK_LIBRARIES(bpgl vpgl vpgl_algo vpgl_io vgl_algo vgl_io vgl vbl vnl vcl)

# algorithms involving cameras
SUBDIRS(algo)

# determine unknown camera parameters
SUBDIRS(icam)

# image homography registration
SUBDIRS(ihog)

IF(BUILD_TESTING)
  SUBDIRS(tests)
ENDIF(BUILD_TESTING)