cmake_minimum_required(VERSION 3.10)

project(cdn-server VERSION 0.3 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Mentinem standardul C++23
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Warning-uri stricte (Foarte util pentru debug la io_uring/pointers)
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

# --- DEPENDINȚE ---

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# 1. Căutăm io_uring (Critic pentru noul server)
find_library(URING_LIB uring REQUIRED)

# 2. Căutăm RocksDB (Înlocuitor SQLite)
find_library(ROCKSDB_LIB rocksdb REQUIRED)

# 3. Dependențe tranzitive RocksDB
# (Pe Linux, RocksDB cere adesea astea la linkare daca nu e compilat static complet)
find_library(SNAPPY_LIB snappy)
find_library(Z_LIB z)
find_library(LZ4_LIB lz4)
find_library(ZSTD_LIB zstd)
find_library(BZ2_LIB bz2)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${OPENSSL_INCLUDE_DIR})

# --- SURSE ---
# Am scos LoadMonitor.cpp si am adaugat ReadOnlyFile.cpp

set(SERVER_SOURCES
        src/main.cpp
        src/cdn/Cache.cpp
        src/cdn/DatabaseManager.cpp  # Implementare noua cu RocksDB
        src/cdn/ConsistentHashing.cpp
        src/cdn/NodeConfig.cpp
        src/cdn/NodeServer.cpp       # Implementare noua cu io_uring
        # Clasa noua pentru Disk I/O
        src/network/TcpListener.cpp
        src/network/TcpSocket.cpp

        # Header files (listate aici ca sa apara in IDE-uri gen VS Code / CLion)
        include/cdn/HttpUtils.hpp
        include/network/NetworkTypes.hpp
        include/cdn/ReadOnlyFile.hpp
)

add_executable(myscdn_node ${SERVER_SOURCES})

target_compile_options(myscdn_node PRIVATE ${PROJECT_WARNINGS})

# --- LINKARE ---

target_link_libraries(myscdn_node PRIVATE
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
        ${URING_LIB}      # liburing
        ${ROCKSDB_LIB}    # librocksdb

        # Compresii pentru RocksDB (le punem conditionat, doar daca au fost gasite)
        $<$<BOOL:${SNAPPY_LIB}>:${SNAPPY_LIB}>
        $<$<BOOL:${Z_LIB}>:${Z_LIB}>
        $<$<BOOL:${LZ4_LIB}>:${LZ4_LIB}>
        $<$<BOOL:${ZSTD_LIB}>:${ZSTD_LIB}>
        $<$<BOOL:${BZ2_LIB}>:${BZ2_LIB}>
)