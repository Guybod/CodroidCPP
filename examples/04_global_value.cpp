/**
 * @file 04_global_value.cpp
 * @brief 全局变量：读取 / 保存 / 删除。
 *
 * Variable 模板构造会用 nlohmann 把值序列化进 val；字符串类型按协议原样写入。
 * 仅需：#include "Codroid/client.hpp"
 */
#include "Codroid/client.hpp"

#include <chrono>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace {

void print_result(const char* action, const Codroid::CommandResult& r) {
    if (r.Ok()) {
        std::cout << action << " 成功\n";
    } else {
        std::cerr << action << " 失败: " << r.error_msg << "\n";
    }
}

/** 美化打印控制器返回的 db JSON */
void print_json(const char* label, const nlohmann::json& j) {
    std::cout << label << ":\n" << j.dump(2) << "\n";
}

}  // namespace

int main() {
    Codroid::InitConsoleUtf8();

    const std::string robot_ip = "192.168.1.136";

    Codroid::CodroidClient robot;
    if (!robot.Connect(robot_ip)) {
        std::cerr << "连接失败\n";
        return 1;
    }

    // 保存前先看一眼当前全局变量
    print_json("保存前 GetGlobalVars", robot.GetGlobalVars(robot.NextRequestId()));

    // 合法变量：整型 / 浮点 / 字符串 / 数组 / 对象
    std::map<std::string, Codroid::Variable> vars;
    vars["v991"] = Codroid::Variable(100, "这是一个整数");
    vars["v992"] = Codroid::Variable(90.4, "这是一个浮点数");
    vars["Test_str"] = Codroid::Variable("Hello Codroid!", "这是一个字符串");
    vars["v993"] = Codroid::Variable(nlohmann::json::array({1, 2, 3, 4, 5}), "这是一个列表");
    vars["v994"] = Codroid::Variable(nlohmann::json{{"aaa", 100}}, "这是一个键值对");

    // 非法变量名应由 SDK 拦截（双下划线前缀、与关键字冲突等）
    std::map<std::string, Codroid::Variable> bad1{{"__v991", Codroid::Variable(100, "")}};
    std::map<std::string, Codroid::Variable> bad2{{"movJ", Codroid::Variable(100, "")}};
    print_result("保存非法名(__前缀)", robot.SaveGlobalVars(bad1, robot.NextRequestId()));
    print_result("保存非法名(关键字)", robot.SaveGlobalVars(bad2, robot.NextRequestId()));

    print_result("保存合法变量", robot.SaveGlobalVars(vars, robot.NextRequestId()));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    print_json("保存后 GetGlobalVars", robot.GetGlobalVars(robot.NextRequestId()));

    // 清理本示例写入的变量
    print_result("删除变量",
                 robot.RemoveGlobalVars({"v991", "v992", "v993", "v994", "Test_str"},
                                        robot.NextRequestId()));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    print_json("删除后 GetGlobalVars", robot.GetGlobalVars(robot.NextRequestId()));

    robot.Disconnect();
    std::cout << "完成。\n";
    return 0;
}
