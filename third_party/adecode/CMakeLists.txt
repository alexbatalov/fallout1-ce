include(FetchContent)

FetchContent_Declare(adecode
    GIT_REPOSITORY "https://github.com/alexbatalov/adecode"
    GIT_TAG e4a8b0f3b66826e0b7779a25d79df8497f8f8087 # v1.0.0
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(adecode)
