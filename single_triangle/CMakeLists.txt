cmake_minimum_required(VERSION 3.10)
project(triben)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

add_executable(${PROJECT_NAME} src/main.cpp)

find_package(glad)
find_package(glfw3)
find_package(glm)
target_link_libraries(${PROJECT_NAME} glad::glad glfw glm::glm)
