project(HttpServer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)

find_package(Qt5 REQUIRED COMPONENTS Network WebSockets Concurrent)

aux_source_directory(. PROJECT_SOURCES)
file(GLOB_RECURSE PROJECT_INCLUDE RELATIVE ${PROJECT_SOURCE_DIR} *.h)

add_library(${PROJECT_NAME} ${PROJECT_SOURCES} ${PROJECT_INCLUDE})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        HttpParser
        Qt::CorePrivate
        Qt::WebSocketsPrivate)

target_link_libraries(${PROJECT_NAME} PUBLIC
        Qt::Network Qt::WebSockets Qt::Concurrent)
target_include_directories(${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include)
