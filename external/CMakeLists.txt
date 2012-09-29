cmake_minimum_required(VERSION 2.8)

add_subdirectory(dynamic)

set(PCRE_MINIMAL_DEFAULT "OFF")
add_subdirectory(pcre-8.30)
