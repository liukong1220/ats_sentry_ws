#include "pb2025_sentry_behavior/plugins/action/advance_patrol_cursor.hpp"

namespace pb2025_sentry_behavior
{

BT::NodeStatus AdvancePatrolCursorAction::tick()
{
  int next_cursor = 0;
  int next_direction = 1;
  if (!getInput("next_cursor", next_cursor) || !getInput("next_direction", next_direction)) {
    return BT::NodeStatus::FAILURE;
  }

  setOutput("patrol_cursor", next_cursor);
  setOutput("patrol_direction", next_direction);
  return BT::NodeStatus::SUCCESS;
}

BT::PortsList AdvancePatrolCursorAction::providedPorts()
{
  return {
    BT::InputPort<int>("next_cursor", "{decision_next_patrol_cursor}", "Next patrol cursor"),
    BT::InputPort<int>(
      "next_direction", "{decision_next_patrol_direction}", "Next patrol direction"),
    BT::OutputPort<int>("patrol_cursor", "{decision_patrol_cursor}", "Updated patrol cursor"),
    BT::OutputPort<int>(
      "patrol_direction", "{decision_patrol_direction}", "Updated patrol direction")};
}

}  // namespace pb2025_sentry_behavior

#include "behaviortree_cpp/bt_factory.h"
BT_REGISTER_NODES(factory)
{
  factory.registerNodeType<pb2025_sentry_behavior::AdvancePatrolCursorAction>(
    "AdvancePatrolCursor");
}
