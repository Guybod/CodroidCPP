/**
 * @file CodroidDefine.h
 * @brief SDK 内部类型：在公开 `types.hpp` 之上叠加 nlohmann JSON 编解码与 TCP Response。
 * @warning 本头仅供 SDK 实现（CodroidController 等）使用，不进入客户发布包。
 */

#ifndef CODROID_DEFINE_H
#define CODROID_DEFINE_H

#include "Codroid/types.hpp"

#include <functional>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace Codroid {
    using json = nlohmann::json;

    /**
     * @brief 单条 TCP JSON 指令的解析结果（与控制器下行帧字段对应）。
     */
    struct Response {
        int id;                ///< 与请求一致的序号
        std::string ty;        ///< 路由类型，如 `Robot/move`、`IO/getValues`
        json db;               ///< 业务载荷 `db` 字段（已解析为 JSON）
        std::string error_msg; ///< 非空表示失败（来自控制器 `err`）
        std::string raw_json;  ///< 最近一次完整响应 JSON 字符串
        Response() : id(0), db(json::object()) {}
    };

    NLOHMANN_JSON_SERIALIZE_ENUM(CoorType, {
        {CoorType::Tool, "tool"}, {CoorType::User, "user"}
    })

    NLOHMANN_JSON_SERIALIZE_ENUM(MoveType, {
        {MoveType::movJ, "movJ"}, {MoveType::movL, "movL"},
        {MoveType::movC, "movC"}, {MoveType::movCircle, "movCircle"}
    })

    NLOHMANN_JSON_SERIALIZE_ENUM(MoveToType, {
        {MoveToType::Stop, -1}, {MoveToType::Home, 0}, {MoveToType::Safe, 1}, {MoveToType::Candle, 2},
        {MoveToType::Packing, 3}, {MoveToType::Joint, 4}, {MoveToType::Line, 5}, {MoveToType::ResumePoint, 6}
    })

    NLOHMANN_JSON_SERIALIZE_ENUM(ExtendArrayType, {
        {ExtendArrayType::Bool, "Bool"}, {ExtendArrayType::UInt8, "UInt8"}, {ExtendArrayType::Int8, "Int8"},
        {ExtendArrayType::UInt16, "UInt16"}, {ExtendArrayType::Int16, "Int16"}, {ExtendArrayType::UInt32, "UInt32"},
        {ExtendArrayType::Int32, "Int32"}, {ExtendArrayType::Float32, "Float32"}
    })

    NLOHMANN_JSON_SERIALIZE_ENUM(JogMode, {
        {JogMode::Joint, 1}, {JogMode::Line, 2}
    })

    /**
     * @brief 主题推送内容（与 C# PublishNotification 对齐：ty / db / 原始 JSON）。
     */
    struct PublishNotification {
        std::string ty;
        nlohmann::json db;
        std::string raw_json;
    };

    using PublishTopicHandler = std::function<void(const PublishNotification&)>;

    /**
     * @brief 订阅句柄：析构或 Dispose 时仅移除本地回调，不向控制器发退订（与 C# 一致）。
     */
    class PublishTopicSubscription {
        std::function<void()> dispose_;

    public:
        PublishTopicSubscription() = default;
        explicit PublishTopicSubscription(std::function<void()> d) : dispose_(std::move(d)) {}

        ~PublishTopicSubscription() { reset(); }

        PublishTopicSubscription(const PublishTopicSubscription&) = delete;
        PublishTopicSubscription& operator=(const PublishTopicSubscription&) = delete;

        PublishTopicSubscription(PublishTopicSubscription&& o) noexcept : dispose_(std::move(o.dispose_)) {
            o.dispose_ = nullptr;
        }

        PublishTopicSubscription& operator=(PublishTopicSubscription&& o) noexcept {
            if (this != &o) {
                reset();
                dispose_ = std::move(o.dispose_);
                o.dispose_ = nullptr;
            }
            return *this;
        }

        void Dispose() { reset(); }

    private:
        void reset() {
            if (dispose_) {
                auto f = std::move(dispose_);
                dispose_ = nullptr;
                f();
            }
        }
    };

    /** @brief 远程脚本运行参数（内部；客户 API 用 `std::string` vars JSON）。 */
    struct RunScriptParams {
        std::string mainCode;
        std::unordered_map<std::string, std::string> subThreads;
        std::unordered_map<std::string, std::string> subPrograms;
        std::unordered_map<std::string, std::string> interrupts;
        json vars = json::object();

        explicit RunScriptParams(const std::string& main) : mainCode(main) {}
    };

} // namespace Codroid

#endif
