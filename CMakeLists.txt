cmake_minimum_required(VERSION 3.23)

project(logger LANGUAGES CXX)

option(DNUM_ENABLE_UBSAN "Build tests with UBSan" ON)

add_library(dynamic_num INTERFACE)

target_include_directories(dynamic_num
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(dynamic_num
  INTERFACE
    cxx_std_17
)

enable_testing()

find_package(GTest CONFIG REQUIRED)

add_executable(dynamic_num_tests
  tests/test_dynamic_num.cpp
)

add_executable(machine_num_tests
  tests/test_machine_num.cpp
)

add_executable(constexpr_cstr_tests
  tests/test_constexpr_cstr.cpp
)

add_executable(strarray_tests
  tests/test_strarray.cpp
)

target_link_libraries(dynamic_num_tests
  PRIVATE
    dynamic_num
    GTest::gtest_main
)

target_link_libraries(machine_num_tests
  PRIVATE
    dynamic_num
    GTest::gtest_main
)

target_link_libraries(constexpr_cstr_tests
  PRIVATE
    dynamic_num
    GTest::gtest_main
)

target_link_libraries(strarray_tests
  PRIVATE
    dynamic_num
    GTest::gtest_main
)

if(WIN32)
  get_filename_component(DNUM_CLANG_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(DNUM_EXTRA_RUNTIME_DLLS)
  foreach(DNUM_DLL_NAME libc++.dll libunwind.dll libwinpthread-1.dll)
    if(EXISTS "${DNUM_CLANG_BIN_DIR}/${DNUM_DLL_NAME}")
      list(APPEND DNUM_EXTRA_RUNTIME_DLLS "${DNUM_CLANG_BIN_DIR}/${DNUM_DLL_NAME}")
    endif()
  endforeach()

  add_custom_command(TARGET dynamic_num_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:dynamic_num_tests>
      ${DNUM_EXTRA_RUNTIME_DLLS}
      $<TARGET_FILE_DIR:dynamic_num_tests>
    COMMAND_EXPAND_LISTS
  )

  add_custom_command(TARGET machine_num_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:machine_num_tests>
      ${DNUM_EXTRA_RUNTIME_DLLS}
      $<TARGET_FILE_DIR:machine_num_tests>
    COMMAND_EXPAND_LISTS
  )

  add_custom_command(TARGET constexpr_cstr_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:constexpr_cstr_tests>
      ${DNUM_EXTRA_RUNTIME_DLLS}
      $<TARGET_FILE_DIR:constexpr_cstr_tests>
    COMMAND_EXPAND_LISTS
  )

  add_custom_command(TARGET strarray_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_RUNTIME_DLLS:strarray_tests>
      ${DNUM_EXTRA_RUNTIME_DLLS}
      $<TARGET_FILE_DIR:strarray_tests>
    COMMAND_EXPAND_LISTS
  )
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(dynamic_num_tests
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )

  target_compile_options(machine_num_tests
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )

  target_compile_options(constexpr_cstr_tests
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )

  target_compile_options(strarray_tests
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
  )
endif()

if(DNUM_ENABLE_UBSAN)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(dynamic_num_tests
      PRIVATE
        -fsanitize=undefined
        -fno-sanitize-recover=undefined
    )

    target_compile_options(machine_num_tests
      PRIVATE
        -fsanitize=undefined
        -fno-sanitize-recover=undefined
    )

    target_link_options(dynamic_num_tests
      PRIVATE
        -fsanitize=undefined
        -fno-sanitize-recover=undefined
    )

    target_link_options(machine_num_tests
      PRIVATE
        -fsanitize=undefined
        -fno-sanitize-recover=undefined
    )

    target_compile_options(constexpr_cstr_tests
      PRIVATE
        -fsanitize=undefined
        -fno-sanitize-recover=undefined
    )

    target_link_options(constexpr_cstr_tests
      PRIVATE
        -fsanitize=undefined
        -fno-sanitize-recover=undefined
    )

    target_compile_options(strarray_tests
      PRIVATE
        -fsanitize=undefined
        -fno-sanitize-recover=undefined
    )

    target_link_options(strarray_tests
      PRIVATE
        -fsanitize=undefined
        -fno-sanitize-recover=undefined
    )
  endif()
endif()

include(GoogleTest)
gtest_discover_tests(dynamic_num_tests)
gtest_discover_tests(machine_num_tests)
gtest_discover_tests(constexpr_cstr_tests)
gtest_discover_tests(strarray_tests)
