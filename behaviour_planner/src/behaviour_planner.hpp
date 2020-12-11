
#ifndef CATKIN_WS_SRC_MOTION_PLANNING_WITH_CARLA_BEHAVIOUR_PLANNER_INCLUDE_BEHAVIOUR_PLANNER_HPP_
#define CATKIN_WS_SRC_MOTION_PLANNING_WITH_CARLA_BEHAVIOUR_PLANNER_INCLUDE_BEHAVIOUR_PLANNER_HPP_
#include <string>
#include <unordered_map>
#include <memory>
#include <planning_msgs/TrajectoryPoint.h>
#include <planning_msgs/Trajectory.h>
#include <thread_pool/thread_pool.hpp>
#include <tf/transform_datatypes.h>
#include "behaviour_strategy/behaviour_strategy.hpp"
#include "agent/agent.hpp"
#include <carla_msgs/CarlaTrafficLightStatusList.h>
#include <carla_msgs/CarlaTrafficLightInfo.h>
#include <behaviour_strategy/mpdm_planner/mpdm_planner.hpp>
#include <carla_msgs/CarlaTrafficLightInfoList.h>
#include "planning_msgs/Behaviour.h"

namespace planning {

class BehaviourPlanner {
 public:
  BehaviourPlanner() = default;
  ~BehaviourPlanner() = default;
  explicit BehaviourPlanner(const ros::NodeHandle &nh);
  void RunOnce();

 private:
  /**
   * @brief: make agent set from objects and traffic light infos.
   * @note: when the traffic is red, traffic light is considered as an agent, otherwise just ignored.
   * @return: true if successful
   */
  bool MakeAgentSet();

  /**
   * @brief: get the key agents according to the lateral and longitudinal distance from ego agent.
   * @return
   */
  bool GetKeyAgents();

  /**
   * @brief: for visualization.
   * @param behaviour: best behaviours
   */
  void VisualizeBehaviourTrajectories(const Behaviour &behaviour);

  void VisualizeAgentTrajectories(const Behaviour& behaviour);

  /**
   * @brief: convert the behaviour to ros msgs.
   * @param behaviour
   * @param behaviour_msg
   * @return
   */
  static bool ConvertBehaviourToRosMsg(const Behaviour &behaviour, planning_msgs::Behaviour &behaviour_msg);

 private:
  ros::NodeHandle nh_;
  int pool_size_ = 6;
  double sample_key_agent_lat_threshold_{};
  double sample_min_lon_threshold_{};
  std::string planner_type_;
  int ego_vehicle_id_ = -1;
  bool has_ego_vehicle_ = false;
  PolicySimulateConfig simulate_config_;

  ////////////////// ServiceClinet //////////////////////
  ros::ServiceClient get_waypoint_client_;

  ////////////////////// Subscriber /////////////////////
  ros::Subscriber ego_vehicle_subscriber_;
  ros::Subscriber objects_subscriber_;
  ros::Subscriber ego_vehicle_info_subscriber_;
  ros::Subscriber traffic_light_info_subscriber_;
  ros::Subscriber traffic_light_status_subscriber_;

  /////////////////////Publisher///////////////////////
  ros::Publisher behaviour_publisher_;
  ros::Publisher visualized_agent_trajectories_publisher_;
  ros::Publisher visualized_behaviour_trajectories_publisher_;

  carla_msgs::CarlaEgoVehicleInfo ego_vehicle_info_;
  carla_msgs::CarlaEgoVehicleStatus ego_vehicle_status_;
  carla_msgs::CarlaTrafficLightStatusList traffic_light_status_list_;
  carla_msgs::CarlaTrafficLightInfoList traffic_light_info_list_;
  derived_object_msgs::ObjectArray objects_list_;
  std::unordered_map<int, Agent> agent_set_;
  std::unordered_map<int, Agent> key_agent_set_;
  std::unique_ptr<BehaviourStrategy> behaviour_strategy_;

  std::unique_ptr<common::ThreadPool> thread_pool_;
};
}
#endif //CATKIN_WS_SRC_MOTION_PLANNING_WITH_CARLA_BEHAVIOUR_PLANNER_INCLUDE_BEHAVIOUR_PLANNER_HPP_