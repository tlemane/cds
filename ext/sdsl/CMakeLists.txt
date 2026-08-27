include(FetchContent)
FetchContent_Declare(sdsl-lite
    GIT_REPOSITORY https://github.com/xxsds/sdsl-lite.git
    GIT_TAG        v3.0.4
    GIT_SHALLOW    TRUE
    SYSTEM
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(sdsl-lite)
