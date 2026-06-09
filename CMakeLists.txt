cmake_minimum_required(VERSION 3.23)

project(logger LANGUAGES CXX)

# Make the requested language mode explicit so both the real build and editor
# tooling see the same standard level.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Toggle optional test-related build behavior from the configure step.
option(BUILD_TESTS "Build unit tests" ON)
option(ENABLE_UBSAN "Build tests with UBSan" ON)

# Header-only target that carries include paths and the required language level.
add_library(dynamic_int INTERFACE)

target_include_directories(dynamic_int
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_features(dynamic_int
  INTERFACE
    cxx_std_17
)

# Reuse the same warning set across real build targets.
function(apply_common_warnings target_name)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(${target_name}
      PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
  endif()
endfunction()

if(WIN32)
  # CLANG64-built executables need a few runtime DLLs beside the .exe when run
  # outside the MSYS2 shell environment.
  get_filename_component(CLANG_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(EXTRA_RUNTIME_DLLS)
  foreach(DLL_NAME libc++.dll libunwind.dll libwinpthread-1.dll)
    if(EXISTS "${CLANG_BIN_DIR}/${DLL_NAME}")
      list(APPEND EXTRA_RUNTIME_DLLS "${CLANG_BIN_DIR}/${DLL_NAME}")
    endif()
  endforeach()
endif()

# Copy compiler/runtime DLLs next to the target after linking on Windows.
function(copy_runtime_dlls target_name)
  if(WIN32)
    add_custom_command(TARGET ${target_name} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_RUNTIME_DLLS:${target_name}>
        ${EXTRA_RUNTIME_DLLS}
        $<TARGET_FILE_DIR:${target_name}>
      COMMAND_EXPAND_LISTS
    )
  endif()
endfunction()

# Enable UBSan only on targets that opt into it.
function(ENABLE_UBSAN target_name)
  if(ENABLE_UBSAN AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
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
  playground.cpp
)

target_link_libraries(playground
  PRIVATE
    dynamic_int
)

apply_common_warnings(playground)
copy_runtime_dlls(playground)

if(BUILD_TESTS)
  # CTest owns test execution; GoogleTest provides the actual test framework.
  enable_testing()
  find_package(GTest CONFIG REQUIRED)

  # Keep test code split across files, but link it into one executable.
  set(TEST_SOURCES
    tests/test_dynamic_int.cpp
    tests/test_machine_num.cpp
    tests/test_constexpr_string_view.cpp
    tests/test_constexpr_string.cpp
    tests/test_heap.cpp
    tests/test_masked_bits.cpp
    tests/test_enum_spec.cpp
    tests/test_enum_random_fuzz.cpp
  )

  add_executable(logger_tests
    ${TEST_SOURCES}
  )

  target_link_libraries(logger_tests
    PRIVATE
      dynamic_int
      GTest::gtest_main
  )

  apply_common_warnings(logger_tests)
  copy_runtime_dlls(logger_tests)
  ENABLE_UBSAN(logger_tests)

  add_executable(bit_tests
    tests/test_bit.cpp
  )

  target_link_libraries(bit_tests
    PRIVATE
      dynamic_int
      GTest::gtest_main
  )

  apply_common_warnings(bit_tests)
  copy_runtime_dlls(bit_tests)
  ENABLE_UBSAN(bit_tests)

  add_executable(bit_fallback_tests
    tests/test_bit.cpp
  )

  target_compile_definitions(bit_fallback_tests
    PRIVATE
      CONSTEXPR_DISABLE_STD_BIT_CAST
      CONSTEXPR_DISABLE_BUILTIN_BIT_CAST
  )

  target_link_libraries(bit_fallback_tests
    PRIVATE
      dynamic_int
      GTest::gtest_main
  )

  apply_common_warnings(bit_fallback_tests)
  copy_runtime_dlls(bit_fallback_tests)
  ENABLE_UBSAN(bit_fallback_tests)

  include(GoogleTest)
  # Register each GoogleTest case with CTest so they can be listed and filtered
  # individually even though they live in one binary.
  gtest_discover_tests(bit_tests)
  gtest_discover_tests(bit_fallback_tests TEST_PREFIX "Fallback.")
  gtest_discover_tests(logger_tests)
endif()
