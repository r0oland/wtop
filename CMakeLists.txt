cmake_minimum_required(VERSION 3.20)

project(wtop VERSION 0.1.0 LANGUAGES CXX)

# Options
option(WTOP_ENABLE_LTO "Enable Link Time Optimization" ON)
option(WTOP_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Determine build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Warnings
if(MSVC)
  set(WTOP_WARNINGS /W4 /permissive- /EHsc /Zc:preprocessor /Zc:__cplusplus)
  if(WTOP_WARNINGS_AS_ERRORS)
    list(APPEND WTOP_WARNINGS /WX)
  endif()
else()
  set(WTOP_WARNINGS -Wall -Wextra -Wpedantic)
  if(WTOP_WARNINGS_AS_ERRORS)
    list(APPEND WTOP_WARNINGS -Werror)
  endif()
endif()

# Collect sources
file(GLOB_RECURSE WTOP_SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE WTOP_HEADERS CONFIGURE_DEPENDS include/*.hpp)
add_executable(wtop WIN32 ${WTOP_SOURCES})

target_include_directories(wtop PRIVATE include)

# Windows specific definitions & libs
if(WIN32)
  target_compile_definitions(wtop PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(wtop PRIVATE user32 gdi32 shell32 iphlpapi pdh shcore)
endif()

# Apply warnings
target_compile_options(wtop PRIVATE ${WTOP_WARNINGS})

# clang-format integration (optional)
find_program(CLANG_FORMAT_EXE NAMES clang-format clang-format.exe)
if(CLANG_FORMAT_EXE)
  add_custom_target(format
    COMMAND ${CLANG_FORMAT_EXE} -i ${WTOP_SOURCES} ${WTOP_HEADERS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-format on sources")
  add_custom_target(format-check
    COMMAND ${CLANG_FORMAT_EXE} --Werror --dry-run ${WTOP_SOURCES} ${WTOP_HEADERS}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking clang-format compliance")
endif()

# Enable LTO if requested
if(WTOP_ENABLE_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT ltoSupported OUTPUT ltoOutput)
  if(ltoSupported)
    set_property(TARGET wtop PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(STATUS "IPO/LTO not supported: ${ltoOutput}")
  endif()
endif()

# Install (optional)
install(TARGETS wtop RUNTIME DESTINATION bin)

# Packaging version info placeholder
configure_file(cmake/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h @ONLY)

target_include_directories(wtop PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
