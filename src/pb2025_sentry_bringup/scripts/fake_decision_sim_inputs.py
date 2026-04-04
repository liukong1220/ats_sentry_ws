#!/usr/bin/env python3

import math

import rclpy
from geometry_msgs.msg import PoseWithCovarianceStamped
from pb_rm_interfaces.msg import GameStatus, RfidStatus, RobotStatus
from rclpy.node import Node


def yaw_to_quaternion(yaw: float):
    half_yaw = yaw * 0.5
    return (0.0, 0.0, math.sin(half_yaw), math.cos(half_yaw))


class FakeDecisionSimInputs(Node):
    def __init__(self):
        super().__init__("fake_decision_sim_inputs")

        self.declare_parameter("publish_rate", 5.0)
        self.declare_parameter("initial_pose_repeats", 10)
        self.declare_parameter("initial_pose_period", 0.5)

        self.declare_parameter("initial_x", 0.0)
        self.declare_parameter("initial_y", 0.0)
        self.declare_parameter("initial_yaw", 0.0)

        self.declare_parameter("game_progress", int(GameStatus.RUNNING))
        self.declare_parameter("stage_remain_time", 420)

        self.declare_parameter("robot_id", 7)
        self.declare_parameter("current_hp", 400)
        self.declare_parameter("maximum_hp", 400)
        self.declare_parameter("shooter_barrel_cooling_value", 60)
        self.declare_parameter("shooter_barrel_heat_limit", 400)
        self.declare_parameter("shooter_17mm_1_barrel_heat", 0)
        self.declare_parameter("projectile_allowance_17mm", 200)
        self.declare_parameter("remaining_gold_coin", 0)

        self.declare_parameter("friendly_fortress_gain_point", False)
        self.declare_parameter("friendly_supply_zone_non_exchange", False)
        self.declare_parameter("friendly_supply_zone_exchange", False)
        self.declare_parameter("center_gain_point", False)

        self.initial_pose_pub = self.create_publisher(
            PoseWithCovarianceStamped, "initialpose", 10
        )
        self.game_status_pub = self.create_publisher(
            GameStatus, "referee/game_status", 10
        )
        self.robot_status_pub = self.create_publisher(
            RobotStatus, "referee/robot_status", 10
        )
        self.rfid_status_pub = self.create_publisher(
            RfidStatus, "referee/rfid_status", 10
        )

        publish_rate = self.get_parameter("publish_rate").value
        self.initial_pose_repeats = int(
            self.get_parameter("initial_pose_repeats").value
        )

        self.create_timer(1.0 / publish_rate, self.publish_referee_messages)
        self.initial_pose_timer = self.create_timer(
            float(self.get_parameter("initial_pose_period").value),
            self.publish_initial_pose,
        )

        self.get_logger().info(
            "Fake loopback decision inputs are active. Publishing referee state and initial pose."
        )

    def publish_initial_pose(self):
        if self.initial_pose_repeats <= 0:
            self.initial_pose_timer.cancel()
            return

        initial_pose = PoseWithCovarianceStamped()
        initial_pose.header.stamp = self.get_clock().now().to_msg()
        initial_pose.header.frame_id = "map"
        initial_pose.pose.pose.position.x = float(self.get_parameter("initial_x").value)
        initial_pose.pose.pose.position.y = float(self.get_parameter("initial_y").value)
        qx, qy, qz, qw = yaw_to_quaternion(
            float(self.get_parameter("initial_yaw").value)
        )
        initial_pose.pose.pose.orientation.x = qx
        initial_pose.pose.pose.orientation.y = qy
        initial_pose.pose.pose.orientation.z = qz
        initial_pose.pose.pose.orientation.w = qw
        initial_pose.pose.covariance[0] = 0.25
        initial_pose.pose.covariance[7] = 0.25
        initial_pose.pose.covariance[35] = 0.0685

        self.initial_pose_pub.publish(initial_pose)
        self.initial_pose_repeats -= 1

    def publish_referee_messages(self):
        game_status = GameStatus()
        game_status.game_progress = int(self.get_parameter("game_progress").value)
        game_status.stage_remain_time = int(
            self.get_parameter("stage_remain_time").value
        )
        self.game_status_pub.publish(game_status)

        robot_status = RobotStatus()
        robot_status.robot_id = int(self.get_parameter("robot_id").value)
        robot_status.current_hp = int(self.get_parameter("current_hp").value)
        robot_status.maximum_hp = int(self.get_parameter("maximum_hp").value)
        robot_status.shooter_barrel_cooling_value = int(
            self.get_parameter("shooter_barrel_cooling_value").value
        )
        robot_status.shooter_barrel_heat_limit = int(
            self.get_parameter("shooter_barrel_heat_limit").value
        )
        robot_status.shooter_17mm_1_barrel_heat = int(
            self.get_parameter("shooter_17mm_1_barrel_heat").value
        )
        robot_status.projectile_allowance_17mm = int(
            self.get_parameter("projectile_allowance_17mm").value
        )
        robot_status.remaining_gold_coin = int(
            self.get_parameter("remaining_gold_coin").value
        )
        robot_status.robot_pos.orientation.w = 1.0
        self.robot_status_pub.publish(robot_status)

        rfid_status = RfidStatus()
        rfid_status.friendly_fortress_gain_point = bool(
            self.get_parameter("friendly_fortress_gain_point").value
        )
        rfid_status.friendly_supply_zone_non_exchange = bool(
            self.get_parameter("friendly_supply_zone_non_exchange").value
        )
        rfid_status.friendly_supply_zone_exchange = bool(
            self.get_parameter("friendly_supply_zone_exchange").value
        )
        rfid_status.center_gain_point = bool(
            self.get_parameter("center_gain_point").value
        )
        self.rfid_status_pub.publish(rfid_status)


def main():
    rclpy.init()
    node = FakeDecisionSimInputs()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
