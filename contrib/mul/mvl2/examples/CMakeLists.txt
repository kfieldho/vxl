# ./mul/mvl2/examples/CMakeLists.txt
#
#   Franck Bettinger
#

LINK_LIBRARIES( mvl2 mbl vil )

IF (NOT WIN32)
  find_package(AVIFile)
  INCLUDE_DIRECTORIES( ${AVIFILE_INCLUDE_DIR} )
  ADD_DEFINITIONS (${AVIFILE_DEFINITIONS} )
ELSE (NOT WIN32)
  SET(AVIFILE_FOUND "YES")
ENDIF (NOT WIN32)

IF (AVIFILE_FOUND)

  ADD_EXECUTABLE(
    copy_image copy_image.cxx
  )

  ADD_EXECUTABLE(
    mvl2_to_divx mvl2_to_divx.cxx
  )

ENDIF (AVIFILE_FOUND)
