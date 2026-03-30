/******************************************************************************
 *                                                                            *
 *  @file     data.h                                                          *
 *  @brief    Common definitions for the whole project.                       *
 *                                                                            *
 *  @authors  Qiang Wu                                                        *
 *  @version  1.0.0                                                           *
 *  @date     2026/3/17                                                      *
 *  @license  Commercial                                                      *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *  Change History                                                            *
 *----------------------------------------------------------------------------*
 *  <Date>     | <Version> | <Author>      | <Description>                    *
 *----------------------------------------------------------------------------*
 *  2026/3/17  | 1.0.0     | Qiang Wu      | Create file                      *
 *----------------------------------------------------------------------------*
 *                                                                            *
 ******************************************************************************/
#pragma once

#define No_Error          0  // 无错误
#define Model_Not_Inited  1  // 模型未初始化
#define Shoulder_Singular 2  // 肩关节奇异
#define Elbow_Singular    3  // 肘关节奇异
#define Wrist_Singular    4  // 腕关节奇异
#define Mode_Is_Invalid   5  // 构型无效
#define Is_Out_Of_Range   6  // 点位超出机器人可达空间
#define IkIs_Not_Exist    7  // 逆解不存在

#ifdef __cplusplus

extern "C" {

#endif

/**
 * @brief 初始化
 *
 * @param dh 改进DH参数，按照a(单位：m) alpha(单位：rad) d(单位：m) theta(单位：rad)的顺序排列
 * 
 */
void init(double dh[6][4]);

/**
 * @brief 正解 
 *
 * @param qIn 关节角度(弧度)
 * 
 * @param toolParam 工具参数(相对末端法兰),依次表示x(单位：m),y(单位：m),z(单位：m)位置偏移和a(单位：rad),b(单位：rad),c(单位：rad)姿态偏移。其中abc代表绕定轴的rpy
 * 
 * @param pOut tcp位姿(相对机器人基座标系),依次表示x(单位：m),y(单位：m),z(单位：m)位置和a(单位：rad),b(单位：rad),c(单位：rad)姿态。其中abc代表绕定轴的rpy
 */
int jntPosToTcpPos(const double qIn[6], const double toolParam[6], double pOut[6]);

/**
 * @brief 逆解
 *
 * @param pIn tcp位姿(相对机器人基座标系),依次表示x(单位：m),y(单位：m),z(单位：m)位置和a(单位：rad),b(单位：rad),c(单位：rad)姿态。其中abc代表绕定轴的rpy
 * 
 * @param toolParam 工具参数(相对末端法兰),依次表示x(单位：m),y(单位：m),z(单位：m)位置偏移和a(单位：rad),b(单位：rad),c(单位：rad)姿态偏移。其中abc代表绕定轴的rpy
 * 
 * @param qRef 参考关节角度(弧度)
 * 
 * @param qOut 关节角度(弧度)
 */
int tcpPosToJntPos(const double pIn[6], const double toolParam[6], const double qRef[6], double qOut[6]);

#ifdef __cplusplus

}

#endif