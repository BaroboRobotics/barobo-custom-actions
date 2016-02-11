cmake_minimum_required(VERSION 3.2.1)
project(wix-custom-actions)

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost 1.54.0 REQUIRED COMPONENTS system filesystem)

set(sources src/uninstallnsispackage.cpp)

add_library(wix-custom-actions MODULE ${sources})
target_include_directories(wix-custom-actions PRIVATE ${Boost_INCLUDE_DIRS} boost-process)
target_link_libraries(wix-custom-actions PRIVATE msi ${Boost_LIBRARIES})

set_target_properties(wix-custom-actions PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
