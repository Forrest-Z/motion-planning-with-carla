#ifndef CATKIN_WS_SRC_MOTION_PLANNING_WITH_CARLA_LOCAL_PLANNER_INCLUDE_TRAFFIC_LIGHTS_TRAFFIC_LIGHT_LIST_HPP_
#define CATKIN_WS_SRC_MOTION_PLANNING_WITH_CARLA_LOCAL_PLANNER_INCLUDE_TRAFFIC_LIGHTS_TRAFFIC_LIGHT_LIST_HPP_
#include <unordered_map>
#include "traffic_light.hpp"
namespace planning {
class TrafficLightList {
 public:
  static TrafficLightList &Instance();
  const std::unordered_map<int, TrafficLight> &TrafficLights() const { return traffic_lights_; }
  void AddTrafficLight(const TrafficLight &traffic_light);
  void AddTrafficLight(const carla_msgs::CarlaTrafficLightStatus &traffic_light_status,
                       const carla_msgs::CarlaTrafficLightInfo &traffic_light_info,
                       const carla_waypoint_types::CarlaWaypoint &carla_waypoint);

 private:
  std::unordered_map<int, TrafficLight> traffic_lights_{};

 private:
  TrafficLightList() = default;
  ~TrafficLightList() = default;
};
}
#endif //CATKIN_WS_SRC_MOTION_PLANNING_WITH_CARLA_LOCAL_PLANNER_INCLUDE_TRAFFIC_LIGHTS_TRAFFIC_LIGHT_LIST_HPP_
