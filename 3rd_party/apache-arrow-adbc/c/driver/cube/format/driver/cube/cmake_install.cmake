# Install script for directory: /home/io/projects/learn_erl/adbc/3rd_party/apache-arrow-adbc/c/driver/cube

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libadbc_driver_cube.so.107.0.0"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libadbc_driver_cube.so.107"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHECK
           FILE "${file}"
           RPATH "")
    endif()
  endforeach()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES
    "/home/io/projects/learn_erl/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/format/driver/cube/libadbc_driver_cube.so.107.0.0"
    "/home/io/projects/learn_erl/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/format/driver/cube/libadbc_driver_cube.so.107"
    )
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libadbc_driver_cube.so.107.0.0"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libadbc_driver_cube.so.107"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/usr/bin/strip" "${file}")
      endif()
    endif()
  endforeach()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/io/projects/learn_erl/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/format/driver/cube/libadbc_driver_cube.so")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "/home/io/projects/learn_erl/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/format/driver/cube/libadbc_driver_cube.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/AdbcDriverCube" TYPE FILE FILES
    "/home/io/projects/learn_erl/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/format/driver/cube/AdbcDriverCubeConfig.cmake"
    "/home/io/projects/learn_erl/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/format/driver/cube/AdbcDriverCubeConfigVersion.cmake"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/AdbcDriverCube/AdbcDriverCubeTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/AdbcDriverCube/AdbcDriverCubeTargets.cmake"
         "/home/io/projects/learn_erl/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/format/driver/cube/CMakeFiles/Export/138b4276eff6ae1ebd0bd9beadc61b97/AdbcDriverCubeTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/AdbcDriverCube/AdbcDriverCubeTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/AdbcDriverCube/AdbcDriverCubeTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/AdbcDriverCube" TYPE FILE FILES "/home/io/projects/learn_erl/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/format/driver/cube/CMakeFiles/Export/138b4276eff6ae1ebd0bd9beadc61b97/AdbcDriverCubeTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^()$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/AdbcDriverCube" TYPE FILE FILES "/home/io/projects/learn_erl/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/format/driver/cube/CMakeFiles/Export/138b4276eff6ae1ebd0bd9beadc61b97/AdbcDriverCubeTargets-noconfig.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/home/io/projects/learn_erl/adbc/3rd_party/apache-arrow-adbc/c/driver/cube/format/driver/cube/adbc-driver-cube.pc")
endif()

