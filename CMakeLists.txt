cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Edit this list when you add or remove source files. If a file is not listed here, it is not built.
target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/plugin-main.cpp src/browser-dock.cpp src/browser-dock.hpp src/browser-sources.cpp src/browser-sources.hpp
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE PLUGIN_NAME="${_name}" PLUGIN_VERSION="${_version}")

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Core Qt6::Widgets)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
