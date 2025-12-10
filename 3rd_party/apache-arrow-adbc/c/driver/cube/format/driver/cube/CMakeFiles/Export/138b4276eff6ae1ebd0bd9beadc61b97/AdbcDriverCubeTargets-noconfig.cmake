#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "AdbcDriverCube::adbc_driver_cube_shared" for configuration ""
set_property(TARGET AdbcDriverCube::adbc_driver_cube_shared APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(AdbcDriverCube::adbc_driver_cube_shared PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libadbc_driver_cube.so.107.0.0"
  IMPORTED_SONAME_NOCONFIG "libadbc_driver_cube.so.107"
  )

list(APPEND _cmake_import_check_targets AdbcDriverCube::adbc_driver_cube_shared )
list(APPEND _cmake_import_check_files_for_AdbcDriverCube::adbc_driver_cube_shared "${_IMPORT_PREFIX}/lib/libadbc_driver_cube.so.107.0.0" )

# Import target "AdbcDriverCube::adbc_driver_cube_static" for configuration ""
set_property(TARGET AdbcDriverCube::adbc_driver_cube_static APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(AdbcDriverCube::adbc_driver_cube_static PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libadbc_driver_cube.a"
  )

list(APPEND _cmake_import_check_targets AdbcDriverCube::adbc_driver_cube_static )
list(APPEND _cmake_import_check_files_for_AdbcDriverCube::adbc_driver_cube_static "${_IMPORT_PREFIX}/lib/libadbc_driver_cube.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
