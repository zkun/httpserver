cmake_minimum_required(VERSION 3.16)

project(HttpServer VERSION 6.9 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 COMPONENTS Core Network)

aux_source_directory(${PROJECT_SOURCE_DIR} PROJECT_SOURCES)
add_library(HttpServer ${PROJECT_SOURCES})

target_link_libraries(HttpServer
    PUBLIC
        Qt::Core
        Qt::Network
        Qt::CorePrivate
        Qt::NetworkPrivate
)

target_link_libraries(HttpServer PUBLIC WebSockets)
target_include_directories(HttpServer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
