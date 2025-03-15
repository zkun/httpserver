cmake_minimum_required(VERSION 3.16)

project(httpserver
    VERSION 0.1
    DESCRIPTION "Qt HTTP Server")

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Network WebSockets)

add_library(httpserver
    src/qhttpserverliterals.h
    src/qhttpserver.h src/qhttpserver.cpp
    src/qhttpserverrequest.h src/qhttpserverrequest_p.h src/qhttpserverrequest.cpp
    src/qhttpserverresponse.h src/qhttpserverresponse_p.h src/qhttpserverresponse.cpp
    src/qabstracthttpserver.h src/qabstracthttpserver_p.h src/qabstracthttpserver.cpp
    src/qhttpserverresponder.h src/qhttpserverresponder_p.h src/qhttpserverresponder.cpp
    http-parser/http_parser.h http-parser/http_parser.c
)

target_include_directories(httpserver PRIVATE src)
target_include_directories(httpserver PRIVATE http-parser)

target_link_libraries(httpserver PRIVATE
    Qt::Network
    Qt::CorePrivate
)

if(Qt5WebSockets_FOUND)
    target_link_libraries(httpserver PRIVATE Qt::WebSockets)
endif()

