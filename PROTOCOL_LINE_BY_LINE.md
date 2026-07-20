# 协议逐条对照清单（Read One, Write One）

使用方式：每读协议原文一条，就在本表补一条，填写“协议原文摘录 / SDK 实现 / 差异”。

**固件要求（本 C++ SDK）**：对外封装的 **全部接口** 均要求控制器固件 **≥ 2.3.3.43**（常量 `MinControllerFirmware`）。下表「备注」中若仍出现更早协议文档版本号，以本行为准。

| 协议章节 | ty | C# API | 协议原文摘录 | db字段约束 | 返回/错误要点 | 对齐状态 | 备注 |
|---|---|---|---|---|---|---|---|
| 1.1/1.2/1.3/1.4 | 通用 | `FutureTcpClient.SendCommand` / `CommonResponse` | TCP/UDP；主接口 9001；JSON UTF-8；检查 `err`；心跳维持；通用帧 `{id,ty,db}` | `id` 可数字或字符串（原文） | 无数据时可仅回 `id,ty` | 部分对齐 | C# 当前 `id` 仅按 `int` 处理，且默认超时 10s |
| 2.1 | `project/runScript` | `RunScript(...)` | 客户端直接发送脚本运行 | `db.scripts` 必填；`vars` 可选 | 成功仅回 `id,ty` | 已对齐 | 支持 `main/subThreads/subPrograms/interrupts/vars` |
| 2.2 | `project/enterRemoteScriptMode` | `EnterRemoteScriptMode()` | 进入远程脚本模式 | 无 `db` | 成功仅回 `id,ty` | 已对齐 | `ty` 已按原文改为 `project/enterRemoteScriptMode` |
| 2.3 | `project/run` | `Run(projectID)` | 运行指定工程ID | `db={id:"工程ID"}` | 工程ID错误可能不返回错误 | 已对齐 | 已知协议特性：错ID可能静默 |
| 2.4 | `project/runByIndex` | `RunByIndex(index)` | 按映射索引运行 | `db` 为 int | 成功仅回 `id,ty` | 已对齐 | — |
| 2.5 | `project/runStep` | `RunStep(projectID)` | 单步运行；运行中可不传 id | `db.id` 可选（按原文语义） | 成功仅回 `id,ty` | 已对齐（实现策略） | SDK 默认强制传 `projectID`，兼容首次启动场景 |
| 2.6 | `project/pause` | `PauseProject()` | 暂停工程 | 无 `db` | 成功仅回 `id,ty` | 已对齐 | — |
| 2.7 | `project/resume` | `ResumeProject()` | 恢复工程 | 无 `db` | 成功仅回 `id,ty` | 已对齐 | — |
| 2.8 | `project/stop` | `StopProject()` | 停止工程 | 无 `db` | 成功仅回 `id,ty` | 已对齐 | — |
| 2.9 | `project/setBreakpoint` | 未实现 | 设置断点（暂时无法使用） | `db={脚本id:[行号...]}` | 成功仅回 `id,ty` | 未实现/可忽略 | 原文标注“暂时无法使用” |
| 2.10 | `project/addBreakpoint` | 未实现 | 增加断点（暂时无法使用） | 同上 | 成功仅回 `id,ty` | 未实现/可忽略 | 原文标注“暂时无法使用” |
| 2.11 | `project/removeBreakpoint` | 未实现 | 删除断点（暂时无法使用） | 同上 | 成功仅回 `id,ty` | 未实现/可忽略 | 原文标注“暂时无法使用” |
| 2.12 | `project/clearBreakpoint` | 未实现 | 清除所有断点（暂时无法使用） | 无 `db` | 成功仅回 `id,ty` | 未实现/可忽略 | 原文标注“暂时无法使用” |
| 2.13 | `project/setStartLine` | 未实现 | 设置主程序启动行（生效一次） | `db=int行号` | 成功仅回 `id,ty` | 未实现 | 可按需补充 |
| 2.14 | `project/clearStartLine` | 未实现 | 清除启动行设置 | 无 `db` | 成功仅回 `id,ty` | 未实现 | 可按需补充 |
| 3.1 | 全局变量定义 | `GlobalVarValueFormatter` / `GlobalVarRawJson` | 全局变量与工程变量以 JSON 字符串保存；支持数字/字符串/数组/Map | `saveVars.db.<name>.val` 为 JSON 字面量字符串 | 运行中改值后保留（掉电保存语义） | 已对齐（SDK编码层） | 持久化行为由控制器保证，SDK不改写 |
| 3.2 | 变量名规则 | `GlobalVarNaming.Validate` | Lua 风格命名；避免 `__` 开头；避免保留字 | 正则 `^[A-Za-z_][A-Za-z0-9_]*$` + 保留字集合 | 非法名抛 `ArgumentException` | 已对齐 | 保留字集合已内置于 `ReservedSet` |
| 3.3 | `globalVar/getVars` | `GetGlobalVars()` / `GetGlobalVarsCatalog()` | 获取所有全局变量 | 无 `db` 请求体（SDK发 `{}`） | 响应 `db` 为对象：键=变量名，值含 `val/nm` | 已对齐 | `GetGlobalVarsCatalog` 会解析为字典 |
| 3.4 | `globalVar/saveVars` | `SaveGlobalVar()/SaveGlobalVars()` | 增量保存；同名变量更新 | `db={name:{val:\"...\",nm?:\"...\"}}` | 成功仅回 `id,ty` | 已对齐 | `nm` 为空时 SDK 不发送 |
| 3.5 | `globalVar/removeVars` | `RemoveGlobalVars()` | 删除变量；不存在不报错 | `db` 为变量名数组 | 成功仅回 `id,ty` | 已对齐 | SDK 仅校验入参名合法 |
| 10.1 | `Robot/apostocpos` | `AposToCpos()/AposToCposPose()` | 正解：关节空间→笛卡尔空间 | `db={jp,coor,tool,ep}`；向量均6维；`jp` 单位deg | 响应 `db` 为6维笛卡尔（mm/deg） | 已对齐 | C# 对 `jp/coor/tool` 强制6维校验 |
| 10.2 | `Robot/cpostoapos` | `CposToApos()/CposToAposJoints()` | 逆解：笛卡尔空间→关节空间；空解可尝试调整 `rj` | `db={cp,rj,ep}`；`cp/rj` 6维；`ep` 可选 | 响应 `db` 为6维关节角（deg），可能为空 | 已对齐 | C# 空数组会抛解析异常，提示调整 `rj` |
| 10.3 | `Robot/calculateRelativePose` | `CalculateRelativePose()/CalculateRelativePoseResult()` | 笛卡尔偏移计算；`coorType`=`user/tool`；`posCoor/coor` 可选 | `db={pos,offset,coorType,posCoor?,coor?}`，均6维(mm/deg) | 响应 `db` 为偏移后6维坐标 | 已对齐 | C# 仅在 `coorType=user` 且传入时发送 `coor` |
| 11.1 | `Robot/jog` | `StartJog()` | 点动启动；需0.5s心跳维持 | `db={mode,speed,index,coorType,coorId}`；`speed` -1~1 | 响应 `db` 通常为null | 已对齐 | C# 有参数校验（见 `RobotMotionValidation.ValidateJog`） |
| 11.2 | `Robot/stopJog` | `StopJog()` | 停止点动 | 原文请求未显式 `db` | 响应 `ty=Robot/stopJog` | 已对齐 | C# 发送 `db=\"\"`，控制器兼容空db |
| 11.3 | `Robot/jogHeartbeat` | `JogHeartbeat()` | 点动心跳，每0.5s | 原文请求未显式 `db` | 响应 `ty=Robot/jogHeartbeat` | 已对齐 | C# 发送 `db=\"\"`，控制器兼容空db |
| 11.4 | `Robot/moveTo` | `MoveTo()` | 运动到预设/规划点；type=4/5需`target` | `db={type,target?}`；`target` 至少含 `cp/jp/ep` 之一 | 响应 `db` 通常为null | 已对齐 | C# 对 type=4/5 强制校验 target |
| 11.5 | `Robot/moveToHeartbeat` | `MoveToHeartbeat()` | RunTo心跳，每0.5s | 原文请求未显式 `db` | 响应 `ty=Robot/moveToHeartbeat` | 已对齐 | C# 发送 `db=\"\"`，控制器兼容空db |
| 11.6 | `Robot/setManualMoveRate` | `SetManualMoveRate()` | 设置手动倍率 | `db=1~100` | 原文响应留空（按你规则同请求） | 已对齐 | C# 校验范围 1~100 |
| 11.7 | `Robot/setAutoMoveRate` | `SetAutoMoveRate()` | 设置自动倍率 | `db=1~100` | 原文响应留空（按你规则同请求） | 已对齐 | C# 校验范围 1~100 |
| 11.8 | `Robot/move` | `Move()` | 运动指令列表；`movC` 必须给 `cp`；避免传空 `coor/tool` 数组 | `db=[MoveInstruction...]`，`targetPoint` 至少 `jp/cp` 其一；`movC/movCircle` 需 `middlePoint` | 接口仅表示接收成功，实际运行需看状态/错误 | 已对齐 | C# 序列化层已规避空 `coor/tool` 写入（通过可选字段） |
| 11.9 | `Robot/pause` | `PauseRobotMotion()` | 暂停运动 | 原文请求未显式 `db` | 响应 `ty=Robot/pause` | 已对齐 | C# 发送 `db=\"\"`；与工程 `project/pause` 语义不同 |
| 11.10 | `Robot/resume` | `ResumeRobotMotion()` | 恢复运动 | 原文请求未显式 `db` | 响应 `ty=Robot/resume` | 已对齐 | C# 发送 `db=\"\"` |
| 11.11 | `Robot/stopMove` | `StopRobotMove()` | 停止运动 | 原文请求未显式 `db` | 响应 `ty=Robot/stopMove` | 已对齐 | C# 发送 `db=\"\"` |
| 12.2 | `Robot/switchOff` | `SwitchOff()` | 下使能 | 原文请求未显式 `db` | 响应 `ty=Robot/switchOff` | 已对齐 | C# 发送 `db=\"\"`，控制器兼容空db |
| 12.3 | `Robot/toManual` | `ToManual()` / `EnterManualModeViaAuto()` | 进入手动模式（≥2.3.3.43） | 原文请求未显式 `db` | 响应 `ty=Robot/toManual` | 已对齐 | C# 提供“先Auto再Manual”组合方法满足模式跳转限制 |
| 12.4 | `Robot/toAuto` | `ToAuto()` | 进入自动模式（≥2.3.3.43） | 原文请求未显式 `db` | 响应 `ty=Robot/toAuto` | 已对齐 | C# 发送 `db=\"\"` |
| 12.5 | `Robot/toRemote` | `ToRemote()` / `EnterRemoteModeViaAuto()` | 进入远程模式（≥2.3.3.43） | 原文请求未显式 `db` | 响应 `ty=Robot/toRemote` | 已对齐 | C# 提供“先Auto再Remote”组合方法满足模式跳转限制 |
| 12.6 | `Robot/switchOnRescue` | 未实现 | 进入救援模式(异常) | 原文请求未显式 `db` | 响应 `ty=Robot/switchOnRescue` | 未实现/可选 | 当前 C# 无该接口封装 |
| 12.7 | `Robot/toSimulation` | `ToSimulation()` | 进入仿真模式 | 原文请求未显式 `db` | 响应 `ty=Robot/toSimulation` | 已对齐 | C# 发送 `db=\"\"` |
| 12.8 | `Robot/toActual` | `ToActual()` | 进入实机模式 | 原文请求未显式 `db` | 响应 `ty=Robot/toActual` | 已对齐 | C# 发送 `db=\"\"` |
| 12.9 | `Robot/startDrag` | `StartDrag()` | 进入拖拽模式（≥2.3.3.43） | 原文请求未显式 `db` | 响应 `ty=Robot/startDrag` | 已对齐 | 远程/手动模式约束由控制器侧保障 |
| 12.10 | `Robot/stopDrag` | `StopDrag()` | 退出拖拽模式（≥2.3.3.43） | 原文请求未显式 `db` | 响应 `ty=Robot/stopDrag` | 已对齐 | C# 发送 `db=\"\"` |
| 12.11 | `System/clearError` | `ClearSystemError()` | 清除错误 | 原文请求未显式 `db` | 响应 `ty=System/clearError` | 已对齐 | C# 发送 `db=\"\"` |
| 13.1 | `IOManager/GetIOValue` | `GetIoValues()/GetDi/GetDo/GetAi/GetAo` | 批量获取多个IO当前值 | `db=[{type,port},...]` | 响应 `db=[{type,port,value},...]` | 已对齐 | C# 支持批量查询并提供单点便捷封装 |
| 13.2 | `IOManager/SetIOValue` | `SetDo()/SetAo()` | 设置IO输出值 | `db={type,port,value}` | 响应 `ty=IOManager/SetIOValue` | 已对齐 | C# 对 DO 值限制 0/1；AO 按 double 写入 |
| 14.1 | `RegisterManager/GetRegisterValue` | `GetRegisterValue()/GetRegisterValues()` | 获取寄存器值 | `db=[address,...]` | 响应 `db=[{address,value},...]` | 已对齐 | C# 解析时按请求顺序/地址一致性校验 |
| 14.2 | `RegisterManager/SetRegisterValue` | `SetRegisterValue(int/double)` | 写入寄存器值 | `db={address,value}` | 响应 `ty=RegisterManager/SetRegisterValue` | 已对齐 | 支持整型/浮点重载 |
| 14.3 | `RegisterManager/setExtendArrayType` | `SetExtendArrayType()` | 设置扩展数组索引类型 | `db={index:0~999,type}` | 响应 `ty=RegisterManager/setExtendArrayType` | 已对齐 | C# 校验 index 范围与 type 枚举值 |
| 14.4 | `RegisterManager/removeExtendArray` | `RemoveExtendArray()` | 删除扩展数组索引（并重置） | `db={index:0~999}` | 响应 `ty=RegisterManager/removeExtendArray` | 已对齐 | C# 校验 index 范围 |
| 15.1 | `publish/topic`（泛化） | `SubscribePublishTopic(topicTy, handler, tc)` | 主题订阅；数据变化或首次订阅时推送 | 请求帧为 `{ty:\"publish/<topic>\",tc}`（无 `id`） | 下行推送 `{ty,db}` | 已对齐（SDK机制） | C# 以具体主题 `publish/...` 发首帧并按 `ty` 分发 |
| 15.2 | `publish/ProjectState` | `SubscribePublishTopic(PublishTopics.ProjectState,...)` | 推送工程状态与脚本行号 | `db` 对象（id/state/isStep/projectType/scripts...） | 变化时推送 | 已对齐（透传） | C# 不强制 schema，`db` 透传 `JsonElement` |
| 15.3 | `publish/VarUpdate` | `SubscribePublishTopic(PublishTopics.VarUpdate,...)` | 变量变化推送（仅变化项） | `db` 为 key-value，key 形如 `global/var/name` | 变化时推送 | 已对齐（透传） | 业务解析在用户回调中完成 |
| 15.4 | `publish/RobotStatus` | `SubscribePublishTopic(PublishTopics.RobotStatus,...)` | 推送机器人状态 | `db` 对象（mode/state/isMoving...） | 变化时推送 | 已对齐（透传） | `CodroidTest` 已有该主题示例 |
| 15.5 | `publish/RobotPosture` | `SubscribePublishTopic(PublishTopics.RobotPosture,...)` | 推送关节与末端位姿 | `db` 对象（joint/end/ep） | 变化时推送 | 已对齐（透传） | — |
| 15.6 | `publish/RobotCoordinate` | `SubscribePublishTopic(PublishTopics.RobotCoordinate,...)` | 推送工具/用户坐标系 | `db` 对象（tool/user） | 变化时推送 | 已对齐（透传） | — |
| 15.7 | `publish/Log` | `SubscribePublishTopic(PublishTopics.Log,...)` | 系统日志推送 | `db` 数组（日志条目） | 变化时推送 | 已对齐（透传） | — |
| 15.8 | `publish/Error` | `SubscribePublishTopic(PublishTopics.Error,...)` | 系统错误推送 | `db` 数组（错误条目） | 变化时推送 | 已对齐（透传） | — |
| 17.1 | CRI 实时数据定义 | `CriRealtimePacketParser` / `CriRealTimeData` | `mask` 位定义；状态1/2位定义；数据项单位以rad/m为主 | 按 mask 顺序拼接，长度动态 | 状态2高8位为CRI错误码；低位含实时控制模式位 | 按固定策略对齐 | **SDK 固定策略（已确认）**：6轴无外部轴、`mask=0xFFFF`、`highPercision=true`、`duration=100`，固定解析 308 字节，解析后统一转为 mm/deg |
| 17.4 | `CRI/StartDataPush`（≥2.3.3.43） | `StartCriDataPush(udpIp, udpPort)` | 开启UDP数据推送（可配 `duration/highPercision/mask`） | `db={ip,port,duration?,highPercision?,mask?}` | 响应 `ty=CRI/StartDataPush` | 已对齐（按固定策略） | **固定发送** `duration=100`、`highPercision=true`、`mask=0xFFFF`；当前不开放自定义参数 |
| 17.5 | `CRI/StopDataPush`（≥2.3.3.43） | `StopCriDataPush(udpIp?, udpPort?)` | 关闭UDP数据推送；多服务场景需传 `ip/port` | `db` 可空；或 `{ip,port}` | 响应 `ty=CRI/StopDataPush` | 已对齐 | C# 支持不传或按需传 `ip/port` |
| 17.6 | `CRI/StartControl` | `StartCriControl(filterType,durationMs,startBuffer)` | 开启实时控制（filterType/duration/startBuffer） | `db={filterType,duration,startBuffer}` | 响应 `ty=CRI/StartControl` | 已对齐 | C# 已封装并校验：filterType 0~3；duration 1~16 且可整除1000（1/2/4/5/8/10）；startBuffer 1~100；默认推荐 1/4/5 |
| 17.7 | `CRI/StopControl` | `StopCriControl()` | 关闭实时控制 | 原文请求未显式 `db` | 响应 `ty=CRI/StopControl` | 已对齐 | C# 发送 `db=\"\"`，控制器兼容空db |
| 19.1 | `Robot/setCollisionSensitivity` | `SetCollisionSensitivity(sensitivity)` | 设置碰撞检测灵敏度（≥2.3.3.43） | `db` 为整数，范围 0~100 | 响应 `db` 为布尔（成功时） | 已对齐 | C# 校验 `0~100`，超出抛 `ArgumentException` |
| 19.2 | `Robot/setPayload` | `SetPayload(payloadId)` | 运行时切换当前负载（≥2.3.3.43） | `db` 为负载 id 整数，SDK 校验 **1~15** | 响应 `db` 可能为 `null` | 已对齐 | 与 `SetDefaultPayloadId`（SaveRobotParameter）不同 |
| 19.2b | `Robot/SaveRobotParameter` | `SetDefaultPayloadId(payloadId)` | 设置默认负载编号（**固件 ≥ 2.3.3.43**） | `db={defaultPayloadId}` | `ty=Robot/SaveRobotParameter` | 已对齐 | 仅下发默认编号字段 |
| 19.3 | `Robot/SaveRobotParameter` | `SetDefaultToolId(toolId)` | 设置默认工具坐标系编号（**≥ 2.3.3.43**） | `db={defaultToolId}` | 同上 | 已对齐 | — |
| 19.4 | `Robot/SaveRobotParameter` | `SaveToolFrames` / `SetToolFrame` | 工具坐标系表（**≥ 2.3.3.43**） | `db={Tool:[...]}`；单槽位先 Get 再 patch | 同上 | 已对齐 | SDK 序号 **1~15** |
| 19.5 | `Robot/SaveRobotParameter` | `SavePayloadFrames` / `SetPayloadFrame` | 负载坐标系表（**≥ 2.3.3.43**） | `db={Payload:[...]}` | 同上 | 已对齐 | SDK 序号 **1~15** |
| 19.6 | `Robot/SaveRobotParameter` | `SetDefaultUserCoordinateId` / `SetUserCoordinateFrame` | 用户坐标系（**≥ 2.3.3.43**） | `defaultCoordinateId` 或 `Coordinate` 数组 | 同上 | 已对齐 | 默认编号与单槽修改分 API |
| 19.7 | `Robot/GetRobotParameter` | `GetRobotParameters()` | 读取设置界面参数（**≥ 2.3.3.43**） | 请求 `db` 空对象 | `db` 含 Tool/Payload/Coordinate 等 | 已对齐 | 示例见 `examples_client/06_robot_parameters.cpp` |

## 本轮对照结论（按当前约定）

1. **`ty` 严格按原文**：已将远程脚本模式接口改为 `project/enterRemoteScriptMode`，并新增 `project/runScript` 对应 API。
2. **`runStep` 策略**：SDK 继续强制传入 `projectID`，用于覆盖“首次启动 / 非暂停态”场景；不影响常规使用。
3. **`id` 策略**：协议允许数字或字符串；SDK 默认使用 **自增数字 id**，便于请求-响应匹配与排查。
4. **全局变量接口核对通过**：命名规则、增量保存、删除语义、`val` JSON 字符串编码均与原文一致。
5. **机器人计算接口（10.x）核对通过**：正解/逆解/相对位姿的 `ty`、`db` 结构、单位语义与 C# 一致。
6. **机器人运动接口（11.x）整体对齐**：jog/moveTo/move/pause/resume/stopMove 均有对应实现；心跳策略与文档一致。
7. **细节备注**：按你最新原文，11.2/11.3/11.5/11.9/11.10/11.11 请求均未显式 `db`；C# 当前发送 `db=\"\"`，与控制器兼容。
8. **机器人控制命令（12.2~12.11）**：除 `Robot/switchOnRescue` 外均已在 C# 封装并对齐。
9. **IO 与寄存器接口（13.x/14.x）核对通过**：批量读、单点写、扩展数组类型/删除均与 C# 对齐。
10. **主题订阅/推送（15.x）核对通过**：C# 具备通用订阅机制与主题常量，推送 `db` 按 `JsonElement` 透传给回调。
11. **CRI（17.x）固定策略已确认**：SDK 按 `duration=100`、`highPercision=true`、`mask=0xFFFF`、6轴无外部轴、308 字节解析实现；`StartDataPush/StopDataPush` 以此为统一约定。
12. **CRI 实时控制启停已封装**：新增 `StartCriControl` / `StopCriControl`，并按策略校验参数；默认推荐 `filterType=1`、`duration=4`、`startBuffer=5`。
13. 文档标注“暂时无法使用”的断点相关接口先记录，不作为当前实现项。
14. **固件版本**：本 C++ SDK 对外封装的 **全部接口**（含 19.x 机器人设置、`CRI/*`、运动/IO 等）统一要求控制器固件 **≥ 2.3.3.43**（`MinControllerFirmware`）。

## 待后续原文继续填

- 若后续提供 CRI 详细二进制布局版本差异（轴数/外部轴/mask组合），再补充细化。
- 协议第 16 / 18 章原文若后续给出，按现有表格格式补条目即可。
