#include "Codroid/CodroidSubscriber.h"
#include <iostream>

namespace Codroid {

CodroidSubscriber::CodroidSubscriber() 
    : socket_(std::make_unique<asio::ip::tcp::socket>(io_context_)) {}

CodroidSubscriber::~CodroidSubscriber() {
    disconnect();
}

bool CodroidSubscriber::connect(const std::string& ip, int port) {
    this->last_ip_ = ip;
    this->last_port_ = port;
    try {
        if (socket_ && socket_->is_open()) socket_->close();
        
        asio::ip::tcp::resolver resolver(io_context_);
        asio::connect(*socket_, resolver.resolve(ip, std::to_string(port)));
        socket_->set_option(asio::ip::tcp::no_delay(true));
        
        // 只有在第一次连接时启动线程，后续重连不需要
        if (!running_) {
            running_ = true;
            thread_ = std::thread(&CodroidSubscriber::runLoop, this);
        }
        return true;
    } catch (std::exception& e) {
        std::cerr << "Subscriber connect error: " << e.what() << std::endl;
        return false;
    }
}

void CodroidSubscriber::disconnect() {
    running_ = false;
    if (socket_ && socket_->is_open()) {
        socket_->close();
    }
    if (thread_.joinable()) thread_.join();
}

bool CodroidSubscriber::subscribe(const std::string& topic, int cycle) {
    // 记录到本地列表，用于掉线后自动恢复
    {
        std::lock_guard<std::mutex> lock(sub_mtx_);
        active_subscriptions_[topic] = cycle;
    }
    
    // 发送 15.1 协议定义的订阅指令
    try {
        nlohmann::json req = {{"ty", "publish/" + topic}, {"tc", cycle}};
        std::string s = req.dump() + "\n";
        asio::write(*socket_, asio::buffer(s));
        return true;
    } catch (...) { return false; }
}

void CodroidSubscriber::runLoop() {
    while (running_) {
        // --- 逻辑 A: 如果 Socket 未连接，启动重连流程 ---
        if (!socket_ || !socket_->is_open()) {
            std::cout << "[Subscriber] Connection lost. Retrying in 2 seconds..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            if (this->connect(last_ip_, last_port_)) {
                std::cout << "[Subscriber] Reconnected! Restoring subscriptions..." << std::endl;
                
                // 关键点：重连成功后，必须自动重新发送之前的订阅请求
                // 否则机器人不会主动推送数据给新连接的 Socket
                std::lock_guard<std::mutex> lock(sub_mtx_);
                for (auto const& [topic, cycle] : active_subscriptions_) {
                    this->subscribe(topic, cycle);
                }
            }
            continue; // 跳过本次循环，重新进入 reading 状态
        }

        // --- 逻辑 B: 正常读取 ---
        std::string raw = receiveOneRaw();
        
        if (raw.empty()) {
            // 如果读到空字符串，通常意味着对端关闭了连接
            std::cerr << "[Subscriber] Remote host closed the connection." << std::endl;
            if (socket_) {
                asio::error_code ec;
                socket_->close(ec); // 标记为关闭，触发下一轮循环的重连逻辑
            }
            continue;
        }

        try {
            nlohmann::json j = nlohmann::json::parse(raw);
            handleMessage(j);
        } catch (const std::exception& e) {
            // 解析失败（数据包截断等情况），不中断线程
            // std::cerr << "[Subscriber] JSON Parse error: " << e.what() << std::endl;
        }
    }
}

void CodroidSubscriber::handleMessage(const json& j) {
    std::string ty = j.value("ty", "");
    if (!j.contains("db") || j["db"].is_null()) return;
    const auto& db = j["db"];

    // 15.4 机器人状态
    if (ty == "publish/RobotStatus" && onStatus_) {
        RobotStatus s;
        s.mode = db.value("mode", 0);
        s.state = db.value("state", 0);
        s.isMoving = (db.value("isMoving", 0) != 0);
        s.moveRate = db.value("moveRate", 0.0);
        s.type = db.value("type", "");
        s.stateName = db.value("stateName", "");
        onStatus_(s);
    }
    // 15.5 机器人位姿
    else if (ty == "publish/RobotPosture" && onPosture_) {
        RobotPosture p;
        p.joint = db.value("joint", std::vector<double>{});
        if (db.contains("end")) {
            p.cart.resize(6);
            p.cart[0] = db["end"].value("x", 0.0);
            p.cart[1] = db["end"].value("y", 0.0);
            p.cart[2] = db["end"].value("z", 0.0);
            p.cart[3] = db["end"].value("a", 0.0);
            p.cart[4] = db["end"].value("b", 0.0);
            p.cart[5] = db["end"].value("c", 0.0);
        }
        onPosture_(p);
    }
    // 15.3 变量更新
    else if (ty == "publish/VarUpdate" && onVar_) {
        std::map<std::string, std::string> vars;
        for (auto it = db.begin(); it != db.end(); ++it) {
            vars[it.key()] = it.value().get<std::string>();
        }
        onVar_(vars);
    }
    // 15.7/15.8 日志与错误
    else if ((ty == "publish/Log" || ty == "publish/Error") && onLog_) {
        for (const auto& logEntry : db) {
            if (logEntry.is_array() && logEntry.size() >= 4) {
                // logEntry[0]是等级, logEntry[3]是内容
                onLog_(logEntry[3].get<std::string>(), logEntry[0].get<int>());
            }
        }
    }
    // 15.2 工程状态
    else if ((ty == "publish/ProjectState") && onProjectStatus_) {
        ProjectState ps;
        
        // 使用 db.value(key, default) 安全提取基础字段
        ps.id = db.value("id", "");
        ps.state = db.value("state", 0);
        ps.isStep = db.value("isStep", false);
        ps.projectType = db.value("projectType", 0);

        // 解析 scripts 字段（注意：只有 state 为 2 或 3 时才会下发此字段）
        if (db.contains("scripts") && db["scripts"].is_object()) {
            const auto& scriptsObj = db["scripts"];
            
            // 遍历 scripts 下面的动态键 (如: "m8wope1d0tka199a")
            for (auto it = scriptsObj.begin(); it != scriptsObj.end(); ++it) {
                // it.key() 是脚本 ID
                // it.value() 是包含 line 字段的 JSON 对象，例如 {"line": 8}
                if (it.value().is_object() && it.value().contains("line")) {
                    // 将 "脚本ID" 和 "行号" 存入 C++ 的 std::map 中
                    ps.scriptLines[it.key()] = it.value().value("line", -1);
                }
            }
        }
        
        // 触发回调函数将解析好的对象传出
        onProjectStatus_(ps);
    }
}

std::string CodroidSubscriber::receiveOneRaw() {
    char chunk[1024];
    while (running_) {
        // 尝试从 buffer 中找完整包
        size_t braces = 0;
        int start = -1;
        for (int i = 0; i < (int)buffer_.length(); ++i) {
            if (buffer_[i] == '{') {
                if (start == -1) start = i;
                braces++;
            } else if (buffer_[i] == '}') {
                if (start != -1) {
                    braces--;
                    if (braces == 0) {
                        std::string res = buffer_.substr(start, i - start + 1);
                        buffer_.erase(0, i + 1);
                        return res;
                    }
                }
            }
        }
        // 没找到，读 Socket
        asio::error_code ec;
        size_t len = socket_->read_some(asio::buffer(chunk, sizeof(chunk)), ec);
        if (ec) return "";
        buffer_.append(chunk, len);
    }
    return "";
}

bool CodroidSubscriber::subscribeProjectState(int cycle){
    return subscribe("ProjectState", cycle);
}
bool CodroidSubscriber::subscribeVarUpdate(int cycle){
    return subscribe("VarUpdate", cycle);
}
bool CodroidSubscriber::subscribeRobotStatus(int cycle){
    return subscribe("RobotStatus", cycle);
}
bool CodroidSubscriber::subscribeRobotPosture(int cycle){
    return subscribe("RobotPosture", cycle);
}
bool CodroidSubscriber::subscribeRobotCoordinate(int cycle){
    return subscribe("RobotCoordinate", cycle);
}
bool CodroidSubscriber::subscribeLog(int cycle){
    return subscribe("Log", cycle);
}
bool CodroidSubscriber::subscribeError(int cycle){
    return subscribe("Error", cycle);
}

} // namespace Codroid