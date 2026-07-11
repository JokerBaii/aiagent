cmake_minimum_required(VERSION 3.20)

project(zhinong_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(zhinong-demo
    src/main.cpp
    src/claim_auditor.cpp
)

target_include_directories(zhinong-demo PRIVATE src)
