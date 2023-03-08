cmake_minimum_required(VERSION 3.19)
project(rapidxml)

set(TARGET rapidxml)
set(OUR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# ==================================================================================================
# Sources and headers
# ==================================================================================================
set(PUBLIC_HDRS
        ${OUR_DIR}/rapidxml.hpp
        ${OUR_DIR}/rapidxml_iterators.hpp
        ${OUR_DIR}/rapidxml_print.hpp
        ${OUR_DIR}/rapidxml_utils.hpp
)

# ==================================================================================================
# Include and target definitions
# ==================================================================================================
include_directories(${OUR_DIR}/)

add_library(${TARGET} INTERFACE)
target_sources(${TARGET} INTERFACE ${PUBLIC_HDRS})
target_include_directories(${TARGET} INTERFACE ${OUR_DIR})
set_target_properties(${TARGET} PROPERTIES FOLDER ThirdParty)

# ==================================================================================================
# Installation
# ==================================================================================================
install(FILES ${PUBLIC_HDRS} DESTINATION include/${TARGET})
