cmake_minimum_required(VERSION 3.1 FATAL_ERROR)

project(TdExample VERSION 1.0 LANGUAGES CXX)

add_subdirectory(td)
include(FindFUSE.cmake)

set(FUSE_COMPILER_FLAGS "-D_FILE_OFFSET_BITS=64")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FUSE_COMPILER_FLAGS}")

add_executable(tgfs tgfs.cpp td.cpp td_file_info.cpp td_chat.cpp td_message.cpp)
target_link_libraries(tgfs Td::TdStatic)
set_property(TARGET tgfs PROPERTY CXX_STANDARD 14)
