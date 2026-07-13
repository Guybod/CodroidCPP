#include "Codroid/CodroidController.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <future>
#include <map>
#include <random>
#include <unordered_set>

// 跨平台网络底层头文件
#if defined(_WIN32)
    #include <winsock2.h>
#else
    #include <sys/socket.h>
    #include <sys/types.h>
#endif

namespace Codroid {

namespace {

bool tryPopCompleteJson(std::string& sticky, std::string& out) {
    size_t brace_count = 0;
    int first_brace = -1;
    for (int i = 0; i < static_cast<int>(sticky.length()); ++i) {
        if (sticky[static_cast<std::size_t>(i)] == '{') {
            if (first_brace == -1)
                first_brace = i;
            brace_count++;
        } else if (sticky[static_cast<std::size_t>(i)] == '}') {
            if (first_brace != -1) {
                brace_count--;
                if (brace_count == 0) {
                    out = sticky.substr(static_cast<std::size_t>(first_brace),
                                        static_cast<std::size_t>(i - first_brace + 1));
                    sticky.erase(0, static_cast<std::size_t>(i + 1));
                    return true;
                }
            }
        }
    }
    return false;
}

bool tryExtractJsonRequestId(const nlohmann::json& j, int& out_id) {
    if (!j.contains("id") || j["id"].is_null())
        return false;
    if (j["id"].is_number_integer()) {
        out_id = j["id"].get<int>();
        return true;
    }
    if (j["id"].is_number_unsigned()) {
        out_id = static_cast<int>(j["id"].get<std::uint64_t>());
        return true;
    }
    if (j["id"].is_string()) {
        try {
            out_id = std::stoi(j["id"].get<std::string>());
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

double readF64LE(const uint8_t* p) {
    double v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

int64_t readI64LE(const uint8_t* p) {
    int64_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

uint16_t readU16LE(const uint8_t* p) {
    uint16_t v;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

/** 与 `CriRealtimePacketParser.RoundToDecimals`（C#，`MidpointRounding.AwayFromZero`）一致，`decimals=3`。 */
double cri_round_to_decimals_away(double value, int decimals) {
    if (value == 0.0 || std::isnan(value) || !std::isfinite(value))
        return value;
    double factor = std::pow(10.0, static_cast<double>(decimals));
    double rounded = std::round(value * factor) / factor;
    return rounded == 0.0 ? 0.0 : rounded;
}

void cri_round_vec_in_place(std::vector<double>& v, int decimals) {
    for (double& x : v)
        x = cri_round_to_decimals_away(x, decimals);
}

} // namespace

CodroidCommandException::CodroidCommandException(int request_id, std::string command_ty,
                                                 std::string controller_error, std::string raw_response_json)
    : CodroidException("CodroidCommandException (id=" + std::to_string(request_id) + ", ty=" + command_ty +
                       "): " + controller_error)
    , request_id_(request_id)
    , command_ty_(std::move(command_ty))
    , controller_error_(std::move(controller_error))
    , raw_response_json_(std::move(raw_response_json)) {}

bool CodroidController::isKinematicsModelInited_ = false;

/**
 * @brief Construct a new Codroid Control Interface object / 创建一个新的 Codroid 控制接口对象
 */
CodroidController::CodroidController()
    : io_context_(),
      cmd_socket_(std::make_unique<asio::ip::tcp::socket>(io_context_)) {}

/**
 * @brief Destructor for Codroid.
 *        Codroid 的析构函数
 * Cleans up the Codroid object and disconnects any active connections.
 * 清理 Codroid 对象并断开所有活动连接
 * This ensures that resources are properly released and the control interface is safely terminated when the object goes out of scope.
 * 这确保资源得到正确释放，并且控制接口在对象超出作用域时能够安全终止
 */
CodroidController::~CodroidController() {
    disconnect();
}

RobotRealtimeState CodroidController::buildRobotRealtimeState_(const CriInternalCache& c, bool data_valid) {
    RobotRealtimeState s;
    s.timestamp_ms = c.timestamp_ms;
    s.data_valid = data_valid;
    if (!data_valid)
        return s;

    constexpr int kDecimals = 3;
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;

    s.status1_raw = c.status_word1;
    s.status2_raw = c.status_word2;

    const uint16_t w1 = c.status_word1;
    s.project_running = (w1 & (1u << 0)) != 0;
    s.project_stopped = (w1 & (1u << 1)) != 0;
    s.project_paused = (w1 & (1u << 2)) != 0;
    s.enabling = (w1 & (1u << 3)) != 0;
    s.not_enabled = (w1 & (1u << 4)) != 0;
    s.manual_mode = (w1 & (1u << 5)) != 0;
    s.dragging = (w1 & (1u << 6)) != 0;
    s.in_motion = (w1 & (1u << 7)) != 0;

    s.collision_stopped = (w1 & (1u << 8)) != 0;
    s.in_safety_position = (w1 & (1u << 9)) != 0;
    s.has_alarm = (w1 & (1u << 10)) != 0;
    s.simulation_mode = (w1 & (1u << 11)) != 0;
    s.emergency_stop_pressed = (w1 & (1u << 12)) != 0;
    s.rescue_mode = (w1 & (1u << 13)) != 0;
    s.auto_mode = (w1 & (1u << 14)) != 0;
    s.remote_mode = (w1 & (1u << 15)) != 0;

    const uint16_t w2 = c.status_word2;
    s.realtime_control_mode = (w2 & (1u << 0)) != 0;
    s.cri_error_code = static_cast<uint8_t>(w2 >> 8);

    s.joint_position.resize(6);
    s.joint_velocity.resize(6);
    for (int i = 0; i < 6; ++i) {
        s.joint_position[static_cast<size_t>(i)] = c.joint_pos_rad[static_cast<size_t>(i)] * kRadToDeg;
        s.joint_velocity[static_cast<size_t>(i)] = c.joint_vel_rad_s[static_cast<size_t>(i)] * kRadToDeg;
    }
    cri_round_vec_in_place(s.joint_position, kDecimals);
    cri_round_vec_in_place(s.joint_velocity, kDecimals);

    s.tcp_pose.resize(6);
    s.tcp_velocity.resize(6);
    for (int i = 0; i < 3; ++i) {
        s.tcp_pose[static_cast<size_t>(i)] = c.tcp_pose[static_cast<size_t>(i)] * M_TO_MM;
        s.tcp_velocity[static_cast<size_t>(i)] = c.tcp_vel[static_cast<size_t>(i)] * M_TO_MM;
    }
    for (int i = 3; i < 6; ++i) {
        s.tcp_pose[static_cast<size_t>(i)] = c.tcp_pose[static_cast<size_t>(i)] * kRadToDeg;
        s.tcp_velocity[static_cast<size_t>(i)] = c.tcp_vel[static_cast<size_t>(i)] * kRadToDeg;
    }
    cri_round_vec_in_place(s.tcp_pose, kDecimals);
    cri_round_vec_in_place(s.tcp_velocity, kDecimals);

    s.tcp_linear_velocity_mm_s = cri_round_to_decimals_away(c.tcp_line_speed_m_s * M_TO_MM, kDecimals);

    s.joint_output_torque.resize(6);
    s.joint_external_force.resize(6);
    for (int i = 0; i < 6; ++i) {
        s.joint_output_torque[static_cast<size_t>(i)] =
            cri_round_to_decimals_away(c.joint_torque_nm[static_cast<size_t>(i)], kDecimals);
        s.joint_external_force[static_cast<size_t>(i)] =
            cri_round_to_decimals_away(c.joint_external_torque_nm[static_cast<size_t>(i)], kDecimals);
    }

    s.external_axis_position.clear();
    return s;
}

RobotRealtimeState CodroidController::getRobotRealtimeState() const {
    std::lock_guard<std::mutex> lock(cri_cache_mtx_);
    return buildRobotRealtimeState_(cri_cache_, cri_cache_valid_);
}

void CodroidController::setCriDataReceivedHandler(CriDataReceivedHandler handler) {
    std::lock_guard<std::mutex> lk(cri_handler_mtx_);
    cri_data_received_handler_ = std::move(handler);
}

void CodroidController::setThrowOnCommandError(bool enable) {
    throw_on_command_error_ = enable;
}

/**
 * @brief Connect to the Codroid server
 *        连接时会设置底层 Socket 超时和禁用 Nagle 算法，确保通信稳定和低延迟。
 * 
 * @param ip   ip 地址
 * @param port 端口号
 * @return true 
 * @return false 
 */
bool CodroidController::connectTcpOnly(const std::string& ip, int port) {
    std::lock_guard<std::recursive_mutex> life(lifecycle_mtx_);
    stopTcpRecvAndDispatch_unlocked_();
    try {
        last_ip_ = ip;
        last_port_ = port;

        if (!cmd_socket_)
            cmd_socket_ = std::make_unique<asio::ip::tcp::socket>(io_context_);

        asio::ip::tcp::resolver resolver(io_context_);
        auto endpoints = resolver.resolve(ip, std::to_string(port));
        asio::connect(*cmd_socket_, endpoints);
        cmd_socket_->set_option(asio::ip::tcp::no_delay(true));

        cmd_buffer_.clear();
        tcp_recv_running_ = true;
        publish_dispatch_running_ = true;
        tcp_recv_thread_ = std::thread(&CodroidController::tcpRecvLoop_, this);
        publish_dispatch_thread_ = std::thread(&CodroidController::publishDispatchLoop_, this);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SDK] TCP connect failed: " << e.what() << std::endl;
        stopTcpRecvAndDispatch_unlocked_();
        return false;
    }
}

int CodroidController::NextRequestId() {
    for (;;) {
        const int n = next_request_id_.fetch_add(1, std::memory_order_relaxed);
        if (n != 0)
            return n;
    }
}

bool CodroidController::connectTcp(const std::string& ip, int port) {
    return connectTcpOnly(ip, port);
}

bool CodroidController::connect(const std::string& ip, int port, std::string local_ip) {
    local_ip_ = std::move(local_ip);

    if (!connectTcpOnly(ip, port))
        return false;

    std::cout << "[SDK] Connected to Codroid Command Channel: " << ip << ":" << port << std::endl;

    toAuto(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    toRemote(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    if (!local_ip_.empty()) {
        if (!startCriUdpPushSession_()) {
            std::cerr << "[SDK] CRI StartDataPush / UDP bind failed." << std::endl;
            disconnect();
            return false;
        }
    }
    return true;
}

/**
 * @brief Disconnect from the Codroid server
 *        断开与 Codroid 服务器的连接
 * 
 */
void CodroidController::stopCriUdpReceiver_() {
    cri_udp_running_ = false;
    if (cri_udp_socket_) {
        asio::error_code ec;
        // 先 shutdown 再 close，否则阻塞在 udp::receive 上的线程在部分系统上不会立刻被唤醒，join 卡死
        cri_udp_socket_->shutdown(asio::socket_base::shutdown_receive, ec);
        cri_udp_socket_->close(ec);
    }
    if (cri_udp_thread_.joinable())
        cri_udp_thread_.join();
    cri_udp_socket_.reset();
    cri_udp_port_ = 0;
}

void CodroidController::criUdpReceiveLoop_() {
    std::array<uint8_t, 1024> buf{};
    while (cri_udp_running_) {
        if (!cri_udp_socket_ || !cri_udp_socket_->is_open())
            break;
        asio::error_code ec;
        std::size_t n = cri_udp_socket_->receive(asio::buffer(buf), 0, ec);
        if (ec || n == 0)
            break;
        parseCriPushPacket_(buf.data(), n);
    }
}

void CodroidController::parseCriPushPacket_(const uint8_t* data, std::size_t len) {
    if (len != kCriPushPacketBytes)
        return;

    CriInternalCache snap;
    std::size_t o = 0;
    snap.timestamp_ms = readI64LE(data + o);
    o += 8;
    snap.status_word1 = readU16LE(data + o);
    o += 2;
    snap.status_word2 = readU16LE(data + o);
    o += 2;

    for (int i = 0; i < kCriAxisCount; ++i) {
        snap.joint_pos_rad[static_cast<std::size_t>(i)] = readF64LE(data + o);
        o += 8;
    }
    for (int i = 0; i < kCriAxisCount; ++i) {
        snap.joint_vel_rad_s[static_cast<std::size_t>(i)] = readF64LE(data + o);
        o += 8;
    }
    for (int i = 0; i < kCriAxisCount; ++i) {
        snap.tcp_pose[static_cast<std::size_t>(i)] = readF64LE(data + o);
        o += 8;
    }
    for (int i = 0; i < kCriAxisCount; ++i) {
        snap.tcp_vel[static_cast<std::size_t>(i)] = readF64LE(data + o);
        o += 8;
    }
    snap.tcp_line_speed_m_s = readF64LE(data + o);
    o += 8;
    for (int i = 0; i < kCriAxisCount; ++i) {
        snap.joint_torque_nm[static_cast<std::size_t>(i)] = readF64LE(data + o);
        o += 8;
    }
    for (int i = 0; i < kCriAxisCount; ++i) {
        snap.joint_external_torque_nm[static_cast<std::size_t>(i)] = readF64LE(data + o);
        o += 8;
    }

    {
        std::lock_guard<std::mutex> lock(cri_cache_mtx_);
        cri_cache_ = snap;
        cri_cache_valid_ = true;
    }

    const RobotRealtimeState delivered = buildRobotRealtimeState_(snap, true);

    CriDataReceivedHandler h_copy;
    {
        std::lock_guard<std::mutex> hk(cri_handler_mtx_);
        h_copy = cri_data_received_handler_;
    }
    if (h_copy)
        h_copy(delivered);
}

bool CodroidController::startCriUdpPushSession_() {
    stopCriUdpReceiver_();
    cri_push_active_ = false;
    {
        std::lock_guard<std::mutex> lock(cri_cache_mtx_);
        cri_cache_valid_ = false;
        cri_cache_ = CriInternalCache{};
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(10000, 65535);

    asio::ip::address bind_addr = asio::ip::make_address(local_ip_);
    int chosen_port = 0;

    for (int attempt = 0; attempt < 32; ++attempt) {
        chosen_port = dist(gen);
        try {
            cri_udp_socket_ = std::make_unique<asio::ip::udp::socket>(io_context_);
            cri_udp_socket_->open(asio::ip::udp::v4());
            cri_udp_socket_->set_option(asio::socket_base::reuse_address(true));
            cri_udp_socket_->bind(asio::ip::udp::endpoint(bind_addr, static_cast<unsigned short>(chosen_port)));
            cri_udp_port_ = chosen_port;
            break;
        } catch (...) {
            cri_udp_socket_.reset();
            if (attempt == 31)
                return false;
        }
    }

    cri_udp_running_ = true;
    cri_udp_thread_ = std::thread(&CodroidController::criUdpReceiveLoop_, this);

    Response pushResp = startDataPush(local_ip_, cri_udp_port_, 100, 1, true);
    if (!pushResp.error_msg.empty()) {
        stopCriUdpReceiver_();
        return false;
    }
    cri_push_active_ = true;
    return true;
}

void CodroidController::disconnect() {
    {
        std::lock_guard<std::mutex> plock(publish_mtx_);
        publish_subs_.clear();
    }
    if (cri_push_active_ && cmd_socket_ && cmd_socket_->is_open()) {
        json db;
        db["ip"] = local_ip_;
        db["port"] = cri_udp_port_;
        sendCommand("CRI/StopDataPush", db, 1);
    }
    cri_push_active_ = false;
    stopCriUdpReceiver_();
    {
        std::lock_guard<std::mutex> lock(cri_cache_mtx_);
        cri_cache_valid_ = false;
    }
    {
        std::lock_guard<std::mutex> lk(cri_handler_mtx_);
        cri_data_received_handler_ = {};
    }

    stopTcpRecvAndDispatch_();
}

void CodroidController::failAllPendingResponses_() {
    std::map<int, std::shared_ptr<std::promise<std::string>>> swap_map;
    {
        std::lock_guard<std::mutex> plock(pending_mtx_);
        swap_map.swap(pending_responses_);
    }
    for (auto& kv : swap_map) {
        try {
            kv.second->set_value("");
        } catch (...) {
        }
    }
}

void CodroidController::stopTcpRecvAndDispatch_() {
    std::lock_guard<std::recursive_mutex> life(lifecycle_mtx_);
    stopTcpRecvAndDispatch_unlocked_();
}

void CodroidController::stopTcpRecvAndDispatch_unlocked_() {
    tcp_recv_running_ = false;
    if (cmd_socket_ && cmd_socket_->is_open()) {
        asio::error_code ec;
        cmd_socket_->shutdown(asio::socket_base::shutdown_both, ec);
        cmd_socket_->close(ec);
    }
    if (tcp_recv_thread_.joinable())
        tcp_recv_thread_.join();

    {
        std::lock_guard<std::mutex> lk(publish_queue_mtx_);
        publish_dispatch_running_ = false;
    }
    publish_dispatch_cv_.notify_all();
    if (publish_dispatch_thread_.joinable())
        publish_dispatch_thread_.join();

    {
        std::lock_guard<std::mutex> lk(publish_queue_mtx_);
        while (!publish_queue_.empty())
            publish_queue_.pop();
    }

    cmd_buffer_.clear();
    failAllPendingResponses_();

    // 与 C# `Disconnect` 语义一致：会话结束 = 收包/分发线程已 join + TCP 套接字释放，避免进程退出后仍挂接 I/O
    cmd_socket_.reset();
}

void CodroidController::stopTcpRecvThreadOnly_unlocked_() {
    tcp_recv_running_ = false;
    if (cmd_socket_ && cmd_socket_->is_open()) {
        asio::error_code ec;
        cmd_socket_->shutdown(asio::socket_base::shutdown_both, ec);
        cmd_socket_->close(ec);
    }
    if (tcp_recv_thread_.joinable())
        tcp_recv_thread_.join();
    cmd_buffer_.clear();
}

void CodroidController::startTcpRecvThreadAfterSocketConnected_unlocked_() {
    cmd_buffer_.clear();
    tcp_recv_running_ = true;
    tcp_recv_thread_ = std::thread(&CodroidController::tcpRecvLoop_, this);
}

void CodroidController::tcpRecvLoop_() {
    char chunk[4096];
    while (tcp_recv_running_.load()) {
        if (!cmd_socket_ || !cmd_socket_->is_open())
            break;
        asio::error_code ec;
        std::size_t n = cmd_socket_->read_some(asio::buffer(chunk), ec);
        if (ec || n == 0)
            break;
        cmd_buffer_.append(chunk, n);
        if (cmd_buffer_.length() > 1024 * 512) {
            cmd_buffer_.clear();
            break;
        }
        while (true) {
            std::string fragment;
            if (!tryPopCompleteJson(cmd_buffer_, fragment))
                break;
            dispatchParsedJson_(fragment);
        }
    }
    tcp_recv_running_ = false;
}

void CodroidController::publishDispatchLoop_() {
    while (true) {
        PublishQueueItem item;
        bool has_item = false;
        {
            std::unique_lock<std::mutex> lk(publish_queue_mtx_);
            publish_dispatch_cv_.wait(lk, [&] {
                return !publish_queue_.empty() || !publish_dispatch_running_.load();
            });
            if (!publish_dispatch_running_.load() && publish_queue_.empty())
                return;
            if (!publish_queue_.empty()) {
                item = std::move(publish_queue_.front());
                publish_queue_.pop();
                has_item = true;
            }
        }
        if (has_item) {
            for (auto& h : item.handlers) {
                if (h) {
                    try {
                        h(item.notification);
                    } catch (...) {
                    }
                }
            }
        }
    }
}

void CodroidController::dispatchParsedJson_(const std::string& fragment) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(fragment);
    } catch (...) {
        return;
    }

    int msg_id = 0;
    if (tryExtractJsonRequestId(j, msg_id)) {
        std::shared_ptr<std::promise<std::string>> prom;
        {
            std::lock_guard<std::mutex> plock(pending_mtx_);
            auto it = pending_responses_.find(msg_id);
            if (it != pending_responses_.end()) {
                prom = it->second;
                pending_responses_.erase(it);
            }
        }
        if (prom) {
            try {
                prom->set_value(fragment);
            } catch (...) {
            }
            return;
        }
        return;
    }

    if (!j.contains("ty") || !j["ty"].is_string())
        return;

    const std::string ty = j["ty"].get<std::string>();
    std::vector<PublishTopicHandler> handlers;
    {
        std::lock_guard<std::mutex> plock(publish_mtx_);
        auto it = publish_subs_.find(ty);
        if (it != publish_subs_.end()) {
            for (const auto& pr : it->second)
                handlers.push_back(pr.second);
        }
    }
    if (handlers.empty())
        return;

    PublishQueueItem qitem;
    qitem.notification.ty = ty;
    qitem.notification.db =
        (j.contains("db") && !j["db"].is_null()) ? j["db"] : nlohmann::json::object();
    qitem.notification.raw_json = fragment;
    qitem.handlers = std::move(handlers);
    {
        std::lock_guard<std::mutex> lk(publish_queue_mtx_);
        publish_queue_.push(std::move(qitem));
    }
    publish_dispatch_cv_.notify_one();
}

/**
 * @brief Send a request to the Codroid server via the Command Channel
 *        通过指令通道向 Codroid 服务器发送请求并同步等待响应
 *
 * @param type request type / 请求类型 (例如 "Robot/switchOn")
 * @param data request data / 请求数据 (JSON 对象)
 * @param id request ID / 请求 ID
 * @return Response / 响应结果结构体
 */
Response CodroidController::sendCommand(const std::string& type, const nlohmann::json& data, int id) {
    auto prom = std::make_shared<std::promise<std::string>>();
    std::future<std::string> fut = prom->get_future();

    {
        std::lock_guard<std::mutex> plock(pending_mtx_);
        if (!pending_responses_.emplace(id, prom).second) {
            throw CodroidException("Duplicate request id " + std::to_string(id) +
                                   ". Use NextRequestId() for concurrent commands.");
        }
    }

    Response resp;
    resp.id = id;
    resp.ty = type;

    const std::string& req_ty_capture = type;
    auto finalize = [&](Response r, const std::string* raw_wire) -> Response {
        if (raw_wire && !raw_wire->empty())
            r.raw_json = *raw_wire;
        if (throw_on_command_error_ && !r.error_msg.empty()) {
            throw CodroidCommandException(r.id, r.ty.empty() ? req_ty_capture : r.ty, r.error_msg,
                                          r.raw_json);
        }
        return r;
    };

    {
        std::unique_lock<std::mutex> lock(cmd_mtx_);

        if (!cmd_socket_ || !cmd_socket_->is_open()) {
            std::cout << "[SDK] Socket closed. Attempting to reconnect..." << std::endl;
            lock.unlock();
            try {
                std::lock_guard<std::recursive_mutex> life(lifecycle_mtx_);
                if (!cmd_socket_)
                    cmd_socket_ = std::make_unique<asio::ip::tcp::socket>(io_context_);
                stopTcpRecvThreadOnly_unlocked_();
                asio::ip::tcp::resolver resolver(io_context_);
                auto endpoints = resolver.resolve(last_ip_, std::to_string(last_port_));
                asio::connect(*cmd_socket_, endpoints);
                cmd_socket_->set_option(asio::ip::tcp::no_delay(true));
                startTcpRecvThreadAfterSocketConnected_unlocked_();
            } catch (const std::exception& e) {
                resp.error_msg =
                    std::string("Command socket is disconnected and reconnection failed: ") + e.what();
            }
            lock.lock();
        }

        if (resp.error_msg.empty()) {
            try {
                nlohmann::json req_json;
                req_json["id"] = id;
                req_json["ty"] = type;
                req_json["db"] = data.is_null() ? nlohmann::json::object() : data;

                std::string request_str = req_json.dump() + "\n";

                asio::error_code ec;
                asio::write(*cmd_socket_, asio::buffer(request_str), ec);

                if (ec) {
                    std::cerr << "[SDK] Write failed (" << ec.message() << "). Retrying once..." << std::endl;
                    lock.unlock();
                    try {
                        std::lock_guard<std::recursive_mutex> life(lifecycle_mtx_);
                        stopTcpRecvThreadOnly_unlocked_();
                        asio::ip::tcp::resolver resolver(io_context_);
                        auto endpoints = resolver.resolve(last_ip_, std::to_string(last_port_));
                        asio::connect(*cmd_socket_, endpoints);
                        cmd_socket_->set_option(asio::ip::tcp::no_delay(true));
                        startTcpRecvThreadAfterSocketConnected_unlocked_();
                    } catch (const std::exception& ex) {
                        resp.error_msg = std::string("Network error during reconnect: ") + ex.what();
                    }
                    lock.lock();
                    if (resp.error_msg.empty()) {
                        ec.clear();
                        asio::write(*cmd_socket_, asio::buffer(request_str), ec);
                        if (ec)
                            resp.error_msg = "Network error during write: " + ec.message();
                    }
                }

            } catch (const std::exception& e) {
                resp.error_msg = std::string("Command Channel network Error: ") + e.what();
                if (cmd_socket_) {
                    asio::error_code ec2;
                    cmd_socket_->close(ec2);
                }
            }
        }
    }

    if (!resp.error_msg.empty()) {
        std::lock_guard<std::mutex> plock(pending_mtx_);
        pending_responses_.erase(id);
        try {
            prom->set_value("");
        } catch (...) {
        }
        return finalize(resp, nullptr);
    }

    std::string response_str;
    try {
        if (fut.wait_for(std::chrono::seconds(10)) != std::future_status::ready) {
            std::lock_guard<std::mutex> plock(pending_mtx_);
            pending_responses_.erase(id);
            resp.error_msg = "Command receive timeout (10s).";
            return finalize(resp, nullptr);
        }
        response_str = fut.get();
    } catch (...) {
        std::lock_guard<std::mutex> plock(pending_mtx_);
        pending_responses_.erase(id);
        resp.error_msg = "Command wait interrupted.";
        return finalize(resp, nullptr);
    }

    if (response_str.empty()) {
        resp.error_msg = "Command socket disconnected while waiting for response.";
        return finalize(resp, nullptr);
    }

    try {
        nlohmann::json j_resp = nlohmann::json::parse(response_str);

        int parsed_id = id;
        if (tryExtractJsonRequestId(j_resp, parsed_id))
            resp.id = parsed_id;
        if (j_resp.contains("ty") && j_resp["ty"].is_string())
            resp.ty = j_resp["ty"].get<std::string>();

        if (j_resp.contains("err") && !j_resp["err"].is_null()) {
            if (j_resp["err"].is_string())
                resp.error_msg = j_resp["err"].get<std::string>();
            else
                resp.error_msg = j_resp["err"].dump();
        } else {
            resp.error_msg = "";
            resp.db = (j_resp.contains("db") && !j_resp["db"].is_null()) ? j_resp["db"] : nlohmann::json::object();
        }
    } catch (const nlohmann::json::parse_error& e) {
        resp.error_msg = std::string("JSON Parse Error: ") + e.what();
    }

    return finalize(resp, &response_str);
}

PublishTopicSubscription CodroidController::subscribePublishTopic(std::string topicTy,
                                                                PublishTopicHandler handler,
                                                                int tc_milliseconds) {
    if (topicTy.empty())
        throw CodroidException("subscribePublishTopic: topicTy is empty");
    if (tc_milliseconds <= 0)
        tc_milliseconds = 100;

    const uint64_t sid = publish_sub_seq_.fetch_add(1, std::memory_order_relaxed);
    bool first_for_topic = false;

    {
        std::lock_guard<std::mutex> plock(publish_mtx_);
        auto& vec = publish_subs_[topicTy];
        first_for_topic = vec.empty();
        vec.emplace_back(sid, std::move(handler));
    }

    auto unsubscribe = [this, topicTy, sid]() {
        std::lock_guard<std::mutex> plock(publish_mtx_);
        auto it = publish_subs_.find(topicTy);
        if (it == publish_subs_.end())
            return;
        auto& vec = it->second;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
                                 [sid](const auto& p) { return p.first == sid; }),
                  vec.end());
        if (vec.empty())
            publish_subs_.erase(it);
    };

    if (!first_for_topic)
        return PublishTopicSubscription(unsubscribe);

    std::lock_guard<std::mutex> clock(cmd_mtx_);
    if (!cmd_socket_ || !cmd_socket_->is_open()) {
        unsubscribe();
        throw CodroidException("subscribePublishTopic: not connected");
    }

    try {
        json frame;
        frame["ty"] = topicTy;
        frame["tc"] = tc_milliseconds;
        std::string line = frame.dump() + "\n";
        asio::error_code ec;
        asio::write(*cmd_socket_, asio::buffer(line), ec);
        if (ec) {
            unsubscribe();
            throw CodroidException("subscribePublishTopic write failed: " + ec.message());
        }
    } catch (const CodroidException&) {
        throw;
    } catch (const std::exception& e) {
        unsubscribe();
        throw CodroidException(std::string("subscribePublishTopic: ") + e.what());
    }

    return PublishTopicSubscription(std::move(unsubscribe));
}

void CodroidController::printResponse(const Response& resp) {
    std::cout << "-----------------------" << std::endl;
    std::cout << "Request ID: " << resp.id << std::endl;
    std::cout << "Type:       " << resp.ty << std::endl;
    
    if (resp.error_msg.empty()) {
        std::cout << "Status:     [SUCCESS]" << std::endl;
        std::cout << "Data:       " << resp.db.dump(4) << std::endl;
    } else {
        std::cout << "Status:     [FAILED]" << std::endl;
        std::cout << "Error:      " << resp.error_msg << std::endl;
    }
    std::cout << "-----------------------" << std::endl << std::endl;
}



// --- 2.1 运行脚本 ---
/**
 * @brief Run a script on the Codroid server
 *        在 Codroid 服务器上运行脚本
 * 
 * @param script script content / 脚本内容
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::runScript(const RunScriptParams& params, int id) {
    json db;
    json scripts;

    // 1. 组装 scripts 对象
    scripts["main"] = params.mainCode;

    // nlohmann/json 会自动将 std::unordered_map 转换为 JSON 的 Object
    if (!params.subThreads.empty()) {
        scripts["subThreads"] = params.subThreads;
    }
    if (!params.subPrograms.empty()) {
        scripts["subPrograms"] = params.subPrograms;
    }
    if (!params.interrupts.empty()) {
        scripts["interrupts"] = params.interrupts;
    }
    
    db["scripts"] = scripts;

    // 2. 组装 vars 对象 (如果 vars 不为空)
    if (!params.vars.empty()) {
        db["vars"] = params.vars;
    }

    // 3. 调用底层的 sendCommand 发送数据
    return sendCommand("project/runScript", db, id);
}

// --- 2.2 进入远程脚本模式 ---
/**
 * @brief Enter remote script mode on the Codroid server
 *        在 Codroid 服务器上进入远程脚本模式
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::enterRemoteScriptMode(int id){
    return sendCommand("project/enterRemoteScriptMode", json::object(), id);
}

// --- 2.3 运行工程(工程ID) ---
/** 
 * @brief Run a project on the Codroid server
 *        在 Codroid 服务器上运行工程
 * 
 * @param projectid project ID / 工程 ID
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::runProject(const std::string& projectid, int id){
    json data;
    data["id"] = projectid;
    return sendCommand("project/run", data, id);
}

// --- 2.4 运行工程(工程索引) ---
/** 
 * @brief Run a project by its index on the Codroid server
 *        在 Codroid 服务器上通过索引运行工程
 * 
 * @param index project index / 工程索引
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::runProjectByIndex(int index, int id){
    return sendCommand("project/runByIndex", index, id);
}

// --- 2.5 单步运行 ---
/** 
 * @brief Run a single step of a project on the Codroid server
 *        在 Codroid 服务器上单步运行工程
 * 
 * @param projectid project ID / 工程 ID
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::runStep(const std::string& projectid, int id){
    json data;
    data["id"] = projectid;
    return sendCommand("project/runStep", data, id);
};
// --- 2.6 暂停工程 ---
/** 
 * @brief Pause a project on the Codroid server
 *        在 Codroid 服务器上暂停工程
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::pauseProject(int id){
    return sendCommand("project/pause", json::object(), id);
};

// --- 2.7 恢复工程 ---
/** 
 * @brief Resume a paused project on the Codroid server
 *        在 Codroid 服务器上恢复暂停的工程
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::resumeProject(int id){
    return sendCommand("project/resume", json::object(), id);
};

// --- 2.8 停止工程 ---
/** 
 * @brief Stop a project on the Codroid server
 *        在 Codroid 服务器上停止工程
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::stopProject(int id){
    return sendCommand("project/stop", json::object(), id);
};

// --- 2.13 设置启动行 ---
/** 
 * @brief Set the start line for a project on the Codroid server
 *        在 Codroid 服务器上设置工程的启动行
 * 
 * @param startline line number to start from / 启动行行号
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::setStartLine(int startline, int id){
    return sendCommand("project/setStartLine", startline, id);
};

// --- 2.14 清除启动行 ---
/** 
 * @brief Clear the start line setting for a project on the Codroid server
 *        在 Codroid 服务器上清除工程的启动行设置
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::clearStartLine(int id){
    return sendCommand("project/clearStartLine", json::object(), id);
};

// --- 3.3 获取全局变量列表 ---
/** 
 * @brief Get the list of global variables from the Codroid server
 *        从 Codroid 服务器获取全局变量列表
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::getGlobalVars(int id){
    return sendCommand("globalVar/getVars", json::object(), id);
};


// --- 3.4 保存全局变量列表 ---
/**
 * @brief Save global variables (incremental) / 保存全局变量（增量保存）
 * @param vars Map of variable name to Variable struct / 变量名与变量信息的映射表
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::saveGlobalVars(const std::map<std::string, Variable>& vars, int id) {
    Response resp;
    resp.id = id;
    resp.ty = "globalVar/saveVars";

    // 1. 基础命名规则校验
    std::string nameError;
    for (const auto& [name, info] : vars) {
        if (!isValidVariableName(name, nameError)) {
            resp.error_msg = "Validation Failed: " + nameError;
            return resp; // 发现任一不合法，直接拦截，不发送请求
        }
    }

    // 2. 构造并发送请求
    json db = json::object();
    for (const auto& [name, info] : vars) {
        json item;
        item["val"] = info.val;
        if (!info.nm.empty()) item["nm"] = info.nm;
        db[name] = item;
    }

    return sendCommand("globalVar/saveVars", db, id);
}

// --- 3.5 删除全局变量 ---
/** 
 * @brief Remove global variables / 删除全局变量
 * @param varNames List of variable names to remove / 要删除的变量名列表
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::removeGlobalVars(const std::vector<std::string>& varNames, int id) {
    json db = json::array();
    for (const auto& name : varNames) {
        db.push_back(name);
    }
    return sendCommand("globalVar/removeVars", db, id);
};

// --- 4.1 工程变量接口 ---
/**
 * @brief Get project variables / 获取工程变量
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::getProjectVar(int id) {
    return sendCommand("project/getVars", json::object(), id);
};

// --- 5.1 初始化RS485 ---
/** 
 * @brief Initialize RS485 communication
 *        初始化RS485通信
 * 
 * @param baudrate Baud rate / 波特率
 * @param stopBit Stop bits / 停止位
 * @param dataBit Data bits / 数据位
 * @param parity Parity / 校验位
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::RS485init(int baudrate,  RS485StopBits stopBit, int dataBit,RS485Parity parity, int id) {
    json db;
    db["baudrate"] = baudrate;
    db["stopBit"] = static_cast<int>(stopBit);
    db["dataBit"] = dataBit;
    db["parity"] = static_cast<int>(parity);
    return sendCommand("EC2RS485/init", db, id);
}

// --- 5.2 RS485 flush ---
/**
 * @brief Flush RS485 buffers / 刷新RS485缓冲区
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::RS485flush(int id) {
    return sendCommand("EC2RS485/flushReadBuffer", json::object(), id);
};

// --- 5.3 RS485 read ---
/**
 * @brief Read data from RS485 / 从RS485读取数据
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::RS485read(int length, int timeout, int id) {
    json db;
    db["length"] = length;
    db["timeout"] = timeout;
    return sendCommand("EC2RS485/read", db, id);
}

// --- 5.4 RS485 write ---
/**
 * @brief RS485 Write Data / RS485 写入数据
 * @param[in] data Vector of bytes to send / 要发送的字节数组 (uint8_t)
 * @param[in] id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::RS485write(const std::vector<uint8_t>& data, int id) {
    // 1. 长度校验：协议规定最大长度为 127
    if (data.size() > 127) {
        Response errResp;
        errResp.id = id;
        errResp.error_msg = "RS485 write failed: Data length exceeds maximum limit of 127 bytes (Current size: " + std::to_string(data.size()) + ")";
        return errResp;
    }

    // 2. 检查数据是否为空
    if (data.empty()) {
        Response errResp;
        errResp.id = id;
        errResp.error_msg = "RS485 write failed: Data is empty";
        return errResp;
    }

    // 3. 构建 db
    // nlohmann::json 可以直接将 std::vector 转换为 JSON 数组 [val1, val2, ...]
    // 此时 db 就是 [2, 10]，而不是 {"data": [2, 10]}
    json db = data; 

    return sendCommand("EC2RS485/write", db, id);
}

// --- 10.1 正解接口 ---
/**
 * @brief forwardKinematics 正解计算 (关节角 -> 笛卡尔坐标)
 * @throw CodroidException 计算失败时抛出
 * @return std::vector<double> [x, y, z, a, b, c]
 */
std::vector<double> CodroidController::forwardKinematics(const FKParams& params, int id) {
    json db;
    db["jp"] = params.jp;
    
    // 可选参数检查：只有不为空时才加入 JSON
    if (!params.coor.empty()) db["coor"] = params.coor;
    if (!params.tool.empty()) db["tool"] = params.tool;
    if (!params.ep.empty())   db["ep"] = params.ep;
    else db["ep"] = json::array(); // 如果不传 ep，根据协议习惯传空数组

    Response resp = sendCommand("Robot/apostocpos", db, id);

    if (!resp.error_msg.empty()) {
        throw CodroidException("FK Failed: " + resp.error_msg);
    }

    try {
        return resp.db.get<std::vector<double>>();
    } catch (const std::exception& e) {
        throw CodroidException("FK Parse Error: " + std::string(e.what()));
    }
}

// --- 10.2 逆解接口 ---
/**
 * @brief inverseKinematics 逆解计算 (笛卡尔坐标 -> 关节角)
 * @throw CodroidException 计算失败时抛出
 * @return std::vector<double> [j1, j2, j3, j4, j5, j6]
 */
std::vector<double> CodroidController::inverseKinematics(const IKParams& params, int id) {
    json db;
    db["cp"] = params.cp;
    
    // 参考关节角处理：如果用户没传，按文档默认 [20,20,20,20,20,20]
    if (!params.rj.empty()) {
        db["rj"] = params.rj;
    } else {
        db["rj"] = std::vector<double>{20, 20, 20, 20, 20, 20};
    }

    if (!params.ep.empty()) db["ep"] = params.ep;
    else db["ep"] = json::array();

    Response resp = sendCommand("Robot/cpostoapos", db, id);

    if (!resp.error_msg.empty()) {
        throw CodroidException("IK Failed: " + resp.error_msg);
    }

    try {
        // 如果逆解失败（返回值为空），机器人通常会返回错误
        // 这里直接返回数组结果
        return resp.db.get<std::vector<double>>();
    } catch (const std::exception& e) {
        throw CodroidException("IK Parse Error: " + std::string(e.what()));
    }
}

// --- 10.3 笛卡尔坐标偏移计算接口 ---
/**
 * @brief calculateRelativePose 计算相对位姿 (直接返回结果数组)
 * @throw CodroidException 当机器人返回错误或网络异常时抛出
 * @return std::vector<double> 计算后的坐标数组 [x,y,z,a,b,c]
 */
std::vector<double> CodroidController::calculateRelativePose(const RelativePoseParams& params, int id) {
    json db;
    db["pos"] = params.pos;
    db["offset"] = params.offset;
    db["coorType"] = params.coorType;

    if (!params.posCoor.empty()) db["posCoor"] = params.posCoor;
    if (params.coorType == CoorType::User && !params.coor.empty()) {
        db["coor"] = params.coor;
    }

    // 1. 发送请求并获取原始 Response
    Response resp = sendCommand("Robot/calculateRelativePose", db, id);

    // 2. 检查是否有错误
    if (!resp.error_msg.empty()) {
        // 如果有错误，直接抛出异常，不再返回数据
        throw CodroidException("Robot Error: " + resp.error_msg);
    }

    // 3. 检查返回的数据格式是否正确
    if (!resp.db.is_array()) {
        throw CodroidException("Protocol Error: Response 'db' is not an array.");
    }

    // 4. 直接返回转换后的 vector
    try {
        return resp.db.get<std::vector<double>>();
    } catch (const std::exception& e) {
        throw CodroidException("Data Parse Error: " + std::string(e.what()));
    }
}

std::vector<double> CodroidController::cposToCpos(const std::vector<double>& cp,
                                                   const std::vector<double>& coor1, const std::vector<double>& tool1,
                                                   const std::vector<double>& coor2, const std::vector<double>& tool2,
                                                   int id) {
    json db;
    db["cp"] = cp;
    db["coor1"] = coor1;
    db["tool1"] = tool1;
    db["coor2"] = coor2;
    db["tool2"] = tool2;

    Response resp = sendCommand("Robot/cpostocpos", db, id);
    if (!resp.error_msg.empty()) {
        throw CodroidException("CposToCpos Failed: " + resp.error_msg);
    }
    try {
        return resp.db.get<std::vector<double>>();
    } catch (const std::exception& e) {
        throw CodroidException("CposToCpos Parse Error: " + std::string(e.what()));
    }
}

// --- 11.1 点动 ---
/** 
 * @brief Jog the robot with specified parameters / 使用指定参数点动机器人
 * @param params Jog parameters / 点动参数
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::jog(const JogParams& params, int id) {
    // 速度边界检查
    if (params.speed < -1.0 || params.speed > 1.0) {
        Response resp;
        resp.id = id;
        resp.error_msg = "Jog speed out of range (-1 to 1)";
        return resp;
    }

    json db;
    db["mode"] = params.mode;         // 自动转为 int
    db["speed"] = params.speed;
    db["index"] = params.index;
    db["coorType"] = params.coorType; // 自动转为 int
    db["coorId"] = params.coorId;

    return sendCommand("Robot/jog", db, id);
}

// --- 11.2 停止点动 ---
/** 
 * @brief Stop jogging the robot / 停止点动机器人
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::stopJog(int id) {
    return sendCommand("Robot/stopJog", json::object(), id);
}

// --- 11.3 点动心跳 ---
/** 
 * @brief Send a jogging heart rate signal every 0.5 seconds to maintain jogging activity / 每0.5s发送点动心跳以保持点动活动
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::jogHeartbeat(int id) {
    return sendCommand("Robot/jogHeartbeat", json::object(), id);
}

// --- 11.4 RunTo ---
/** 
 * @brief Run the robot to a specified position
 *        运行机器人到指定位置
 * @param params 运动参数
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::moveTo(const MoveToParams& params, int id) {
    json db;
    db["type"] = static_cast<int>(params.type);

    // 仅 4 (Joint Planning) 和 5 (Line Planning) 需要 target 字段
    if (params.type == MoveToType::Joint || params.type == MoveToType::Line) {
        
        // 校验：必须至少提供一种坐标
        if (params.target.cp.empty() && params.target.jp.empty()) {
            Response err;
            err.id = id;
            err.error_msg = "MoveTo Failed: Target must contain either 'cp' or 'jp'.";
            return err;
        }

        json target_obj = json::object();

        // 根据是否有值，动态添加字段
        if (!params.target.cp.empty()) {
            target_obj["cp"] = params.target.cp;
        }
        if (!params.target.jp.empty()) {
            target_obj["jp"] = params.target.jp;
        }

        // 始终带上 ep，防止协议解析异常
        target_obj["ep"] = json::array();

        db["target"] = target_obj;
    }

    return sendCommand("Robot/moveTo", db, id);
}

// --- 11.5 RunTo心跳 ---
/** 
 * @brief Send a RunTo heart rate signal every 0.5 seconds to maintain RunTo activity / 每0.5s发送RunTo心跳以保持RunTo活动
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::moveToHeartbeat(int id) {
    return sendCommand("Robot/moveToHeartbeat", json::object(), id);
}

// --- 11.6 设置手动运动倍率 ---
/** 
 * @brief Set the manual motion rate of the robot / 设置机器人的手动运动倍率
 * @param speed Speed [0~100] / 速度
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::setManualSpeedRate(int speed, int id) {
    // 校验：速度倍率必须在 0 ~ 100 之间
    if (speed < 0 || speed > 100) {
        Response resp;
        resp.id = id;
        resp.ty = "Robot/setManualSpeedRate";
        resp.error_msg = "Invalid speed rate: " + std::to_string(speed) + ". Value must be between 0 and 100.";
        return resp;
    }

    return sendCommand("Robot/setManualMoveRate", speed, id);
}

// --- 11.7 设置自动运动倍率 ---
/** 
 * @brief Set the automatic motion rate of the robot / 设置机器人的自动运动倍率
 * @param speed Speed [0~100] / 速度
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::setAutoSpeedRate(int speed, int id) {
    // 校验：速度倍率必须在 0 ~ 100 之间
    if (speed < 0 || speed > 100) {
        Response resp;
        resp.id = id;
        resp.ty = "Robot/setAutoMoveRate";
        resp.error_msg = "Invalid speed rate: " + std::to_string(speed) + ". Value must be between 0 and 100.";
        return resp;
    }

    return sendCommand("Robot/setAutoMoveRate", speed, id);
}

// --- 11.8 运动指令(move) ---
/** 
 * @brief Move the robot along a specified path / 按照指定路径运动机器人
 * @param path Vector of move instructions / 运动指令的向量
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::move(const std::vector<MoveInstruction>& path, int id) {
    json db = json::array();
    for (const auto& inst : path) {
        db.push_back(packInstruction(inst));
    }
    return sendCommand("Robot/move", db, id);
}

// --- 11.8 运动指令(moveJ) ---
/** 
 * @brief Move the robot in joint space with a single instruction / 使用单条指令在关节空间中运动机器人
 * @param inst Move instruction / 运动指令
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::movJ(const MoveInstruction& inst, int id) {
    MoveInstruction tmp = inst; tmp.type = MoveType::movJ;
    return move({tmp}, id);
}

// --- 11.8 运动指令(moveJ) 重载版本 - 直接参数 ---
/** 
 * @brief Move the robot in joint space with specified parameters / 使用指定参数在关节空间中运动机器人
 * @param jp Joint positions / 关节位置
 * @param speed Speed / 速度
 * @param acc Acceleration / 加速度
 * @param coor Coordinate system ID (optional) / 坐标系ID（可选）
 * @param tool Tool ID (optional) / 工具ID（可选）
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::movJ(const JointPoint& target, double speed, double acc,
                                double blend, double relativeBlend,
                                const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    JointPoint joint = target;
    return movJ(MovePoint::Joint(std::move(joint)), speed, acc, blend, relativeBlend, coor, tool, id);
}

Response CodroidController::movJ(const CartesianPoint& target, double speed, double acc,
                                double blend, double relativeBlend,
                                const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    MovePoint point;
    point.cp = target.cp;
    point.rj = target.rj;
    return movJ(point, speed, acc, blend, relativeBlend, coor, tool, id);
}

Response CodroidController::movJ(const MovePoint& target, double speed, double acc,
                                double blend, double relativeBlend,
                                const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    if (target.jp.empty() && target.cp.empty()) {
        Response err;
        err.id = id;
        err.error_msg = "movJ Failed: target must be Joint or Cartesian (jp or cp).";
        return err;
    }
    MoveInstruction inst;
    inst.targetPoint = target;
    inst.speed = speed;
    inst.acc = acc;
    inst.blend = blend;
    inst.relativeBlend = relativeBlend;
    inst.coor = coor;
    inst.tool = tool;
    return movJ(inst, id);
}

// --- 11.8 运动指令(moveL) ---
/** 
 * @brief Move the robot in Cartesian space with a single instruction / 使用单条指令在笛卡尔空间中运动机器人
 * @param inst Move instruction / 运动指令
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::movL(const MoveInstruction& inst, int id) {
    MoveInstruction tmp = inst; tmp.type = MoveType::movL;
    return move({tmp}, id);
}

// --- 11.8 运动指令(moveL) 重载版本 - 直接参数 --
/** 
 * @brief Move the robot in Cartesian space with specified parameters / 使用指定参数在笛卡尔空间中运动机器人
 * @param cp Cartesian position / 笛卡尔位置
 * @param speed Speed / 速度
 * @param acc Acceleration / 加速度
 * @param coor Coordinate system ID (optional) / 坐标系ID（可选）
 * @param tool Tool ID (optional) / 工具ID（可选）
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::movL(const CartesianPoint& target, double speed, double acc,
                                 double blend, double relativeBlend,
                                 const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    MovePoint point;
    point.cp = target.cp;
    point.rj = target.rj;
    return movL(point, speed, acc, blend, relativeBlend, coor, tool, id);
}

Response CodroidController::movL(const JointPoint& target, double speed, double acc,
                                 double blend, double relativeBlend,
                                 const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    JointPoint joint = target;
    return movL(MovePoint::Joint(std::move(joint)), speed, acc, blend, relativeBlend, coor, tool, id);
}

Response CodroidController::movL(const MovePoint& target, double speed, double acc,
                                 double blend, double relativeBlend,
                                 const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    if (target.jp.empty() && target.cp.empty()) {
        Response err;
        err.id = id;
        err.error_msg = "movL Failed: target must be Joint or Cartesian (jp or cp).";
        return err;
    }
    MoveInstruction inst;
    inst.targetPoint = target;
    inst.speed = speed;
    inst.acc = acc;
    inst.blend = blend;
    inst.relativeBlend = relativeBlend;
    inst.coor = coor;
    inst.tool = tool;
    return movL(inst, id);
}


// --- 11.9 暂停运动 ---
/** 
 * @brief Pause the robot's motion / 暂停机器人的运动
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::movC(const MoveInstruction& inst, int id) {
    return move({inst}, id);
}

Response CodroidController::movC(const CartesianPoint& middle, const CartesianPoint& target,
                                double speed, double acc,
                                double blend, double relativeBlend,
                                const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    MoveInstruction inst;
    inst.type = MoveType::movC;
    inst.middlePoint.cp = middle.cp;
    inst.middlePoint.rj = middle.rj;
    inst.targetPoint.cp = target.cp;
    inst.targetPoint.rj = target.rj;
    inst.speed = speed;
    inst.acc = acc;
    inst.blend = blend;
    inst.relativeBlend = relativeBlend;
    inst.coor = coor;
    inst.tool = tool;
    return movC(inst, id);
}

Response CodroidController::movCircle(const MoveInstruction& inst, int id) {
    return move({inst}, id);
}

Response CodroidController::movCircle(const CartesianPoint& middle, const CartesianPoint& target, int circleNum,
                                     double speed, double acc,
                                     double blend, double relativeBlend,
                                     const std::vector<double>& coor, const std::vector<double>& tool, int id) {
    MoveInstruction inst;
    inst.type = MoveType::movCircle;
    inst.circleNum = circleNum;
    inst.middlePoint.cp = middle.cp;
    inst.middlePoint.rj = middle.rj;
    inst.targetPoint.cp = target.cp;
    inst.targetPoint.rj = target.rj;
    inst.speed = speed;
    inst.acc = acc;
    inst.blend = blend;
    inst.relativeBlend = relativeBlend;
    inst.coor = coor;
    inst.tool = tool;
    return movCircle(inst, id);
}

Response CodroidController::pauseMove(int id) {
    return sendCommand("Robot/pause", json::object(), id);
}

// --- 11.10 恢复运动 ---
/** 
 * @brief Resume the robot's motion / 恢复机器人的运动
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::resumeMove(int id) {
    return sendCommand("Robot/resume", json::object(), id);
}

// --- 11.11 停止运动 ---
/** 
 * @brief Stop the robot's motion / 停止机器人的运动
 * @param id Request ID / 请求ID
 * @return Response Standard response / 标准响应
 */
Response CodroidController::stopMove(int id) {
    return sendCommand("Robot/stopMove", json::object(), id);
}

// --- 12.1 上使能 ---
/** 
 * @brief Switch on the robot (enable)
 *        上使能机器人（启用）
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::switchOn(int id) {
    return sendCommand("Robot/switchOn", json::object(), id);
}

// --- 12.2 下使能 ---
/** 
 * @brief Switch off the robot (disable)
 *        下使能机器人（禁用）
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::switchOff(int id) {
    return sendCommand("Robot/switchOff", json::object(), id);
}

// ---12.3 进入手动模式 ---
/** 
 * @brief Enter manual mode
 *        进入手动模式
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::toManual(int id) {
    sendCommand("Robot/toAuto", json::object(), id); // 先切自动，确保状态正确
    return sendCommand("Robot/toManual", json::object(), id);
}

Response CodroidController::toManualDirect(int id) {
    return sendCommand("Robot/toManual", json(""), id);
}

// --- 12.4 进入自动模式 ---
/** 
 * @brief Enter automatic mode
 *        进入自动模式
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::toAuto(int id) {
    return sendCommand("Robot/toAuto", json::object(), id);
}

// --- 12.5 进入远程模式 ---
/** 
 * @brief Enter remote mode
 *        进入远程模式
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::toRemote(int id) {
    sendCommand("Robot/toAuto", json::object(), id); // 先切自动，确保状态正确
    return sendCommand("Robot/toRemote", json::object(), id);
}

Response CodroidController::toRemoteDirect(int id) {
    return sendCommand("Robot/toRemote", json(""), id);
}

// --- 12.7 进入仿真模式 ---
/** 
 * @brief Enter simulation mode
 *        进入仿真模式
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::toSimulation(int id) {
    return sendCommand("Robot/toSimulation", json::object(), id);
}

// --- 12.8 进入实机模式 ---
/** 
 * @brief Enter real machine mode
 *        进入实机模式
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::toActual(int id) {
    return sendCommand("Robot/toActual", json::object(), id);
}

// --- 12.9 进入拖拽模式 ---
/** 
 * @brief Enter drag mode
 *        进入拖拽模式
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::startDrag(int id) {
    return sendCommand("Robot/startDrag", json::object(), id);
}    

// --- 12.10 退出拖拽模式 ---
/** 
 * @brief Exit drag mode
 *        退出拖拽模式
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::stopDrag(int id) {
    return sendCommand("Robot/stopDrag", json::object(), id);
}

// --- 12.11 清除错误 ---
/** 
 * @brief Clear robot errors
 *        清除机器人错误
 * 
 * @param id request ID / 请求 ID
 * @return Response / 响应结果
 */
Response CodroidController::clearError(int id) {
    return sendCommand("System/clearError", json(""), id);
}

Response CodroidController::zeroForceCalibration(int calibrationTimeMs, int id) {
    json db;
    db["calibrationTimeMs"] = calibrationTimeMs;
    return sendCommand("Robot/ZeroForceCalibration", db, id);
}

Response CodroidController::initForceControl(ForceFrame frame,
                                             const std::vector<ForceAxisMode>& axisMode,
                                             const json& compliance,
                                             const json& constantForce,
                                             const std::vector<double>& userFrameRpy,
                                             const std::vector<double>& desiredWrench,
                                             const json& forceLimit,
                                             int id) {
    if (axisMode.size() != 6) {
        Response r;
        r.id = id;
        r.ty = "Robot/initForceControl";
        r.error_msg = "axisMode must contain exactly 6 elements";
        return r;
    }
    json db;
    db["algo"] = static_cast<int>(ForceControlAlgo::Admittance);
    db["frame"] = static_cast<int>(frame);
    db["axisMode"] = json::array();
    for (ForceAxisMode mode : axisMode) {
        db["axisMode"].push_back(static_cast<int>(mode));
    }
    if (!compliance.empty())
        db["compliance"] = compliance;
    if (!constantForce.empty())
        db["constantForce"] = constantForce;
    if (!userFrameRpy.empty())
        db["userFrameRpy"] = userFrameRpy;
    if (!desiredWrench.empty())
        db["desiredWrench"] = desiredWrench;
    if (!forceLimit.empty())
        db["forceLimit"] = forceLimit;
    return sendCommand("Robot/initForceControl", db, id);
}

Response CodroidController::startForceControl(int id) {
    return sendCommand("Robot/startForceControl", json::object(), id);
}

Response CodroidController::stopForceControl(int smoothTimeMs, int id) {
    json db;
    db["smoothTimeMs"] = smoothTimeMs;
    return sendCommand("Robot/stopForceControl", db, id);
}

Response CodroidController::tuneForceParams(const std::vector<double>& stiffness,
                                            const std::vector<double>& damping,
                                            const std::vector<double>& mass,
                                            const std::vector<double>& desiredForce,
                                            const std::vector<double>& kp,
                                            const std::vector<double>& kd,
                                            double rampTime,
                                            int id) {
    json db = json::object();
    if (!stiffness.empty())
        db["stiffness"] = stiffness;
    if (!damping.empty())
        db["damping"] = damping;
    if (!mass.empty())
        db["mass"] = mass;
    if (!desiredForce.empty())
        db["desiredForce"] = desiredForce;
    if (!kp.empty())
        db["kp"] = kp;
    if (!kd.empty())
        db["kd"] = kd;
    if (rampTime >= 0.0)
        db["rampTime"] = rampTime;
    return sendCommand("Robot/tuneForceParams", db, id);
}

Response CodroidController::startContactDetection(const std::vector<double>& direction,
                                                  double feedVelocity,
                                                  double contactForceThreshold,
                                                  double velDropRatio,
                                                  double maxTravel,
                                                  double timeoutMs,
                                                  int id) {
    if (direction.size() != 6) {
        Response r;
        r.id = id;
        r.ty = "Robot/startContactDetection";
        r.error_msg = "direction must contain exactly 6 elements";
        return r;
    }
    json db;
    db["direction"] = direction;
    if (feedVelocity >= 0.0)
        db["feedVelocity"] = feedVelocity;
    if (contactForceThreshold >= 0.0)
        db["contactForceThreshold"] = contactForceThreshold;
    if (velDropRatio >= 0.0)
        db["velDropRatio"] = velDropRatio;
    if (maxTravel >= 0.0)
        db["maxTravel"] = maxTravel;
    if (timeoutMs >= 0.0)
        db["timeoutMs"] = timeoutMs;
    return sendCommand("Robot/startContactDetection", db, id);
}

Response CodroidController::setOverforceProtection(int enable,
                                                   const std::vector<double>& forceThreshold,
                                                   double holdMs,
                                                   int id) {
    if (!forceThreshold.empty() && forceThreshold.size() != 6) {
        Response r;
        r.id = id;
        r.ty = "Robot/setOverforceProtection";
        r.error_msg = "forceThreshold must contain exactly 6 elements";
        return r;
    }
    json db = json::object();
    if (enable >= 0)
        db["enable"] = (enable != 0);
    if (!forceThreshold.empty())
        db["forceThreshold"] = forceThreshold;
    if (holdMs >= 0.0)
        db["holdMs"] = holdMs;
    return sendCommand("Robot/setOverforceProtection", db, id);
}

Response CodroidController::setForceDataHealth(int enable,
                                               double timeoutMs,
                                               double maxPacketLossRatio,
                                               int packetLossWindow,
                                               double forceSaturation,
                                               double torqueSaturation,
                                               int id) {
    json db = json::object();
    if (enable >= 0)
        db["enable"] = (enable != 0);
    if (timeoutMs >= 0.0)
        db["timeoutMs"] = timeoutMs;
    if (maxPacketLossRatio >= 0.0)
        db["maxPacketLossRatio"] = maxPacketLossRatio;
    if (packetLossWindow >= 0)
        db["packetLossWindow"] = packetLossWindow;
    if (forceSaturation >= 0.0)
        db["forceSaturation"] = forceSaturation;
    if (torqueSaturation >= 0.0)
        db["torqueSaturation"] = torqueSaturation;
    return sendCommand("Robot/setForceDataHealth", db, id);
}

ForceControlState CodroidController::getForceState(int id) {
    ForceControlState out;
    Response resp = sendCommand("Robot/getForceState", json(""), id);
    if (!resp.error_msg.empty() || !resp.db.is_object())
        return out;
    const auto& db = resp.db;
    out.enabled = db.value("enabled", false);
    out.pending = db.value("pending", false);
    out.algo = db.value("algo", 0);
    out.valid = db.value("valid", false);
    out.is_contact = db.value("isContact", false);
    out.is_overforce = db.value("isOverforce", false);
    out.health = db.value("health", 0);
    if (db.contains("wrenchTcp") && db["wrenchTcp"].is_array())
        out.wrench_tcp = db["wrenchTcp"].get<std::vector<double>>();
    if (db.contains("wrenchBase") && db["wrenchBase"].is_array())
        out.wrench_base = db["wrenchBase"].get<std::vector<double>>();
    if (db.contains("desiredWrench") && db["desiredWrench"].is_array())
        out.desired_wrench = db["desiredWrench"].get<std::vector<double>>();
    if (db.contains("trackError") && db["trackError"].is_array())
        out.track_error = db["trackError"].get<std::vector<double>>();
    if (db.contains("axisMode") && db["axisMode"].is_array())
        out.axis_mode = db["axisMode"].get<std::vector<int>>();
    return out;
}

// --- 13.1 获取IO状态 ---

/** 
 * @brief Get the values of multiple IOs
 *        获取多个IO的值
 * 
 * @param queryList 查询列表
 * @param id Request ID / 请求ID
 * @return std::vector<IOInfo> IO信息列表
 */
std::vector<IOInfo> CodroidController::getIOValues(const std::vector<IOInfo>& queryList, int id) {
    std::vector<IOInfo> results;
    
    // 1. 构建请求 db 数组
    json db_request = json::array();
    for (const auto& item : queryList) {
        db_request.push_back({{"type", item.type}, {"port", item.port}});
    }

    // 2. 发送指令
    Response resp = sendCommand("IOManager/GetIOValue", db_request, id);

    // 3. 解析响应
    if (resp.error_msg.empty() && resp.db.is_array()) {
        try {
            for (const auto& j_item : resp.db) {
                IOInfo info;
                info.type = j_item.value("type", "");
                info.port = j_item.value("port", 0);
                // 获取 value，支持 int 或 double
                info.value = j_item.value("value", 0.0);
                results.push_back(info);
            }
        } catch (...) {
            // 解析异常处理
        }
    }
    return results;
}

/** 
 * @brief Get the status of a digital input
 *        获取数字输入状态
 * 
 * @param index Input index / 输入索引
 * @param id Request ID / 请求ID
 * @return int Status value / 状态值
 */
int CodroidController::getDI(int port, int id) {
    auto res = getIOValues({IOInfo("DI", port)}, id);
    return res.empty() ? -1 : static_cast<int>(res[0].value);
}

/** 
 * @brief Get the status of a digital output
 *        获取数字输出状态
 * 
 * @param index Output index / 输出索引
 * @param id Request ID / 请求ID
 * @return int Status value / 状态值
 */
int CodroidController::getDO(int port, int id) {
    auto res = getIOValues({IOInfo("DO", port)}, id);
    return res.empty() ? -1 : static_cast<int>(res[0].value);
}

/** 
 * @brief Get the value of an analog input
 *        获取模拟输入值
 * 
 * @param index Input index / 输入索引
 * @param id Request ID / 请求ID
 * @return double Value / 值
 */
double CodroidController::getAI(int port, int id) {
    auto res = getIOValues({IOInfo("AI", port)}, id);
    return res.empty() ? -1.0 : res[0].value;
}

/** 
 * @brief Get the value of an analog output
 *        获取模拟输出值
 * 
 * @param index Output index / 输出索引
 * @param id Request ID / 请求ID
 * @return double Value / 值
 */
double CodroidController::getAO(int port, int id) {
    auto res = getIOValues({IOInfo("AO", port)}, id);
    return res.empty() ? -1.0 : res[0].value;
}

// --- 13.2 设置IO状态 ---
/** 
 * @brief Set the value of an IO
 *        设置IO的值
 * 
 * @param type IO类型
 * @param port IO端口
 * @param value 值
 * @param id 请求ID
 * @return Response 响应结果
 */
Response CodroidController::setIOValue(const std::string& type, int port, double value, int id) {
    nlohmann::json db;
    db["type"] = type;
    db["port"] = port;
    db["value"] = value;

    return sendCommand("IOManager/SetIOValue", db, id);
}

/**
 * @brief Set the value of a digital output
 *        设置数字输出值
 * 
 * @param port Output port / 输出端口
 * @param value Value to set / 要设置的值
 * @param id Request ID / 请求ID
 * @return Response 响应结果
 */
Response CodroidController::setDO(int port, int value, int id) {
    // 可以在这里加一个简单的逻辑校验
    double val = (value != 0) ? 1.0 : 0.0;
    return setIOValue("DO", port, val, id);
}

/** 
 * @brief Set the value of an analog output
 *        设置模拟输出值
 * 
 * @param port Output port / 输出端口
 * @param value Value to set / 要设置的值
 * @param id Request ID / 请求ID
 * @return Response 响应结果
 */
Response CodroidController::setAO(int port, double value, int id) {
    return setIOValue("AO", port, value, id);
}

// --- 14.1 获取寄存器值 ---
/** 
 * @brief Get the values of multiple registers
 *        获取多个寄存器的值
 * 
 * @param addresses Register addresses / 寄存器地址数组
 * @param id Request ID / 请求ID
 * @return std::vector<RegisterInfo> Register values / 寄存器值数组
 */
std::vector<RegisterInfo> CodroidController::getRegisterValues(const std::vector<int>& addresses, int id) {
    std::vector<RegisterInfo> results;
    
    // 发送地址数组
    Response resp = sendCommand("RegisterManager/GetRegisterValue", addresses, id);

    if (resp.error_msg.empty() && resp.db.is_array()) {
        for (const auto& item : resp.db) {
            results.push_back({
                item.value("address", 0),
                item.value("value", 0.0)
            });
        }
    }
    return results;
}

double CodroidController::getRegisterValue(int address, int id) {
    auto res = getRegisterValues({address}, id);
    if (!res.empty()) {
        return res[0].value;
    }
    
    return -1;
}


// --- 14.2 设置寄存器值 ---
/** 
 * @brief Set the value of a register
 *        设置寄存器的值
 * 
 * @param address Register address / 寄存器地址
 * @param value Value to set / 要设置的值
 * @param id Request ID / 请求ID
 * @return Response 响应结果
 */
Response CodroidController::setRegisterValue(int address, double value, int id) {
    nlohmann::json db;
    db["address"] = address;
    db["value"] = value;
    return sendCommand("RegisterManager/SetRegisterValue", db, id);
}

// --- 14.3 设置扩展数组类型 ---
/** 
 * @brief Set the type of an extend array
 *        设置扩展数组的类型
 * 
 * @param index Extend array index (0-999) / 扩展数组索引（0-999）
 * @param type Extend array type / 扩展数组类型
 * @param id Request ID / 请求ID
 * @return Response 响应结果
 */
Response CodroidController::setExtendArrayType(int index, ExtendArrayType type, int id) {
    // 参数校验
    if (index < 0 || index > 999) {
        Response r; r.id = id; r.error_msg = "Index out of range (0-999)";
        return r;
    }

    nlohmann::json db;
    db["index"] = index;
    db["type"] = type; // 自动通过 NLOHMANN_JSON_SERIALIZE_ENUM 转为字符串

    return sendCommand("RegisterManager/setExtendArrayType", db, id);
}

// --- 14.4 删除扩展数组 ---
/** 
 * @brief Remove an extend array
 *        删除一个扩展数组
 * 
 * @param index Extend array index (0-999) / 扩展数组索引（0-999）
 * @param id Request ID / 请求ID
 * @return Response 响应结果
 */
Response CodroidController::removeExtendArray(int index, int id) {
    if (index < 0 || index > 999) {
        Response r; r.id = id; r.error_msg = "Index out of range (0-999)";
        return r;
    }

    nlohmann::json db;
    db["index"] = index;
    return sendCommand("RegisterManager/removeExtendArray", db, id);
}

// --- 17.2 开启数据推送流 ---
/** 
 * @brief Start data push stream
 *        开启数据推送流
 * 
 * @param ip IP address / IP 地址
 * @param port Port / 端口
 * @param duration Data push interval （1000/frequency） / 数据推送间隔
 * @param id Request ID / 请求ID
 * @return Response 响应结果
 */
Response CodroidController::startDataPush(const std::string& ip, int port, int duration, int id, bool highPercision) {
    if (port < 10000 || port > 65535) {
        Response r;
        r.id = id;
        r.error_msg = "Invalid port range (10000-65535)";
        return r;
    }
    if (duration < 1 || duration > 1000) {
        Response r;
        r.id = id;
        r.error_msg = "Duration must be in [1, 1000] ms";
        return r;
    }

    nlohmann::json db;
    db["ip"] = ip;
    db["port"] = port;
    db["duration"] = duration;
    if (highPercision) {
        db["highPercision"] = true;
        db["mask"] = 0xFFFF;
    }

    return sendCommand("CRI/StartDataPush", db, id);
}

// --- 17.3 关闭数据推送流 ---
/** 
 * @brief Stop data push stream
 *        关闭数据推送流
 * 
 * @param id Request ID / 请求ID
 * @return Response 响应结果
 */
Response CodroidController::stopDataPush(int id) {
    Response r = sendCommand("CRI/StopDataPush", nlohmann::json::object(), id);
    // 与 C# `StopCriDataPush` 一致：无论 TCP 成功与否都关闭本机 UDP（`AGENTS.md` / SDK 契约）
    cri_push_active_ = false;
    {
        std::lock_guard<std::mutex> lock(cri_cache_mtx_);
        cri_cache_valid_ = false;
        cri_cache_ = CriInternalCache{};
    }
    stopCriUdpReceiver_();
    return r;
}

Response CodroidController::stopDataPush(const std::string& ip, int port, int id) {
    nlohmann::json db;
    db["ip"] = ip;
    db["port"] = port;
    Response r = sendCommand("CRI/StopDataPush", db, id);
    cri_push_active_ = false;
    {
        std::lock_guard<std::mutex> lock(cri_cache_mtx_);
        cri_cache_valid_ = false;
        cri_cache_ = CriInternalCache{};
    }
    stopCriUdpReceiver_();
    return r;
}

// --- 17.4 开启实时控制 ---
/** 
 * @brief Start real-time control
 *        开启实时控制
 * 
 * @param duration Control cycle (ms) / 控制周期（ms）
 * @param startBuffer Initial command buffer size / 初始指令缓冲区大小
 * @param filterTypeint Filter type (0: no filter, 1: low-pass filter) / 滤波类型（0：不滤波，1：低通滤波）
 * @param id Request ID / 请求ID
 * @return Response 响应结果
 */
Response CodroidController::startControl(int duration, int startBuffer, int filterType, int id){
    // 校验指令间隔 (1-16ms)
    if (duration < 1 || duration > 16) {
        Response r; r.id = id; r.error_msg = "Real-time duration must be [1, 16] ms";
        return r;
    }
    if (1000 % duration != 0) {
        Response r; r.id = id; r.error_msg = "Real-time duration must divide 1000 ms evenly";
        return r;
    }
    if (filterType < 0 || filterType > 3) {
        Response r; r.id = id; r.error_msg = "filterType must be [0, 3]";
        return r;
    }
    // 校验缓冲点 (1-100)
    if (startBuffer < 1 || startBuffer > 100) {
        Response r; r.id = id; r.error_msg = "Start buffer must be [1, 100]";
        return r;
    }

    nlohmann::json db;
    db["filterType"] = static_cast<int>(filterType);
    db["duration"] = duration;
    db["startBuffer"] = startBuffer;

    return sendCommand("CRI/StartControl", db, id);
}

// --- 17.5 关闭实时控制 ---
/** 
 * @brief Stop real-time control
 *        关闭实时控制
 * 
 * @param id Request ID / 请求ID
 * @return Response 响应结果
 */
Response CodroidController::stopControl(int id){
    return sendCommand("CRI/StopControl", json(""), id);
}

// --- 19.1 设置碰撞灵敏度 ---
/** 
 * @brief Set collision sensitivity
 *        设置碰撞灵敏度
 * 
 * @param level Collision sensitivity level (0-100) / 碰撞灵敏度等级（0-100）
 * @param id Request ID / 请求ID
 * @return Response 响应结果
 */
Response CodroidController::setCollisionSensitivity(int level, int id) {
    if (level < 0 || level > 100) {
        Response r; r.id = id; r.error_msg = "Collision sensitivity level must be between 0 and 100";
        return r;
    }
    return sendCommand("Robot/setCollisionSensitivity", level, id);
}

// --- 19.2 设置负载参数 ---
/** 
 * @brief Set payload parameters
 *        设置负载参数
 * 
 * @param weight Payload weight (kg) / 负载重量（kg）
 * @param centerOfGravity Payload center of gravity (x,y,z in mm) / 负载重心（mm，x,y,z）
 * @param id Request ID / 请求ID
 * @return Response 响应结果
 */
Response CodroidController::setPayload(int payloadId, int id) {
    if (payloadId < 0 || payloadId > 15) {
        Response r; r.id = id; r.error_msg = "Payload ID must be between 0 and 15";
        return r;
    }
    return sendCommand("Robot/setPayload", payloadId, id);
}

namespace {

constexpr int kRobotSlotIdMin = 1;
constexpr int kRobotSlotIdMax = 15;

Response make_local_error(int id, const std::string& msg) {
    Response r;
    r.id = id;
    r.error_msg = msg;
    return r;
}

bool is_valid_slot_id(int slot_id) {
    return slot_id >= kRobotSlotIdMin && slot_id <= kRobotSlotIdMax;
}

double json_number(const nlohmann::json& j, const char* key, double fallback = 0.0) {
    if (!j.contains(key))
        return fallback;
    const auto& v = j.at(key);
    if (v.is_number())
        return v.get<double>();
    if (v.is_number_integer())
        return static_cast<double>(v.get<int64_t>());
    return fallback;
}

int json_int(const nlohmann::json& j, const char* key, int fallback = 0) {
    if (!j.contains(key))
        return fallback;
    const auto& v = j.at(key);
    if (v.is_number_integer())
        return static_cast<int>(v.get<int64_t>());
    if (v.is_number())
        return static_cast<int>(v.get<double>());
    return fallback;
}

RobotFrameEntry parse_frame_entry(const nlohmann::json& j) {
    RobotFrameEntry e;
    e.id = json_int(j, "id", 0);
    e.x = json_number(j, "x");
    e.y = json_number(j, "y");
    e.z = json_number(j, "z");
    e.a = json_number(j, "a");
    e.b = json_number(j, "b");
    e.c = json_number(j, "c");
    return e;
}

RobotPayloadEntry parse_payload_entry(const nlohmann::json& j) {
    RobotPayloadEntry e;
    e.id = json_int(j, "id", 0);
    e.m = json_number(j, "m");
    e.mx = json_number(j, "mx");
    e.my = json_number(j, "my");
    e.mz = json_number(j, "mz");
    return e;
}

std::vector<RobotFrameEntry> parse_frame_array(const nlohmann::json& db, const char* key) {
    std::vector<RobotFrameEntry> out;
    if (!db.contains(key) || !db.at(key).is_array())
        return out;
    for (const auto& item : db.at(key)) {
        if (item.is_object())
            out.push_back(parse_frame_entry(item));
    }
    return out;
}

std::vector<RobotPayloadEntry> parse_payload_array(const nlohmann::json& db, const char* key) {
    std::vector<RobotPayloadEntry> out;
    if (!db.contains(key) || !db.at(key).is_array())
        return out;
    for (const auto& item : db.at(key)) {
        if (item.is_object())
            out.push_back(parse_payload_entry(item));
    }
    return out;
}

RobotParameters parse_robot_parameters(const nlohmann::json& db) {
    RobotParameters p;
    p.default_tool_id = json_int(db, "defaultToolId");
    p.default_payload_id = json_int(db, "defaultPayloadId");
    p.default_coordinate_id = json_int(db, "defaultCoordinateId");
    p.max_payload = json_number(db, "maxPayload");
    p.tool = parse_frame_array(db, "Tool");
    p.payload = parse_payload_array(db, "Payload");
    p.coordinate = parse_frame_array(db, "Coordinate");
    return p;
}

nlohmann::json frame_entry_to_json(const RobotFrameEntry& e) {
    return nlohmann::json{{"id", e.id},
                          {"x", e.x},
                          {"y", e.y},
                          {"z", e.z},
                          {"a", e.a},
                          {"b", e.b},
                          {"c", e.c}};
}

nlohmann::json payload_entry_to_json(const RobotPayloadEntry& e) {
    return nlohmann::json{{"id", e.id}, {"m", e.m}, {"mx", e.mx}, {"my", e.my}, {"mz", e.mz}};
}

nlohmann::json frames_to_json(const std::vector<RobotFrameEntry>& frames) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& f : frames)
        arr.push_back(frame_entry_to_json(f));
    return arr;
}

nlohmann::json payloads_to_json(const std::vector<RobotPayloadEntry>& frames) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& f : frames)
        arr.push_back(payload_entry_to_json(f));
    return arr;
}

/** 直接下发前：序号仅允许 1~15，且不可重复。 */
bool validate_direct_tool_frames(const std::vector<RobotFrameEntry>& frames, std::string& err) {
    std::unordered_set<int> seen;
    for (const auto& f : frames) {
        if (!is_valid_slot_id(f.id)) {
            err = "Tool frame id must be between 1 and 15";
            return false;
        }
        if (!seen.insert(f.id).second) {
            err = "Duplicate Tool frame id " + std::to_string(f.id);
            return false;
        }
    }
    return true;
}

bool validate_direct_payload_frames(const std::vector<RobotPayloadEntry>& frames, std::string& err) {
    std::unordered_set<int> seen;
    for (const auto& f : frames) {
        if (!is_valid_slot_id(f.id)) {
            err = "Payload frame id must be between 1 and 15";
            return false;
        }
        if (!seen.insert(f.id).second) {
            err = "Duplicate Payload frame id " + std::to_string(f.id);
            return false;
        }
    }
    return true;
}

bool patch_frame_by_id(std::vector<RobotFrameEntry>& frames, int frame_id, const RobotFrameEntry& patch,
                        std::string& err) {
    for (auto& f : frames) {
        if (f.id == frame_id) {
            f = patch;
            f.id = frame_id;
            return true;
        }
    }
    err = "Frame id " + std::to_string(frame_id) + " not found in controller parameters";
    return false;
}

bool patch_payload_by_id(std::vector<RobotPayloadEntry>& frames, int frame_id, const RobotPayloadEntry& patch,
                         std::string& err) {
    for (auto& f : frames) {
        if (f.id == frame_id) {
            f = patch;
            f.id = frame_id;
            return true;
        }
    }
    err = "Payload id " + std::to_string(frame_id) + " not found in controller parameters";
    return false;
}

} // namespace

Response CodroidController::getRobotParameter(int id) {
    return sendCommand("Robot/GetRobotParameter", nlohmann::json::object(), id);
}

Response CodroidController::saveRobotParameter(const nlohmann::json& db, int id) {
    return sendCommand("Robot/SaveRobotParameter", db, id);
}

Response CodroidController::setDefaultPayloadId(int payloadId, int id) {
    if (!is_valid_slot_id(payloadId))
        return make_local_error(id, "defaultPayloadId must be between 1 and 15");
    nlohmann::json db;
    db["defaultPayloadId"] = payloadId;
    return saveRobotParameter(db, id);
}

Response CodroidController::setDefaultToolId(int toolId, int id) {
    if (!is_valid_slot_id(toolId))
        return make_local_error(id, "defaultToolId must be between 1 and 15");
    nlohmann::json db;
    db["defaultToolId"] = toolId;
    return saveRobotParameter(db, id);
}

Response CodroidController::setDefaultCoordinateId(int coordinateId, int id) {
    if (!is_valid_slot_id(coordinateId))
        return make_local_error(id, "defaultCoordinateId must be between 1 and 15");
    nlohmann::json db;
    db["defaultCoordinateId"] = coordinateId;
    return saveRobotParameter(db, id);
}

Response CodroidController::saveToolFrames(const std::vector<RobotFrameEntry>& frames, int id) {
    std::string err;
    if (!validate_direct_tool_frames(frames, err))
        return make_local_error(id, err);
    nlohmann::json db;
    db["Tool"] = frames_to_json(frames);
    return saveRobotParameter(db, id);
}

Response CodroidController::setToolFrame(int frame_id, const RobotFrameEntry& frame, int id) {
    if (!is_valid_slot_id(frame_id))
        return make_local_error(id, "Tool frame id must be between 1 and 15");

    const int get_id = NextRequestId();
    Response got = getRobotParameter(get_id);
    if (!got.error_msg.empty())
        return got;

    RobotParameters params = parse_robot_parameters(got.db);
    RobotFrameEntry patch = frame;
    patch.id = frame_id;
    std::string err;
    if (!patch_frame_by_id(params.tool, frame_id, patch, err))
        return make_local_error(id, err);

    nlohmann::json db;
    db["Tool"] = frames_to_json(params.tool);
    return saveRobotParameter(db, id);
}

Response CodroidController::savePayloadFrames(const std::vector<RobotPayloadEntry>& frames, int id) {
    std::string err;
    if (!validate_direct_payload_frames(frames, err))
        return make_local_error(id, err);
    nlohmann::json db;
    db["Payload"] = payloads_to_json(frames);
    return saveRobotParameter(db, id);
}

Response CodroidController::setPayloadFrame(int frame_id, const RobotPayloadEntry& frame, int id) {
    if (!is_valid_slot_id(frame_id))
        return make_local_error(id, "Payload frame id must be between 1 and 15");

    const int get_id = NextRequestId();
    Response got = getRobotParameter(get_id);
    if (!got.error_msg.empty())
        return got;

    RobotParameters params = parse_robot_parameters(got.db);
    RobotPayloadEntry patch = frame;
    patch.id = frame_id;
    std::string err;
    if (!patch_payload_by_id(params.payload, frame_id, patch, err))
        return make_local_error(id, err);

    nlohmann::json db;
    db["Payload"] = payloads_to_json(params.payload);
    return saveRobotParameter(db, id);
}

Response CodroidController::setUserCoordinateFrame(int frame_id, const RobotFrameEntry& frame, int id) {
    if (!is_valid_slot_id(frame_id))
        return make_local_error(id, "User coordinate id must be between 1 and 15");

    const int get_id = NextRequestId();
    Response got = getRobotParameter(get_id);
    if (!got.error_msg.empty())
        return got;

    RobotParameters params = parse_robot_parameters(got.db);
    RobotFrameEntry patch = frame;
    patch.id = frame_id;
    std::string err;
    if (!patch_frame_by_id(params.coordinate, frame_id, patch, err))
        return make_local_error(id, err);

    nlohmann::json db;
    db["Coordinate"] = frames_to_json(params.coordinate);
    return saveRobotParameter(db, id);
}

Response CodroidController::saveUserCoordinateFrames(const std::vector<RobotFrameEntry>& frames, int id) {
    std::string err;
    if (!validate_direct_tool_frames(frames, err))
        return make_local_error(id, err);
    nlohmann::json db;
    db["Coordinate"] = frames_to_json(frames);
    return saveRobotParameter(db, id);
}

/**
 * @brief Validate a variable name according to the rules specified by the Codroid server
 *        根据 Codroid 服务器指定的规则验证变量名
 * 
 * @param name 变量名
 * @param outError 输出错误信息
 * @return true 验证通过
 * @return false 验证失败
 */
bool CodroidController::isValidVariableName(const std::string& name, std::string& outError) {
    // 1. 检查是否为空
    if (name.empty()) {
        outError = "Variable name cannot be empty";
        return false;
    }

    // 2. 检查关键字
    if (luaKeywords.find(name) != luaKeywords.end()) {
        outError = "Variable name '" + name + "' is a reserved keyword";
        return false;
    }

    // 3. 检查双下划线开头
    if (name.length() >= 2 && name.substr(0, 2) == "__") {
        outError = "Variable name '" + name + "' cannot start with '__'";
        return false;
    }

    // 4. 基本命名规范检查 (只允许字母数字下划线，且不能数字开头)
    if (std::isdigit(name[0])) {
        outError = "Variable name '" + name + "' cannot start with a digit";
        return false;
    }

    return true;
}

/**
 * @brief Pack a move instruction into a JSON object / 将运动指令打包成 JSON 对象
 * @param inst Move instruction / 运动指令
 * @return json Packed JSON object / 打包后的 JSON 对象
 */
json CodroidController::packInstruction(const MoveInstruction& inst) {
    json j;
    j["type"] = inst.type;
    j["speed"] = inst.speed;
    j["acc"] = inst.acc;

    // 处理 blend / relativeBlend（互斥：同时传入时 relativeBlend 被忽略）
    if (inst.blend >= 0) j["blend"] = inst.blend;
    if (inst.relativeBlend >= 0) j["relativeBlend"] = inst.relativeBlend;

    if (inst.type == MoveType::movCircle) j["circleNum"] = inst.circleNum;

    // 处理 targetPoint
    json tp = json::object();
    // 1. 优先处理 jp
    if (!inst.targetPoint.jp.empty()) {
        tp["jp"] = inst.targetPoint.jp;
    } 
    // 2. 如果没有 jp，处理 cp 和 rj
    else if (!inst.targetPoint.cp.empty()) {
        tp["cp"] = inst.targetPoint.cp;
        // 如果没有 rj，补全默认值 [20,20,20,20,20,20]
        if (inst.targetPoint.rj.empty()) {
            tp["rj"] = std::vector<double>{20, 20, 20, 20, 20, 20};
        } else {
            tp["rj"] = inst.targetPoint.rj;
        }
    }
    j["targetPoint"] = tp;

    // 处理中间点 (movC/Circle)
    if (inst.type == MoveType::movC || inst.type == MoveType::movCircle) {
        json mp = json::object();
        if (!inst.middlePoint.cp.empty()) mp["cp"] = inst.middlePoint.cp;
        if (!inst.middlePoint.rj.empty()) mp["rj"] = inst.middlePoint.rj.empty() ? std::vector<double>(6, 20) : inst.middlePoint.rj;
        j["middlePoint"] = mp;
    }

    // --- 修复后端崩溃 Bug：只有不为空才发 ---
    if (!inst.coor.empty()) j["coor"] = inst.coor;
    if (!inst.tool.empty()) j["tool"] = inst.tool;

    return j;
}

// ==========================================
// 运动学 (Kinematics) 接口实现
// ==========================================

// bool CodroidController::kinematicsInit(const std::vector<std::vector<double>>& dh_matrix) {
//     #ifdef _WIN32
//     // Windows 下暂时返回错误码，不调用底层函数
//     std::cerr << "Kinematics is not supported on Windows yet!" << std::endl;
//     return false; 
//     #endif
    
//     // 1. 安全校验：确保传入的是 6 行 4 列的矩阵
//     if (dh_matrix.size() != 6) {
//         std::cerr << "[Kinematics] Error: DH matrix must have exactly 6 rows." << std::endl;
//         return false;
//     }

//     // 2. 将 C++ 的 std::vector 转换为 C 语言需要的二维数组 double[6][4]
//     double dh_c_array[6][4];
//     for (int i = 0; i < 6; ++i) {
//         if (dh_matrix[i].size() != 4) {
//             std::cerr << "[Kinematics] Error: Row " << i << " in DH matrix must have exactly 4 columns." << std::endl;
//             return false;
//         }
//         for (int j = 0; j < 4; ++j) {
//             dh_c_array[i][j] = dh_matrix[i][j];
//         }
//     }

//     // 3. 调用底层的 C 函数进行初始化
//     init(dh_c_array);

//     // 4. 标记为已初始化
//     isKinematicsModelInited_ = true;
//     return true;
// }

// bool CodroidController::kinematicsInit_mm_deg(const std::vector<std::vector<double>>& dh_matrix) {
//     #ifdef _WIN32
//     // Windows 下暂时返回错误码，不调用底层函数
//     std::cerr << "Kinematics is not supported on Windows yet!" << std::endl;
//     return false; 
//     #endif
    
//     if (dh_matrix.size() != 6) {
//         std::cerr << "[Kinematics] Error: DH matrix must have 6 rows." << std::endl;
//         return false;
//     }

//     double dh_c_array[6][4];
//     for (int i = 0; i < 6; ++i) {
//         if (dh_matrix[i].size() != 4) return false;
        
//         // DH 参数转换：假设标准顺序为 [a, alpha, d, theta]
//         // a 和 d 是长度 (mm -> m), alpha 和 theta 是角度 (deg -> rad)
//         dh_c_array[i][0] = dh_matrix[i][0] * MM_TO_M;  // a (mm -> m)
//         dh_c_array[i][1] = dh_matrix[i][1] * DEG_TO_RAD; // alpha (deg -> rad)
//         dh_c_array[i][2] = dh_matrix[i][2] * MM_TO_M;  // d (mm -> m)
//         dh_c_array[i][3] = dh_matrix[i][3] * DEG_TO_RAD; // theta (deg -> rad)
//     }

//     init(dh_c_array);
//     isKinematicsModelInited_ = true;
//     return true;
// }

// int CodroidController::kinematicsFk(const std::vector<double>& qIn, 
//                                           const std::vector<double>& toolParam, 
//                                           std::vector<double>& tcpPosOut) {
//     // 1. 状态校验
//     if (!isKinematicsModelInited_) {
//         std::cerr << "[Kinematics] Error: Model is not initialized. Call kinematicsInit first!" << std::endl;
//         return Model_Not_Inited; // 返回定义在 robotModel.h 中的错误码 1
//     }

//     // 2. 参数长度校验 (防止数组越界段错误)
//     if (qIn.size() != 6 || toolParam.size() != 6) {
//         std::cerr << "[Kinematics] Error: Input vectors (qIn, toolParam) must have exactly 6 elements." << std::endl;
//         return -1; 
//     }

//     // 3. 准备 C 风格的输入数组
//     double qIn_c[6], toolParam_c[6], pOut_c[6] = {0.0};
//     for (int i = 0; i < 6; ++i) {
//         qIn_c[i] = qIn[i] * DEG_TO_RAD; // 关节角 deg -> rad
        
//         // toolParam 通常是 [x, y, z, rx, ry, rz]
//         if (i < 3) toolParam_c[i] = toolParam[i] * MM_TO_M;   // 偏移距离 mm -> m
//         else       toolParam_c[i] = toolParam[i] * DEG_TO_RAD; // 偏移角度 deg -> rad
//     }

//     // 4. 调用底层正解算法
//     #ifdef _WIN32
//     // Windows 下暂时返回错误码，不调用底层函数
//     std::cerr << "Kinematics is not supported on Windows yet!" << std::endl;
//     return -1; 
//     #else
//         // Linux 下才调用真实的 .so 函数
//         int errCode = jntPosToTcpPos(qIn_c, toolParam_c, pOut_c);
//     #endif
    

//     // 5. 如果没有错误，将结果写回传出参数
//     if (errCode == No_Error) {
//         tcpPosOut.resize(6);
//         for (int i = 0; i < 6; ++i) {
//             double rawVal = (i < 3) ? (pOut_c[i] * M_TO_MM) : (pOut_c[i] * RAD_TO_DEG);
            
//             // 【数值格式化】：保留 3 位小数
//             tcpPosOut[i] = std::round(rawVal * 1000.0) / 1000.0;
//         }
        
//     }

//     return errCode;
// }

// int CodroidController::kinematicsIk(const std::vector<double>& tcpPosIn, 
//                                           const std::vector<double>& toolParam, 
//                                           const std::vector<double>& qRef, 
//                                           std::vector<double>& qOut) {
//     // 1. 状态校验
//     if (!isKinematicsModelInited_) {
//         std::cerr << "[Kinematics] Error: Model is not initialized. Call kinematicsInit first!" << std::endl;
//         return Model_Not_Inited;
//     }

//     // 2. 参数长度校验
//     if (tcpPosIn.size() != 6 || toolParam.size() != 6 || qRef.size() != 6) {
//         std::cerr << "[Kinematics] Error: Input vectors (tcpPosIn, toolParam, qRef) must have exactly 6 elements." << std::endl;
//         return -1;
//     }

//     // 3. 准备 C 风格的输入数组
//     double tcpPosIn_c[6], toolParam_c[6], qRef_c[6], qOut_c[6] = {0.0};
//     for (int i = 0; i < 6; ++i) {
//         // TCP 输入位姿转换
//         if (i < 3) tcpPosIn_c[i] = tcpPosIn[i] * MM_TO_M;   // mm -> m
//         else       tcpPosIn_c[i] = tcpPosIn[i] * DEG_TO_RAD; // deg -> rad

//         // 工具参数转换
//         if (i < 3) toolParam_c[i] = toolParam[i] * MM_TO_M;   // mm -> m
//         else       toolParam_c[i] = toolParam[i] * DEG_TO_RAD; // deg -> rad

//         // 参考关节角转换
//         qRef_c[i] = qRef[i] * DEG_TO_RAD; // deg -> rad
//     }

//     // 4. 调用底层逆解算法
//     #ifdef _WIN32
//     // Windows 下暂时返回错误码，不调用底层函数
//     std::cerr << "Kinematics is not supported on Windows yet!" << std::endl;
//     return -1; 
//     #else
//         // Linux 下才调用真实的 .so 函数
//         int errCode = tcpPosToJntPos(tcpPosIn_c, toolParam_c, qRef_c, qOut_c);
//     #endif

//     // 5. 如果没有错误，将结果写回传出参数
//     if (errCode == No_Error) {
//         qOut.resize(6);
//         for (int i = 0; i < 6; ++i) {
//             double rawAngle = qOut_c[i] * RAD_TO_DEG;
            
//             // 【数值格式化】：保留 3 位小数
//             qOut[i] = std::round(rawAngle * 1000.0) / 1000.0;
//         }
//     }

//     return errCode;
// }

// ==========================================
// 运动学 (Kinematics) 接口实现
// ==========================================

bool CodroidController::kinematicsInit(const std::vector<std::vector<double>>& dh_matrix) {
#ifdef _WIN32
    std::cerr << "[Kinematics] Error: Kinematics is not supported on Windows." << std::endl;
    return false;
#else
    if (dh_matrix.size() != 6) return false;

    double dh_c_array[6][4];
    for (int i = 0; i < 6; ++i) {
        if (dh_matrix[i].size() != 4) return false;
        for (int j = 0; j < 4; ++j) dh_c_array[i][j] = dh_matrix[i][j];
    }

    init(dh_c_array); // 只有 Linux 下才会看到这行调用
    isKinematicsModelInited_ = true;
    return true;
#endif
}

bool CodroidController::kinematicsInit_mm_deg(const std::vector<std::vector<double>>& dh_matrix) {
#ifdef _WIN32
    std::cerr << "[Kinematics] Error: Kinematics is not supported on Windows." << std::endl;
    return false;
#else
    if (dh_matrix.size() != 6) return false;

    double dh_c_array[6][4];
    for (int i = 0; i < 6; ++i) {
        if (dh_matrix[i].size() != 4) return false;
        dh_c_array[i][0] = dh_matrix[i][0] * MM_TO_M;
        dh_c_array[i][1] = dh_matrix[i][1] * DEG_TO_RAD;
        dh_c_array[i][2] = dh_matrix[i][2] * MM_TO_M;
        dh_c_array[i][3] = dh_matrix[i][3] * DEG_TO_RAD;
    }

    init(dh_c_array);
    isKinematicsModelInited_ = true;
    return true;
#endif
}

int CodroidController::kinematicsFk(const std::vector<double>& qIn, 
                                          const std::vector<double>& toolParam, 
                                          std::vector<double>& tcpPosOut) {
#ifdef _WIN32
    std::cerr << "[Kinematics] Error: Kinematics is not supported on Windows." << std::endl;
    return -1;
#else
    if (!isKinematicsModelInited_) return Model_Not_Inited;
    if (qIn.size() != 6 || toolParam.size() != 6) return -1;

    double qIn_c[6], toolParam_c[6], pOut_c[6] = {0.0};
    for (int i = 0; i < 6; ++i) {
        qIn_c[i] = qIn[i] * DEG_TO_RAD;
        if (i < 3) toolParam_c[i] = toolParam[i] * MM_TO_M;
        else       toolParam_c[i] = toolParam[i] * DEG_TO_RAD;
    }

    int errCode = jntPosToTcpPos(qIn_c, toolParam_c, pOut_c);

    if (errCode == No_Error) {
        tcpPosOut.resize(6);
        for (int i = 0; i < 6; ++i) {
            double rawVal = (i < 3) ? (pOut_c[i] * M_TO_MM) : (pOut_c[i] * RAD_TO_DEG);
            tcpPosOut[i] = std::round(rawVal * 1000.0) / 1000.0;
        }
    }
    return errCode;
#endif
}

int CodroidController::kinematicsIk(const std::vector<double>& tcpPosIn, 
                                          const std::vector<double>& toolParam, 
                                          const std::vector<double>& qRef, 
                                          std::vector<double>& qOut) {
#ifdef _WIN32
    std::cerr << "[Kinematics] Error: Kinematics is not supported on Windows." << std::endl;
    return -1;
#else
    if (!isKinematicsModelInited_) return Model_Not_Inited;
    if (tcpPosIn.size() != 6 || toolParam.size() != 6 || qRef.size() != 6) return -1;

    double tcpPosIn_c[6], toolParam_c[6], qRef_c[6], qOut_c[6] = {0.0};
    for (int i = 0; i < 6; ++i) {
        if (i < 3) tcpPosIn_c[i] = tcpPosIn[i] * MM_TO_M;
        else       tcpPosIn_c[i] = tcpPosIn[i] * DEG_TO_RAD;
        if (i < 3) toolParam_c[i] = toolParam[i] * MM_TO_M;
        else       toolParam_c[i] = toolParam[i] * DEG_TO_RAD;
        qRef_c[i] = qRef[i] * DEG_TO_RAD;
    }

    int errCode = tcpPosToJntPos(tcpPosIn_c, toolParam_c, qRef_c, qOut_c);

    if (errCode == No_Error) {
        qOut.resize(6);
        for (int i = 0; i < 6; ++i) {
            double rawAngle = qOut_c[i] * RAD_TO_DEG;
            qOut[i] = std::round(rawAngle * 1000.0) / 1000.0;
        }
    }
    return errCode;
#endif
}

} // namespace Codroid
