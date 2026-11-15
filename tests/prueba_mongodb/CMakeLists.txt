cmake_minimum_required(VERSION 3.25)

project(PruebaMongoDB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(mongocxx 4.6 CONFIG REQUIRED)
find_package(bsoncxx 4.6 CONFIG REQUIRED)

add_executable(PruebaMongoDB main.cpp)

target_link_libraries(PruebaMongoDB PRIVATE
    mongo::mongocxx_shared
    mongo::bsoncxx_shared
)