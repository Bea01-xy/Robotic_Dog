# RobStride RS04 电机驱动说明

本文档说明 `Device/Motor/Motor_RS.c/.h` 的常用 API、反馈数据，以及如何实现 MIT 运控。

## 1. 协议与默认配置

- 协议：RobStride MIT 协议
- CAN：CAN 2.0 经典标准帧，默认 1 Mbps
- 控制帧 ID：`(mode << 8) | motor_id`
  - `mode = 0`：MIT 运控
  - `mode = 1`：位置模式
  - `mode = 2`：速度模式
- 反馈帧 ID：主机 CAN ID，默认 `0xFD`
  - `Byte0`：电机 CAN ID
- 当前工程默认值：
  - 电机 CAN ID：`2`
  - 主机 CAN ID：`0xFD`
  - 实例：`Motor_RS04`

RS04 参数量程：

| 参数 | 范围 |
|---|---|
| 位置 | `-12.57 ~ +12.57 rad` |
| 速度 | `-15.0 ~ +15.0 rad/s` |
| 力矩 | `-120.0 ~ +120.0 Nm` |
| Kp | `0 ~ 5000` |
| Kd | `0 ~ 100` |

## 2. 常用 API

| API | 作用 |
|---|---|
| `RS_Motor_ProtocolSwitch()` | 切换 CAN 协议：私有 / CANopen / MIT |
| `RS_Motor_Command()` | 使能、失能、标零、清错、切换运行模式 |
| `RS_Motor_CAN_TxMessage()` | 发送 MIT 运控帧 |
| `RS_Motor_Position_CAN_TxMessage()` | 发送位置模式帧 |
| `RS_Motor_Speed_CAN_TxMessage()` | 发送速度模式帧 |
| `RS_Motor_Info_Update()` | 解析反馈帧，更新 `Motor_RS04.Data` |

### 2.1 协议切换

```c
RS_Motor_ProtocolSwitch(&FDCAN1_TxFrame,
                        &Motor_RS04,
                        MOTOR_RS_PROTOCOL_MIT);
```

发送私有协议类型 25 扩展帧：

```text
ID   = 0x1900FD02
Data = 01 02 03 04 05 06 02 00
```

切换后必须给电机重新上电才生效。

### 2.2 命令接口

```c
RS_Motor_Command(&FDCAN1_TxFrame, &Motor_RS04, CMD);
```

可用命令：

```c
MOTOR_RS_ENABLE
MOTOR_RS_DISABLE
MOTOR_RS_SAVE_ZERO
MOTOR_RS_CLEAR_ERROR
MOTOR_RS_SET_MIT_MODE
MOTOR_RS_SET_POSITION_MODE
MOTOR_RS_SET_SPEED_MODE
MOTOR_RS_SAVE_PARAMS
```

注意：切换运行模式前必须先 `MOTOR_RS_DISABLE`。

### 2.3 MIT 运控接口

```c
RS_Motor_CAN_TxMessage(&FDCAN1_TxFrame,
                       &Motor_RS04,
                       Position,   // rad
                       Velocity,   // rad/s
                       KP,         // 0 ~ 5000
                       KD,         // 0 ~ 100
                       Torque);    // Nm
```

函数内部会按 RS04 量程限幅并编码为标准帧。

### 2.4 位置模式与速度模式

```c
/* 位置模式：目标位置 + 目标速度 */
RS_Motor_Position_CAN_TxMessage(&FDCAN1_TxFrame, &Motor_RS04,
                                target_position,
                                target_velocity);

/* 速度模式：目标速度 + 电流限制 */
RS_Motor_Speed_CAN_TxMessage(&FDCAN1_TxFrame, &Motor_RS04,
                             target_velocity,
                             current_limit);
```

使用前需要先切换到对应运行模式并使能。

### 2.5 反馈解析

CAN1 接收回调中已经调用：

```c
RS_Motor_Info_Update(Identifier, Data, &Motor_RS04);
```

反馈更新后可读取：

```c
Motor_RS04.Data.Position       // rad
Motor_RS04.Data.Velocity       // rad/s
Motor_RS04.Data.Torque         // Nm
Motor_RS04.Data.Temperature    // °C
Motor_RS04.Data.Mode_State     // 0 Reset / 1 Cali / 2 Motor
Motor_RS04.Data.Fault
Motor_RS04.Data.Warning
Motor_RS04.Data.offline
```

## 3. MIT 运控实现

MIT 运控一次给出五个参数：

```text
目标位置、目标速度、Kp、Kd、前馈力矩
```

电机内部按类似下面的阻抗控制计算输出：

```text
torque = Kp * (position_ref - position)
       + Kd * (velocity_ref - velocity)
       + torque_ff
```

### 3.1 初始化顺序

```c
/* 1. 确保已经切换到 MIT 协议并重新上电 */

/* 2. 先停止 */
RS_Motor_Command(&FDCAN1_TxFrame, &Motor_RS04, MOTOR_RS_DISABLE);
osDelay(20);

/* 3. 切换到 MIT 运控模式 */
RS_Motor_Command(&FDCAN1_TxFrame, &Motor_RS04, MOTOR_RS_SET_MIT_MODE);
osDelay(20);

/* 4. 使能 */
RS_Motor_Command(&FDCAN1_TxFrame, &Motor_RS04, MOTOR_RS_ENABLE);
osDelay(20);
```

### 3.2 周期发送

建议 1 ms 周期：

```c
for (;;)
{
    RS_Motor_CAN_TxMessage(&FDCAN1_TxFrame,
                           &Motor_RS04,
                           target_position,
                           0.0f,
                           30.0f,
                           1.0f,
                           0.0f);

    osDelay(1);
}
```

### 3.3 典型用法

#### 位置保持

```c
RS_Motor_CAN_TxMessage(&FDCAN1_TxFrame, &Motor_RS04,
                       target_position,
                       0.0f,
                       30.0f,
                       1.0f,
                       0.0f);
```

#### 速度/阻尼控制

```c
RS_Motor_CAN_TxMessage(&FDCAN1_TxFrame, &Motor_RS04,
                       Motor_RS04.Data.Position,
                       target_velocity,
                       0.0f,
                       2.0f,
                       0.0f);
```

#### 纯力矩/前馈

```c
RS_Motor_CAN_TxMessage(&FDCAN1_TxFrame, &Motor_RS04,
                       Motor_RS04.Data.Position,
                       0.0f,
                       0.0f,
                       0.0f,
                       target_torque);
```

#### 位置 + 重力前馈

```c
RS_Motor_CAN_TxMessage(&FDCAN1_TxFrame, &Motor_RS04,
                       target_position,
                       0.0f,
                       20.0f,
                       1.0f,
                       gravity_torque);
```

## 4. 参数调节建议

| 目标 | Kp | Kd | 说明 |
|---|---:|---:|---|
| 位置保持 | 10~100 | 0.1~2 | 先小后大 |
| 柔顺位置 | 1~20 | 0.1~1 | Kp 小，抗冲击 |
| 速度阻尼 | 0 | 0.5~5 | 只靠 Kd 跟踪速度 |
| 纯力矩 | 0 | 0 | 只给 Torque |
| 重力补偿 | 0~50 | 0.1~2 | Torque 给重力前馈 |

建议从较小的 Kp/Kd 开始，逐步增加，避免一上电就大幅抖动。

## 5. 注意点

- `RS_Motor_CAN_TxMessage()` 不会自动使能，也不会自动切换模式。
- 当前 `CAN_Task.c` 使用的是速度模式；要改用 MIT 运控需改成：
  ```text
  DISABLE → SET_MIT_MODE → ENABLE → RS_Motor_CAN_TxMessage()
  ```
- MIT 位置范围是 `±12.57 rad`，超过会限幅。
- 反馈帧 ID 是主机 ID `0xFD`，`Byte0` 才是电机 ID `2`。
- 同一套 `FDCAN1_TxFrame` 不要在多个任务里并发发送，否则帧内容可能被覆盖。
- 如果需要多颗 RS 电机，复制 `Motor_RS_Info_Typedef` 实例，分别配置 CAN ID，并在接收回调里对每个实例调用 `RS_Motor_Info_Update()`。
