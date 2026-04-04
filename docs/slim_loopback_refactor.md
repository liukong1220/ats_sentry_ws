# 精简版导航、决策与 Loopback 仿真框架说明

## 1. 文档目的

这份文档只描述当前项目已经保留下来的主线能力：

- 导航
- 决策
- 轻量仿真

这份文档不再讨论已经移除的旧视觉链路、`auto_aim_interfaces`、Gazebo 世界与 Gazebo 桥接链路，避免后续使用时混淆。

## 2. 当前项目定位

当前工作区已经被整理为一个适合 ROS 2 Humble 的精简框架，核心目标是：

- 用 `Nav2` 完成哨兵导航
- 用 `pb2025_sentry_behavior` 承载行为树决策
- 用 `src/loopback_sim` 提供纯软件闭环仿真环境

它现在更适合做下面这些事情：

- 导航链路联调
- 决策链路联调
- 行为树调试
- 纯软件闭环验证
- 后续实机接入前的基础验证

## 3. 已移除的内容

本次精简中，已经从源码工作区移除以下内容：

- `src/pb2025_rm_vision`
- `src/interfaces/auto_aim_interfaces`
- `src/rmu_gazebo_simulator`
- `src/dependencies/rmoss_gazebo`

同时，相关残余依赖也已经同步清理，避免出现“包删掉了，但代码里还留着引用”的情况。

## 4. 当前主线包

当前和主线直接相关的包主要是：

- `src/loopback_sim`
  - 提供轻量仿真环境
  - 负责生成 `/odom`、`/scan`、`/clock` 等仿真数据
- `src/pb2025_sentry_bringup`
  - 提供精简后的启动入口
  - 当前主入口是 `loopback_decision_sim.launch.py`
- `src/pb2025_sentry_behavior`
  - 提供行为树决策服务
- `src/standard_robot_pp_ros2`
  - 当前不是 loopback 仿真主链的一部分
  - 更偏向后续实机串口与协议接入

## 5. 当前是否需要接实机

结论很明确：

- 当前这套 `loopback` 仿真框架不需要接实机雷达
- 不需要接实机串口
- 不需要接真实哨兵本体
- 不需要保留 Gazebo 才能运行

原因是当前仿真链路已经自己补齐了基础输入：

- `loopback_sim` 提供仿真的 `/odom`、`/scan`、TF、`/clock`
- `fake_decision_sim_inputs.py` 提供假的裁判系统话题和 `initialpose`
- `loopback_decision_sim.launch.py` 不会启动 `standard_robot_pp_ros2` 这一类实机串口节点

因此，如果你当前的目标只是：

- 验证导航是否跑通
- 验证行为树决策是否会发目标
- 验证导航与决策能否在同一套仿真里形成闭环

那么你只需要软件环境，不需要外接硬件。

## 6. 什么时候才需要回到实机链路

只有在你要做下面这些事情时，才需要接回实机：

- 测试真实串口收发
- 测试真实底盘控制
- 测试真实雷达或激光雷达输入
- 测试真实裁判系统数据
- 验证导航在真实传感器与真实底盘上的表现

换句话说：

- `loopback` 适合做导航与决策联调
- 实机链路适合做硬件联调与赛场部署验证

这两条链路现在已经分开了。

## 7. 当前推荐启动入口

当前最推荐的入口是：

- `src/pb2025_sentry_bringup/launch/loopback_decision_sim.launch.py`

这个入口会完成以下事情：

- 启动静态 TF
- 启动地图服务
- 启动 Nav2 导航链
- 启动 `nav2_loopback_sim`
- 启动假的裁判系统输入与 `initialpose` 发布脚本
- 延迟启动 `pb2025_sentry_behavior`
- 按需启动 RViz

## 8. 编译方法

### 8.1 推荐整仓编译命令

在工作区根目录执行：

```bash
cd /home/ats/ats_sentry_ws
colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release --parallel-workers 10
```

这条命令在当前工作区已经实测可用。

### 8.2 第一次切换到 `--symlink-install` 时的注意事项

如果你之前已经用“非 `--symlink-install`”模式构建过工作区，那么第一次切到 `--symlink-install` 之前，建议先清理旧构建产物：

```bash
cd /home/ats/ats_sentry_ws
rm -rf build install log
colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release --parallel-workers 10
```

原因是旧的普通安装产物有时会和新的符号链接安装模式冲突，导致类似“已有目录不能被移除并替换为符号链接”的报错。

### 8.3 为什么推荐 `--symlink-install`

当前工程里有很多 launch、YAML、地图和行为树文件。使用 `--symlink-install` 后：

- 修改这些非编译资源时通常不需要整仓重编
- 重新 `source install/setup.bash` 并重启 launch 后，修改更容易直接生效

### 8.4 局部重编命令

如果你只改了 bringup 包，可以用：

```bash
colcon build --packages-select pb2025_sentry_bringup --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release
```

如果你只改了行为树服务，可以用：

```bash
colcon build --packages-select pb2025_sentry_behavior --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release
```

## 9. 启动方法

### 9.1 不开 RViz

```bash
source /home/ats/ats_sentry_ws/install/setup.bash
ros2 launch pb2025_sentry_bringup loopback_decision_sim.launch.py use_rviz:=False
```

### 9.2 开 RViz

```bash
source /home/ats/ats_sentry_ws/install/setup.bash
ros2 launch pb2025_sentry_bringup loopback_decision_sim.launch.py use_rviz:=True
```

## 10. 启动后应看到的关键节点

如果系统正常，通常应当能看到以下关键节点：

- `/loopback_simulator`
- `/map_server`
- `/controller_server`
- `/planner_server`
- `/smoother_server`
- `/behavior_server`
- `/bt_navigator`
- `/pb2025_sentry_behavior_server`
- `/pb2025_sentry_behavior_client`
- `/fake_decision_sim_inputs`

当前运行主链里，不应看到 `standard_robot_pp_ros2` 的实机节点。这也是当前“不依赖实机串口”的直接证据。

## 11. 启动后的自检命令

### 11.1 看节点

```bash
ros2 node list
```

### 11.2 看动作接口

```bash
ros2 action list
```

至少应当能看到：

- `/navigate_to_pose`
- `/pb2025_sentry_behavior`

### 11.3 看关键话题

```bash
ros2 topic list
```

至少应当存在：

- `/clock`
- `/odom`
- `/scan`
- `/goal_pose`
- `/cmd_vel`
- `/referee/game_status`
- `/referee/robot_status`
- `/referee/rfid_status`

### 11.4 验证决策是否真的在发导航目标

```bash
timeout 10s ros2 topic echo /goal_pose --once
```

如果有输出，说明决策侧已经在向导航侧发送目标点。

### 11.5 验证导航是否真的在输出速度

```bash
timeout 10s ros2 topic echo /cmd_vel --once
```

如果有输出，说明导航控制链已经在工作。

### 11.6 验证机器人是否真的被导航驱动

可以直接观察启动日志中的 `bt_navigator` 与 `controller_server` 输出。只要看到：

- `Begin navigating from current location ... to ...`
- `Received a goal, begin computing control effort`
- `Goal succeeded`

就说明系统不是“节点起来了但没有闭环”，而是确实完成了导航行为。

注意：如果目标点本来就离当前位置很近，机器人很快到达目标后，连续两次查看 `/odom` 位置有可能已经不再变化。这不代表系统没动，而是代表它已经到点停住了。

## 12. 截至 2026-04-04 的本次实测结果

本轮已完成以下实测：

- 顶层 `build/ install/ log/` 已清理
- 使用以下命令整仓重新构建成功

```bash
colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release --parallel-workers 10
```

- 构建过程中没有出现阻塞性编译错误
- 发现并修复了 `fake_decision_sim_inputs.py` 不能作为可执行节点启动的问题
  - 原因是脚本缺少 shebang，且源文件没有执行权限
  - 现已补上 `#!/usr/bin/env python3` 并设置为可执行
- 重新启动 `loopback_decision_sim.launch.py` 后，已确认：
  - `/navigate_to_pose` 动作存在
  - `/pb2025_sentry_behavior` 动作存在
  - `/goal_pose` 有实际消息
  - `/cmd_vel` 有实际消息
  - Nav2 与行为树节点均正常启动
  - `bt_navigator` 日志中出现了导航开始、重复接收目标、以及目标成功的记录

当前可以确认：

- 当前工作区可以编译
- 当前主入口可以启动
- 当前导航和决策在 loopback 环境中可以形成闭环

## 13. 当前仍然存在但不阻塞主链的历史问题

当前项目已经可以用于“导航 + 决策 + loopback 仿真”的主线验证，但还不能称为“整仓所有测试全绿”。本轮重新核验后，仍有以下非阻塞问题：

- `nav2_loopback_sim`
  - 仍有 `test_copyright.py` 失败
- `pb2025_sentry_behavior`
  - `copyright`
  - `cpplint`
  - `clang_format`
  - `black`
  - 主要问题是版权头缺失、文件首部空行和格式风格债务
- `pb2025_sentry_bringup`
  - `copyright`
  - `black`
  - 主要问题是新旧 launch 与脚本文件缺少版权头，且部分 Python 文件风格还未统一
- `standard_robot_pp_ros2`
  - 本轮测试结果也不是全绿
  - 主要仍然是历史风格和版权头类问题，不是本轮主链编译错误

这些问题目前的性质是：

- 会影响 lint 和测试结果
- 不会阻塞当前主入口的编译
- 不会阻塞当前 loopback 主入口启动
- 不会阻塞行为树向 Nav2 发目标

如果你的目标是“先把导航、决策和轻量仿真主链跑通”，这些问题不是当前阻塞项。  
如果你的目标是“把仓库整理到测试全绿、风格一致”，这些问题仍然需要后续继续收尾。

## 14. 当前运行时的已知现象

当前运行时可能看到以下现象：

- 启动早期，局部代价地图可能短暂提示等待 `base_link -> odom` 的 TF
- 行为树当前会以固定频率重复发布同一个导航目标，因此 `bt_navigator` 日志里会看到多次 `Received goal preemption request`

目前检查结果是：

- 这两类现象不会阻塞系统启动
- 不会阻塞 `/goal_pose` 发布
- 不会阻塞 `/cmd_vel` 输出
- 不会阻塞目标到达

其中第二点属于后续可以继续优化的行为树细节，不是当前主链不可用的阻塞错误。

## 15. 当前框架的边界

当前框架已经可以完成“导航 + 决策 + 轻量仿真”的一体化验证，但它仍然有边界：

- 它不是 Gazebo 物理仿真
- 它不验证真实串口链路
- 它不验证真实雷达硬件输入
- 它不验证真实底盘控制器
- 它不验证真实赛场通信时序

所以它适合做：

- 软件联调
- 行为树调试
- Nav2 参数调试
- 决策逻辑闭环验证

但它不能完全替代：

- 实机联调
- 真实传感器标定
- 真实底盘与控制链验收

## 16. 当前结论

如果你的目标是保留一个：

- 可编译
- 可启动
- 可运行导航
- 可运行决策
- 可做轻量仿真联调

的 ROS 2 Humble 哨兵框架，那么当前这个版本已经成立。

并且对你前面最关心的问题，结论仍然明确：

- 现在使用这套 `loopback` 仿真，不需要接实机雷达
- 不需要接串口
- 不需要接真实哨兵本体

你可以先在纯软件环境下把导航和决策调通，之后再决定是否把实机链路接回来。
