#ifndef RMUL_ROBOT_DECISION_HPP
#define RMUL_ROBOT_DECISION_HPP

#include <rclcpp/rclcpp.hpp>
#include "pb_rm_interfaces/msg/buff.hpp"
#include "pb_rm_interfaces/msg/event_data.hpp"
#include "pb_rm_interfaces/msg/game_robot_hp.hpp"
#include "pb_rm_interfaces/msg/game_status.hpp"
#include "pb_rm_interfaces/msg/ground_robot_position.hpp"
#include "pb_rm_interfaces/msg/rfid_status.hpp"
#include "pb_rm_interfaces/msg/robot_status.hpp"
// #include "geometry_msgs/msg/pose_stamped.hpp" // 添加PoseStamped消息支持
#include <tf2/LinearMath/Quaternion.h>
#include <random> 
#include "standard_robot_pp_ros2/packet_typedef.hpp"
#include "standard_robot_pp_ros2/2025_robot_decision.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include<cmath>
#include "nav2_msgs/action/navigate_through_poses.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp> // 添加tf2转换支持
#include <std_msgs/msg/bool.hpp>
#include <limits>

namespace standard_robot_pp_ros2
{

class RobotDecisionNode : public rclcpp::Node
{
public:
    // 构造函数，初始化RobotDecisionNode对象，接受NodeOptions参数
    explicit RobotDecisionNode(const rclcpp::NodeOptions & options);

private:

    // Action客户端类型定义
    using NavigateThroughPoses = nav2_msgs::action::NavigateThroughPoses;
    using GoalHandleNavigate = rclcpp_action::ClientGoalHandle<NavigateThroughPoses>;

    // 参数结构体定义
    struct DecisionParams
    {
        struct TimeThresholds {
            int abundant;
            int normal;
            int tense;
            int critical;
        } time;

        struct HpThresholds {
            struct TimeStages {
                int high;
                int medium;
                int low;

            };

            TimeStages abundant;
            TimeStages normal;
            TimeStages tense;
            TimeStages critical;
        } hp;

        struct DecisionConfig {
            double path_tolerance;
            double patrol_switch_distance;
            bool patrol_only_mode;
            bool force_patrol_enable;
            int patrol_cycle_limit;
            int patrol_min_index;
            int patrol_max_index;
            std::string nav2_action_server;  // 新增动作服务名称参数
        } config;
    };
    
    // Action客户端
    rclcpp_action::Client<NavigateThroughPoses>::SharedPtr nav2_client_;
    GoalHandleNavigate::SharedPtr current_goal_handle_;

    //参数存储
    std::vector<geometry_msgs::msg::Point> rmul_goal_point_;
    DecisionParams params_;

    //回调函数说明
    void gameStatusCallback(const pb_rm_interfaces::msg::GameStatus::SharedPtr msg);    // 消息订阅回调函数，处理接收到的比赛状态信息
    void rfidStatusCallback(const pb_rm_interfaces::msg::RfidStatus::SharedPtr msg);    // 消息订阅回调函数，处理接收到的RFID状态信息
    void robotStatusCallback(const pb_rm_interfaces::msg::RobotStatus::SharedPtr msg);    // 消息订阅回调函数，处理接收到的机器人状态信息

    //功能函数说明
    void executeDecisionLogic();    // 执行决策逻辑的函数
    // void processRFIDData();    // RFID数据处理函数
    // void sendNavigationGoal(float goal_x, float goal_y,float goal_z= 0.0f, float goal_yaw = 0.0f);    // 发布目标点函数（支持自定义 yaw 偏航角）
    // Action客户端类型定义
    void sendNavigationGoal(float x, float y, float z = 0.0f, float yaw = 0.0f);
    // int getGameTimeState();    // 比赛时间划分阶段函数
    void robotDecision();    // ATS_rmul2025决策函数
    void handleNormalHpBehavior(size_t& index, double tolerance);
    void handleLowHpBehavior(double tolerance);
    void handleCriticalHpBehavior(double tolerance);
    size_t findNearestPoint(const geometry_msgs::msg::Point&);
    bool checkDistance(const geometry_msgs::msg::Point&, double);
    double distanceToTarget(const geometry_msgs::msg::Point&) const;
    bool isSameGoal(const geometry_msgs::msg::Point& target, float yaw) const;
    bool isSamePatrolSegmentGoal(size_t start_index, size_t end_index) const;
    size_t computeNextPatrolIndex(size_t start_index, int & direction) const;
    geometry_msgs::msg::PoseStamped buildPoseStamped(
        const geometry_msgs::msg::Point& target, float yaw) const;
    void sendPatrolSegmentGoal(size_t start_index, size_t end_index);
    void advancePatrolIndex();
    void logDecisionState(int hp, size_t index);
    void triggerStopSequence();

    // 成员变量

    int patrol_direction_ = 1;      // 1: 正向, -1: 反向
    size_t current_patrol_index_ = 1;  // 当前巡逻索引
    size_t patrol_min_index_ = 1;    // 最小索引（对应目标点1）
    size_t patrol_max_index_ = 3;    // 最大索引（对应目标点3）

    std::mutex nav_mutex_;  // 新增互斥锁
    pb_rm_interfaces::msg::GameStatus game_status_msg_; // 比赛状态、剩余时间信息
    pb_rm_interfaces::msg::RfidStatus rfid_status_msg_; // RFID状态信息
    pb_rm_interfaces::msg::RobotStatus robot_status_msg_; // （哨兵）机器人本体信息
    geometry_msgs::msg::PoseStamped current_pose_; // 机器人当前位置信息

    rclcpp::TimerBase::SharedPtr stop_reset_timer_;
    NavigateThroughPoses::Feedback::SharedPtr current_feedback_;
    rclcpp_action::ResultCode last_nav_result_;
    bool has_navigation_feedback_ = false;
    bool goal_request_pending_ = false;
    bool goal_active_ = false;
    bool has_goal_target_ = false;
    bool has_patrol_segment_target_ = false;
    geometry_msgs::msg::Point current_goal_target_;
    float current_goal_yaw_ = 0.0f;
    size_t current_patrol_segment_start_ = 0;
    size_t current_patrol_segment_end_ = 0;
    int completed_patrol_cycles_ = 0;
    bool patrol_finished_ = false;
    bool patrol_finish_stop_sent_ = false;
    bool robot_move_; // 哨兵运动标志位
    rclcpp::TimerBase::SharedPtr timer_;
    // bool rfid_capture_center_ = false; // 中心增益点捕获标志位

    //订阅者和发布者
    rclcpp::Subscription<pb_rm_interfaces::msg::GameStatus>::SharedPtr game_status_sub_;
    rclcpp::Subscription<pb_rm_interfaces::msg::RfidStatus>::SharedPtr rfid_status_sub_;
    rclcpp::Subscription<pb_rm_interfaces::msg::RobotStatus>::SharedPtr robot_status_sub_;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr goal_publisher_;
    rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr stop_flag_pub_;
    
};

} // namespace standard_robot_pp_ros2

#endif // RMUL_ROBOT_DECISION_HPP
