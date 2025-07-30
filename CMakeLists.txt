cmake_minimum_required(VERSION 3.10)
project(p2pshare)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(p2pshare
    main.cpp
    src/file_manager.cpp
)

find_package(OpenSSL REQUIRED)
target_link_libraries(p2pshare OpenSSL::Crypto)