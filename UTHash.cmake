include(FetchContent)

FetchContent_Declare(uthash
    GIT_REPOSITORY https://github.com/troydhanson/uthash

    # For reproducibility use a commit id rather than a tag
    GIT_TAG f69112c04f1b6e059b8071cb391a1fcc83791a00)
FetchContent_MakeAvailable(uthash)

set(UTHASH_INCLUDE_DIRS "${uthash_SOURCE_DIR}/src/")
