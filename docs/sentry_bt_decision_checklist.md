# Sentry BT 决策迁移清单

## 1. 是否可以全部使用 NavigateThroughPoses

可以，但不建议无条件全覆盖。建议按场景使用：

- 适合 `NavigateThroughPoses`：
  - 固定巡逻路线（多点连续经过）。
  - 希望减少“单点到达后再重发目标”的停顿。
- 不适合强行全用：
  - 高频抢占/切换分支（例如血量和弹药状态频繁跳变）会导致 action 经常被取消重建。
  - BehaviorTree.ROS2 的 Action Halt 在部分版本/场景下仍有已知不稳定风险（本仓库 README 也提到）。

结论：`巡逻主路径` 用 `NavigateThroughPoses`，`应急单点` 允许单点发送（本次实现已支持单点也走 ThroughPoses，一样可用）。

## 2. 迁移目标与文件落点

- 新增 ThroughPoses 节点：
  - `src/pb2025_sentry_behavior/plugins/action/send_nav2_waypoints.cpp`
  - `src/pb2025_sentry_behavior/include/pb2025_sentry_behavior/plugins/action/send_nav2_waypoints.hpp`
- 新增状态区间条件节点（支持 hp/ammo min-max）：
  - `src/pb2025_sentry_behavior/plugins/condition/is_robot_status_in_range.cpp`
  - `src/pb2025_sentry_behavior/include/pb2025_sentry_behavior/plugins/condition/is_robot_status_in_range.hpp`
- 新增最近点选择节点（用 RobotStatus.robot_pos + 候选点列表）：
  - `src/pb2025_sentry_behavior/plugins/action/select_nearest_goal.cpp`
  - `src/pb2025_sentry_behavior/include/pb2025_sentry_behavior/plugins/action/select_nearest_goal.hpp`
- 新增 BT 决策树：
  - `src/pb2025_sentry_behavior/behavior_trees/rmul_2025_decision_bt.xml`
- 启动默认目标树已切换：
  - `src/pb2025_sentry_behavior/params/sentry_behavior.yaml`

## 3. 当前决策逻辑映射

- 比赛未开始：
  - 发布 `cmd_spin=0` 和 `cmd_vel=0`（停转停移）。
- 弹药 `<= 70`：
  - 回安全点（safe_goal）。
- HP `<= 150`：
  - 回安全点（safe_goal）。
- `150 < HP <= 200`：
  - 从 `retreat_candidates` 里选最近点并前往。
- `HP > 200` 且弹药充足：
  - 执行 `patrol_waypoints` 的 ThroughPoses 巡逻。

## 4. 只改 BT 文件坐标的方法

在 `rmul_2025_decision_bt.xml` 里修改 Script 常量：

- `safe_goal := 'x;y;yaw'`
- `patrol_waypoints := 'x1;y1;yaw1|x2;y2;yaw2|x3;y3;yaw3|...'`
- `retreat_candidates := 'x1;y1;yaw1|x2;y2;yaw2|...'`

格式规则：

- 单点：`x;y;yaw`
- 多点：用 `|` 分隔多个单点
- 坐标系：`map`
- 角度单位：`rad`

## 5. 验证清单

- `colcon build --packages-select pb2025_sentry_behavior`
- `ros2 launch pb2025_sentry_behavior pb2025_sentry_behavior_launch.py`
- 运行时确认：
  - `target_tree` 为 `rmul_2025_decision_bt`
  - Nav2 action 服务存在：`/navigate_through_poses`
  - 进入不同血量/弹药分支时，日志切换与行为一致
  - 巡逻点切换无明显停顿
