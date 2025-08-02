cmake_minimum_required(VERSION 3.10)
project(p2pshare)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(p2pserver
    src/main_server.cpp
    src/file_manager.cpp
    src/peer_server.cpp
    src/sha1_util.cpp
)

add_executable(p2pclient
    src/main_client.cpp
    src/peer_client.cpp
    src/sha1_util.cpp
)

find_package(OpenSSL REQUIRED)
target_link_libraries(p2pserver OpenSSL::Crypto)
target_link_libraries(p2pclient OpenSSL::Crypto)