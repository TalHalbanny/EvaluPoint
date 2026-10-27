cmake_minimum_required(VERSION 3.16)
project(gdtevaluator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gdtevaluator main.cpp mainLogic.hpp)

if(WIN32)
    target_link_libraries(gdtevaluator PRIVATE comdlg32 glu32 opengl32 gdi32 user32)
endif()
