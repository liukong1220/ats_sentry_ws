// Copyright 2025 Lihan Chen
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "pb2025_sentry_behavior/plugins/action/send_nav2_waypoints.hpp"

#include "pb2025_sentry_behavior/custom_types.hpp"

namespace pb2025_sentry_behavior
{

SendNav2WaypointsAction::SendNav2WaypointsAction(
  const std::string & name, const BT::NodeConfig & conf, const BT::RosNodeParams & params)
: RosActionNode<nav2_msgs::action::NavigateThroughPoses>(name, conf, params)
{
}

bool SendNav2WaypointsAction::setGoal(nav2_msgs::action::NavigateThroughPoses::Goal & goal)
{
  std::vector<geometry_msgs::msg::PoseStamped> poses;

  auto waypoints = getInput<std::vector<geometry_msgs::msg::PoseStamped>>("goals");
  if (waypoints && !waypoints->empty()) {
    poses = *waypoints;
  } else {
    auto single_goal = getInput<geometry_msgs::msg::PoseStamped>("goal");
    if (!single_goal) {
      RCLCPP_ERROR(logger(), "Input ports 'goals' and 'goal' are both empty.");
      return false;
    }
    poses.push_back(*single_goal);
  }

  goal.poses.clear();
  goal.poses.reserve(poses.size());
  for (auto pose : poses) {
    pose.header.frame_id = "map";
    pose.header.stamp = now();
    goal.poses.push_back(pose);
  }
  return true;
}

BT::NodeStatus SendNav2WaypointsAction::onResultReceived(const WrappedResult & wr)
{
  switch (wr.code) {
    case rclcpp_action::ResultCode::SUCCEEDED:
      RCLCPP_INFO(logger(), "NavigateThroughPoses succeeded.");
      return BT::NodeStatus::SUCCESS;

    case rclcpp_action::ResultCode::ABORTED:
      RCLCPP_ERROR(logger(), "NavigateThroughPoses aborted by server.");
      return BT::NodeStatus::FAILURE;

    case rclcpp_action::ResultCode::CANCELED:
      RCLCPP_WARN(logger(), "NavigateThroughPoses canceled.");
      return BT::NodeStatus::FAILURE;

    default:
      RCLCPP_ERROR(
        logger(), "Unknown NavigateThroughPoses result code: %d", static_cast<int>(wr.code));
      return BT::NodeStatus::FAILURE;
  }
}

BT::NodeStatus SendNav2WaypointsAction::onFeedback(
  const std::shared_ptr<const nav2_msgs::action::NavigateThroughPoses::Feedback> feedback)
{
  RCLCPP_DEBUG(
    logger(), "Distance remaining: %.3f, poses remaining: %d", feedback->distance_remaining,
    feedback->number_of_poses_remaining);
  return BT::NodeStatus::RUNNING;
}

void SendNav2WaypointsAction::onHalt()
{
  RCLCPP_INFO(logger(), "SendNav2WaypointsAction has been halted.");
}

BT::NodeStatus SendNav2WaypointsAction::onFailure(BT::ActionNodeErrorCode error)
{
  RCLCPP_ERROR(logger(), "SendNav2WaypointsAction failed with error code: %d", error);
  return BT::NodeStatus::FAILURE;
}

BT::PortsList SendNav2WaypointsAction::providedPorts()
{
  BT::PortsList additional_ports = {
    BT::InputPort<geometry_msgs::msg::PoseStamped>(
      "goal", "0;0;0", "Single goal fallback when no list is provided"),
    BT::InputPort<std::vector<geometry_msgs::msg::PoseStamped>>(
      "goals", "0;0;0|0;0;0",
      "Expected waypoint list for nav2. Format: x;y;yaw|x;y;yaw|..."),
  };
  return providedBasicPorts(additional_ports);
}

}  // namespace pb2025_sentry_behavior

#include "behaviortree_ros2/plugins.hpp"
CreateRosNodePlugin(pb2025_sentry_behavior::SendNav2WaypointsAction, "SendNav2Waypoints");
