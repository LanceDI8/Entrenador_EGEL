cmake_minimum_required(VERSION 3.25)

project(PruebaQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets HttpServer)

qt_standard_project_setup()
qt_add_executable(PruebaQt WIN32 main.cpp)

target_link_libraries(PruebaQt PRIVATE
    Qt6::Widgets
    Qt6::HttpServer
)