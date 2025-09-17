#include "standard_robot_pp_ros2/2025_robot_decision.hpp"

#include <ament_index_cpp/get_package_share_directory.hpp>
#include <behaviortree_cpp_v3/utils/shared_library.h>
#include <chrono>
#include <stdexcept>
#include <sstream>

using namespace std::chrono_literals;
namespace standard_robot_pp_ros2
{
  RobotDecisionNode::RobotDecisionNode(const rclcpp::NodeOptions & options)
  : rclcpp::Node("robot_decision_node", options)
  {
    RCLCPP_INFO(get_logger(), "RobotDecisionNode (BehaviorTree) starting...");

    // ---- ROS I/O ----
    game_status_sub_ = create_subscription<pb_rm_interfaces::msg::GameStatus>(
      "referee/game_status", 10, std::bind(&RobotDecisionNode::onGameStatus, this, std::placeholders::_1));

    rfid_status_sub_ = create_subscription<pb_rm_interfaces::msg::RfidStatus>(
      "referee/rfid_status", 10, std::bind(&RobotDecisionNode::onRfidStatus, this, std::placeholders::_1));

    robot_status_sub_ = create_subscription<pb_rm_interfaces::msg::RobotStatus>(
      "referee/robot_status", 10, std::bind(&RobotDecisionNode::onRobotStatus, this, std::placeholders::_1));

    stop_flag_pub_ = create_publisher<std_msgs::msg::Bool>("stop_flag", 10);

    // ---- 参数 ----
    loadParams();
    setupPatrolRange();

    // ---- Nav2 Action ----
    nav_client_ = rclcpp_action::create_client<NavigateToPose>(this, params_.nav_action);
    if (!nav_client_->wait_for_action_server(5s)) {
      RCLCPP_ERROR(get_logger(), "Nav2 action server [%s] not available", params_.nav_action.c_str());
      throw std::runtime_error("Nav2 action server not available");
    }

    // ---- 注册 BT 节点并加载 XML ----
    registerBTNodes();
    const auto share_dir = ament_index_cpp::get_package_share_directory("robot_decision_bt");
    const auto xml_path  = share_dir + std::string("/bt_xml/robot_decision_tree.xml");
    bt_tree_ = bt_factory_.createTreeFromFile(xml_path);
    RCLCPP_INFO(get_logger(), "Loaded BT XML: %s", xml_path.c_str());

    // ---- 定时 tick ----
    tick_timer_ = create_wall_timer(1s, std::bind(&RobotDecisionNode::tickOnce, this));
  }

  void RobotDecisionNode::loadParams()
  {
    // nav action
    declare_parameter<std::string>("robot_decision.decision_config.nav2_action_server", params_.nav_action);
    get_parameter("robot_decision.decision_config.nav2_action_server", params_.nav_action);

    declare_parameter<double>("robot_decision.decision_config.path_tolerance", params_.path_tolerance);
    get_parameter("robot_decision.decision_config.path_tolerance", params_.path_tolerance);

    // goal points
    std::vector<double> gx, gy, gz;
    const std::vector<double> default_goals;
    declare_parameter<std::vector<double>>("goal_points.x", default_goals);
    declare_parameter<std::vector<double>>("goal_points.y", default_goals);
    declare_parameter<std::vector<double>>("goal_points.z", default_goals);
    get_parameter("goal_points.x", gx);
    get_parameter("goal_points.y", gy);
    get_parameter("goal_points.z", gz);

    if (gx.size() != gy.size() || gx.size() != gz.size() || gx.size() < 4) {
      RCLCPP_FATAL(get_logger(), "goal_points size invalid. Need >=4 and x/y/z same size.");
      throw std::runtime_error("Invalid goal_points");
    }
    goal_points_.resize(gx.size());
    for (size_t i = 0; i < gx.size(); ++i) {
      goal_points_[i] = GoalPoint{gx[i], gy[i], gz[i]};
    }
    RCLCPP_INFO(get_logger(), "Loaded %zu goal points", goal_points_.size());

    // time thresholds
    declare_parameter<int>("time_thresholds.abundant", params_.time.abundant);
    declare_parameter<int>("time_thresholds.normal",   params_.time.normal);
    declare_parameter<int>("time_thresholds.tense",    params_.time.tense);
    declare_parameter<int>("time_thresholds.critical", params_.time.critical);
    get_parameter("time_thresholds.abundant", params_.time.abundant);
    get_parameter("time_thresholds.normal",   params_.time.normal);
    get_parameter("time_thresholds.tense",    params_.time.tense);
    get_parameter("time_thresholds.critical", params_.time.critical);

    // hp thresholds
    auto load_hp = [this](const std::string& ns, Thresholds& t) {
      declare_parameter<int>(ns + ".high",   300);
      declare_parameter<int>(ns + ".medium", 200);
      declare_parameter<int>(ns + ".low",    150);
      get_parameter(ns + ".high",   t.high);
      get_parameter(ns + ".medium", t.medium);
      get_parameter(ns + ".low",    t.low);
    };
    load_hp("hp_thresholds.abundant", params_.hp.abundant);
    load_hp("hp_thresholds.normal",   params_.hp.normal);
    load_hp("hp_thresholds.tense",    params_.hp.tense);
    load_hp("hp_thresholds.critical", params_.hp.critical);
  }

  void RobotDecisionNode::setupPatrolRange()
  {
    // 索引规则：0=supply, [1..N-3]=patrol, N-2=defense, N-1=safe
    if (goal_points_.size() < 4) {
      throw std::runtime_error("Goal points must be >= 4");
    }
    patrol_min_index_ = 1;
    patrol_max_index_ = goal_points_.size() - 3;  // 至少 = 1
    if (patrol_max_index_ < patrol_min_index_) {
      patrol_max_index_ = patrol_min_index_;
    }
    patrol_index_ = patrol_min_index_;
    patrol_direction_ = 1;

    std::stringstream ss;
    ss << "Patrol range: [" << patrol_min_index_ << " .. " << patrol_max_index_ << "]";
    RCLCPP_INFO(get_logger(), "%s", ss.str().c_str());
  }

  void RobotDecisionNode::registerBTNodes()
  {
    // --- 条件 ---
    bt_factory_.registerSimpleCondition("IsGameStarted", [this](BT::TreeNode &) {
      return (game_status_.game_progress == 4) ?
            BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    });

    bt_factory_.registerSimpleCondition("IsAmmoLow", [this](BT::TreeNode &) {
      // 17mm 弹丸余量 <= 70 视为低弹药
      return (robot_status_.projectile_allowance_17mm <= 70) ?
            BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    });

    bt_factory_.registerSimpleCondition("IsHpNormal", [this](BT::TreeNode &) {
      const auto hp = static_cast<int>(robot_status_.current_hp);
      const auto& th = params_.hp.normal;
      return (hp > th.low && hp <= th.high) ?
            BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    });

    bt_factory_.registerSimpleCondition("IsHpLow", [this](BT::TreeNode &) {
      const auto hp = static_cast<int>(robot_status_.current_hp);
      const auto& th = params_.hp.normal; // 以 normal 区间的 medium 作为\"较低\"上界
      return (hp > th.low && hp <= th.medium) ?
            BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    });

    bt_factory_.registerSimpleCondition("IsHpCritical", [this](BT::TreeNode &) {
      const auto hp = static_cast<int>(robot_status_.current_hp);
      return (hp <= params_.hp.critical.low) ?
            BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    });

    bt_factory_.registerSimpleCondition("IsTimeNormal", [this](BT::TreeNode &) {
      int s = getGameTimeState();
      return (s == 1 || s == 2) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    });

    bt_factory_.registerSimpleCondition("IsTimeTense", [this](BT::TreeNode &) {
      return (getGameTimeState() == 3) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    });

    bt_factory_.registerSimpleCondition("IsTimeCritical", [this](BT::TreeNode &) {
      return (getGameTimeState() == 4) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    });

    bt_factory_.registerSimpleCondition("IsTimeOver", [this](BT::TreeNode &) {
      return (getGameTimeState() == 5) ? BT::NodeStatus::SUCCESS : BT::NodeStatus::FAILURE;
    });

    // --- 动作：供给/退防/安全 ---
    auto send_to_index = [this](size_t idx, const char* name){
      return [this, idx, name](BT::TreeNode&) {
        if (idx >= goal_points_.size()) {
          RCLCPP_ERROR(get_logger(), "[%s] index out of range: %zu", name, idx);
          return BT::NodeStatus::FAILURE;
        }
        const auto& p = goal_points_[idx];
        RCLCPP_INFO(get_logger(), "GoTo(%s) -> (%.2f, %.2f, %.2f)", name, p.x, p.y, p.z);
        sendNavigationGoal(p.x, p.y, p.z, 0.0);
        return BT::NodeStatus::SUCCESS;
      };
    };

    bt_factory_.registerSimpleAction("SendNav_Supply",  send_to_index(0, "supply"));
    bt_factory_.registerSimpleAction("SendNav_Defense", send_to_index(goal_points_.size()-2, "defense"));
    bt_factory_.registerSimpleAction("SendNav_Safe",    send_to_index(goal_points_.size()-1, "safe"));

    // --- 动作：巡逻（多点往返） ---
    bt_factory_.registerSimpleAction("SendNav_Patrol", [this](BT::TreeNode&) {
      if (goal_points_.empty()) return BT::NodeStatus::FAILURE;

      // 发送当前巡逻点
      const auto idx = patrol_index_;
      const auto& p = goal_points_[idx];
      RCLCPP_INFO(get_logger(), "Patrol -> idx=%zu (%.2f, %.2f, %.2f) dir=%d",
                  idx, p.x, p.y, p.z, patrol_direction_);
      sendNavigationGoal(p.x, p.y, p.z, 0.0);

      // 更新索引（往返）
      patrol_index_ += patrol_direction_;
      if (patrol_index_ > patrol_max_index_) {
        patrol_index_ = (patrol_max_index_ > patrol_min_index_) ? patrol_max_index_ - 1 : patrol_min_index_;
        patrol_direction_ = -1;
      } else if (patrol_index_ < patrol_min_index_) {
        patrol_index_ = (patrol_max_index_ > patrol_min_index_) ? patrol_min_index_ + 1 : patrol_min_index_;
        patrol_direction_ = 1;
      }

      return BT::NodeStatus::SUCCESS;
    });

    // --- 动作：停止/取消 ---
    bt_factory_.registerSimpleAction("TriggerStop", [this](BT::TreeNode&) {
      triggerStopOnce();
      return BT::NodeStatus::SUCCESS;
    });

    bt_factory_.registerSimpleAction("CancelGoal", [this](BT::TreeNode&) {
      cancelCurrentGoal();
      return BT::NodeStatus::SUCCESS;
    });
  }

  void RobotDecisionNode::tickOnce()
  {
    auto status = bt_tree_.tickRoot();
    RCLCPP_DEBUG(get_logger(), "BT tick -> %s", BT::toStr(status, true).c_str());
  }

  int RobotDecisionNode::getGameTimeState() const
  {
    // 返回：1=充足, 2=常规, 3=紧张, 4=危急, 5=结束
    const int t = static_cast<int>(game_status_.stage_remain_time);
    if (t == 0)                 return 5;
    if (t <= params_.time.critical) return 4;
    if (t <= params_.time.tense)    return 3;
    if (t <= params_.time.normal)   return 2;
    return 1;
  }

  void RobotDecisionNode::sendNavigationGoal(double x, double y, double z, double yaw_rad)
  {
    if (!nav_client_ || !nav_client_->action_server_is_ready()) {
      RCLCPP_WARN(get_logger(), "Nav2 action server not ready");
      return;
    }

    std::lock_guard<std::mutex> lk(nav_mutex_);
    // 取消旧目标
    if (current_goal_handle_) {
      nav_client_->async_cancel_goal(current_goal_handle_);
      current_goal_handle_.reset();
    }

    NavigateToPose::Goal goal;
    goal.pose.header.stamp = now();
    goal.pose.header.frame_id = "map";
    goal.pose.pose.position.x = x;
    goal.pose.pose.position.y = y;
    goal.pose.pose.position.z = z;

    tf2::Quaternion q; q.setRPY(0, 0, yaw_rad);
    goal.pose.pose.orientation = tf2::toMsg(q);

    auto opts = rclcpp_action::Client<NavigateToPose>::SendGoalOptions();
    opts.goal_response_callback = [this](GoalHandleNav::SharedPtr gh) {
      if (!gh) RCLCPP_WARN(get_logger(), "Goal rejected");
      current_goal_handle_ = gh;
    };
    opts.result_callback = [this](const GoalHandleNav::WrappedResult & res) {
      last_nav_result_ = res.code;
      RCLCPP_INFO(get_logger(), "Nav2 result: %d", static_cast<int>(res.code));
    };

    nav_client_->async_send_goal(goal, opts);
  }

  void RobotDecisionNode::cancelCurrentGoal()
  {
    std::lock_guard<std::mutex> lk(nav_mutex_);
    if (current_goal_handle_) {
      nav_client_->async_cancel_goal(current_goal_handle_);
      current_goal_handle_.reset();
      RCLCPP_INFO(get_logger(), "Cancelled current Nav2 goal");
    }
  }

  void RobotDecisionNode::triggerStopOnce()
  {
    std_msgs::msg::Bool msg; msg.data = true;
    stop_flag_pub_->publish(msg);
    RCLCPP_INFO(get_logger(), "Publish stop_flag = true");
  }

  void RobotDecisionNode::onGameStatus(const pb_rm_interfaces::msg::GameStatus::SharedPtr msg)
  {
    game_status_ = *msg;
  }

  void RobotDecisionNode::onRfidStatus(const pb_rm_interfaces::msg::RfidStatus::SharedPtr msg)
  {
    rfid_status_ = *msg;
  }

  void RobotDecisionNode::onRobotStatus(const pb_rm_interfaces::msg::RobotStatus::SharedPtr msg)
  {
    robot_status_ = *msg;
  }
}
