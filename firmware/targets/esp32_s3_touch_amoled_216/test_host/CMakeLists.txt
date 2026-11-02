cmake_minimum_required(VERSION 3.16)
project(number_bomb_host LANGUAGES CXX)

add_executable(number_bomb_test
    number_bomb_test.cpp
    ../main/number_bomb.cpp
)
target_include_directories(number_bomb_test PRIVATE ../main)
target_compile_features(number_bomb_test PRIVATE cxx_std_17)
enable_testing()
add_test(NAME number_bomb COMMAND number_bomb_test)
