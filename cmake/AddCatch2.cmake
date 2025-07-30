include(FetchContent)
FetchContent_declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.0
)

FetchContent_MakeAvailable(Catch2)
if(EXISTS ${PROJECT_ROOT_DIR}/build/_deps/catch2-src/src/catch2/catch_all.hpp)
    message(STATUS "Downloaded Catch2 successfully. Now building ...")
else()
    message(FATAL_ERROR "Failed to fetch Catch2.")
endif()
message(STATUS "Added Catch2 to CMake module path: ${CMAKE_MODULE_PATH}")
