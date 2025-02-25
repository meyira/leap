#project(SHAKE256 C)
enable_language(ASM)
set(KECCAK_SRCS ${KECCAK_SRCS}
        SHAKE256.cpp
        keccak/KeccakSpongeWidth1600.c
        keccak/KeccakHash.c
        keccak/KeccakP-1600-AVX2.s
        )
  set_property(SOURCE avx2/KeccakP-1600-AVX2.s PROPERTY LANGUAGE C)


add_library(keccak256 SHARED ${KECCAK_SRCS})
target_include_directories(keccak256 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>
        $<INSTALL_INTERFACE:include>)

target_include_directories(keccak256 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
        $<INSTALL_INTERFACE:include>)

install(TARGETS keccak256 DESTINATION lib)
install(DIRECTORY . DESTINATION include/keccak256 FILES_MATCHING PATTERN "*.h")

