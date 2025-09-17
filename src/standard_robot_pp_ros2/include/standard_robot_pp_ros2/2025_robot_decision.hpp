#ifndef ROBOT_DECISION_BT__2025_ROBOT_DECISION_HPP_
#define ROBOT_DECISION_BT__2025_ROBOT_DECISION_HPP_

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

#include <std_msgs/msg/bool.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/point.hpp>
#include <nav2_msgs/action/navigate_to_pose.hpp>

#include <pb_rm_interfaces/msg/game_status.hpp>
#include <pb_rm_interfaces/msg/rfid_status.hpp>
#include <pb_rm_interfaces/msg/robot_status.hpp>

#include <behaviortree_cpp_v3/bt_factory.h>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>

#include <vector>
#include <string>
#include <mutex>

namespace standard_robot_pp_ros2
{

  struct GoalPoint { double x{0}, y{0}, z{0}; };

  struct Thresholds { int high{0}, medium{0}, low{0}; };

  struct Params {
    struct { int abundant{420}, normal{360}, tense{180}, critical{60}; } time;
    struct {
      Thresholds abundant, normal, tense, critical;
    } hp;
    std::string nav_action{"/navigate_to_pose"};
    double path_tolerance{0.1};
  };

  class RobotDecisionNode : public rclcpp::Node
  {
  public:
    using NavigateToPose = nav2_msgs::action::NavigateToPose;
    using GoalHandleNav  = rclcpp_action::ClientGoalHandle<NavigateToPose>;

    explicit RobotDecisionNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

  private:
    // ---------- ROS ----------
    rclcpp::Subscription<pb_rm_interfaces::msg::GameStatus>::SharedPtr game_status_sub_;
    rclcpp::Subscription<pb_rm_interfaces::msg::RfidStatus>::SharedPtr  rfid_status_sub_;
    rclcpp::Subscription<pb_rm_interfaces::msg::RobotStatus>::SharedPtr robot_status_sub_;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr stop_flag_pub_;
    rclcpp_action::Client<NavigateToPose>::SharedPtr nav_client_;

    rclcpp::TimerBase::SharedPtr tick_timer_;
    std::mutex nav_mutex_;
    GoalHandleNav::SharedPtr current_goal_handle_;
    rclcpp_action::ResultCode last_nav_result_{rclcpp_action::ResultCode::UNKNOWN};

    // 最新状态
    pb_rm_interfaces::msg::GameStatus  game_status_;
    pb_rm_interfaces::msg::RfidStatus  rfid_status_;
    pb_rm_interfaces::msg::RobotStatus robot_status_;

    // 目标点
    std::vector<GoalPoint> goal_points_;

    // 参数
    Params params_;

    // 巡逻状态（1..max 在多个巡逻点中往返）
    size_t patrol_index_{1};
    int    patrol_direction_{1};
    size_t patrol_min_index_{1};
    size_t patrol_max_index_{1}; // 初始化后根据点数量设定为 size-3

    // ---------- BT ----------
    BT::BehaviorTreeFactory bt_factory_;
    BT::Tree bt_tree_;

    // ---------- 内部函数 ----------
    void loadParams();
    void setupPatrolRange();
    void registerBTNodes();
    void tickOnce();

    // 条件用
    int  getGameTimeState() const;

    // 动作用
    void sendNavigationGoal(double x, double y, double z, double yaw_rad = 0.0);
    void cancelCurrentGoal();
    void triggerStopOnce();

    // 回调
    void onGameStatus(const pb_rm_interfaces::msg::GameStatus::SharedPtr msg);
    void onRfidStatus (const pb_rm_interfaces::msg::RfidStatus::SharedPtr  msg);
    void onRobotStatus(const pb_rm_interfaces::msg::RobotStatus::SharedPtr msg);
  };
}

#endif  // ROBOT_DECISION_BT__2025_ROBOT_DECISION_HPP_

