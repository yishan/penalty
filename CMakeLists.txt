cmake_minimum_required(VERSION 3.16)
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/penalty/VERSION" PROJECT_VER LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/penalty/VERSION")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(FoloToy-AI-Passport)
