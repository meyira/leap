cmake_minimum_required(VERSION 3.20)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_SOURCE_DIR ..)
set(PROJECT_SOURCE_DIR ..)
set(PROJECT_BINARY_DIR "../build")
set(PROJECT_VERSION "0.1")

set(CMAKE_C_FLAGS "-ffunction-sections -Wall -msse2 \
-msse4.1 -mpclmul -march=core-avx2 -Wno-ignored-attributes \
-fPIC -fno-tree-vectorize -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++17 -maes -mrdseed")

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO " -O3 -g -ggdb -rdynamic")
set(CMAKE_CXX_FLAGS_DEBUG "-g -ggdb -rdynamic")

set(SRCS ${SRCS}
  oprf/BCH.cpp
  oprf/ntt-falcon.cpp
  oprf/PRF.cpp
  oprf/Subset128.cpp
  psi/SpringNRPSIClient.cpp
  psi/SpringNRPSIServer.cpp
  )

add_library(leap SHARED ${SRCS})
set_target_properties(leap PROPERTIES VERSION ${PROJECT_VERSION})

add_subdirectory(thirdparty/SHAKE256)
add_subdirectory(psi/cuckoofilter)

target_include_directories(leap PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../")
target_include_directories(leap PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/oprf/")
find_package(libOTe REQUIRED)
if (NOT libOTe_FOUND)
  message(STATUS "libOTe was not found, please install according to README")
endif ()
target_link_libraries(leap PUBLIC keccak256 cuckoo)

add_subdirectory(tests)

