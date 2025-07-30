# Configure testing.
set(TEST_BINS yatpool.test)
file(GLOB_RECURSE TEST_SOURCE_FILES ${PROJECT_ROOT_DIR}/tests/*.c)

include(AddCatch2)

message(STATUS "Testing enabled. Building test binaries ...")
add_executable(${TEST_BINS} ${TEST_SOURCE_FILES})

target_include_directories(${TEST_BINS} PUBLIC ${PROJECT_ROOT_DIR}/src)
target_include_directories(${TEST_BINS} PUBLIC ${PROJECT_ROOT_DIR}/third_party/Catch2/include)
target_link_libraries(${TEST_BINS} PRIVATE 
    ${PROJECT_LIBRARY_NAME}
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(${TEST_BINS})
