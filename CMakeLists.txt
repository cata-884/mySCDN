cmake_minimum_required(VERSION 3.16)
project(mySCDN VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic -Werror=return-type)

find_package(Threads REQUIRED)

set(SOURCES
    src/main.cpp
    src/network/TcpSocket.cpp
    src/network/TcpServer.cpp
    src/cdn/Cache.cpp
    src/cdn/HashRing.cpp
    src/cdn/NodeConfig.cpp
    src/cdn/NodeServer.cpp
    src/cdn/LoadMonitor.cpp
)

add_executable(myscdn_node ${SOURCES})
target_include_directories(myscdn_node PRIVATE include)
target_link_libraries(myscdn_node PRIVATE Threads::Threads)


