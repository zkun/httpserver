cmake_minimum_required(VERSION 3.16)

project(httpserver
    VERSION 0.1
    DESCRIPTION "Qt HTTP Server")

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Network WebSockets)

add_library(httpserver
    qabstracthttpserver.cpp qabstracthttpserver.h qabstracthttpserver_p.h
    qhttpserver.cpp qhttpserver.h qhttpserver_p.h
    qhttpserverliterals.cpp qhttpserverliterals_p.h
    qhttpserverrequest.cpp qhttpserverrequest.h qhttpserverrequest_p.h
    qhttpserverresponder.cpp qhttpserverresponder.h qhttpserverresponder_p.h
    qhttpserverresponse.cpp qhttpserverresponse.h qhttpserverresponse_p.h
    qhttpserverrouter.cpp qhttpserverrouter.h qhttpserverrouter_p.h
    qhttpserverrouterrule.cpp qhttpserverrouterrule.h qhttpserverrouterrule_p.h
    qhttpserverrouterviewtraits.h
)

target_link_libraries(httpserver PRIVATE httpparser)
target_include_directories(httpserver PUBLIC ${PROJECT_SOURCE_DIR})

target_link_libraries(httpserver PRIVATE
    Qt::Network
    Qt::CorePrivate
    Qt::WebSockets
)
