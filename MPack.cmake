include(FetchContent)

FetchContent_Declare(mpack
    GIT_REPOSITORY https://github.com/ludocode/mpack.git

    # For reproducibility use a commit id rather than a tag
    GIT_TAG 79d3fcd3e04338b06e82d01a62f4aa98c7bad5f7)
FetchContent_MakeAvailable(mpack)

set(MPACK_DIR "${mpack_SOURCE_DIR}/src/mpack/")

add_library(mpack STATIC
    "${MPACK_DIR}/mpack-common.c"
    "${MPACK_DIR}/mpack-expect.c"
    "${MPACK_DIR}/mpack-node.c"
    "${MPACK_DIR}/mpack-platform.c"
    "${MPACK_DIR}/mpack-reader.c"
    "${MPACK_DIR}/mpack-writer.c"
)

target_include_directories(mpack PUBLIC "${MPACK_DIR}")
target_compile_definitions(mpack PRIVATE MPACK_HAS_GENERIC)