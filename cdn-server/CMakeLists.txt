cmake_minimum_required(VERSION 3.10)

project(cdn-server VERSION 0.2 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(PROJECT_WARNINGS /W4 /permissive-)
else()
    set(PROJECT_WARNINGS 
        -Wall -Wextra -pedantic -Wshadow 
        -Wnon-virtual-dtor -Wold-style-cast 
        -Wcast-align -Wunused -Woverloaded-virtual 
        -Wconversion
    )
endif()

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

find_library(SQLITE3_LIB sqlite3)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${OPENSSL_INCLUDE_DIR})

set(SERVER_SOURCES 
    src/main.cpp
    src/cdn/Cache.cpp
    src/cdn/DatabaseManager.cpp
    src/cdn/HashRing.cpp
    src/cdn/LoadMonitor.cpp
    src/cdn/NodeConfig.cpp
    src/cdn/NodeServer.cpp
        src/network/TcpListener.cpp
    src/network/TcpSocket.cpp
)

add_executable(myscdn_node ${SERVER_SOURCES})

target_compile_options(myscdn_node PRIVATE ${PROJECT_WARNINGS})

target_link_libraries(myscdn_node PRIVATE 
    Threads::Threads
    OpenSSL::SSL 
    OpenSSL::Crypto
)

if(SQLITE3_LIB)
    target_link_libraries(myscdn_node PRIVATE sqlite3)
endif()
