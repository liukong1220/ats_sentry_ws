// Copyright 2026
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

#include "pb2025_sentry_behavior/plugins/action/select_nearest_goal.hpp"

#include <cmath>
#include <limits>

#include "pb2025_sentry_behavior/custom_types.hpp"

namespace pb2025_sentry_behavior
{

SelectNearestGoalAction::SelectNearestGoalAction(
  const std::string & name, const BT::NodeConfig & config)
: BT::SyncActionNode(name, config)
{
}

BT::PortsList SelectNearestGoalAction::providedPorts()
{
  return {
    BT::InputPort<pb_rm_interfaces::msg::RobotStatus>(
      "key_port", "{@referee_robotStatus}", "RobotStatus port on blackboard"),
    BT::InputPort<std::vector<geometry_msgs::msg::PoseStamped>>(
      "candidates", "0;0;0|0;0;0", "Candidate goals format: x;y;yaw|x;y;yaw|..."),
    BT::OutputPort<geometry_msgs::msg::PoseStamped>("goal", "{selected_goal}", "Nearest goal"),
  };
}

BT::NodeStatus SelectNearestGoalAction::tick()
{
  auto status_msg = getInput<pb_rm_interfaces::msg::RobotStatus>("key_port");
  if (!status_msg) {
    return BT::NodeStatus::FAILURE;
  }

  auto candidates = getInput<std::vector<geometry_msgs::msg::PoseStamped>>("candidates");
  if (!candidates || candidates->empty()) {
    return BT::NodeStatus::FAILURE;
  }

  const auto & current = status_msg->robot_pos.position;
  size_t nearest_idx = 0;
  double min_dist_sq = std::numeric_limits<double>::max();

  for (size_t i = 0; i < candidates->size(); ++i) {
    const auto & p = (*candidates)[i].pose.position;
    const double dx = current.x - p.x;
    const double dy = current.y - p.y;
    const double dist_sq = dx * dx + dy * dy;
    if (dist_sq < min_dist_sq) {
      min_dist_sq = dist_sq;
      nearest_idx = i;
    }
  }

  setOutput("goal", (*candidates)[nearest_idx]);
  return BT::NodeStatus::SUCCESS;
}

}  // namespace pb2025_sentry_behavior

#include "behaviortree_cpp/bt_factory.h"
BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<pb2025_sentry_behavior::SelectNearestGoalAction>("SelectNearestGoal");
}
