/**
 * @file CodroidController.h
 * @brief 控制接口头文件
 */

#ifndef CODROID_CONTROLLER_H
#define CODROID_CONTROLLER_H

#include "CodroidDefine.h"
#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <asio.hpp>
#include <memory>
#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include "../../kinematics/robotModel.h"

namespace Codroid {

/**
 * @section codroid_threading 线程安全（仅用本 SDK 时的约定）
 *
 * - **可多线程并发**调用 `sendCommand` 及所有最终走 TCP 的高层 API，前提是：同一时刻任意两条在途请求使用 **不同的** `id`。
 *   请使用 `NextRequestId()` 生成单调递增 id（与 C# `NextId` 语义一致）。若重复注册同一 `id`，将抛出 `CodroidException`。
 * - **`connect` / `connectTcpOnly` / `disconnect`** 与 **收包线程生命周期** 由内部 `lifecycle_mtx_` 串行化；不要在多线程上同时调用 `disconnect` 与其它指令而不做外部同步（单客户端实例通常单线程断开即可）。
 * - **`getRobotRealtimeState`** 与 CRI 缓存使用独立互斥量，可与指令线程并发读取。
 * - **推送回调**在专用分发线程上执行；回调内可再调 `sendCommand`，但请避免在回调内长时间阻塞或自旋，否则会积压推送。SDK **不保证** 用户在回调或业务层自造的死锁/竞态。
 * - **“绝对”安全**：无法保证（用户可在回调里与其它锁乱序）。在遵守上述约定且仅通过本 SDK 公开 API 访问控制器时，SDK 内部对 TCP 写、pending 匹配、连接/收包线程启停做了互斥保护。
 */

// 使用 C++17 的 inline constexpr，避免重复定义错误，且效率最高
#ifndef M_PI
    inline constexpr double M_PI = 3.14159265358979323846;
#endif
    inline constexpr double DEG_TO_RAD = M_PI / 180.0;
    inline constexpr double RAD_TO_DEG = 180.0 / M_PI;
    inline constexpr double MM_TO_M    = 0.001;
    inline constexpr double M_TO_MM    = 1000.0;

class CODROID_API CodroidController {
public:
    /** @brief @~english Construct controller (no network I/O). @~chinese 构造函数，不发起网络连接。 */
    CodroidController();
    /** @brief @~english Destructor @~chinese 析构函数 */
    ~CodroidController();

    // 禁止拷贝，防止 Socket 管理混乱
    CodroidController(const CodroidController&) = delete;
    CodroidController& operator=(const CodroidController&) = delete;

    /**
     * @brief @~english Connect to the robot @~chinese 连接机器人
     * 
     * @~english 
     * @param ip Robot IP address.
     * @param port TCP port.(default 9001)
     * @~chinese
     * @param ip 机器人IP地址。
     * @param port TCP 端口号。(默认 9001)
     * @param local_ip @~english Host IP for CRI/StartDataPush UDP (empty disables auto push). @~chinese 本机接收 CRI 实时推送的 IP；空字符串则不开启 UDP 推送。
     * 
     * @return @~english true if success. 
     *         @~chinese 连接成功返回 true。
     */
    bool connect(const std::string& ip, int port = 9001, std::string local_ip = {});
    /**
     * @brief @~english TCP connect only (no mode switch, no CRI). Matches C# Connect. @~chinese 仅建立 TCP，不切模式、不启 CRI；与 C# Connect 对齐。
     */
    bool connectTcp(const std::string& ip, int port = 9001);
    /**
     * @brief 结束会话：停 CRI UDP、**join** TCP 收包与主题分发线程、关闭并释放指令套接字（可再次 `connect`）。
     */
    void disconnect();

    /** @brief @~english Send a command to the robot @~chinese 向机器人发送指令
     * @~english
     * @param type Command type, e.g. "runScript", "switchOn", etc.
     * @param data Command data in JSON format, specific to each command type.
     * @param id Request ID for matching responses, default is 1.
     * @~chinese
     * @param type 指令类型，例如 "runScript"、"switchOn" 等。
     * @param data 指令数据，JSON 格式，根据不同指令类型具体定义。
     * @param id 请求 ID，用于匹配响应，默认为 1。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response sendCommand(const std::string& type, const json& data, int id);

    /**
     * @brief 为并发 `sendCommand` 生成唯一请求 id（原子递增，与 C# NextId 对齐）。
     * @note 多线程发指令时请使用本方法，勿手写重复 id。
     */
    int NextRequestId();

    /** @brief @~english Print the response of a command @~chinese 打印指令的响应
     * @~english
     * @param action Action name.
     * @param resp Response object.
     * @~chinese
     * @param action 操作名称。
     * @param resp 响应对象。
     */
    static void printResponse(const Response& resp);

    /**
     * @brief @~english Subscribe to a publish topic (protocol 15.x). First subscription for @p topicTy sends `{ty,tc}` without id. @~chinese 订阅推送主题；该 topic 首次订阅时下发 `{ty,tc}`，无 id。
     */
    PublishTopicSubscription subscribePublishTopic(std::string topicTy,
                                                   PublishTopicHandler handler,
                                                   int tc_milliseconds = 100);

    // --- 2.1 运行脚本
        /** @brief @~english run script on the robot @~chinese 在机器人上运行脚本
     * @~english
     * @param params script oarams
     * @param id Request ID for matching responses, default is 1.
     * @~chinese 
     * @param params 脚本参数配置结构体。
     * @param id 请求ID，默认为1
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response runScript(const RunScriptParams& params, int id = 1);

    // --- 2.2 进入远程脚本模式 ---
    /** @brief @~english Enter remote script mode on the robot @~chinese 在机器人上进入远程脚本模式
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response enterRemoteScriptMode(int id = 1);

    // --- 2.3 运行工程(工程ID) ---
    /** @brief @~english Run a project by its projectID on the robot @~chinese 在机器人上通过工程ID运行工程
     * @~english
     * @param projectid Project ID.
     * @param id Request ID.
     * @~chinese
     * @param projectid 工程 ID。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response runProject(const std::string& projectid, int id = 1);

    // --- 2.4 运行工程(工程索引) ---
    /** @brief @~english Run a project by its index on the robot @~chinese 在机器人上通过索引运行工程
     * @~english
     * @param index Project index.
     * @param id Request ID.
     * @~chinese
     * @param index 工程索引。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response runProjectByIndex(int index, int id = 1);

    // --- 2.5 单步运行 ---
    /** @brief @~english Run a project step by step on the robot @~chinese 在机器人上单步运行工程
     * @~english
     * @param projectid Project ID.
     * @param id Request ID.
     * @~chinese
     * @param projectid 工程 ID。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response runStep(const std::string& projectid, int id = 1);

    // --- 2.6 暂停工程 ---
    /** @brief @~english Pause the currently running project on the robot @~chinese 暂停机器人上正在运行的工程
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response pauseProject(int id = 1);

    // --- 2.7 恢复工程 ---
    /** @brief @~english Resume the currently paused project on the robot @~chinese 恢复机器人上暂停的工程
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response resumeProject(int id = 1);

    // --- 2.8 停止工程 ---
    /** @brief @~english Stop the currently running project on the robot @~chinese 停止机器人上正在运行的工程
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response stopProject(int id = 1);

    // --- 2.13 设置启动行 ---
    /** @brief @~english Set the start line for project execution on the robot @~chinese 设置机器人上工程执行的启动行
     * @~english
     * @param startline Line number to start execution from.
     * @param id Request ID.
     * @~chinese
     * @param startline 要开始执行的行号。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response setStartLine(int startline, int id = 1);
    
    // --- 2.14 清除启动行 ---
    /** @brief @~english Clear the start line setting for project execution on the robot @~chinese 清除机器人上工程执行的启动行设置
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response clearStartLine(int id = 1);

    // --- 3.3 获取全局变量列表 ---
    /** @brief @~english Get the list of global variables from the robot @~chinese 获取机器人上的全局变量列表
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the list of global variables.
     *         @~chinese  包含全局变量列表的响应对象。
     */
    Response getGlobalVars(int id = 1);

    // --- 3.4 保存全局变量列表 ---
    /** @brief @~english Save the list of global variables to the robot @~chinese 将全局变量列表保存到机器人
     * @~english
     * @param vars Map of global variables to save.
     * @param id Request ID.
     * @~chinese
     * @param vars 要保存的全局变量映射。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response saveGlobalVars(const std::map<std::string, Variable>& vars, int id = 1);

    // --- 3.5 删除全局变量 ---
    /** @brief @~english Remove global variables from the robot @~chinese 从机器人上删除全局变量
     * @~english
     * @param varNames Vector of variable names to remove.
     * @param id Request ID.
     * @~chinese
     * @param varNames 要删除的变量名称向量。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response removeGlobalVars(const std::vector<std::string>& varNames, int id = 1);

    // --- 4.1 工程变量接口 ---
    /** @brief @~english Get the value of a project variable from the robot @~chinese 从机器人上获取工程变量的值
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the value of the project variable.
     *         @~chinese  包含工程变量值的响应对象。
     */
    Response getProjectVar(int id = 1);

    // --- 5.1 初始化RS485 ---
    /** @brief @~english Initialize the RS485 communication on the robot @~chinese 初始化机器人上的RS485通信
     * @~english
     * @param baudrate Baud rate for RS485 communication.
     * @param stopBit Stop bits for RS485 communication.
     * @param dataBit Data bits for RS485 communication.
     * @param parity Parity for RS485 communication.
     * @param id Request ID.
     * @~chinese
     * @param baudrate RS485通信的波特率。
     * @param stopBit RS485通信的停止位。
     * @param dataBit RS485通信的数据位。
     * @param parity RS485通信的校验位。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response RS485init(int baudrate, RS485StopBits stopBit = RS485StopBits::One, int dataBit = 8, RS485Parity parity = RS485Parity::None, int id = 1);

    // --- 5.2 RS485 flush ---
    /** @brief @~english Flush the RS485 communication buffer on the robot @~chinese 清空机器人上的RS485通信缓冲区
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response RS485flush(int id = 1);

    // --- 5.3 RS485 read ---
    /** @brief @~english Read data from the RS485 communication on the robot @~chinese 从机器人上的RS485通信读取数据
     * @~english
     * @param length Number of bytes to read.
     * @param timeout Timeout for the read operation.
     * @param id Request ID.
     * @~chinese
     * @param length 要读取的字节数。
     * @param timeout 读取操作的超时时间。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response RS485read(int length,int timeout = 5000, int id = 1);

    // --- 5.4 RS485 write ---
    /** @brief @~english Write data to the RS485 communication on the robot @~chinese 向机器人上的RS485通信写入数据
     * @~english
     * @param data Data to write.
     * @param id Request ID.
     * @~chinese
     * @param data 要写入的数据。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response RS485write(const std::vector<uint8_t>& data, int id = 1);

    // --- 10.1 正解接口 ---
    /** @brief @~english Calculate the forward kinematics of the robot @~chinese 计算机器人的正运动学
     * @~english
     * @param params Forward kinematics parameters.
     * @param id Request ID.
     * @~chinese
     * @param params 正运动学参数。
     * @param id 请求 ID。
     * @return @~english  Vector of joint angles.
     *         @~chinese  关节角度向量。
     */
    std::vector<double> forwardKinematics(const FKParams& params, int id = 1);

    // --- 10.2 逆解接口 ---
    /** @brief @~english Calculate the inverse kinematics of the robot @~chinese 计算机器人的逆运动学
     * @~english
     * @param params Inverse kinematics parameters.
     * @param id Request ID.
     * @~chinese
     * @param params 逆运动学参数。
     * @param id 请求 ID。
     * @return @~english  Vector of joint angles.
     *         @~chinese  关节角度向量。
     */
    std::vector<double> inverseKinematics(const IKParams& params, int id = 1);
    
    // --- 10.3 笛卡尔坐标偏移计算接口 ---
    /** @brief @~english Calculate the relative pose of the robot @~chinese 计算机器人的相对姿态
     * @~english
     * @param params Relative pose parameters.
     * @param id Request ID.
     * @~chinese
     * @param params 相对姿态参数。
     * @param id 请求 ID。
     * @return @~english  Vector of Calculate position.
     *         @~chinese  笛卡尔坐标偏移向量。
     */
    std::vector<double> calculateRelativePose(const RelativePoseParams& params, int id = 1);

    // --- 11.1 点动 ---
    /** @brief @~english Perform a jog operation on the robot @~chinese 对机器人执行点动操作
     * @~english
     * @param params Jog parameters.
     * @param id Request ID.
     * @~chinese
     * @param params 点动参数。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response jog(const JogParams& params, int id = 1);

    // --- 11.2 停止点动 ---
    /** @brief @~english Stop the jog operation on the robot @~chinese 停止机器人上的点动操作
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response stopJog(int id = 1);

    // --- 11.3 点动心跳 ---
    /** @brief @~english Send a heartbeat signal for the jog operation, at least once every 500ms @~chinese 发送点动操作的心跳信号,至少每500ms发送一次
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response jogHeartbeat(int id = 1);

    // --- 11.4 moveTo ---
    /** @brief @~english Move the robot to a specific position @~chinese 将机器人移动到特定位置
     * @~english
     * @param params Move to parameters.
     * @param id Request ID.
     * @~chinese
     * @param params 移动到参数。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response moveTo(const MoveToParams& params, int id = 1);

    // --- 11.5 moveTo心跳 ---
    /** @brief @~english Send a heartbeat signal for the moveTo operation, at least once every 500ms  @~chinese 发送 moveTo 操作的心跳信号,至少每500ms发送一次
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response moveToHeartbeat(int id = 1);

    // --- 11.6 设置手动运动倍率 ---
    /** @brief @~english Set the manual speed rate for the robot's movement @~chinese 设置机器人的手动运动倍率
     * @~english
     * @param speed Speed rate as a percentage (0-100).
     * @param id Request ID.
     * @~chinese
     * @param speed 速度倍率，百分比形式（0-100）。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response setManualSpeedRate(int speed, int id = 1);

    // --- 11.7 设置自动运动倍率 ---
    /** @brief @~english Set the automatic speed rate for the robot's movement @~chinese 设置机器人的自动运动倍率
     * @~english
     * @param speed Speed rate as a percentage (0-100).
     * @param id Request ID.
     * @~chinese
     * @param speed 速度倍率，百分比形式（0-100）。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response setAutoSpeedRate(int speed, int id = 1);

    // --- 11.8 运动指令 ---
    // --- 核心运动接口 ---
    /** @brief @~english Move the robot along a specified path @~chinese 按指定路径移动机器人
     * @~english
     * @param path Path to follow.
     * @param id Request ID.
     * @~chinese
     * @param path 路径。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response move(const std::vector<MoveInstruction>& path, int id = 1);
    
    // --- movJ 重载 ---
    /** @brief @~english Move the robot to a specific joint position @~chinese 将机器人移动到特定关节位置
     * @~english
     * @param inst Move instruction.
     * @param id Request ID.
     * @~chinese
     * @param inst 移动指令。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response movJ(const MoveInstruction& inst, int id = 1);
    /** @brief @~english Move the robot to a specific joint position @~chinese 将机器人移动到特定关节位置
     * @~english
     * @param jp Joint positions.
     * @param speed Speed rate .
     * @param acc Acceleration rate .
     * @param id Request ID.
     * @~chinese
     * @param jp 关节位置。
     * @param speed 速度
     * @param acc 加速度
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response movJ(const std::vector<double>& jp, double speed, double acc, int id = 1);

    // --- movL 重载 ---
    /** @brief @~english Move the robot along a linear path @~chinese 按直线路径移动机器人
     * @~english
     * @param inst Move instruction.
     * @param id Request ID.
     * @~chinese
     * @param inst 移动指令。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response movL(const MoveInstruction& inst, int id = 1);
    /** @brief @~english Move the robot along a linear path @~chinese 按直线路径移动机器人
     * @~english
     * @param cp Cartesian position.
     * @param speed Speed rate.
     * @param acc Acceleration rate.
     * @param coor Optional coordinate system for the target point, default is world coordinate system.
     * @param tool Optional tool coordinate system for the target point.
     * @param id Request ID.
     * @~chinese
     * @param cp 笛卡尔位置。
     * @param speed 速度
     * @param acc 加速度
     * @param coor 目标点的可选坐标系，默认是世界坐标系。
     * @param tool 目标点的可选工具坐标系。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response movL(const std::vector<double>& cp, double speed, double acc, 
                  const std::vector<double>& coor = {}, const std::vector<double>& tool = {}, int id = 1);
    
    // --- movC 重载 ---
    /** @brief @~english Move the robot along a circular path @~chinese 按圆弧路径移动机器人
     * @~english
     * @param inst Move instruction.
     * @param id Request ID.
     * @~chinese
     * @param inst 移动指令。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response movC(const MoveInstruction& inst, int id = 1);

    // --- movCircle (整圆运动) ---
    /** @brief @~english Move the robot along a circular path that forms a complete circle @~chinese 按圆弧路径移动机器人，形成一个完整的圆
     * @~english
     * @param inst Move instruction.
     * @param id Request ID.
     * @~chinese
     * @param inst 移动指令。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response movCircle(const MoveInstruction& inst, int id = 1);

    // --- 11.9 暂停运动 ---
    /** @brief @~english Pause the current movement of the robot @~chinese 暂停机器人当前的运动
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response pauseMove(int id = 1);

    // --- 11.10 恢复运动 ---
    /** @brief @~english Resume the paused movement of the robot @~chinese 恢复机器人暂停的运动
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response resumeMove(int id = 1);

    // --- 11.11 停止运动 ---
    /** @brief @~english Stop the current movement of the robot immediately @~chinese 立即停止机器人当前的运动
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response stopMove(int id = 1);

    // --- 12.1 上使能 ---
    /** @brief @~english Switch on the robot @~chinese 上使能机器人
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response switchOn(int id = 1);

    // --- 12.2 下使能 ---
    /** @brief @~english Switch off the robot @~chinese 下使能机器人
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response switchOff(int id = 1);

    // --- 12.3 进入手动模式
    /** @brief @~english Enter manual mode on the robot @~chinese 进入机器人的手动模式
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response toManual(int id = 1);
    /** @brief @~english Robot/toManual only (no prior ToAuto). C# ToManual. @~chinese 仅下发 Robot/toManual，不先切自动。 */
    Response toManualDirect(int id = 1);

    // --- 12.4 进入自动模式
    /** @brief @~english Enter automatic mode on the robot @~chinese 进入机器人的自动模式
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response toAuto(int id = 1);

    // --- 12.5 进入远程模式
    /** @brief @~english Enter remote mode on the robot @~chinese 进入机器人的远程模式
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response toRemote(int id = 1);
    /** @brief @~english Robot/toRemote only (no prior ToAuto). C# ToRemote. @~chinese 仅下发 Robot/toRemote，不先切自动。 */
    Response toRemoteDirect(int id = 1);

    // --- 12.7 进入仿真模式 ---
    /** @brief @~english Enter simulation mode on the robot @~chinese 进入机器人的仿真模式
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response toSimulation(int id = 1);

    // --- 12.8 进入实机模式 ---
    /** @brief @~english Enter actual mode on the robot @~chinese 进入机器人的实机模式
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response toActual(int id = 1);

    // --- 12.9 进入拖拽模式 ---
    /** @brief @~english Enter drag mode on the robot @~chinese 进入机器人的拖拽模式
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response startDrag(int id = 1);

    // --- 12.10 退出拖拽模式 ---
    /** @brief @~english Exit drag mode on the robot @~chinese 退出机器人的拖拽模式
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response stopDrag(int id = 1);

    // --- 12.11 清除错误 ---
    /** @brief @~english Clear errors on the robot @~chinese 清除机器人上的错误
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response clearError(int id = 1);

    // --- 13.1 获取IO状态 ---
    /** @brief @~english Get the status of IO ports on the robot @~chinese 获取机器人上IO端口的状态
     * @~english
     * @param queryList List of IO ports to query.
     * @param id Request ID.
     * @~chinese
     * @param queryList 要查询的IO端口列表。
     * @param id 请求 ID。
     * @return @~english  Vector of IOInfo objects containing the status of the queried IO ports.
     *         @~chinese  包含查询IO端口状态的IOInfo对象向量。
     */
    std::vector<IOInfo> getIOValues(const std::vector<IOInfo>& queryList, int id = 1);
    // --- getDI/getDO/getAI/getAO 便捷接口 ---
    /** @brief @~english Get the value of a digital input port @~chinese 获取数字输入端口的值
     * @~english
     * @param port Port number.
     * @param id Request ID.
     * @~chinese
     * @param port 端口号。
     * @param id 请求 ID。
     * @return @~english  The value of the specified digital input port.
     *         @~chinese  指定数字输入端口的值。
     */
    int getDI(int port, int id = 1);
    /** @brief @~english Get the value of a digital output port @~chinese 获取数字输出端口的值
     * @~english
     * @param port Port number.
     * @param id Request ID.
     * @~chinese
     * @param port 端口号。
     * @param id 请求 ID。
     * @return @~english  The value of the specified digital output port.
     *         @~chinese  指定数字输出端口的值。
     */
    int getDO(int port, int id = 1);
    /** @brief @~english Get the value of an analog input port @~chinese 获取模拟输入端口的值
     * @~english
     * @param port Port number.
     * @param id Request ID.
     * @~chinese
     * @param port 端口号。
     * @param id 请求 ID。
     * @return @~english  The value of the specified analog input port.
     *         @~chinese  指定模拟输入端口的值。
     */
    double getAI(int port, int id = 1);
    /** @brief @~english Get the value of an analog output port @~chinese 获取模拟输出端口的值
     * @~english
     * @param port Port number.
     * @param id Request ID.
     * @~chinese
     * @param port 端口号。
     * @param id 请求 ID。
     * @return @~english  The value of the specified analog output port.
     *         @~chinese  指定模拟输出端口的值。
     */
    double getAO(int port, int id = 1);

    // --- 13.2 设置IO状态 ---
    /** @brief @~english Set the value of IO ports on the robot @~chinese 设置机器人上IO端口的值
     * @~english
     * @param type Type of IO ("DI", "DO", "AI", "AO").
     * @param port Port number.
     * @param value Value to set.
     * @param id Request ID.
     * @~chinese
     * @param type IO类型（"DI"、"DO"、"AI"、"AO"）。
     * @param port 端口号。
     * @param value 要设置的值。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response setIOValue(const std::string& type, int port, double value, int id = 1);
    /** @brief @~english Set the value of a digital output port @~chinese 设置数字输出端口的值
     * @~english
     * @param port Port number.
     * @param value Value to set.
     * @param id Request ID.
     * @~chinese
     * @param port 端口号。
     * @param value 要设置的值。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response setDO(int port, int value, int id = 1);
    /** @brief @~english Set the value of an analog output port @~chinese 设置模拟输出端口的值
     * @~english
     * @param port Port number.
     * @param value Value to set.
     * @param id Request ID.
     * @~chinese
     * @param port 端口号。
     * @param value 要设置的值。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response setAO(int port, double value, int id = 1);

    // --- 14.1 获取寄存器值 ---
    /** @brief @~english Get the values of registers on the robot @~chinese 获取机器人上寄存器的值
     * @~english
     * @param addresses Vector of register addresses to query.
     * @param id Request ID.
     * @~chinese
     * @param addresses 要查询的寄存器地址向量。
     * @param id 请求 ID。
     * @return @~english  Vector of RegisterInfo objects containing the values of the queried registers.
     *         @~chinese  包含查询寄存器值的RegisterInfo对象向量。
     */
    std::vector<RegisterInfo> getRegisterValues(const std::vector<int>& addresses, int id = 1);
    /** @brief @~english Get the value of a single register on the robot @~chinese 获取机器人上单个寄存器的值
     * @~english
     * @param address Register address to query.
     * @param id Request ID.
     * @~chinese
     * @param address 要查询的寄存器地址。
     * @param id 请求 ID。
     * @return @~english  The value of the specified register.
     *         @~chinese  指定寄存器的值。
     */
    double getRegisterValue(const int address, int id = 1);

    // --- 14.2 设置寄存器值 ---
    /** @brief @~english Set the value of a register on the robot @~chinese 设置机器人上寄存器的值
     * @~english
     * @param address Register address to set.
     * @param value Value to set.
     * @param id Request ID.
     * @~chinese
     * @param address 要设置的寄存器地址。
     * @param value 要设置的值。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response setRegisterValue(int address, double value, int id = 1);
    
    // --- 14.3 设置扩展数组数据类型 ---
    /** @brief @~english Set the data type of an extended array on the robot @~chinese 设置机器人上扩展数组的数据类型
     * @~english
     * @param index Index of the extended array.
     * @param type Data type to set.
     * @param id Request ID.
     * @~chinese
     * @param index 扩展数组的索引。
     * @param type 要设置的数据类型。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response setExtendArrayType(int index, ExtendArrayType type, int id = 1);
    
    // --- 14.4 删除扩展数组索引 ---
    /** @brief @~english Remove an extended array index from the robot @~chinese 从机器人上删除一个扩展数组索引
     * @~english
     * @param index Index of the extended array to remove.
     * @param id Request ID.
     * @~chinese
     * @param index 要删除的扩展数组索引。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response removeExtendArray(int index, int id = 1);

    // --- 17.2 开启数据推送流 ---
    /** @brief @~english Start a data push stream from the robot to a specified IP and port @~chinese 开启从机器人到指定IP和端口的数据推送流
     * @~english
     * @param ip IP address to push data to.
     * @param port Port number to push data to.
     * @param duration The interval time of the data push stream, in milliseconds (1000/frequency)
     * @param id Request ID.
     * @param highPercision @~english Use double samples when true (omit mask to use default 0xFFFF). @~chinese 为 true 时 highPercision: double；不传 mask 时固件默认 0xFFFF。
     * @~chinese
     * @param ip 要推送数据的IP地址。
     * @param port 要推送数据的端口号。
     * @param duration 数据推送流的间隔时间，单位为毫秒(1000/频率)
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response startDataPush(const std::string& ip, int port, int duration, int id = 1, bool highPercision = false);

    // --- 17.3 关闭数据推送流 ---
    /** @brief @~english Stop the data push stream on the robot @~chinese 关闭机器人上的数据推送流
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response stopDataPush(int id = 1);
    /** @brief @~english Stop a specific CRI push endpoint @~chinese 按 IP/端口停止一路 CRI 推送
     * @~english
     * @param ip Target IP previously used in StartDataPush.
     * @param port Target port previously used in StartDataPush.
     * @param id Request ID.
     * @~chinese
     * @param ip 与 StartDataPush 中一致的 IP。
     * @param port 与 StartDataPush 中一致的端口。
     * @param id 请求 ID。
     */
    Response stopDataPush(const std::string& ip, int port, int id = 1);

    /** @brief 线程安全快照；UDP 载荷经解析后为 **毫米/度**（与 C# **`CriData`**、`AGENTS.md` §2.3.4 一致）。 */
    RobotRealtimeState getRobotRealtimeState() const;

    /**
     * @brief 等价于 C# **`CriDataReceived`**：每帧 308 字节解析成功后回调；在工作线程触发，请勿阻塞。
     * @note 传入独立快照副本，可与 **`getRobotRealtimeState`** 并行使用。
     */
    using CriDataReceivedHandler = std::function<void(const RobotRealtimeState&)>;
    void setCriDataReceivedHandler(CriDataReceivedHandler handler);

    /** 若为 true：**`sendCommand`** 在 **`error_msg` 非空**（含超时/网络）时 **`throw CodroidCommandException`**；默认 **false** 保持仅用 Response。 */
    void setThrowOnCommandError(bool enable);
    /** @copydoc setThrowOnCommandError */
    bool getThrowOnCommandError() const noexcept { return throw_on_command_error_; }

    /** @brief @~english UDP port bound for CRI push when connect(ip,port,local_ip) started push; 0 if disabled. @~chinese 开启 CRI 推送时本机绑定端口，用于 StopDataPush(ip, port)；未开启为 0。 */
    int getCriUdpListenPort() const { return cri_udp_port_; }

    // --- 17.4 开启实时控制 ---
    /** @brief @~english Start real-time control on the robot with specified parameters @~chinese 以指定参数开启机器人的实时控制
     * @~english
     * @param duration Interval time for real-time control, in milliseconds. (1000/frequency)
     * @param startBuffer Initial buffer array before starting control and how many position messages are received before movement begins
     * @param filterTypeint Filter type (0 - disable filtering, 1 - average filter value)
     * @param id Request ID.
     * @~chinese
     * @param duration 实时控制的间隔时间，单位为毫秒。（1000/频率）
     * @param startBuffer 开始控制前的初始缓冲数组，及收到多少个位置消息后才开始运动
     * @param filterTypeint 滤波类型（0-关闭滤波 1-平均滤波值）
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response startControl(int duration, int startBuffer = 5, int filterTypeint = 1, int id = 1);

    // --- 17.5 关闭实时控制 ---
    /** @brief @~english Stop the real-time control on the robot @~chinese 关闭机器人上的实时控制
     * @~english
     * @param id Request ID.
     * @~chinese
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response stopControl(int id = 1);
    
    // --- 19.1 设置碰撞灵敏度 ---
    /** @brief @~english Set the collision sensitivity level on the robot @~chinese 设置机器人上的碰撞灵敏度等级
     * @~english
     * @param level Collision sensitivity level (0-5).
     * @param id Request ID.
     * @~chinese
     * @param level 碰撞灵敏度等级（0-5）。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response setCollisionSensitivity(int level, int id = 1);

    // --- 19.2 设置负载参数 ---
    /** @brief @~english Set the payload parameters on the robot @~chinese 设置机器人上的负载参数
     * @~english
     * @param payloadId ID of the payload to set.
     * @param id Request ID.
     * @~chinese
     * @param payloadId 要设置的负载ID。
     * @param id 请求 ID。
     * @return @~english  Response object containing the result of the command execution.
     *         @~chinese  包含指令执行结果的响应对象。
     */
    Response setPayload(int payloadId, int id = 1);
    
    // ==========================================
    // 运动学 (Kinematics) 接口
    // ==========================================

    /**
     * @brief @~english Initialize the kinematics model with DH parameters.
     *        @~chinese 使用 DH 参数初始化运动学模型。
     * @param dh_matrix @~english A 6x4 matrix containing DH parameters [a, alpha, d, theta].
     *                  @~chinese 一个 6x4 的矩阵，包含 DH 参数 [a, alpha, d, theta]。
     * @return @~english True if initialization is successful, False if invalid parameters.
     *         @~chinese 成功返回 true，参数格式错误返回 false。
     */
    static bool kinematicsInit(const std::vector<std::vector<double>>& dh_matrix);
    static bool kinematicsInit_mm_deg(const std::vector<std::vector<double>>& dh_matrix);

    /**
     * @brief @~english Forward Kinematics (FK): Calculate TCP pose from joint angles.
     *        @~chinese 正向运动学 (FK)：根据关节角度计算 TCP 位姿。
     * @param qIn       @~english 6 joint angles in radians. @~chinese 6个关节的角度 (单位: rad)。
     * @param toolParam @~english 6 tool offset parameters [x, y, z, r, p, y]. @~chinese 6个工具坐标偏移量。
     * @param tcpPosOut @~english (Output) Calculated TCP pose [x, y, z, r, p, y]. @~chinese (输出) 计算出的 TCP 位姿。
     * @return int      @~english Error code (0 for success). @~chinese 错误码 (0 代表成功)。
     */
    static int kinematicsFk(const std::vector<double>& qIn, 
                     const std::vector<double>& toolParam, 
                     std::vector<double>& tcpPosOut);

    /**
     * @brief @~english Inverse Kinematics (IK): Calculate joint angles from TCP pose.
     *        @~chinese 逆向运动学 (IK)：根据 TCP 位姿计算关节角度。
     * @param tcpPosIn  @~english Target TCP pose [x, y, z, r, p, y]. @~chinese 目标 TCP 位姿。
     * @param toolParam @~english 6 tool offset parameters [x, y, z, r, p, y]. @~chinese 6个工具坐标偏移量。
     * @param qRef      @~english 6 reference joint angles (for multiple solutions). @~chinese 6个参考关节角度。
     * @param qOut      @~english (Output) Calculated joint angles in radians. @~chinese (输出) 计算出的关节角度。
     * @return int      @~english Error code (0 for success). @~chinese 错误码 (0 代表成功)。
     */
    static int kinematicsIk(const std::vector<double>& tcpPosIn, 
                     const std::vector<double>& toolParam, 
                     const std::vector<double>& qRef, 
                     std::vector<double>& qOut);

private:
    /** 内部：CRI 原始帧缓存（308 字节布局解析结果） */
    struct CriInternalCache {
        int64_t timestamp_ms = 0;
        uint16_t status_word1 = 0;
        uint16_t status_word2 = 0;
        std::array<double, 6> joint_pos_rad{};
        std::array<double, 6> joint_vel_rad_s{};
        std::array<double, 6> tcp_pose{};
        std::array<double, 6> tcp_vel{};
        double tcp_line_speed_m_s = 0;
        std::array<double, 6> joint_torque_nm{};
        std::array<double, 6> joint_external_torque_nm{};
    };

    static RobotRealtimeState buildRobotRealtimeState_(const CriInternalCache& c, bool data_valid);

    // 用于记录模型是否已经初始化（防止未初始化就调用正逆解导致崩溃）
    static bool isKinematicsModelInited_;

    asio::io_context io_context_;

    std::unique_ptr<asio::ip::tcp::socket> cmd_socket_;
    std::mutex cmd_mtx_; // 指令通道加锁，保证请求-响应不交叉

    std::string cmd_buffer_; // 仅由 tcpRecvLoop_ 写入与切分

    std::mutex publish_mtx_;
    std::unordered_map<std::string, std::vector<std::pair<uint64_t, PublishTopicHandler>>> publish_subs_;
    std::atomic<uint64_t> publish_sub_seq_{1};

    // --- TCP 收包线程 + 推送分发线程（与 C# FutureTcpClient 收包模型对齐）---
    std::thread tcp_recv_thread_;
    std::atomic<bool> tcp_recv_running_{false};

    struct PublishQueueItem {
        PublishNotification notification;
        std::vector<PublishTopicHandler> handlers;
    };
    std::thread publish_dispatch_thread_;
    std::atomic<bool> publish_dispatch_running_{false};
    std::mutex publish_queue_mtx_;
    std::condition_variable publish_dispatch_cv_;
    std::queue<PublishQueueItem> publish_queue_;

    std::mutex pending_mtx_;
    std::map<int, std::shared_ptr<std::promise<std::string>>> pending_responses_;
    std::atomic<int> next_request_id_{1};

    /** 保护 connect / disconnect / 收包线程启停及 sendCommand 内重连临界区（可重入）。 */
    std::recursive_mutex lifecycle_mtx_;

    void tcpRecvLoop_();
    void publishDispatchLoop_();
    void dispatchParsedJson_(const std::string& fragment);
    /** 停止 TCP 收包/推送分发线程并关闭指令 socket；会唤醒所有等待中的 sendCommand。 */
    void stopTcpRecvAndDispatch_();
    void stopTcpRecvAndDispatch_unlocked_();
    void stopTcpRecvThreadOnly_unlocked_();
    void startTcpRecvThreadAfterSocketConnected_unlocked_();
    void failAllPendingResponses_();

    // --- 新增：用于记录连接信息，实现重连 ---
    std::string last_ip_;
    int last_port_ = 9001;
    bool auto_reconnect_ = true; // 是否开启自动重连

    std::string local_ip_;
    int cri_udp_port_ = 0;
    std::unique_ptr<asio::ip::udp::socket> cri_udp_socket_;
    std::thread cri_udp_thread_;
    std::atomic<bool> cri_udp_running_{false};
    bool cri_push_active_ = false;

    static constexpr std::size_t kCriPushPacketBytes = 308;
    static constexpr int kCriAxisCount = 6;

    mutable std::mutex cri_cache_mtx_;
    CriInternalCache cri_cache_{};
    bool cri_cache_valid_ = false;

    std::mutex cri_handler_mtx_;
    CriDataReceivedHandler cri_data_received_handler_;
    bool throw_on_command_error_{false};

    bool connectTcpOnly(const std::string& ip, int port);
    void stopCriUdpReceiver_();
    bool startCriUdpPushSession_();
    void criUdpReceiveLoop_();
    void parseCriPushPacket_(const uint8_t* data, std::size_t len);

    // 内部校验函数
    bool isValidVariableName(const std::string& name, std::string& outError);

    // 关键字集合 (Lua 风格)
    const std::unordered_set<std::string> luaKeywords = {
        "and", "break", "do", "else", "elseif", "end","false", "for", "function", "goto", 
        "if", "in","local", "nil", "not", "or", "repeat", "return","then", "true", "until",
        "while", "table", "math","DO", "DOGroup", "DIO", "DIOGroup", "AO", "AIO","ModbusTCP",
        "setSpeedJ", "setAccJ", "setSpeedL", "setAccL", "setBlender","setMoveRate","getCoor",
        "getTool", "setCoor", "editCoor", "setTool", "editTool","setPayload","enableVibrationSuppression",
        "disableVibrationSuppression","setCollisionDetectionSensitivity","initComplianceControl",
        "enableComplianceControl","disableComplianceControl","forceControlZeroCalibrate",
        "setFilterPeriod","searchSuccessed","getJoint", "getTCP", "getCoor", "getTool",
        "aposToCpos","cposToApos", "cposToCpos","posOffset", "posTrans", "coorRel", "toolRel",
        "getJointTorque","getJointExternalTorque","createTray", "getTrayPos", "posInverse",
        "distance", "interPos","planeTrans","getTrajStart", "getTrajEnd", "arrayAdd", "arraySub",
        "coorTrans","movJ", "movL", "movC", "movCircle", "movLW", "movCW", "movTraj","setWeave",
        "weaveStart", "weaveEnd","setDO", "getDI", "getDO", "setDOGroup", "getDIGroup","getDOGroup",
        "setAO", "getAI", "getAO","getRegisterBool", "setRegisterBool", "getRegisterInt","setRegisterInt", 
        "getRegisterFloat", "setRegisterFloat","RS485init", "RS485flush", "RS485write", "RS485read",
        "readCoils", "readDiscreteInputs", "readHoldingRegisters","readInputRegisters","writeSingleCoil", 
        "writeSingleRegister", "writeMultipleCoils","writeMultipleRegisters","createSocketClient", 
        "connectSocketClient", "writeSocketClient","readSocketClient", "closeSocketClient","wait", 
        "waitCondition", "systemTime", "stopProject","pauseProject", "runScript", "pauseScript", 
        "resumeScript","stopScript", "callModule", "print","setInterruptInterval", "setInterruptCondition",
        "clearInterrupt","strcmp", "strToNumberArray", "arrayToStr","enableMultiWeld", "getCurSeam", 
        "isMultiWeldFinished","setMultiWeldOffset", "weldNextSeam", "resetMultiWeld","searchStart", 
        "setMasterFlag", "getOffsetValue", "search","searchEnd", "searchOffset", "searchOffsetEnd", 
        "searchError"
    };
    json packInstruction(const MoveInstruction& inst);
};

}

#endif /* CODROID_CONTROLLER_H */