# Copyright (c) 2018, Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.

if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif()

if(POLICY CMP0063)
  cmake_policy(SET CMP0063 NEW)
endif()

project(neo)
set(MSVC_DEF_ADDITIONAL_EXPORTS "")

if(NOT TARGET ${BIKSIM_LIB_NAME})
  add_subdirectory(builtin_kernels_simulation)
endif()

hide_subdir(builtin_kernels_simulation)
hide_subdir(dll)
hide_subdir(instrumentation)

add_library(${NEO_STATIC_LIB_NAME} STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
  ${CMAKE_CURRENT_SOURCE_DIR}/enable_gens.cmake
  $<TARGET_OBJECTS:${BIKSIM_LIB_NAME}>
)

add_subdirectories()
add_subdirectory(instrumentation${IGDRCL__INSTRUMENTATION_DIR_SUFFIX})
include(enable_gens.cmake)

# Enable SSE4/AVX2 options for files that need them
if(MSVC)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/command_queue/local_id_gen_avx2.cpp PROPERTIES COMPILE_FLAGS /arch:AVX2)
else()
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/command_queue/local_id_gen_avx2.cpp PROPERTIES COMPILE_FLAGS -mavx2)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/command_queue/local_id_gen_sse4.cpp PROPERTIES COMPILE_FLAGS -msse4.2)
endif()

# Put Driver version into define
if(NEO_DRIVER_VERSION)
  set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/device/device_caps.cpp PROPERTIES COMPILE_DEFINITIONS NEO_DRIVER_VERSION="${NEO_DRIVER_VERSION}")
endif()

if(WIN32)
  if("${IGDRCL_OPTION__BITS}" STREQUAL "32" )
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /SAFESEH:NO")
  endif()
endif()

target_link_libraries(${NEO_STATIC_LIB_NAME} elflib)

target_include_directories(${NEO_STATIC_LIB_NAME} PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${KMDAF_HEADERS_DIR}
)

target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
  ${KHRONOS_HEADERS_DIR}
  ${CIF_BASE_DIR}
  ${GMM_INCLUDE_PATHS}
  ${GTPIN_HEADERS_DIR}
  ${HW_SRC_INCLUDE_PATH}
  ${IGC_OCL_ADAPTOR_DIR}
  ${IGDRCL__IGC_INCLUDE_DIR}
  ${THIRD_PARTY_DIR}
  ${UMKM_SHAREDDATA_INCLUDE_PATHS}
)

if(WIN32)
  set(IGDRCL_EXTRA_LIBS Ws2_32)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
    ${WDK_INCLUDE_PATHS}
    os_interface/windows
  )
  target_compile_definitions(${NEO_STATIC_LIB_NAME} PRIVATE OGL=1)
else()
  set(IGDRCL_EXTRA_LIBS dl pthread)
  target_include_directories(${NEO_STATIC_LIB_NAME} PUBLIC
    os_interface/linux
    ${I915_INCLUDES_DIR}
  )
endif()

target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC DEFAULT_PLATFORM=${DEFAULT_SUPPORTED_PLATFORM})

link_directories(${GMM_LIB_PATHS})

target_link_libraries(${NEO_STATIC_LIB_NAME} ${GMMUMD_LIB_NAME})

if(INSTRUMENTATION_LIB_NAME)
  add_dependencies(${NEO_STATIC_LIB_NAME} ${INSTRUMENTATION_LIB_NAME})
endif()

list(APPEND LIB_FLAGS_DEFINITIONS -DCIF_HEADERS_ONLY_BUILD ${SUPPORTED_GEN_FLAGS_DEFINITONS})

target_compile_definitions(${NEO_STATIC_LIB_NAME} PUBLIC ${LIB_FLAGS_DEFINITIONS})

set_target_properties(${NEO_STATIC_LIB_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

set_property(TARGET ${NEO_STATIC_LIB_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS} ${TSAN_FLAGS})

set_target_properties(${NEO_STATIC_LIB_NAME} PROPERTIES FOLDER "opencl runtime")

set(EXPORTS_FILENAME "")
if(WIN32)
    set(EXPORTS_FILENAME "${CMAKE_CURRENT_BINARY_DIR}/OpenCLExports${IGDRCL_OPTION__BITS}.def")
    set(MSVC_DEF_LIB_NAME "igdrcl${IGDRCL_OPTION__BITS}")
    set(MSVC_DEF_HEADER "This file was generated during CMake project configuration - please don't edit")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/dll/windows/OpenCLExports.def.in" "${EXPORTS_FILENAME}")
endif(WIN32)

if(${GENERATE_EXECUTABLE})
  if(GTPIN_HEADERS_DIR)
    foreach(GEN_TYPE ${ALL_GEN_TYPES})
      string(TOLOWER ${GEN_TYPE} GEN_TYPE_LOWER)
      GEN_CONTAINS_PLATFORMS("SUPPORTED" ${GEN_TYPE} GENX_HAS_PLATFORMS)
      if(${GENX_HAS_PLATFORMS})
        target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${GEN_TYPE_LOWER}/gtpin_setup_${GEN_TYPE_LOWER}.cpp)
      endif(${GENX_HAS_PLATFORMS})
    endforeach()
  endif()

  list(APPEND NEO_DYNAMIC_LIB__TARGET_OBJECTS
    $<TARGET_OBJECTS:${SHARINGS_ENABLE_LIB_NAME}>
    $<TARGET_OBJECTS:${BUILTINS_SOURCES_LIB_NAME}>
    $<TARGET_OBJECTS:${BUILTINS_BINARIES_LIB_NAME}>
    $<TARGET_OBJECTS:${SCHEDULER_BINARY_LIB_NAME}>
  )
  add_library(${NEO_DYNAMIC_LIB_NAME} SHARED
    ${NEO_DYNAMIC_LIB__TARGET_OBJECTS}
  )

  add_subdirectory(dll)

  if(HAVE_INSTRUMENTATION)
    target_link_libraries(${NEO_DYNAMIC_LIB_NAME} ${INSTRUMENTATION_LIB_NAME})
  endif()

  target_link_libraries(${NEO_DYNAMIC_LIB_NAME} ${NEO_STATIC_LIB_NAME} ${IGDRCL_EXTRA_LIBS})

  target_include_directories(${NEO_DYNAMIC_LIB_NAME} BEFORE PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    ${INSTRUMENTATION_INCLUDE_PATH}
  )

  if(WIN32)
    target_link_libraries(${NEO_DYNAMIC_LIB_NAME} dxgi)
  else()
    target_include_directories(${NEO_DYNAMIC_LIB_NAME} PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/dll/linux/devices${BRANCH_DIR_SUFFIX}
    )
    set_property(TARGET ${NEO_DYNAMIC_LIB_NAME}
      APPEND_STRING PROPERTY LINK_FLAGS " -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/dll/linux/ocl.exports"
    )
    set_property(TARGET ${NEO_DYNAMIC_LIB_NAME}
      APPEND_STRING PROPERTY LINK_FLAGS " -Wl,-Bsymbolic"
    )
    set_property(TARGET ${NEO_DYNAMIC_LIB_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS ${ASAN_FLAGS})
  endif()

  set_target_properties(${NEO_DYNAMIC_LIB_NAME} PROPERTIES
    DEBUG_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
    RELEASE_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
    RELEASEINTERNAL_OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
    OUTPUT_NAME "${NEO_DLL_NAME_BASE}${IGDRCL_NAME_POSTFIX}${IGDRCL_OPTION__BITS}"
  )
  set_target_properties(${NEO_DYNAMIC_LIB_NAME} PROPERTIES FOLDER "opencl runtime")
  create_project_source_tree_with_exports(${NEO_DYNAMIC_LIB_NAME} "${EXPORTS_FILENAME}")
endif(${GENERATE_EXECUTABLE})

create_project_source_tree(${NEO_STATIC_LIB_NAME})

if(UNIX AND NOT (TARGET clang-tidy))
  add_custom_target(clang-tidy
    DEPENDS scheduler
  )
  add_custom_command(
    TARGET clang-tidy
    POST_BUILD
    COMMAND echo clang-tidy...
    COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -name *.cpp -print0 | xargs -0 -I{} -P`nproc` clang-tidy -p ${IGDRCL_BINARY_DIR} {} | tee ${IGDRCL_BINARY_DIR}/clang-tidy.log
    WORKING_DIRECTORY ${IGDRCL_SOURCE_DIR}
  )
endif()

set(IGDRCL_EXTRA_LIBS ${IGDRCL_EXTRA_LIBS} PARENT_SCOPE)
