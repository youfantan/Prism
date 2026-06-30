cmake_minimum_required(VERSION 4.2)
project(Prism)

set(CMAKE_CXX_STANDARD 20)

# add include directories
include_directories(include)
include_directories(thirdparty/include)

# set UNICODE for Win32 API
add_compile_definitions(-DUNICODE _CRT_SECURE_NO_WARNINGS)

# copy ./shaders and ./assets to the build directory
set(COPY_DIRS shaders assets textures)
add_custom_target(copy_dirs ALL)
foreach(dir ${COPY_DIRS})
    add_custom_command(POST_BUILD TARGET copy_dirs
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/${dir}
            $<TARGET_FILE_DIR:pipline-demo>/${dir}
    )
endforeach ()

# copy *.dll in ./thirdparty/link/release to the build directory when configured as Release;
# or copy *.dll and *.pdb in ./thirdparty/link/debug to the build directory when configured as Debug.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(THIRDPARTY_LINK_DIR "${CMAKE_SOURCE_DIR}/thirdparty/link/debug")
    set(THIRDPARTY_COPY_PATTERN "*.dll")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(THIRDPARTY_LINK_DIR "${CMAKE_SOURCE_DIR}/thirdparty/link/release")
    set(THIRDPARTY_COPY_PATTERN "*.dll;*.pdb")
endif()
file(GLOB THIRDPARTY_COPY_FILES
        LIST_DIRECTORIES false
        ${THIRDPARTY_LINK_DIR}/${THIRDPARTY_COPY_PATTERN}
)
add_custom_target(copy_binaries ALL)
foreach(file ${THIRDPARTY_COPY_FILES})
    add_custom_command(POST_BUILD TARGET copy_binaries
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${file}
            $<TARGET_FILE_DIR:pipline-demo>
    )
endforeach()

# add link directories
link_directories(${THIRDPARTY_LINK_DIR})

add_executable(pipline-demo
        include/render/framework.h
        include/base.h
        include/render/queue.h
        include/render/resource.h
        include/render/drawcall.h
        include/render/sync.h
        include/utils.h
        include/mlog.h
        include/io/shader.h
        include/transform/camera.h
        src/transform/camera.cpp
        include/transform/helper.h
        include/piplines/ui_pipeline.h
        include/io/font.h
        include/io/image.h
        src/utils.cpp
        include/render/mesh.h
        include/piplines/realistic_style_pipeline.h
        src/render/framework.cpp
        demo/pipeline_demo.cpp
        include/render/render_context.h
        include/render/render_target.h
)
target_link_libraries(pipline-demo d3d12 dxgi d3dcompiler dxcompiler dxguid DirectXTex bcrypt turbojpeg freetype)
add_dependencies(pipline-demo copy_dirs copy_binaries)