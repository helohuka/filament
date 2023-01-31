cmake_minimum_required (VERSION 3.6)

add_library(main SHARED
    src/main/cpp/main.cpp
)

# Ordering is significant in the following list. The PRIVATE qualifier prevents transitive deps.
target_link_libraries(main
    PRIVATE gamedriver
)

