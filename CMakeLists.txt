cmake_minimum_required(VERSION 3.28)
project(Ticket_System)

set(CMAKE_CXX_STANDARD 20)

include_directories(src/backend/include)
include_directories(src/backend/include/third_party)

# Main executable
add_executable(code src/backend/main.cpp)
#target_compile_options(code PRIVATE -O2)

# Unit test executables
file(GLOB UNIT_TEST_SOURCES src/backend/unit_test/*.cpp)

foreach(test_src ${UNIT_TEST_SOURCES})
    # Get the filename without directory and extension
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})

    # Set same include directories for the test executable
    target_include_directories(${test_name} PRIVATE
            src/backend/include
            src/backend/include/third_party
    )

    set_target_properties(${test_name} PROPERTIES CXX_STANDARD 20)
endforeach()
