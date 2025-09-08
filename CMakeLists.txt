cmake_minimum_required(VERSION 3.26)
project(CLI_Manager)

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(IMGUI_DIR $ENV{LIB_HOME}/imgui-docking)
set(GLFW_DIR $ENV{LIB_HOME}/glfw)

set(IMGUI_BACKENDS "glfw_opengl")
#set(IMGUI_BACKENDS "win32_dx11")

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static")

#set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

add_definitions(-DUNICODE -D_UNICODE)


# add header path
include_directories(
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
        ${IMGUI_DIR}/misc/cpp
        app/inc
)

# set common source
file(GLOB SRC
        ${IMGUI_DIR}/*.cpp
        ${IMGUI_DIR}/misc/cpp/*.cpp
        app/src/*.*
        main.cpp
)


if(IMGUI_BACKENDS STREQUAL "glfw_opengl")
    add_definitions(-DIMGUI_IMPL_OPENGL_LOADER_GL3W)

    include_directories(
            ${GLFW_DIR}/include
            ${IMGUI_DIR}/examples/libs/gl3w # for GL/gl3w.h
    )

    file(GLOB PLATFORM_SRC
            ${IMGUI_DIR}/examples/libs/gl3w/GL/gl3w.*
            ${IMGUI_DIR}/backends/imgui_impl_glfw.*
            ${IMGUI_DIR}/backends/imgui_impl_opengl3.*
    )

    link_directories(
            ${GLFW_DIR}/build/src
    )
endif()

# 链接库和选项
if(IMGUI_BACKENDS STREQUAL "win32_dx11")
    add_definitions(-DUSE_WIN32_BACKEND)
    file(GLOB PLATFORM_SRC
            ${IMGUI_DIR}/backends/imgui_impl_win32.*
            ${IMGUI_DIR}/backends/imgui_impl_dx11.*
    )
endif()

# generate binary
add_executable(${PROJECT_NAME} WIN32 ${SRC} ${PLATFORM_SRC})

if(IMGUI_BACKENDS STREQUAL "glfw_opengl")
    target_link_libraries(${PROJECT_NAME}
            glfw3.a
            opengl32
    )
endif()
# 链接库和选项
if(IMGUI_BACKENDS STREQUAL "win32_dx11")
    target_link_libraries(${PROJECT_NAME}
            d3d11
            dxgi
            user32
            gdi32
            ole32
            dwmapi
            d3dcompiler
    )
endif()

# 设置链接选项
set_target_properties(${PROJECT_NAME} PROPERTIES
        LINK_FLAGS "-static -static-libgcc -static-libstdc++ -Wl,-Bstatic -lpthread -Wl,-subsystem,windows"
)
