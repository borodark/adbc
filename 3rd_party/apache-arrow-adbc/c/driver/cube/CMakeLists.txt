# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

# Find libpq for PostgreSQL protocol support
if(WIN32)
  # XXX: for now, assume vcpkg
  find_package(PostgreSQL REQUIRED)
  set(LIBPQ_LINK_LIBRARIES PostgreSQL::PostgreSQL wsock32 ws2_32)
  set(LIBPQ_STATIC_LIBRARIES PostgreSQL::PostgreSQL)
  set(LIBPQ_INCLUDE_DIRS)
else()
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(LIBPQ QUIET libpq)
  endif()

  if(NOT LIBPQ_FOUND)
    # Try to find libpq manually
    find_library(LIBPQ_LIBRARY NAMES pq)
    find_path(LIBPQ_INCLUDE_DIR NAMES libpq-fe.h)

    if(LIBPQ_LIBRARY AND LIBPQ_INCLUDE_DIR)
      set(LIBPQ_LINK_LIBRARIES ${LIBPQ_LIBRARY})
      set(LIBPQ_STATIC_LIBRARIES ${LIBPQ_LIBRARY})
      set(LIBPQ_INCLUDE_DIRS ${LIBPQ_INCLUDE_DIR})
      message(STATUS "Found libpq: ${LIBPQ_LIBRARY}")
    else()
      message(WARNING "libpq not found - building without PostgreSQL support")
      set(LIBPQ_LINK_LIBRARIES)
      set(LIBPQ_STATIC_LIBRARIES)
      set(LIBPQ_INCLUDE_DIRS)
    endif()
  endif()
endif()

add_arrow_lib(adbc_driver_cube
              SOURCES
              cube.cc
              database.cc
              connection.cc
              statement.cc
              arrow_reader.cc
              parameter_converter.cc
              cube_types.cc
              metadata.cc
              native_protocol.cc
              native_client.cc
              OUTPUTS
              ADBC_LIBRARIES
              CMAKE_PACKAGE_NAME
              AdbcDriverCube
              PKG_CONFIG_NAME
              adbc-driver-cube
              SHARED_LINK_FLAGS
              ${ADBC_LINK_FLAGS}
              SHARED_LINK_LIBS
              adbc_driver_common
              adbc_driver_framework
              ${LIBPQ_LINK_LIBRARIES}
              STATIC_LINK_LIBS
              adbc_driver_common
              adbc_driver_framework
              ${LIBPQ_STATIC_LIBRARIES})

foreach(LIB_TARGET ${ADBC_LIBRARIES})
  target_compile_definitions(${LIB_TARGET} PRIVATE ADBC_EXPORTING)
  target_include_directories(${LIB_TARGET} SYSTEM
                             PRIVATE ${REPOSITORY_ROOT}/c/ ${REPOSITORY_ROOT}/c/include/
                                     ${REPOSITORY_ROOT}/c/driver ${LIBPQ_INCLUDE_DIRS})

  if(NOT ADBC_DEFINE_COMMON_ENTRYPOINTS)
    target_compile_definitions(${LIB_TARGET} PRIVATE ${ADBC_TARGET_COMPILE_DEFINITIONS})
  endif()
endforeach()

if(ADBC_TEST_LINKAGE STREQUAL "shared")
  set(TEST_LINK_LIBS adbc_driver_cube_shared)
else()
  set(TEST_LINK_LIBS adbc_driver_cube_static)
endif()

if(ADBC_BUILD_TESTS)
  add_test_case(driver_cube_test
                PREFIX
                adbc
                EXTRA_LABELS
                driver-cube
                SOURCES
                cube_test.cc
                EXTRA_LINK_LIBS
                adbc_driver_common
                adbc_validation
                ${TEST_LINK_LIBS})
  target_compile_features(adbc-driver-cube-test PRIVATE cxx_std_17)
  target_include_directories(adbc-driver-cube-test SYSTEM
                             PRIVATE ${REPOSITORY_ROOT}/c/ ${REPOSITORY_ROOT}/c/include/
                                     ${REPOSITORY_ROOT}/c/driver)
  adbc_configure_target(adbc-driver-cube-test)
endif()
