cmake_minimum_required(VERSION 3.10)
project(CodroidSDK LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


if(WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601) # 指定支持 Windows 7 及以上系统
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. 核心宏：所有平台都需要
add_definitions(-DASIO_STANDALONE)

# 2. 针对 Windows MSVC 的特殊处理
if(MSVC)
    add_compile_options("/w")
    # 或者如果你只想关闭特定的、最烦人的几个警告，用下面的：
    # /wd4251 : 关于 STL 类（如 std::vector）需要 dll-interface 的警告（最常见）
    # /wd4244 : 大数转小数可能丢失精度的警告
    # /wd4267 : size_t 转 int 可能丢失精度的警告
    add_compile_options("/wd4251" "/wd4244" "/wd4267")

    # 定义宏以禁用安全函数警告（如 strcpy, sprintf 等）
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_SCL_SECURE_NO_WARNINGS)
    
    add_definitions(-D_WIN32_WINNT=0x0601)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

# 3. 包含路径
include_directories(include)
include_directories(SYSTEM third_party)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 4. 定义源文件
set(LIB_SOURCES
    src/CodroidController.cpp
    src/client.cpp
    src/cri_realtime_dispatcher.cpp
    src/trajectory_generator.cpp
)

# 5. 构建动态库
add_library(Codroid SHARED ${LIB_SOURCES})
target_compile_definitions(Codroid PRIVATE CODROID_EXPORTS)

# 6. 针对不同操作系统的链接处理
if(WIN32)
    # Windows 环境 (目前你还没拿到 Windows 下的 .dll 运动学库，所以这里不用动)
    target_link_libraries(Codroid PRIVATE ws2_32 mswsock)
    if(MINGW)
        target_link_options(Codroid PRIVATE "-static-libgcc" "-static-libstdc++")
    endif()
    set_target_properties(Codroid PROPERTIES OUTPUT_NAME "Codroid")
else()
    # ==========================================
    # Linux 环境：集成第三方运动学动态库
    # ==========================================
    set_target_properties(Codroid PROPERTIES OUTPUT_NAME "Codroid")
    
    # 1. 链接基础的系统库
    target_link_libraries(Codroid PRIVATE pthread)
    
    # 2. 暴露运动学的头文件路径给 Codroid 内部源码使用
    # 注意这里用 PRIVATE，代表只有 Codroid 内部源码(比如 .cpp) 知道这些头文件，不会污染给外部用户
    # target_include_directories(Codroid PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/kinematics)
    target_include_directories(Codroid PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/kinematics)
    target_include_directories(Codroid PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

    # 3. 告诉链接器去哪里找这几个第三方的 .so
    target_link_directories(Codroid PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/kinematics)

    # 4. 将第三方的 .so 链接到你的 libCodroid.so 中
    # 注意顺序：Fk_Ik_so 必须写在 kdl 前面，因为它依赖 kdl
    target_link_libraries(Codroid PUBLIC Fk_Ik_so kdl)

    # 5. 设置 RPATH (Run-time search path)
    # 因为 libCodroid.so 依赖了第三方的 libFk_Ik_so.so，
    # 当别人运行链接了 SDK 的程序时，系统必须知道去哪里加载第三方的 .so。
    # 这里使用 $ORIGIN，代表“去(libCodroid.so)当前所在的目录找”。
    # 这样以后交付 SDK 时，只要把 libCodroid.so 和第三方 .so 放在同一个文件夹下就能完美运行！
    # 开发树中运动学 .so 仍在 kinematics/，故同时加入该路径，build_linux 下直接 ./example 可运行。
    set_target_properties(Codroid PROPERTIES 
        INSTALL_RPATH "$ORIGIN;${CMAKE_CURRENT_SOURCE_DIR}/kinematics"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

# 7. 示例程序 (仅在本地开发/测试时构建)
if(NOT CMAKE_CROSSCOMPILING)
    file(GLOB EXAMPLE_SOURCES "examples/*.cpp")
    foreach(test_file ${EXAMPLE_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        add_executable(${test_name} ${test_file})
        target_link_libraries(${test_name} PRIVATE Codroid)
        
        # Linux：$ORIGIN 找同目录的 libCodroid.so；kinematics 供依赖解析（与库内 RUNPATH 一致）
        if(WIN32)
            set_target_properties(${test_name} PROPERTIES 
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/kinematics"
            )
        else()
            set_target_properties(${test_name} PROPERTIES 
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "$ORIGIN;${CMAKE_CURRENT_SOURCE_DIR}/kinematics"
            )
        endif()

        message(STATUS "Adding Example: ${test_name}")
    endforeach()

    file(GLOB CLIENT_EXAMPLE_SOURCES "examples_client/*.cpp")
    foreach(test_file ${CLIENT_EXAMPLE_SOURCES})
        get_filename_component(test_name ${test_file} NAME_WE)
        set(target_name "client_${test_name}")
        add_executable(${target_name} ${test_file})
        target_link_libraries(${target_name} PRIVATE Codroid)

        if(WIN32)
            set_target_properties(${target_name} PROPERTIES
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/kinematics"
            )
        else()
            set_target_properties(${target_name} PROPERTIES
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "$ORIGIN;${CMAKE_CURRENT_SOURCE_DIR}/kinematics"
            )
        endif()

        message(STATUS "Adding Client Example: ${target_name}")
    endforeach()
endif()

# =========================================================
# 8. 运动学模块 (Kinematics) 独立测试程序
# =========================================================
# 注意：.so 文件是 Linux 专属的，所以只在 Linux 环境下编译此模块
if(NOT WIN32)
    # 把 kinematics/main.cpp 编译为一个叫 ik_test 的可执行程序
    add_executable(kinematics kinematics/kinematics.cpp)

    # 包含头文件目录 (让 main.cpp 能找到 robotModel.h)
    target_include_directories(kinematics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/kinematics)

    # 包含动态库目录 (让链接器能找到那两个 .so)
    target_link_directories(kinematics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/kinematics)

    # 链接动态库 (注意顺序：Fk_Ik_so 依赖 kdl，所以 Fk_Ik_so 写在前面)
    target_link_libraries(kinematics PRIVATE Fk_Ik_so kdl)

    # 【核心神技】设置 RPATH，把 .so 的绝对路径写死进可执行文件里
    # 这样你运行 ./ik_test 的时候，系统就不会报 "cannot open shared object file" 错误了
    set_target_properties(kinematics PROPERTIES 
        INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/kinematics"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
    
    message(STATUS "Adding Kinematics Test: kinematics")
endif()