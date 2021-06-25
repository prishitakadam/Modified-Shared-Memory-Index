cmake_minimum_required(VERSION 3.8)

project(Remote_Btree)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -pthread -W -Wextra -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wsign-compare -Wshadow -Wno-unused-parameter -Wno-unused-variable -Woverloaded-virtual -Wnon-virtual-dtor -Wno-missing-field-initializers -Wno-strict-aliasing")

set(CMAKE_CXX_STANDARD 17)
set(SOURCES util/thread_local.cc
            rdma.cc
            port/port_posix.cc)
include_directories($(PROJECT_SOURCE_DIR))
include_directories($(PROJECT_SOURCE_DIR)/util)
include_directories($(PROJECT_SOURCE_DIR)/port)
add_executable(Demo_compute multithreaded_memory_allocations.cc ${SOURCES})
add_executable(Demo_memory server.cc ${SOURCES})
#target_link_libraries(RDMA ibverbs)
target_link_libraries(Demo_compute ibverbs)
target_link_libraries(Demo_memory ibverbs)