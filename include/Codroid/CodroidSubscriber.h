/**
 * @file CodroidSubscriber.h
 * @brief @~english Header file for the CodroidSubscriber class @~chinese CodroidSubscriber 类的头文件
 */

#ifndef CODROID_SUBSCRIBER_H
#define CODROID_SUBSCRIBER_H

#include <asio.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <vector>
#include <map>
#include <mutex>
#include "CodroidDefine.h"

namespace Codroid {

using json = nlohmann::json;

/** 
 * @brief @~english Robot status callback type @~chinese 机器人状态回调类型 
 */
using StatusCallback = std::function<void(const RobotStatus&)>;

/** 
 * @brief @~english Robot posture callback type @~chinese 机器人位姿回调类型 
 */
using PostureCallback = std::function<void(const RobotPosture&)>;

/** 
 * @brief @~english Variable update callback type @~chinese 变量更新回调类型 
 */
using VarCallback    = std::function<void(const std::map<std::string, std::string>&)>;

/** 
 * @brief @~english System log callback type @~chinese 系统日志回调类型 
 */
using LogCallback    = std::function<void(const std::string& msg, int level)>;

/** 
 * @brief @~english Project Status callback type @~chinese 工程状态回调类型 
 */
using ProjectStatusCallback    = std::function<void(const ProjectState&)>;

/**
 * @class CodroidSubscriber
 * @brief @~english Dedicated class for asynchronous data subscription via a separate TCP channel.
 *        @~chinese 用于通过独立 TCP 通道进行异步数据订阅的专用类。
 * @details @~english This class manages its own background thread to handle continuous data pushes from the robot (e.g., status, posture, logs) without blocking command execution.
 *          @~chinese 该类管理独立的后台线程，用于处理来自机器人的持续数据推送（如状态、位姿、日志），不会阻塞指令执行。
 */
class CODROID_API CodroidSubscriber {
public:
    /**
     * @brief @~english Constructor @~chinese 构造函数
     */
    CodroidSubscriber();

    /**
     * @brief @~english Destructor @~chinese 析构函数
     */
    ~CodroidSubscriber();

    /**
     * @brief @~english Connect to the robot subscription server @~chinese 连接到机器人订阅服务器
     * @param ip @~english Robot IP address @~chinese 机器人 IP 地址
     * @param port @~english TCP port number (default 9001) @~chinese TCP 端口号 (默认 9001)
     * @return @~english true if connection established; false otherwise @~chinese 连接成功返回 true，否则返回 false
     */
    bool connect(const std::string& ip, int port = 9001);

    /**
     * @brief @~english Terminate connection and stop background processing @~chinese 终止连接并停止后台处理
     */
    void disconnect();

    /**
     * @brief @~english Subscribe to a specific data topic @~chinese 订阅特定的数据主题
     * @details @~english Sends a protocol 15.1 command to the robot to start pushing data for the given topic.
     *          @~chinese 向机器人发送协议 15.1 指令，开始推送指定主题的数据。
     * @param topic @~english Topic name (e.g., "RobotStatus", "RobotPosture", "Log") @~chinese 主题名称 (例如 "RobotStatus", "RobotPosture", "Log")
     * @param cycle @~english Push cycle in milliseconds (0 for default/change-based) @~chinese 推送周期，单位毫秒 (0 表示默认或仅在变化时推送)
     * @return @~english true if subscription command sent successfully @~chinese 订阅指令发送成功返回 true
     */
    bool subscribe(const std::string& topic, int cycle = 0);

    bool subscribeProjectState(int cycle=100);
    bool subscribeVarUpdate(int cycle=100);
    bool subscribeRobotStatus(int cycle=100);
    bool subscribeRobotPosture(int cycle=100);
    bool subscribeRobotCoordinate(int cycle=100);
    bool subscribeLog(int cycle=100);
    bool subscribeError(int cycle=100);

    /**
     * @brief @~english Set robot status update callback @~chinese 设置机器人状态更新回调
     * @param cb @~english Callback function @~chinese 回调函数
     */
    void setStatusCallback(StatusCallback cb) { onStatus_ = cb; }

    /**
     * @brief @~english Set robot status update callback @~chinese 设置机器人状态更新回调
     * @param cb @~english Callback function @~chinese 回调函数
     */
    void setProjectStatusCallback(ProjectStatusCallback cb) { onProjectStatus_ = cb; }

    /**
     * @brief @~english Set robot posture update callback @~chinese 设置机器人位姿更新回调
     * @param cb @~english Callback function @~chinese 回调函数
     */
    void setPostureCallback(Codroid::PostureCallback cb) { onPosture_ = cb; }

    /**
     * @brief @~english Set variable update callback @~chinese 设置变量更新回调
     * @param cb @~english Callback function @~chinese 回调函数
     */
    void setVarCallback(VarCallback cb) { onVar_ = cb; }

    /**
     * @brief @~english Set system log callback @~chinese 设置系统日志回调
     * @param cb @~english Callback function @~chinese 回调函数
     */
    void setLogCallback(LogCallback cb) { onLog_ = cb; }

private:
    /** @brief @~english Main background processing loop @~chinese 后台处理主循环 */
    void runLoop();

    /** @brief @~english Message dispatcher @~chinese 消息分发处理 */
    void handleMessage(const json& j);

    /** @brief @~english Read one complete JSON package @~chinese 读取一个完整的 JSON 数据包 */
    std::string receiveOneRaw();

    asio::io_context io_context_;
    std::unique_ptr<asio::ip::tcp::socket> socket_;
    
    std::string buffer_;
    std::thread thread_;
    std::atomic<bool> running_{false};

    StatusCallback onStatus_;
    PostureCallback onPosture_;
    VarCallback onVar_;
    LogCallback onLog_;
    ProjectStatusCallback onProjectStatus_;


    // 记录已订阅的主题及其周期，用于重连后恢复
    // key: topic_name, value: cycle_time
    std::map<std::string, int> active_subscriptions_;
    std::mutex sub_mtx_;

    std::string last_ip_;
    int last_port_;
};

} // namespace Codroid

#endif // CODROID_SUBSCRIBER_H