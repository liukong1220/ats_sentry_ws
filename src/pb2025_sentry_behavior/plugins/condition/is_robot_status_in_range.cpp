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

#include "pb2025_sentry_behavior/plugins/condition/is_robot_status_in_range.hpp"

namespace pb2025_sentry_behavior
{

IsRobotStatusInRangeCondition::IsRobotStatusInRangeCondition(
  const std::string & name, const BT::NodeConfig & config)
: BT::SimpleConditionNode(
    name, std::bind(&IsRobotStatusInRangeCondition::checkRobotStatusRange, this), config)
{
}

BT::NodeStatus IsRobotStatusInRangeCondition::checkRobotStatusRange()
{
  auto msg = getInput<pb_rm_interfaces::msg::RobotStatus>("key_port");
  if (!msg) {
    RCLCPP_ERROR(logger_, "RobotStatus message is not available");
    return BT::NodeStatus::FAILURE;
  }

  int hp_min = 0;
  int hp_max = -1;
  int ammo_min = 0;
  int ammo_max = -1;
  int heat_max = 350;

  getInput("hp_min", hp_min);
  getInput("hp_max", hp_max);
  getInput("ammo_min", ammo_min);
  getInput("ammo_max", ammo_max);
  getInput("heat_max", heat_max);

  const int hp = msg->current_hp;
  const int ammo = msg->projectile_allowance_17mm;
  const int heat = msg->shooter_17mm_1_barrel_heat;

  const bool hp_min_ok = (hp >= hp_min);
  const bool hp_max_ok = (hp_max < 0) || (hp <= hp_max);
  const bool ammo_min_ok = (ammo >= ammo_min);
  const bool ammo_max_ok = (ammo_max < 0) || (ammo <= ammo_max);
  const bool heat_ok = (heat <= heat_max);

  return (hp_min_ok && hp_max_ok && ammo_min_ok && ammo_max_ok && heat_ok) ?
           BT::NodeStatus::SUCCESS :
           BT::NodeStatus::FAILURE;
}

BT::PortsList IsRobotStatusInRangeCondition::providedPorts()
{
  return {
    BT::InputPort<pb_rm_interfaces::msg::RobotStatus>(
      "key_port", "{@referee_robotStatus}", "RobotStatus port on blackboard"),
    BT::InputPort<int>("hp_min", 0, "Minimum HP threshold"),
    BT::InputPort<int>("hp_max", -1, "Maximum HP threshold, -1 means disabled"),
    BT::InputPort<int>("ammo_min", 0, "Minimum ammo threshold"),
    BT::InputPort<int>("ammo_max", -1, "Maximum ammo threshold, -1 means disabled"),
    BT::InputPort<int>("heat_max", 350, "Maximum heat threshold"),
  };
}

}  // namespace pb2025_sentry_behavior

#include "behaviortree_cpp/bt_factory.h"
BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<pb2025_sentry_behavior::IsRobotStatusInRangeCondition>(
    "IsRobotStatusInRange");
}
