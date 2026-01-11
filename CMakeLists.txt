cmake_minimum_required(VERSION 3.10)
project(mySCDN VERSION 0.1 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(PROJECT_WARNINGS 
        /W4       
        /permissive- 
    )
else()
    set(PROJECT_WARNINGS 
        -Wall 
        -Wextra  
        -pedantic
        -Wshadow            
        -Wnon-virtual-dtor  
        -Wold-style-cast    
        -Wcast-align        
        -Wunused            
        -Woverloaded-virtual 
        -Wconversion        
    )
endif()

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)

find_library(SQLITE3_LIB sqlite3)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SERVER_SOURCES 
    src/main.cpp
    src/cdn/Cache.cpp
    src/cdn/DatabaseManager.cpp
    src/cdn/HashRing.cpp
    src/cdn/LoadMonitor.cpp
    src/cdn/NodeConfig.cpp
    src/cdn/NodeServer.cpp
    src/network/TcpServer.cpp
    src/network/TcpSocket.cpp
)

add_executable(myscdn_node ${SERVER_SOURCES})

target_compile_options(myscdn_node PRIVATE ${PROJECT_WARNINGS})

if(SQLITE3_LIB)
    target_link_libraries(myscdn_node PRIVATE Threads::Threads sqlite3)
else()
    target_link_libraries(myscdn_node PRIVATE Threads::Threads)
endif()

set(CLIENT_SOURCES 
    src/client.cpp 
    src/network/TcpSocket.cpp
)

add_executable(myscdn_client ${CLIENT_SOURCES})

target_compile_options(myscdn_client PRIVATE ${PROJECT_WARNINGS})

find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

target_include_directories(myscdn_client SYSTEM PRIVATE 
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
)

target_link_libraries(myscdn_client PRIVATE 
    ${SDL2_LIBRARIES} 
    ${SDL2_MIXER_LIBRARIES} 
    ${CURSES_LIBRARIES} 
    Threads::Threads
)