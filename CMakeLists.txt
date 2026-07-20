cmake_minimum_required(VERSION 3.10)
project(CodroidSDK LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CODROID_BUILD_KINEMATICS_TOOLS "Build standalone kinematics/ tools (not part of SDK lib)" OFF)

# ===============================
# 通用编译选项
# ===============================
if(MSVC)
    add_compile_options(
        /w
        /source-charset:utf-8
        /execution-charset:utf-8
    )
    add_definitions(
        -D_CRT_SECURE_NO_WARNINGS
        -D_SCL_SECURE_NO_WARNINGS
        -D_WIN32_WINNT=0x0601
    )
elseif(MINGW OR CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
    )
endif()

# ASIO 独立模式（仅 SDK 实现编译单元需要）
add_definitions(-DASIO_STANDALONE)

# ===============================
# 头文件
# ===============================
include_directories(
    include
)
# SYSTEM 抑制 third_party 的编译器警告（如 asio 空指针类型推导）
include_directories(SYSTEM
    third_party
)

# ===============================
# 核心库源文件
# ===============================
set(LIB_SOURCES
    src/CodroidController.cpp
    src/client.cpp
    src/cri_realtime_dispatcher.cpp
    src/trajectory_generator.cpp
)

add_library(Codroid SHARED ${LIB_SOURCES})
target_compile_definitions(Codroid PRIVATE CODROID_EXPORTS)
target_include_directories(Codroid
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
)

if (WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# ===============================
# 平台相关链接（客户侧无 KDL / Fk_Ik）
# ===============================
if(WIN32)
    target_link_libraries(Codroid PRIVATE ws2_32 mswsock)

    if(MINGW)
        target_link_options(Codroid PRIVATE
            -static-libgcc
            -static-libstdc++
        )
    endif()

    set(CODROID_EXAMPLE_LIBS ws2_32)
else()
    target_link_libraries(Codroid PRIVATE pthread)

    set_target_properties(Codroid PROPERTIES
        INSTALL_RPATH "$ORIGIN"
        BUILD_WITH_INSTALL_RPATH TRUE
    )

    set(CODROID_RUNTIME_RPATH "$ORIGIN")
endif()

# ===============================
# 示例程序（CodroidClient，仅 #include "Codroid/client.hpp"）
# ===============================
if(NOT CMAKE_CROSSCOMPILING)
    file(GLOB EXAMPLE_SOURCES "examples/*.cpp")
    foreach(test_file ${EXAMPLE_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        add_executable(${test_name} ${test_file})
        target_link_libraries(${test_name} PRIVATE
            Codroid
            ${CODROID_EXAMPLE_LIBS}
        )

        if(NOT WIN32)
            set_target_properties(${test_name} PROPERTIES
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "${CODROID_RUNTIME_RPATH}"
            )
        else()
            set_target_properties(${test_name} PROPERTIES
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "$ORIGIN"
            )
        endif()

        message(STATUS "Adding Example: ${test_name}")
    endforeach()
endif()

# ===============================
# 可选：独立运动学工具（不链入 libCodroid）
# ===============================
if(NOT WIN32 AND CODROID_BUILD_KINEMATICS_TOOLS)
    add_executable(kinematics_test kinematics/kinematics.cpp)

    target_include_directories(kinematics_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/kinematics
    )

    target_link_directories(kinematics_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/kinematics
    )

    target_link_libraries(kinematics_test PRIVATE
        Fk_Ik_so
        kdl
    )

    set_target_properties(kinematics_test PROPERTIES
        INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/kinematics"
        BUILD_WITH_INSTALL_RPATH TRUE
    )

    message(STATUS "Adding Kinematics Test: kinematics_test")
endif()
