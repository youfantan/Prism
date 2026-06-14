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
            $<TARGET_FILE_DIR:PrismRenderer>/${dir}
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
            "${file}"
            "$<TARGET_FILE_DIR:PrismRenderer>/"
    )
endforeach()

# add link directories
link_directories(${THIRDPARTY_LINK_DIR})

add_executable(PrismRenderer
        src/main.cpp
        include/io/texture.h
        include/render/framework.h
        src/render/framework.cpp
        include/base.h
        include/render/queue.h
        src/render/queue.cpp
        include/render/resource.h
        include/render/drawcall.h
        include/render/sync.h
        include/utils.h
        src/io/texture.cpp
        include/mlog.h
        include/io/shader.h
        include/transform/camera.h
        src/transform/camera.cpp
        src/render/sync.cpp
        src/render/resource.cpp
        src/render/drawcall.cpp
        include/transform/helper.h
)
target_link_libraries(PrismRenderer d3d12 dxgi d3dcompiler turbojpeg)
add_dependencies(PrismRenderer copy_dirs copy_binaries)
set_target_properties(PrismRenderer PROPERTIES
        WIN32_EXECUTABLE TRUE
)