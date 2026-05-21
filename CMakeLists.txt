cmake_minimum_required(VERSION 3.10)
project(CodroidSDK LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===============================
# 通用编译选项
# ===============================
if(MSVC)
    add_compile_options(
        /w          # 关闭警告（或按需打开）
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

# ASIO 独立模式
add_definitions(-DASIO_STANDALONE)

# ===============================
# 头文件
# ===============================
include_directories(
    include
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

# ===============================
# 平台相关链接
# ===============================
if(WIN32)
    # ✅ Windows 核心依赖
    target_link_libraries(Codroid PRIVATE ws2_32 mswsock)

    if(MINGW)
        target_link_options(Codroid PRIVATE
            -static-libgcc
            -static-libstdc++
        )
    endif()

    # ✅ 给 examples 用的公共库
    set(CODROID_EXAMPLE_LIBS ws2_32)

else()
    # ===============================
    # Linux
    # ===============================
    target_link_libraries(Codroid
        PUBLIC pthread
        PUBLIC Fk_Ik_so
        PUBLIC kdl
    )

    target_include_directories(Codroid PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/kinematics
    )

    target_link_directories(Codroid PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/kinematics
    )

    set_target_properties(Codroid PROPERTIES
        INSTALL_RPATH "$ORIGIN;${CMAKE_CURRENT_SOURCE_DIR}/kinematics"
        BUILD_WITH_INSTALL_RPATH TRUE
    )

    # 链接 Codroid 的示例会 PUBLIC 继承 libkdl.so，RUNPATH 须含 kinematics/
    set(CODROID_RUNTIME_RPATH "$ORIGIN;${CMAKE_CURRENT_SOURCE_DIR}/kinematics")
endif()

# ===============================
# 示例程序
# ===============================
if(NOT CMAKE_CROSSCOMPILING)
    # ---- examples ----
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

    # ---- client examples ----
    file(GLOB CLIENT_EXAMPLE_SOURCES "examples_client/*.cpp")
    foreach(test_file ${CLIENT_EXAMPLE_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        set(target_name "client_${test_name}")

        add_executable(${target_name} ${test_file})
        target_link_libraries(${target_name} PRIVATE
            Codroid
            ${CODROID_EXAMPLE_LIBS}
        )

        if(NOT WIN32)
            set_target_properties(${target_name} PROPERTIES
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "${CODROID_RUNTIME_RPATH}"
            )
        else()
            set_target_properties(${target_name} PROPERTIES
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "$ORIGIN"
            )
        endif()

        message(STATUS "Adding Client Example: ${target_name}")
    endforeach()
endif()

# ===============================
# Linux 运动学测试程序
# ===============================
if(NOT WIN32)
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