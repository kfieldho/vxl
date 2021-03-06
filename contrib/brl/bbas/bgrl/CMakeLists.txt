# Brown Graph Library
# brl/bbas/bgrl/CMakeLists.txt

doxygen_add_library(contrib/brl/bbas/bgrl
  DEPENDS core/vbl core/vsl
  PACKAGE contrib/brl
  DESCRIPTION "Brown Graph Library"
  )

INCLUDE_DIRECTORIES(${BRL_INCLUDE_DIR} )
INCLUDE_DIRECTORIES(${BRL_INCLUDE_DIR}/bbas )

SET(bgrl_sources
    bgrl_vertex.cxx          bgrl_vertex.h           bgrl_vertex_sptr.h
    bgrl_edge.cxx            bgrl_edge.h             bgrl_edge_sptr.h
    bgrl_graph.cxx           bgrl_graph.h            bgrl_graph_sptr.h
    bgrl_search_func.cxx     bgrl_search_func.h      bgrl_search_func_sptr.h
)
AUX_SOURCE_DIRECTORY(Templates bgrl_sources)

ADD_LIBRARY(bgrl ${bgrl_sources})
INSTALL_TARGETS(/lib bgrl)
INSTALL_NOBASE_HEADER_FILES(/include/vxl/contrib/brl/bbas/bgrl ${bgrl_sources})

TARGET_LINK_LIBRARIES(bgrl vbl_io vbl vsl)

IF(BUILD_TESTING)
  add_subdirectory(tests)
ENDIF(BUILD_TESTING)
