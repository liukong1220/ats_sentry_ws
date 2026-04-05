#include "pb2025_sentry_behavior/plugins/action/send_nav_through_poses.hpp"

namespace pb2025_sentry_behavior
{

SendNavThroughPosesAction::SendNavThroughPosesAction(
  const std::string & name, const BT::NodeConfig & config)
: BT::StatefulActionNode(name, config), node_(decision::getNodeFromBlackboard(*this))
{
  logger_ = node_->get_logger();
  action_name_ = "/navigate_through_poses";
  node_->get_parameter("decision.decision_config.nav2_action_server", action_name_);
  node_->get_parameter(
    "decision.decision_config.goal_position_tolerance", path_compare_tolerance_);

  action_client_ = rclcpp_action::create_client<NavigateThroughPoses>(node_, action_name_);
}

BT::PortsList SendNavThroughPosesAction::providedPorts()
{
  return {
    BT::InputPort<nav_msgs::msg::Path>("path", "{decision_path}", "Decision path"),
    BT::OutputPort<geometry_msgs::msg::PoseStamped>(
      "current_pose", "{decision_current_pose}", "Current navigation feedback pose")};
}

BT::NodeStatus SendNavThroughPosesAction::onStart()
{
  auto path = getInput<nav_msgs::msg::Path>("path");
  if (!path || path->poses.empty()) {
    RCLCPP_ERROR(logger_, "SendNavThroughPosesAction did not receive a valid path input");
    return BT::NodeStatus::FAILURE;
  }

  if (!action_client_->wait_for_action_server(std::chrono::seconds(2))) {
    RCLCPP_ERROR(logger_, "Action server %s is not available", action_name_.c_str());
    return BT::NodeStatus::FAILURE;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if ((goal_pending_ || current_goal_handle_) &&
      decision::pathEquivalent(active_path_, *path, path_compare_tolerance_))
    {
      if (has_current_pose_) {
        setOutput("current_pose", latest_pose_);
      }
      return BT::NodeStatus::RUNNING;
    }
  }

  cancelCurrentGoal();

  NavigateThroughPoses::Goal goal;
  goal.poses = path->poses;
  for (auto & pose : goal.poses) {
    pose.header.stamp = node_->now();
    if (pose.header.frame_id.empty()) {
      pose.header.frame_id = "map";
    }
  }

  std::uint64_t request_id = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    request_id = ++goal_request_id_;
    result_received_ = false;
    goal_pending_ = true;
    goal_succeeded_ = false;
    last_result_code_ = rclcpp_action::ResultCode::UNKNOWN;
    active_path_ = *path;
  }

  auto send_goal_options =
    rclcpp_action::Client<NavigateThroughPoses>::SendGoalOptions();
  send_goal_options.goal_response_callback = [this, request_id](const GoalHandle::SharedPtr handle) {
      goalResponseCallback(request_id, handle);
    };
  send_goal_options.feedback_callback =
    [this, request_id](
      GoalHandle::SharedPtr handle,
      const std::shared_ptr<const NavigateThroughPoses::Feedback> feedback) {
        feedbackCallback(request_id, handle, feedback);
      };
  send_goal_options.result_callback = [this, request_id](const GoalHandle::WrappedResult & result) {
      resultCallback(request_id, result);
    };

  action_client_->async_send_goal(goal, send_goal_options);
  return BT::NodeStatus::RUNNING;
}

BT::NodeStatus SendNavThroughPosesAction::onRunning()
{
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (has_current_pose_) {
      setOutput("current_pose", latest_pose_);
    }

    if (!result_received_) {
      return BT::NodeStatus::RUNNING;
    }

    if (goal_succeeded_) {
      current_goal_handle_.reset();
      goal_pending_ = false;
      return BT::NodeStatus::SUCCESS;
    }

    current_goal_handle_.reset();
    goal_pending_ = false;
    return BT::NodeStatus::FAILURE;
  }
}

void SendNavThroughPosesAction::onHalted()
{
  cancelCurrentGoal();
}

void SendNavThroughPosesAction::goalResponseCallback(
  std::uint64_t request_id, const GoalHandle::SharedPtr & goal_handle)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (request_id != goal_request_id_) {
    return;
  }
  goal_pending_ = false;
  current_goal_handle_ = goal_handle;
  if (!goal_handle) {
    result_received_ = true;
    goal_succeeded_ = false;
    last_result_code_ = rclcpp_action::ResultCode::ABORTED;
    RCLCPP_ERROR(logger_, "NavigateThroughPoses goal was rejected by server");
  }
}

void SendNavThroughPosesAction::feedbackCallback(
  std::uint64_t request_id, GoalHandle::SharedPtr,
  const std::shared_ptr<const NavigateThroughPoses::Feedback> feedback)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (request_id != goal_request_id_) {
    return;
  }
  latest_pose_ = feedback->current_pose;
  has_current_pose_ = true;
}

void SendNavThroughPosesAction::resultCallback(
  std::uint64_t request_id, const GoalHandle::WrappedResult & result)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (request_id != goal_request_id_) {
    return;
  }
  result_received_ = true;
  last_result_code_ = result.code;
  goal_succeeded_ = (result.code == rclcpp_action::ResultCode::SUCCEEDED);
}

void SendNavThroughPosesAction::cancelCurrentGoal()
{
  GoalHandle::SharedPtr goal_handle;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    ++goal_request_id_;
    goal_handle = current_goal_handle_;
    current_goal_handle_.reset();
    goal_pending_ = false;
    result_received_ = false;
    goal_succeeded_ = false;
  }
  if (goal_handle) {
    action_client_->async_cancel_goal(goal_handle);
  }
}

}  // namespace pb2025_sentry_behavior

#include "behaviortree_cpp/bt_factory.h"
BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<pb2025_sentry_behavior::SendNavThroughPosesAction>(
    "SendNavThroughPoses");
}
