cmake_minimum_required(VERSION 3.10)

project(cdn-server VERSION 0.2 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Configurare Warning-uri (comun pentru Server si Client) ---
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

# --- Pachete Comune & Server ---
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_library(SQLITE3_LIB sqlite3)

# --- Build Client (optional, OFF in Docker) ---
option(BUILD_CLIENT "Build the audio client" ON)

if(BUILD_CLIENT)
    find_package(Curses REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)
endif()

# --- Include Directories ---
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${OPENSSL_INCLUDE_DIR})

# ==========================================
# TARGET: SERVER (myscdn_node)
# ==========================================
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

target_link_libraries(myscdn_node PRIVATE 
    Threads::Threads
    OpenSSL::SSL 
    OpenSSL::Crypto
)

if(SQLITE3_LIB)
    target_link_libraries(myscdn_node PRIVATE sqlite3)
endif()

# ==========================================
# TARGET: CLIENT (client) - optional
# ==========================================
if(BUILD_CLIENT)
add_executable(client
    src/client.cpp              # Langa main.cpp
    src/network/TcpSocket.cpp   # Reutilizam logica de socket
)

# Setam warning-urile si pentru client
target_compile_options(client PRIVATE ${PROJECT_WARNINGS})

# Includem header-ele specifice clientului (SDL, Ncurses)
target_include_directories(client PRIVATE
    ${CURSES_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
)

# Linkam bibliotecile clientului
target_link_libraries(client PRIVATE
    ${CURSES_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
    Threads::Threads
)
endif()