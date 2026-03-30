#include <stdio.h>
#include "robotModel.h"

int main() {
    double pi = 3.141592654;
    double pi_2 = 0.5 * pi;
    double dh[6][4]{
        {0,     0,     0.18,   0    },
        {0,     -pi_2, 0.182,  -pi_2},
        {0.425, 0,     -0.155, 0    },
        {0.330, pi,    -0.164, -pi_2},
        {0,     pi_2,  -0.164, 0    },
        {0,     pi_2,  0.1665, 0    }
    };
    double qIn[6]       = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    double qRef[6]      = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    double qOut[6]      = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double toolParam[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double pIn[6]       = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    double pOut[6]      = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    init(dh);
    auto err = jntPosToTcpPos(qIn, toolParam, pOut);
    if (err != No_Error) {
        return err;
    }
    printf("qIn:%f,%f,%f,%f,%f,%f\n", qIn[0], qIn[1], qIn[2], qIn[3], qIn[4], qIn[5]);
    printf("pOut:%f,%f,%f,%f,%f,%f\n", pOut[0], pOut[1], pOut[2], pOut[3], pOut[4], pOut[5]);
    for (int i = 0; i < 6; i++) {
        pIn[i] = pOut[i];
    }
    err = tcpPosToJntPos(pIn, toolParam, qRef, qOut);
    if (err != No_Error) {
        return err;
    }
    printf("pIn:%f,%f,%f,%f,%f,%f\n", pIn[0], pIn[1], pIn[2], pIn[3], pIn[4], pIn[5]);
    printf("qOut:%f,%f,%f,%f,%f,%f\n", qOut[0], qOut[1], qOut[2], qOut[3], qOut[4], qOut[5]);
    return 0;
}