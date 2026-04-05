# 轻量 Loopback 仿真说明

## 1. 目标

这个文档只讲一件事：

- 如何不用 Gazebo
- 不接串口
- 不接雷达
- 不接实机裁判系统

直接在 `loopback_sim` 上测试你的哨兵导航与决策。

## 2. 当前特点

当前轻量仿真链路有这些特点：

- 默认不依赖比赛时间、血量、RFID
- 默认用 `decision/sim_mode` 驱动决策
- 决策输出已经是 `NavigateThroughPoses`
- 可以选用更完整的“手动裁判仿真输入”模式

## 3. 默认轻量仿真

启动：

```bash
source install/setup.bash
ros2 launch pb2025_sentry_bringup loopback_decision_sim.launch.py
```

默认是：

- `behavior_params_file=sentry_behavior_loopback.yaml`
- `decision.input_source=simulation`
- `decision_mode=patrol`

切模式：

```bash
ros2 param set /fake_decision_sim_inputs decision_mode patrol
ros2 param set /fake_decision_sim_inputs decision_mode anchor
ros2 param set /fake_decision_sim_inputs decision_mode retreat
ros2 param set /fake_decision_sim_inputs decision_mode safe
```

## 4. 手动裁判仿真

如果你要测试血量、比赛时间逻辑：

```bash
source install/setup.bash
ros2 launch pb2025_sentry_bringup loopback_decision_sim.launch.py \
  use_rviz:=true \
  publish_referee_inputs:=True \
  publish_decision_mode:=False \
  behavior_params_file:=/home/ats/ats_sentry_ws/install/pb2025_sentry_behavior/share/pb2025_sentry_behavior/params/sentry_behavior.yaml
```

运行后常用操作：

```bash
ros2 param set /fake_decision_sim_inputs current_hp 100
ros2 param set /fake_decision_sim_inputs stage_remain_time 30
ros2 param set /fake_decision_sim_inputs game_progress 4
```

## 5. 不想让假输入节点接管话题时

如果你要自己手动发 topic：

```bash
ros2 launch pb2025_sentry_bringup loopback_decision_sim.launch.py \
  use_rviz:=False \
  publish_referee_inputs:=False \
  publish_decision_mode:=False
```

然后自己发布：

```bash
ros2 topic pub /decision/sim_mode std_msgs/msg/String "{data: anchor}" -1
```

或者：

```bash
ros2 topic pub /referee/game_status pb_rm_interfaces/msg/GameStatus "{game_progress: 4, stage_remain_time: 30}" -1
```

## 6. 平滑性调节

最关键的两个参数：

- `decision.point_roles.patrol_indices`
- `decision.decision_config.patrol_preview_points`

原则：

- 巡逻点数量越多，可规划的 Through Poses 越连续
- `patrol_preview_points` 越大，一次发给 Nav2 的未来点越多

但要注意：

- 如果你只有 2 个巡逻点，预览点再大也提升有限
- 真想更丝滑，最好把巡逻点拆得更细

运行时可直接调：

```bash
ros2 param set /pb2025_sentry_behavior_server decision.decision_config.patrol_preview_points 3
```

## 7. 不需要的东西

使用当前 loopback 时，不需要：

- Gazebo
- 串口
- 实机底盘
- 实机雷达
- 实机裁判系统

只有你主动切到“手动裁判仿真模式”时，才会在仿真里构造裁判话题。
