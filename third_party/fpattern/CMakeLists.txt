include(FetchContent)

FetchContent_Declare(fpattern
    GIT_REPOSITORY "https://github.com/alexbatalov/fpattern"
    GIT_TAG 8523173ec252c3b796fcdfca0fcc6329642fbbe3 # v1.9
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(fpattern)
