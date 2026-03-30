#include <iostream>
#include <iomanip> // 用于美化输出
#include <thread>
#include <chrono>
#include "Codroid/CodroidControlInterface.h"

void globalVal_test(Codroid::CodroidControlInterface& robot) {
    // 测试获取全局变量列表接口
    std::cout << "Getting Global Variables..." << std::endl;
    auto resVars = robot.getGlobalVars(103);
    Codroid::CodroidControlInterface::printResponse(resVars);

    // 测试保存全局变量接口
    std::map<std::string, Codroid::Variable> myVars;
    std::map<std::string, Codroid::Variable> myfailedVars1; // 用于测试非法变量名
    std::map<std::string, Codroid::Variable> myfailedVars2; // 用于测试非法变量名

    // 1. 保存整数
    myVars["v991"] = Codroid::Variable(100, "这是一个整数");

    // 2. 保存浮点数
    myVars["v992"] = Codroid::Variable(90.4, "这是一个浮点数");

    // 3. 保存字符串 (注意：根据协议示例，字符串可能需要带转义引号)
    myVars["Test_str"] = Codroid::Variable("Hello Codroid!", "这是一个字符串");

    // 4. 保存列表/数组 (直接使用 nlohmann::json 语法)
    myVars["v993"] = Codroid::Variable(nlohmann::json::array({1, 2, 3, 4, 5}), "这是一个列表");

    // 5. 保存键值对/对象
    nlohmann::json obj = {{"aaa", 100}};
    myVars["v994"] = Codroid::Variable(obj, "这是一个键值对");

    // 6. 测试变量命名规则 (以下是非法的示例，非法的示例会被 SDK 内部拦截并返回错误)
    myfailedVars1["__v991"] = Codroid::Variable(100, "");
    myfailedVars2["movJ"] = Codroid::Variable(100, "");

    // 发送请求保存全局变量

    // 注意：非法变量会被 SDK 内部拦截并返回错误，合法变量会成功保存
    auto res1 = robot.saveGlobalVars(myfailedVars1, 123);
    Codroid::CodroidControlInterface::printResponse(res1); // 预期失败，返回错误信息
    auto res2 = robot.saveGlobalVars(myfailedVars2, 123);
    Codroid::CodroidControlInterface::printResponse(res2); // 预期失败，返回错误信息

    auto res = robot.saveGlobalVars(myVars, 123);
    
    if (res.error_msg.empty()) {
        std::cout << "Global variables saved successfully!" << std::endl;
    }
    Codroid::CodroidControlInterface::printResponse(res);   
    // 等待一段时间再获取变量
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // 测试获取全局变量列表接口
    std::cout << "Getting Global Variables..." << std::endl;
    auto resVars2 = robot.getGlobalVars(103);
    Codroid::CodroidControlInterface::printResponse(resVars2);

    // 删除全局变量
    std::vector<std::string> varsToDelete = {"v991", "v992", "v993", "v994"};
    auto resDel = robot.removeGlobalVars(varsToDelete, 124);
    Codroid::CodroidControlInterface::printResponse(resDel);
    // 等待一段时间再获取变量
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    // 测试获取全局变量列表接口
    std::cout << "Getting Global Variables..." << std::endl;
    auto resVars3 = robot.getGlobalVars(103);
    Codroid::CodroidControlInterface::printResponse( resVars3);
}

int main() {
    Codroid::CodroidControlInterface robot;
    std::string robot_ip = "192.168.1.136"; // 替换为实际的机器人 IP 地址

    if (!robot.connect(robot_ip)) {
        std::cerr << "Failed to connect to robot." << std::endl;
        return -1;
    }
    std::cout << "Connected to robot successfully!" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "Starting global variable test..." << std::endl;
    globalVal_test(robot);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    robot.disconnect();
    return 0;
}