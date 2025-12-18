cmake_minimum_required(VERSION 3.10)
project(SimpleFPS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GLFW
find_package(glfw3 3.3 REQUIRED)
# OpenGL
find_package(OpenGL REQUIRED)

# Пути к исходникам
set(SOURCES
    src/main.cpp
    src/maze.cpp
    src/camera.cpp
    src/shader.cpp
    src/glad.c
)

# Включаем include путь для заголовков
include_directories(src)

# Создаём исполняемый файл
add_executable(SimpleFPS ${SOURCES})

# Линкуем библиотеки
target_link_libraries(SimpleFPS glfw OpenGL::GL)

# Копируем шейдеры и текстуры в папку сборки
file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${CMAKE_SOURCE_DIR}/textures DESTINATION ${CMAKE_BINARY_DIR})

