cmake_minimum_required(VERSION 3.4)
project(toggle)
add_executable(toggle WIN32 toggle.c)
add_definitions(-DUNICODE -D_UNICODE)
target_sources(toggle PRIVATE toggle.rc)
#target_sources(toggle PRIVATE toggle.exe.manifest)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
# Release binary output at top level
set_target_properties(toggle PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "..")
# cmake -S . -B build  &&  cmake --build build --config Release
