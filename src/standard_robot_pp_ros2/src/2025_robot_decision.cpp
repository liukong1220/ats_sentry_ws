#include "standard_robot_pp_ros2/2025_robot_decision.hpp"

namespace standard_robot_pp_ros2
{

    RobotDecisionNode::RobotDecisionNode(const rclcpp::NodeOptions & options)
    : Node("RobotDecisionNode", options),
      nav2_client_(nullptr),      // 按类声明顺序初始化
      current_goal_handle_(nullptr),
      rmul_goal_point_(),
      params_(),
      current_feedback_(std::make_shared<NavigateToPose::Feedback>()),
      last_nav_result_(rclcpp_action::ResultCode::UNKNOWN),
      robot_move_(false)
    {
        RCLCPP_INFO(get_logger(), "Start RobotDecisionNode!");


        // 加载动作服务名称参数
        params_.config.nav2_action_server = declare_parameter<std::string>(
            "robot_decision.decision_config.nav2_action_server", 
            "/navigate_to_pose"
        );

    // 参数声明部分修改为完整命名空间路径
        auto declare_ns_param = [this](const std::string& name, auto&& default_val) {
            return this->declare_parameter<std::decay_t<decltype(default_val)>>(
		name, 
                std::forward<decltype(default_val)>(default_val)
            );
        };

        // 目标点参数
        auto goal_x = declare_ns_param("goal_points.x",
            std::vector<double>{0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        auto goal_y = declare_ns_param("goal_points.y",
            std::vector<double>{0.0, 0.0, 0.0, 0.0, 0.0, 0.0});
        auto goal_z = declare_ns_param("goal_points.z",
            std::vector<double>{0.0, 0.0, 0.0, 0.0, 0.0, 0.0});

        // 添加调试日志输出
        RCLCPP_INFO(this->get_logger(), "Loaded goal_x: %f", goal_x[5]);
        RCLCPP_INFO(this->get_logger(), "Loaded goal_y: %f", goal_y[5]);
        RCLCPP_INFO(this->get_logger(), "Loaded goal_z: %f", goal_z[5]);


        // 验证坐标维度一致性
        if (goal_x.size() != goal_y.size() || goal_x.size() != goal_z.size()) {
            RCLCPP_FATAL(
                this->get_logger(),
                "三维坐标参数长度不匹配！x:%zu vs y:%zu vs z:%zu",
                goal_x.size(),
                goal_y.size(),
                goal_z.size()
            );
            throw std::runtime_error("三维坐标参数长度不匹配");
        }

        // 构建三维目标点数组
        rmul_goal_point_.clear();
        for (size_t i = 0; i < goal_x.size(); ++i) {
            geometry_msgs::msg::Point p;
            p.x = goal_x[i];
            p.y = goal_y[i];
            p.z = goal_z[i];  // 使用三维坐标
            rmul_goal_point_.push_back(p);
        }
    
        // 检查目标点数量（根据实际需求保留或修改）
        // if (rmul_goal_point_.size() < 4) {
        //     RCLCPP_FATAL(get_logger(),
        //             "需要至少4个三维目标点配置，当前数量: %zu", 
        //             rmul_goal_point_.size());
        //     throw std::runtime_error("Invalid goal points configuration");
        // }

        // 调试日志输出
        for (const auto& point : rmul_goal_point_) {
            RCLCPP_INFO(
                this->get_logger(),
                "三维路径点坐标: (%.2f, %.2f, %.2f)", // ✅ 补全括号和分号
                point.x,
                point.y,
                point.z
            ); // 补全闭合括号
        }

    // 加载时间阈值参数
    params_.time.abundant = declare_ns_param("time_thresholds.abundant", 420);
    params_.time.normal = declare_ns_param("time_thresholds.normal", 360);
    params_.time.tense = declare_ns_param("time_thresholds.tense", 180);
    params_.time.critical = declare_ns_param("time_thresholds.critical", 60);

    // 加载血量阈值参数
    params_.hp.abundant.high =    declare_ns_param("hp_thresholds.abundant.high", 300);
    params_.hp.abundant.medium = declare_ns_param("hp_thresholds.abundant.medium", 200);
    params_.hp.abundant.low =    declare_ns_param("hp_thresholds.abundant.low", 150);

    params_.hp.normal.high = declare_ns_param("hp_thresholds.normal.high", 300);
    params_.hp.normal.medium = declare_ns_param("hp_thresholds.normal.medium", 200);
    params_.hp.normal.low = declare_ns_param("hp_thresholds.normal.low", 150);


    params_.hp.tense.high  = declare_ns_param("hp_thresholds.tense.high", 300);
    params_.hp.tense.medium = declare_ns_param("hp_thresholds.tense.medium", 200);
    params_.hp.tense.low = declare_ns_param("hp_thresholds.tense.low", 150);

    params_.hp.critical.high = declare_ns_param("hp_thresholds.critical.high", 300);
    params_.hp.critical.medium = declare_ns_param("hp_thresholds.critical.medium", 200);
    params_.hp.critical.low = declare_ns_param("hp_thresholds.critical.low", 100);
    
    // 加载决策配置参数
    params_.config.path_tolerance = declare_ns_param("decision_config.path_tolerance", 0.1);

    // 调试输出结构体参数值
    RCLCPP_INFO(get_logger(), "[结构体参数验证]");
    RCLCPP_INFO(get_logger(), "时间阈值-充足: %d", params_.time.abundant);
    RCLCPP_INFO(get_logger(), "血量阈值-中等: %d", params_.hp.critical.medium);
    RCLCPP_INFO(get_logger(), "路径容差: %.2f", params_.config.path_tolerance);

    // 订阅比赛状态、RFID状态和机器人状态消息
        game_status_sub_ = create_subscription<pb_rm_interfaces::msg::GameStatus>(
            "referee/game_status", 10, std::bind(&RobotDecisionNode::gameStatusCallback, this, std::placeholders::_1));

        rfid_status_sub_ = create_subscription<pb_rm_interfaces::msg::RfidStatus>(
            "referee/rfid_status", 10, std::bind(&RobotDecisionNode::rfidStatusCallback, this, std::placeholders::_1));

        robot_status_sub_ = create_subscription<pb_rm_interfaces::msg::RobotStatus>(
            "referee/robot_status", 10, std::bind(&RobotDecisionNode::robotStatusCallback, this, std::placeholders::_1));

        stop_flag_pub_  = create_publisher<std_msgs::msg::Bool>(
            "stop_flag", 10); // 发布停止标志位

            
        // 初始化Action客户端
        nav2_client_ = rclcpp_action::create_client<NavigateToPose>(
            this,
            params_.config.nav2_action_server
        );

        // 动作服务器连接检测
        if (!nav2_client_->wait_for_action_server(std::chrono::seconds(5))) {
            RCLCPP_FATAL(get_logger(), "Action server %s not available", 
                params_.config.nav2_action_server.c_str());
            throw std::runtime_error("Navigation action server not available");
        }


        // 创建定时器，定期执行决策逻辑        // 创建定时器，定期执行决策逻辑
        timer_ = create_wall_timer(std::chrono::milliseconds(200), [this]() {
            std::lock_guard<std::mutex> lock(nav_mutex_);  // 添加互斥锁
            try {
                executeDecisionLogic();
            } catch (const std::exception& e) {
                RCLCPP_ERROR(get_logger(), "决策异常: %s", e.what());
            }
        });

    }

    void RobotDecisionNode::gameStatusCallback(const pb_rm_interfaces::msg::GameStatus::SharedPtr msg)
    {
        game_status_msg_ = *msg;
        RCLCPP_INFO(get_logger(), "比赛状态: %d",game_status_msg_.game_progress);
    }

    void RobotDecisionNode::rfidStatusCallback(const pb_rm_interfaces::msg::RfidStatus::SharedPtr msg)
    {
        rfid_status_msg_ = *msg;
    }

    void RobotDecisionNode::robotStatusCallback(const pb_rm_interfaces::msg::RobotStatus::SharedPtr msg)
    {
        robot_status_msg_ = *msg;
    }

    void RobotDecisionNode::executeDecisionLogic()
    {

        //判断比赛是否开始
        if (game_status_msg_.game_progress == 3)
        {
            robot_move_ = false;

        }
        else if (game_status_msg_.game_progress == 4)
        {

            robot_move_ = true;

        }
        else
        {
            robot_move_ = false;
        }

        // 决策函数
        robotDecision();
    }


    void RobotDecisionNode::sendNavigationGoal(float x, float y, float z ,float yaw)
    {

        // 检查动作服务器状态
        if (!nav2_client_->action_server_is_ready()) {
            RCLCPP_ERROR(get_logger(), "动作服务器不可用");
            return;
        }

        geometry_msgs::msg::Point target_point;
        target_point.x = x;
        target_point.y = y;
        target_point.z = z;

        if (isSameGoal(target_point, yaw)) {
            return;
        }

        // 只有目标点真正变化时才取消当前目标，避免巡逻过程中不断重置 Nav2
        if (current_goal_handle_) {
            auto future_cancel = nav2_client_->async_cancel_goal(current_goal_handle_);
            (void)future_cancel;
            RCLCPP_DEBUG(get_logger(), "已发送取消请求");
        }
        goal_active_ = false;
        goal_request_pending_ = true;
        has_goal_target_ = true;
        current_goal_target_ = target_point;
        current_goal_yaw_ = yaw;
        last_nav_result_ = rclcpp_action::ResultCode::UNKNOWN;


        // 构建动作目标
        auto goal_msg = NavigateToPose::Goal();
        goal_msg.pose.header.stamp = this->now();
        goal_msg.pose.header.frame_id = "map";

        goal_msg.pose.pose.position.x = x;
        goal_msg.pose.pose.position.y = y;
        goal_msg.pose.pose.position.z = z;

        tf2::Quaternion q;
        q.setRPY(0.0, 0.0, yaw);
        goal_msg.pose.pose.orientation = tf2::toMsg(q);

        // 设置发送选项
        auto send_goal_options = rclcpp_action::Client<NavigateToPose>::SendGoalOptions();

        // 目标响应回调
        send_goal_options.goal_response_callback =
            [this, target_point, yaw](const GoalHandleNavigate::SharedPtr & goal_handle) {
                goal_request_pending_ = false;
                if (!goal_handle) {
                    RCLCPP_ERROR(get_logger(), "目标被服务器拒绝");
                    const bool target_still_current =
                        has_goal_target_ &&
                        std::fabs(current_goal_target_.x - target_point.x) < 1e-3 &&
                        std::fabs(current_goal_target_.y - target_point.y) < 1e-3 &&
                        std::fabs(current_goal_target_.z - target_point.z) < 1e-3 &&
                        std::fabs(current_goal_yaw_ - yaw) < 1e-3;
                    if (target_still_current) {
                        goal_active_ = false;
                        has_goal_target_ = false;
                    }
                } else {
                    current_goal_handle_ = goal_handle;  // 关键修改：存储目标句柄
                    goal_active_ = true;
                    RCLCPP_INFO(get_logger(), "目标已接受，ID: %s", 
                        rclcpp_action::to_string(goal_handle->get_goal_id()).c_str());
                }
            };

        // 在send_goal_options添加反馈回调
        send_goal_options.feedback_callback = 
            [this](
                GoalHandleNavigate::SharedPtr,
                const std::shared_ptr<const NavigateToPose::Feedback> feedback) {
                    // current_feedback_ = feedback; // 直接替换指针
                    current_feedback_->current_pose = feedback->current_pose; // 复制数据而非指针
                    has_navigation_feedback_ = true;
                    RCLCPP_DEBUG(get_logger(), "收到导航反馈");
            };

        // 结果回调
        send_goal_options.result_callback =
            [this, target_point, yaw](const GoalHandleNavigate::WrappedResult & result) {
                last_nav_result_ = result.code; // 记录导航结果
                switch (result.code) {
                    case rclcpp_action::ResultCode::SUCCEEDED:
                        RCLCPP_INFO(get_logger(), "导航成功抵达目标点");
                        break;
                    case rclcpp_action::ResultCode::ABORTED:
                        RCLCPP_WARN(get_logger(), "导航被服务器中止");
                        break;
                    case rclcpp_action::ResultCode::CANCELED:
                        RCLCPP_DEBUG(get_logger(), "导航任务被取消");
                        break;
                    default:
                        RCLCPP_ERROR(get_logger(), "未知导航结果");
                        break;
                }
                const bool target_still_current =
                    has_goal_target_ &&
                    std::fabs(current_goal_target_.x - target_point.x) < 1e-3 &&
                    std::fabs(current_goal_target_.y - target_point.y) < 1e-3 &&
                    std::fabs(current_goal_target_.z - target_point.z) < 1e-3 &&
                    std::fabs(current_goal_yaw_ - yaw) < 1e-3;
                if (target_still_current) {
                    goal_request_pending_ = false;
                    goal_active_ = false;
                    current_goal_handle_.reset(); // 重置目标句柄
                    has_goal_target_ = false;
                }
            };
    
        // 异步发送目标
        //处理发送结果
        try {
            // 异步发送目标（移除spin_until_future_complete）
            auto future_goal_handle = nav2_client_->async_send_goal(goal_msg, send_goal_options);
            RCLCPP_DEBUG(get_logger(), "已异步发送导航目标");
        } catch (const std::exception& e) {
            RCLCPP_ERROR(get_logger(), "发送目标异常: %s", e.what());
        }
    }


    // int RobotDecisionNode::getGameTimeState()
    // {
    //     RCLCPP_INFO(get_logger(), "当前游戏时间状态: %d", game_status_msg_.stage_remain_time);
    //     if (game_status_msg_.stage_remain_time > params_.time.normal && game_status_msg_.stage_remain_time <=params_.time.abundant)
    //     {
    //         return 1; // 时间充足
    //     }
    //     else if (game_status_msg_.stage_remain_time > params_.time.tense && game_status_msg_.stage_remain_time <= params_.time.normal)
    //     {
    //         return 2; // 时间常规
    //     }
    //     else if (game_status_msg_.stage_remain_time > params_.time.critical && game_status_msg_.stage_remain_time <= params_.time.tense)
    //     {
    //         return 3; // 时间紧张
    //     }
    //     else if (game_status_msg_.stage_remain_time <= params_.time.critical) {
    //         return 4; // 时间严重紧张
    //     }
    //     else if (game_status_msg_.stage_remain_time == 0)
    //     {
    //         return 5;
    //     }
    //     else {
    //         RCLCPP_WARN(get_logger(), "未知时间状态: %d", game_status_msg_.stage_remain_time);
    //         return -1;
    //     }
    //     return 0;

    // }

    void RobotDecisionNode::robotDecision()
    {
        auto stop_msg = std_msgs::msg::Bool();
        stop_msg.data = false;
        stop_flag_pub_->publish(stop_msg);
        // int current_game_state = getGameTimeState();
        int current_hp = robot_status_msg_.current_hp;
        int projectile_allowance = robot_status_msg_.projectile_allowance_17mm;
        int maximum_hp = robot_status_msg_.maximum_hp;
        RCLCPP_INFO(get_logger(), "当前血量: %d , 当前弹药: %d, 最大血量: %d", 
         current_hp, projectile_allowance, maximum_hp);

        const double tolerance = params_.config.path_tolerance; // 定义容差阈值
        size_t current_goal_index = current_patrol_index_;

        switch(robot_move_) {
            case true:
                if (projectile_allowance <= 70) 
                {
                    sendNavigationGoal(
                        rmul_goal_point_[0].x,
                        rmul_goal_point_[0].y,
                        rmul_goal_point_[0].z);
                        break;
                triggerStopSequence();
                    
                }

                if(game_status_msg_.stage_remain_time>0 && game_status_msg_.stage_remain_time<=420)
                 {
                    if(current_hp>params_.hp.normal.medium&&current_hp <=params_.hp.normal.high)
                    {
                        handleNormalHpBehavior(current_goal_index, tolerance);
                        break;
                    }
                    else if(current_hp>params_.hp.normal.low&&current_hp <=params_.hp.normal.medium)
                    {
                        handleLowHpBehavior(tolerance);
                        break;
                    }
                    else
                    {
                        handleCriticalHpBehavior(tolerance);
                        break;
                    }
                    break;
                }
                // switch (current_game_state)
                // {
                //     case 1: // 时间充足
                //         if(current_hp>params_.hp.abundant.medium&&current_hp <=params_.hp.abundant.high) 
                //         {
                //             // handleNormalHpBehavior(current_goal_index, tolerance);
                //             sendNavigationGoal(
                //                 rmul_goal_point_[4].x, 
                //                 rmul_goal_point_[4].y, 
                //                 rmul_goal_point_[4].z);

                //                 auto stop_msg = std_msgs::msg::Bool();
                //                 stop_msg.data = true;
                //                 stop_flag_pub_->publish(stop_msg);
                //             break;
                //         }
                //         else if(current_hp>params_.hp.abundant.low&&current_hp <=params_.hp.abundant.medium)
                //         {
                //             handleLowHpBehavior(tolerance);
                //             break;
                //         }
                //         else
                //         {
                //             handleCriticalHpBehavior(tolerance);
                //             if(checkDistance(rmul_goal_point_[0], tolerance) &&
                //             current_hp == maximum_hp)
                //             {
                //                 // handleNormalHpBehavior(current_goal_index, tolerance);
                //                 auto stop_msg = std_msgs::msg::Bool();
                //                 stop_msg.data = false;
                //                 stop_flag_pub_->publish(stop_msg);
                //                 sendNavigationGoal(
                //                 rmul_goal_point_[4].x, 
                //                 rmul_goal_point_[4].y, 
                //                 rmul_goal_point_[4].z);
                //             }
                //             break;
                //         }
                //         break;

                //     case 2: // 时间常规
                //         if(current_hp>params_.hp.normal.medium&&current_hp <=params_.hp.normal.high) 
                //         {
                //             handleNormalHpBehavior(current_goal_index, tolerance);
                //             break;
                //         }
                //         else if(current_hp>params_.hp.normal.low&&current_hp <=params_.hp.normal.medium)
                //         {
                //             handleLowHpBehavior(tolerance);
                //             break;
                //         }
                //         else
                //         {
                //             handleCriticalHpBehavior(tolerance);
                //             if (checkDistance(rmul_goal_point_[0], tolerance) && 
                //             current_hp == maximum_hp) 
                //             {
                //                 handleNormalHpBehavior(current_goal_index, tolerance);
                //             }
                //             break;
                //         }
                //         break;
                //     case 3: // 时间紧张
                //         if(current_hp>params_.hp.tense.medium&&current_hp <=params_.hp.tense.high) 
                //         {
                //             handleNormalHpBehavior(current_goal_index, tolerance);
                //             break;
                //         }
                //         else if(current_hp>params_.hp.tense.low&&current_hp <=params_.hp.tense.medium)
                //         {
                //             handleLowHpBehavior(tolerance);
                //             break;
                //         }
                //         else
                //         {
                //             handleCriticalHpBehavior(tolerance);
                //             if (checkDistance(rmul_goal_point_[0], tolerance) && 
                //             current_hp == maximum_hp) 
                //             {
                //                 handleNormalHpBehavior(current_goal_index, tolerance);
                //             }
                //             break;
                //         }
                //         break;
                //     case 4: // 时间严重紧张
                //         if(current_hp>params_.hp.critical.medium&&current_hp <=params_.hp.critical.high) 
                //         {
                //             handleNormalHpBehavior(current_goal_index, tolerance);
                //             break;
                //         }
                //         else if(current_hp>params_.hp.critical.low&&current_hp <=params_.hp.critical.medium)
                //         {
                //             handleLowHpBehavior(tolerance);
                //             break;
                //         }
                //         else
                //         {
                //             handleCriticalHpBehavior(tolerance);
                //             if (checkDistance(rmul_goal_point_[0], tolerance) && 
                //             current_hp == maximum_hp) 
                //             {
                //                 handleNormalHpBehavior(current_goal_index, tolerance);
                //             }
                //             break;
                //         }
                //         break;
                //     case 5:
                //         sendNavigationGoal(
                //             rmul_goal_point_[5].x,
                //             rmul_goal_point_[5].y,
                //             rmul_goal_point_[5].z);
                //         break;
                //     default:
                //         RCLCPP_ERROR(get_logger(), "未知游戏状态");
                //         break;
                // }
                //break;
            default:
                RCLCPP_WARN(get_logger(), "移动禁止状态");
                break;
            }

    }



    // 辅助函数实现

    // void RobotDecisionNode::handleNormalHpBehavior(size_t& index, double tolerance) {
    //     RCLCPP_INFO(get_logger(), "血量良好 (>250<=400)，执行常规任务");

    //     // 确保索引在0-2范围内
    //     index = index % 3;
    //     const size_t target_index = index + 1;  // 映射到1-3号点

    //     // 越界保护
    //     if (target_index >= rmul_goal_point_.size()) {
    //         RCLCPP_ERROR(get_logger(), "目标点索引越界，重置为初始点");
    //         index = 0;
    //         return;
    //     }

    //     // 发布当前目标点
    //     const auto& target = rmul_goal_point_[target_index];
    //     sendNavigationGoal(target.x, target.y, target.z);
    //     // 检查是否到达目标点
    //     if (checkDistance(target, tolerance) /*||
    //     last_nav_result_ == rclcpp_action::ResultCode::ABORTED*/) {
    //RCLCPP_INFO(get_logger(), "血量良好 (>250<=400)，执行常规任务");

        // 动态计算最大索引（假设目标点1对应索引0，目标点3对应索引2）
    //         // 记录当前到达的点
    //         RCLCPP_INFO(get_logger(), "已到达任务点 [%zu]", target_index);

    //         // 🔔 触发 stop=true -> 10秒后 stop=false
    //         triggerStopSequence();

    //         // 更新到下一个目标点
    //         index = (index + 1) % 3;
    //         const size_t next_target_index = index + 1;
    //         RCLCPP_INFO(get_logger(), "下一目标点 [%zu]", next_target_index);
    //     }
    // }

    void RobotDecisionNode::handleNormalHpBehavior(size_t& index, double tolerance)
    
    {
        (void)index;
        const size_t target_index = current_patrol_index_;
        RCLCPP_INFO(get_logger(), "血量良好 (>250<=400)，执行常规任务");

        const auto& target = rmul_goal_point_[target_index];
        if (checkDistance(target, tolerance) ||
            last_nav_result_ == rclcpp_action::ResultCode::SUCCEEDED) {
            advancePatrolIndex();
            const auto& next_target = rmul_goal_point_[current_patrol_index_];
            sendNavigationGoal(next_target.x, next_target.y, next_target.z);
            return;
        }

        sendNavigationGoal(target.x, target.y, target.z);
    }

    void RobotDecisionNode::handleLowHpBehavior(double tolerance)
    {
        RCLCPP_INFO(get_logger(), "血量较低 (150<x<250)，寻找最近退防点");

        if (!has_navigation_feedback_) {
            RCLCPP_WARN(get_logger(), "等待导航反馈数据...");
            return ;
        }

        const auto& current_position = current_feedback_->current_pose.pose.position;
        const size_t nearest_index = findNearestPoint(current_position);

        if (nearest_index >= rmul_goal_point_.size()) {
            RCLCPP_ERROR(get_logger(), "无效索引: %zu", nearest_index);
            return;
        }

        const auto& target_point = rmul_goal_point_[nearest_index];
        sendNavigationGoal(target_point.x, target_point.y, target_point.z);

        if (checkDistance(target_point, tolerance)) {
            RCLCPP_INFO(get_logger(), "已到达退防点[%zu]", nearest_index);
            triggerStopSequence();
        }

        RCLCPP_INFO(get_logger(), "正在前往退防点[%zu] (%.2f, %.2f)", nearest_index, target_point.x, target_point.y);

        // 修改handleLowHpBehavior中的距离计算（第447-448行）
        RCLCPP_INFO(get_logger(), "目标点坐标: (%.2f,%.2f) 当前距离: %.2f",
            target_point.x, target_point.y,
            sqrt(pow(current_feedback_->current_pose.pose.position.x-target_point.x, 2) +
                pow(current_feedback_->current_pose.pose.position.y-target_point.y, 2)));
    }

    // 修改后的危急血处理函数
    void RobotDecisionNode::handleCriticalHpBehavior(double tolerance)
    {
       RCLCPP_INFO(get_logger(), "血量危急 (<150)，回到安全点");
        
        // 发布目标点
        const auto& target = rmul_goal_point_[0];

        sendNavigationGoal(target.x, target.y, target.z);
        if (checkDistance(target, tolerance)) {
            triggerStopSequence();
        }
        // 到达检测和停止指令发送
        // if (checkDistance(target, tolerance)) {
        //     // triggerStopSequence();
        // } else {
        //     // 如果正在前往安全点但未到达，保持停止指令为false
        //     auto stop_msg = std_msgs::msg::Bool();
        //     stop_msg.data = false;
        //     stop_flag_pub_->publish(stop_msg);
        // }
    }

    bool RobotDecisionNode::checkDistance(const geometry_msgs::msg::Point& target, double tolerance)
    {

        if (!has_navigation_feedback_) {
            RCLCPP_WARN(get_logger(), "等待导航反馈数据...");
            return false;
        }


        const auto& current_pose = current_feedback_->current_pose.pose.position;
        const double dx = current_pose.x - target.x;
        const double dy = current_pose.y - target.y;
        return (sqrt(dx*dx + dy*dy) <= tolerance);
    }

    size_t RobotDecisionNode::findNearestPoint(const geometry_msgs::msg::Point& reference_point)
    {
    
                // 安全检查
        if (rmul_goal_point_.empty()) {
            RCLCPP_ERROR(get_logger(), "目标点列表为空！");
            return 0;
        }

        if (!has_navigation_feedback_) {
            RCLCPP_WARN(get_logger(), "导航反馈异常，返回安全点");
            return 0; // 默认返回索引0的安全点
        }

        // 初始化最近点索引和最小距离
        size_t nearest_index = 0;
        double min_distance = std::numeric_limits<double>::max();
    
        // 假设安全点之后的点都是退防点（索引1-4）
        const bool is_critical_mode = (&reference_point == &rmul_goal_point_[0]);
        const size_t start_index = is_critical_mode ? 1 : 0;  // 危急模式跳过安全点
        const size_t end_index = (rmul_goal_point_.size() > 1) ?
                                (rmul_goal_point_.size() - 1) : 0; // 确保索引不越界
    
        for (size_t i = start_index; i <= end_index; ++i) {
            const double dx = reference_point.x - rmul_goal_point_[i].x;
            const double dy = reference_point.y - rmul_goal_point_[i].y;
            const double distance = dx*dx + dy*dy;
    
            if (distance < min_distance) {
                min_distance = distance;
                nearest_index = i;
            }
        }
        RCLCPP_DEBUG(get_logger(), "参考点: (%.2f,%.2f) 搜索范围: %zu-%zu",
    reference_point.x, reference_point.y, start_index, end_index);

        RCLCPP_DEBUG(get_logger(), "最近退防点索引: %zu (距离: %.2f)", 
            nearest_index, sqrt(min_distance));
        return nearest_index;
        
    }

    bool RobotDecisionNode::isSameGoal(const geometry_msgs::msg::Point& target, float yaw) const
    {
        if (!has_goal_target_) {
            return false;
        }

        constexpr double position_epsilon = 1e-3;
        constexpr double yaw_epsilon = 1e-3;
        const bool same_position =
            std::fabs(current_goal_target_.x - target.x) < position_epsilon &&
            std::fabs(current_goal_target_.y - target.y) < position_epsilon &&
            std::fabs(current_goal_target_.z - target.z) < position_epsilon;
        const bool same_yaw = std::fabs(current_goal_yaw_ - yaw) < yaw_epsilon;

        return same_position && same_yaw && (goal_request_pending_ || goal_active_);
    }

    void RobotDecisionNode::advancePatrolIndex()
    {
        const int next_index = static_cast<int>(current_patrol_index_) + patrol_direction_;

        if (next_index > static_cast<int>(patrol_max_index_)) {
            patrol_direction_ = -1;
            current_patrol_index_ = patrol_max_index_ - 1;
        } else if (next_index < static_cast<int>(patrol_min_index_)) {
            patrol_direction_ = 1;
            current_patrol_index_ = patrol_min_index_ + 1;
        } else {
            current_patrol_index_ = static_cast<size_t>(next_index);
        }

        last_nav_result_ = rclcpp_action::ResultCode::UNKNOWN;
        RCLCPP_INFO(get_logger(), "更新巡逻索引至: %zu", current_patrol_index_);
    }

    void RobotDecisionNode::triggerStopSequence()
{
    if (!stop_flag_pub_) {
        RCLCPP_WARN(get_logger(), "停止标志发布器未初始化");
        return;
    }

    // 立即发布 stop = true
    auto stop_msg = std_msgs::msg::Bool();
    stop_msg.data = true;
    stop_flag_pub_->publish(stop_msg);
    RCLCPP_INFO(get_logger(), "[Stop] 已发送 stop=true");

    // // 防止计时器重复创建
    // if (stop_reset_timer_) {
    //     stop_reset_timer_->cancel();
    // }

    // // 启动一个一次性定时器，在10秒后发布 stop=false
    // stop_reset_timer_ = this->create_wall_timer(
    //     std::chrono::seconds(10),
    //     [this]() {
    //         auto stop_msg = std_msgs::msg::Bool();
    //         stop_msg.data = false;
    //         stop_flag_pub_->publish(stop_msg);
    //         RCLCPP_INFO(this->get_logger(), "[Stop] 10秒后发送 stop=false");

    //         // 关闭并释放定时器
    //         stop_reset_timer_->cancel();
    //         stop_reset_timer_.reset();
    //     });
}

    void RobotDecisionNode::logDecisionState(int hp, size_t index)
    {
        RCLCPP_INFO(get_logger(), "[状态] HP: %d, 移动: %s", 
                hp, robot_move_ ? "允许" : "禁止");
        RCLCPP_INFO(get_logger(), "[目标点] 索引: %zu 坐标: (%.2f, %.2f)", 
                index,
                rmul_goal_point_[index].x,
                rmul_goal_point_[index].y);
    }

} // namespace standard_robot_pp_ros2

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(standard_robot_pp_ros2::RobotDecisionNode)
