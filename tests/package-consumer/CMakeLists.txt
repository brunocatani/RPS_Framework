cmake_minimum_required(VERSION 4.2)
project(RPSFrameworkPackageConsumer LANGUAGES CXX)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

find_package(RPSFramework CONFIG REQUIRED)

add_executable(RPSFrameworkPackageConsumer main.cpp)
target_compile_features(RPSFrameworkPackageConsumer PRIVATE cxx_std_23)
target_link_libraries(RPSFrameworkPackageConsumer PRIVATE RPS::Addresses RPS::Runtime)
