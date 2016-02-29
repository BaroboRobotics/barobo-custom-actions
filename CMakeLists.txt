cmake_minimum_required(VERSION 3.2.1)
# Tell Visual C++ to statically link with the C runtime, so we can make a
# totally standalone custom action DLL.
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX ${CMAKE_CURRENT_SOURCE_DIR}/cxx_flag_overrides.cmake)
project(barobo-custom-actions)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost 1.59.0 REQUIRED COMPONENTS system filesystem)

add_subdirectory(cxx-util)

set(sources src/customactions.cpp)

add_library(barobo-custom-actions MODULE ${sources})
target_include_directories(barobo-custom-actions PRIVATE ${Boost_INCLUDE_DIRS} boost-process)
target_link_libraries(barobo-custom-actions PRIVATE cxx-util ${Boost_LIBRARIES})

set_target_properties(barobo-custom-actions PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
