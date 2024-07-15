target_compile_definitions(trinity-compile-option-interface
  INTERFACE
    _WIN32_WINNT=0x0A00                     # Windows 10
    NTDDI_VERSION=0x0A000007                # 19H1 (1903)
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    TRINITY_REQUIRED_WINDOWS_BUILD=18362)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
  include(${CMAKE_SOURCE_DIR}/cmake/compiler/msvc/settings.cmake)
elseif(CMAKE_CXX_PLATFORM_ID MATCHES "MinGW")
  include(${CMAKE_SOURCE_DIR}/cmake/compiler/mingw/settings.cmake)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  include(${CMAKE_SOURCE_DIR}/cmake/compiler/clang/settings.cmake)
endif()
