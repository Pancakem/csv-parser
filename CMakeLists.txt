cmake_minimum_required(VERSION 3.27)

project(csv-parser)

include(FetchContent)
FetchContent_Declare(lexy URL https://lexy.foonathan.net/download/lexy-src.zip)
FetchContent_MakeAvailable(lexy)

add_executable(csv-parser)
target_sources(csv-parser PRIVATE main.cpp)
target_link_libraries(csv-parser PRIVATE foonathan::lexy)
