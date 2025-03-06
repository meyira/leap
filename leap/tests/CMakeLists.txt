cmake_minimum_required(VERSION 3.4.1)
set(TEST_SRCS
  oprf-ref.cpp
  test-psi.cpp
  baseOT/KyberOtPreprocessing.cpp
  baseOT/SimplestOtPreprocessing.cpp
  baseOT/SilentSimplestOtPreprocessing.cpp
  baseOT/SilentKyberOtPreprocessing.cpp
  )

find_package(libOTe REQUIRED)

foreach (filename ${TEST_SRCS})
  get_filename_component(testname ${filename} NAME_WE)
  add_executable(${testname} ${filename})
  target_link_libraries(${testname} leap pthread   oc::libOTe)
  target_compile_options(${testname} PRIVATE -O3)
  target_link_options(${testname} PRIVATE -O3)
endforeach ()
