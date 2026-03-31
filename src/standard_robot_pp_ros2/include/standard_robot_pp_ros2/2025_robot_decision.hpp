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
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp> // 添加tf2转换支持
#include <std_msgs/msg/bool.hpp>
#include <limits>
#include <mutex>

namespace standard_robot_pp_ros2
{

class RobotDecisionNode : public rclcpp::Node
{
public:
    // 构造函数，初始化RobotDecisionNode对象，接受NodeOptions参数
    explicit RobotDecisionNode(const rclcpp::NodeOptions & options);

private:

    // Action客户端类型定义
    using NavigateToPose = nav2_msgs::action::NavigateToPose;
    using GoalHandleNavigate = rclcpp_action::ClientGoalHandle<NavigateToPose>;

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
            std::string nav2_action_server;  // 新增动作服务名称参数
            int decision_period_ms;
            double goal_republish_interval_s;
            double goal_position_tolerance;
            double waypoint_stop_duration_s;
        } config;
    };
    
    // Action客户端
    rclcpp_action::Client<NavigateToPose>::SharedPtr nav2_client_;
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
    int getGameTimeState();    // 比赛时间划分阶段函数
    void robotDecision();    // ATS_rmul2025决策函数
    void handleNormalHpBehavior(double tolerance);
    void handleLowHpBehavior(double tolerance);
    void handleCriticalHpBehavior(double tolerance);
    size_t findNearestPoint(
        const geometry_msgs::msg::Point& reference_point,
        const std::vector<size_t>& candidate_indices);
    bool checkDistance(const geometry_msgs::msg::Point&, double);
    void logDecisionState(int hp, size_t index);
    void triggerStopSequence();
    void publishStopFlag(bool stop);
    void updateStopState();

    // 成员变量

    size_t supply_safe_point_index_ = 0;  // 补给安全点索引
    std::vector<size_t> patrol_indices_;  // 正常血量巡逻点索引列表
    std::vector<size_t> low_hp_candidate_indices_;  // 低血量最近退防点候选列表
    size_t critical_time_target_index_ = 0;  // 时间严重紧张时强制目标点
    int patrol_direction_ = 1;  // 1: 正向, -1: 反向
    size_t patrol_cursor_ = 0;  // 巡逻列表游标
    size_t low_hp_target_index_ = std::numeric_limits<size_t>::max();

    std::mutex nav_mutex_;  // 新增互斥锁
    pb_rm_interfaces::msg::GameStatus game_status_msg_; // 比赛状态、剩余时间信息
    pb_rm_interfaces::msg::RfidStatus rfid_status_msg_; // RFID状态信息
    pb_rm_interfaces::msg::RobotStatus robot_status_msg_; // （哨兵）机器人本体信息
    geometry_msgs::msg::PoseStamped current_pose_; // 机器人当前位置信息

    rclcpp::TimerBase::SharedPtr stop_reset_timer_;
    NavigateToPose::Feedback::SharedPtr current_feedback_;
    rclcpp_action::ResultCode last_nav_result_;
    bool robot_move_; // 哨兵运动标志位
    bool has_feedback_ = false;
    bool has_last_goal_ = false;
    bool goal_request_pending_ = false;
    geometry_msgs::msg::Point last_goal_point_;
    double last_goal_yaw_ = 0.0;
    rclcpp::Time last_goal_send_time_;
    bool stop_flag_state_ = false;
    bool stop_hold_active_ = false;
    rclcpp::Time stop_release_time_;
    int last_game_progress_ = -1;
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
