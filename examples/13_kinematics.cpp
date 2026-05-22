#include "../include/Codroid/CodroidController.h"
#include "Codroid/console_utf8.hpp"
#include <vector>
#include <iostream>

int main() {
    Codroid::InitConsoleUtf8();
    double pi = 3.141592654;
    double pi_2 = 0.5 * pi;
    // S5-90-ECO-V2
    std::vector<std::vector<double>> dh = {
        {0,     0,     0.18,   0    },
        {0,     -pi_2, 0.182,  -pi_2},
        {0.425, 0,     -0.155, 0    },
        {0.330, pi,    -0.164, -pi_2},
        {0,     pi_2,  -0.164, 0    },
        {0,     pi_2,  0.161, 0    }
    };

    // 1. 直接通过类名调用静态方法初始化
    if (!Codroid::CodroidController::kinematicsInit(dh)) {
        std::cerr << "Init failed!" << std::endl;
        return -1;
    }

    std::vector<double> qIn = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::vector<double> toolParam = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    std::vector<double> tcpPosOut;

    // 2. 直接通过类名调用静态方法做正解
    int errFk = Codroid::CodroidController::kinematicsFk(qIn, toolParam, tcpPosOut);
    if (errFk == 0) {
        std::cout << "FK Success! X: " << tcpPosOut[0] << std::endl;
        std::cout << "FK Success! Y: " << tcpPosOut[1] << std::endl;
        std::cout << "FK Success! Z: " << tcpPosOut[2] << std::endl;
        std::cout << "FK Success! A: " << tcpPosOut[3] << std::endl;
        std::cout << "FK Success! B: " << tcpPosOut[4] << std::endl;
        std::cout << "FK Success! C: " << tcpPosOut[5] << std::endl;
    }

    return 0;
}