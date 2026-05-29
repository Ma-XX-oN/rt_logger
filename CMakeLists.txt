cmake_minimum_required(VERSION 3.23)

project(logger LANGUAGES CXX)

# Toggle optional test-related build behavior from the configure step.
option(DINT_BUILD_TESTS "Build unit tests" ON)
option(DINT_ENABLE_UBSAN "Build tests with UBSan" ON)

# Header-only target that carries include paths and the required language level.
add_library(dynamic_int INTERFACE)

target_include_directories(dynamic_int
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(dynamic_int
  INTERFACE
    cxx_std_17
)

# Reuse the same warning set across real build targets.
function(dint_apply_common_warnings target_name)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name}
      PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
  endif()
endfunction()

if(WIN32)
  # CLANG64-built executables need a few runtime DLLs beside the .exe when run
  # outside the MSYS2 shell environment.
  get_filename_component(DINT_CLANG_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(DINT_EXTRA_RUNTIME_DLLS)
  foreach(DINT_DLL_NAME libc++.dll libunwind.dll libwinpthread-1.dll)
    if(EXISTS "${DINT_CLANG_BIN_DIR}/${DINT_DLL_NAME}")
      list(APPEND DINT_EXTRA_RUNTIME_DLLS "${DINT_CLANG_BIN_DIR}/${DINT_DLL_NAME}")
    endif()
  endforeach()
endif()

# Copy compiler/runtime DLLs next to the target after linking on Windows.
function(dint_copy_runtime_dlls target_name)
  if(WIN32)
    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${target_name}>
        ${DINT_EXTRA_RUNTIME_DLLS}
        $<TARGET_FILE_DIR:${target_name}>
      COMMAND_EXPAND_LISTS
    )
  endif()
endfunction()

# Enable UBSan only on targets that opt into it.
function(dint_enable_ubsan target_name)
  if(DINT_ENABLE_UBSAN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name}
      PRIVATE
        -fsanitize=undefined
        -fno-sanitize-recover=undefined
    )

    target_link_options(${target_name}
      PRIVATE
        -fsanitize=undefined
        -fno-sanitize-recover=undefined
    )
  endif()
endfunction()

# Small scratch executable for manual experiments while developing the library.
add_executable(playground
  include/playground.cpp
)

target_link_libraries(playground
  PRIVATE
    dynamic_int
)

dint_apply_common_warnings(playground)
dint_copy_runtime_dlls(playground)

if(DINT_BUILD_TESTS)
  # CTest owns test execution; GoogleTest provides the actual test framework.
  enable_testing()
  find_package(GTest CONFIG REQUIRED)

  # Keep test code split across files, but link it into one executable.
  set(DINT_TEST_SOURCES
    tests/test_dynamic_int.cpp
    tests/test_machine_num.cpp
    tests/test_constexpr_cstr.cpp
    tests/test_constexpr_astr.cpp
  )

  add_executable(logger_tests
    ${DINT_TEST_SOURCES}
  )

  target_link_libraries(logger_tests
    PRIVATE
      dynamic_int
      GTest::gtest_main
  )

  dint_apply_common_warnings(logger_tests)
  dint_copy_runtime_dlls(logger_tests)
  dint_enable_ubsan(logger_tests)

  include(GoogleTest)
  # Register each GoogleTest case with CTest so they can be listed and filtered
  # individually even though they live in one binary.
  gtest_discover_tests(logger_tests)
endif()
