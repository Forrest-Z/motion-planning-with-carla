#include <motion_planner/frenet_lattice_planner/constraint_checker.hpp>
#include "motion_planner/frenet_lattice_planner/frenet_lattice_planner.hpp"
#include "motion_planner/frenet_lattice_planner/polynomial_trajectory_evaluator.hpp"
#include "motion_planner/frenet_lattice_planner/end_condition_sampler.hpp"
#include "coordinate_transformer.hpp"
#include "obstacle_filter/obstacle_filter.hpp"
#include "collision_checker/collision_checker.hpp"

namespace planning {

bool FrenetLatticePlanner::Process(const planning_msgs::TrajectoryPoint &init_trajectory_point,
                                   const ManeuverGoal &maneuver_goal,
                                   planning_msgs::Trajectory &pub_trajectory,
                                   std::vector<planning_msgs::Trajectory> *valid_trajectories) {
  if (maneuver_goal.maneuver_infos.empty()) {
    ROS_FATAL("[FrenetLatticePlanner::Process]: No reference line provided");
    return false;
  }
  size_t index = 0;
  size_t failed_ref_plan_num = 0;
  std::vector<std::pair<planning_msgs::Trajectory, double>> optimal_trajectories;
  for (const auto &maneuver_info : maneuver_goal.maneuver_infos) {
    std::pair<planning_msgs::Trajectory, double> optimal_trajectory;
    auto result = PlanningOnRef(init_trajectory_point, maneuver_info, optimal_trajectory, valid_trajectories);
    if (!result) {
      ROS_DEBUG("[FrenetLatticePlanner::Process], failed plan on reference line: %zu", index);
      failed_ref_plan_num++;
    }
    if (index == 0) {
      optimal_trajectory.second += 20.0;
    }
    optimal_trajectories.push_back(optimal_trajectory);
    index++;
  }
  if (failed_ref_plan_num >= maneuver_goal.maneuver_infos.size()) {
    ROS_FATAL("[FrenetLatticePlanner::Process], the process is failed on every reference line");
    return false;
  }
  std::sort(optimal_trajectories.begin(), optimal_trajectories.end(),
            [](const std::pair<planning_msgs::Trajectory, double> &p0,
               const std::pair<planning_msgs::Trajectory, double> &p1) -> bool {
              return p0.second < p1.second;
            });

  pub_trajectory = optimal_trajectories.front().first;
  return true;
}

bool FrenetLatticePlanner::PlanningOnRef(const planning_msgs::TrajectoryPoint &init_trajectory_point,
                                         const ManeuverInfo &maneuver_info,
                                         std::pair<planning_msgs::Trajectory, double> &optimal_trajectory,
                                         std::vector<planning_msgs::Trajectory> *valid_trajectories) {

  const auto &ref_line = maneuver_info.ptr_ref_line;
  std::array<double, 3> init_s{};
  std::array<double, 3> init_d{};
  FrenetLatticePlanner::GetInitCondition(ref_line, init_trajectory_point, &init_s, &init_d);
  const auto &obstacles_map = ObstacleFilter::Instance().Obstacles();
  std::vector<std::shared_ptr<Obstacle>> obstacle_vec;
  obstacle_vec.reserve(obstacles_map.size());
  for (const auto &obstacle : obstacles_map) {
    obstacle_vec.push_back(obstacle.second);
  }
  auto st_graph = std::make_shared<STGraph>(obstacle_vec, ref_line,
                                            init_s[0],
                                            init_s[0] + PlanningConfig::Instance().max_lookahead_distance(),
                                            0.0, PlanningConfig::Instance().max_lookahead_time(),
                                            init_d);
  std::vector<std::shared_ptr<Polynomial>> lon_traj_vec;
  std::vector<std::shared_ptr<Polynomial>> lat_traj_vec;

  auto end_condition_sampler = std::make_shared<EndConditionSampler>(init_s, init_d, ref_line, st_graph);
  FrenetLatticePlanner::GenerateLonTrajectories(maneuver_info, init_s, end_condition_sampler, &lon_traj_vec);
  FrenetLatticePlanner::GenerateLatTrajectories(init_d, end_condition_sampler, &lat_traj_vec);
  PolynomialTrajectoryEvaluator trajectory_evaluator = PolynomialTrajectoryEvaluator(init_s,
                                                                                     maneuver_info,
                                                                                     lon_traj_vec,
                                                                                     lat_traj_vec,
                                                                                     ref_line, st_graph);
  CollisionChecker collision_checker = CollisionChecker(ref_line, st_graph, init_s[0], init_d[0]);
  size_t collision_failure_count = 0;
  size_t combined_constraint_failure_count = 0;
  size_t lon_vel_failure_count = 0;
  size_t lon_acc_failure_count = 0;
  size_t lon_jerk_failure_count = 0;
  size_t curvature_failure_count = 0;
  size_t lat_acc_failure_count = 0;
  size_t lat_jerk_failure_count = 0;
  size_t num_lattice_traj = 0;
  while (trajectory_evaluator.has_more_trajectory_pairs()) {
    double trajectory_pair_cost = trajectory_evaluator.top_trajectory_pair_cost();
    auto trajectory_pair = trajectory_evaluator.next_top_trajectory_pair();
    auto combined_trajectory = CombineTrajectories(ref_line, *trajectory_pair.first, *trajectory_pair.second,
                                                   init_trajectory_point.relative_time);

    auto result = ConstraintChecker::ValidTrajectory(combined_trajectory);
    if (result != ConstraintChecker::Result::VALID) {
      ++combined_constraint_failure_count;
      switch (result) {
        case ConstraintChecker::Result::LON_VELOCITY_OUT_OF_BOUND:lon_vel_failure_count += 1;
          break;
        case ConstraintChecker::Result::LON_ACCELERATION_OUT_OF_BOUND:lon_acc_failure_count += 1;
          break;
        case ConstraintChecker::Result::LON_JERK_OUT_OF_BOUND:lon_jerk_failure_count += 1;
          break;
        case ConstraintChecker::Result::CURVATURE_OUT_OF_BOUND:curvature_failure_count += 1;
          break;
        case ConstraintChecker::Result::LAT_ACCELERATION_OUT_OF_BOUND:lat_acc_failure_count += 1;
          break;
        case ConstraintChecker::Result::LAT_JERK_OUT_OF_BOUND:lat_jerk_failure_count += 1;
          break;
        case ConstraintChecker::Result::VALID:
        default:break;
      }
      continue;
    }
    if (collision_checker.IsCollision(combined_trajectory)) {
      ++collision_failure_count;
      continue;
    }
    num_lattice_traj += 1;
    // set visualized trajectory vectors
    if (valid_trajectories != nullptr) {
      valid_trajectories->emplace_back(combined_trajectory);
      if (num_lattice_traj == 1) {
        optimal_trajectory.second = trajectory_pair_cost;
        optimal_trajectory.first = combined_trajectory;
      }
    } else {
      optimal_trajectory.second = trajectory_pair_cost;
      optimal_trajectory.first = combined_trajectory;
      break;
    }
  }
  if (num_lattice_traj > 0) {
    return false;
  }
  return true;
}

planning_msgs::Trajectory FrenetLatticePlanner::CombineTrajectories(const std::shared_ptr<ReferenceLine> &ptr_ref_line,
                                                                    const Polynomial &lon_traj_vec,
                                                                    const Polynomial &lat_traj_vec,
                                                                    double start_time) {
  return planning_msgs::Trajectory();
}

void FrenetLatticePlanner::GenerateLatTrajectories(const std::array<double, 3> &init_d,
                                                   const std::shared_ptr<EndConditionSampler> &end_condition_sampler,
                                                   std::vector<std::shared_ptr<Polynomial>> *ptr_lat_traj_vec) {
  if (ptr_lat_traj_vec == nullptr) {
    return;
  }
  ptr_lat_traj_vec->clear();
  auto lat_end_condtions = end_condition_sampler->SampleLatEndCondition();
  FrenetLatticePlanner::GeneratePolynomialTrajectories(init_d, lat_end_condtions, 5, ptr_lat_traj_vec);
}

void FrenetLatticePlanner::GenerateLonTrajectories(const ManeuverInfo &maneuver_info,
                                                   const std::array<double, 3> &init_s,
                                                   const std::shared_ptr<EndConditionSampler> &end_condition_sampler,
                                                   std::vector<std::shared_ptr<Polynomial>> *ptr_lon_traj_vec) {
  if (ptr_lon_traj_vec == nullptr) {
    ROS_DEBUG("[FrenetLatticePlanner::GenerateLonTrajectories]. "
              "Failed to generate lon trajectories, because ptr_lon_traj_vec is nullptr");
    return;
  }
  ptr_lon_traj_vec->clear();
  FrenetLatticePlanner::GenerateCruisingLonTrajectories(PlanningConfig::Instance().target_speed(), init_s,
                                                        end_condition_sampler, ptr_lon_traj_vec);
  FrenetLatticePlanner::GenerateOvertakeAndFollowingLonTrajectories(init_s, end_condition_sampler, ptr_lon_traj_vec);
  if (maneuver_info.has_stop_point) {
    FrenetLatticePlanner::GenerateStoppingLonTrajectories(maneuver_info.maneuver_target.target_s,
                                                          init_s, end_condition_sampler,
                                                          ptr_lon_traj_vec);
  }
}

void FrenetLatticePlanner::GenerateCruisingLonTrajectories(double cruise_speed,
                                                           const std::array<double, 3> &init_s,
                                                           const std::shared_ptr<EndConditionSampler> &end_condition_sampler,
                                                           std::vector<std::shared_ptr<Polynomial>> *ptr_lon_traj_vec) {
  auto end_conditions = end_condition_sampler->SampleLonEndConditionForCruising(cruise_speed);
  FrenetLatticePlanner::GeneratePolynomialTrajectories(init_s, end_conditions, 4, ptr_lon_traj_vec);
}

void FrenetLatticePlanner::GenerateStoppingLonTrajectories(double stop_s,
                                                           const std::array<double, 3> &init_s,
                                                           const std::shared_ptr<EndConditionSampler> &end_condition_sampler,
                                                           std::vector<std::shared_ptr<Polynomial>> *ptr_lon_traj_vec) {
  auto end_conditions = end_condition_sampler->SampleLonEndConditionForStopping(stop_s);
  FrenetLatticePlanner::GeneratePolynomialTrajectories(init_s, end_conditions, 5, ptr_lon_traj_vec);
}

void FrenetLatticePlanner::GenerateOvertakeAndFollowingLonTrajectories(const std::array<double, 3> &init_s,
                                                                       const std::shared_ptr<EndConditionSampler> &end_condition_sampler,
                                                                       std::vector<std::shared_ptr<Polynomial>> *ptr_lon_traj_vec) {
  auto end_conditions = end_condition_sampler->SampleLonEndConditionWithSTGraph();
  FrenetLatticePlanner::GeneratePolynomialTrajectories(init_s, end_conditions, 5, ptr_lon_traj_vec);
}

void FrenetLatticePlanner::GeneratePolynomialTrajectories(const std::array<double, 3> &init_condition,
                                                          const std::vector<std::pair<std::array<double, 3>,
                                                                                      double>> &end_conditions,
                                                          size_t order,
                                                          std::vector<std::shared_ptr<Polynomial>> *ptr_traj_vec) {

  if (ptr_traj_vec == nullptr) {
    ROS_FATAL("[FrenetLatticePlanner::GeneratePolynomialTrajectories], the pre_traj_vec is nullptr");
    return;
  }
  if (end_conditions.empty()) {
    ROS_FATAL("[FrenetLatticePlanner::GeneratePolynomialTrajectories], the end conditions vector is empty");
    return;
  }
  ROS_DEBUG("[FrenetLatticePlanner::GeneratePolynomialTrajectories], "
            "the end conditions vector's size: %zu", end_conditions.size());
  ptr_traj_vec->reserve(ptr_traj_vec->size() + end_conditions.size());
  switch (order) {
    case 4: {
      for (const auto &end_condition : end_conditions) {
        auto ptr_trajectory =
            std::make_shared<QuarticPolynomial>(init_condition[0], init_condition[1], init_condition[2],
                                                end_condition.first[1], end_condition.first[2], end_condition.second);
        ptr_traj_vec->push_back(ptr_trajectory);
      }
      break;
    }
    case 5: {
      for (const auto &end_condition : end_conditions) {
        auto ptr_trajectory =
            std::make_shared<QuinticPolynomial>(init_condition, end_condition.first, end_condition.second);
        ptr_traj_vec->push_back(ptr_trajectory);
        break;
      }
      break;
    }
    default:break;
  }
}

void FrenetLatticePlanner::GetInitCondition(const std::shared_ptr<ReferenceLine> &ptr_ref_line,
                                            const planning_msgs::TrajectoryPoint &init_trajectory_point,
                                            std::array<double, 3> *const init_s,
                                            std::array<double, 3> *const init_d) {
  ReferencePoint matched_ref_point;
  double matched_s;
  if (!ptr_ref_line->GetMatchedPoint(init_trajectory_point.path_point.x,
                                     init_trajectory_point.path_point.y,
                                     &matched_ref_point,
                                     &matched_s)) {
    ROS_FATAL("[FrenetLatticePlanner::Plan] failed because the GetMatchedPoint failed");
    return;
  }
  CoordinateTransformer::CartesianToFrenet(matched_s, matched_ref_point.heading(),
                                           matched_ref_point.x(),
                                           matched_ref_point.y(),
                                           matched_ref_point.kappa(),
                                           matched_ref_point.dkappa(),
                                           init_trajectory_point.path_point.x,
                                           init_trajectory_point.path_point.y,
                                           init_trajectory_point.vel,
                                           init_trajectory_point.acc,
                                           init_trajectory_point.path_point.theta,
                                           init_trajectory_point.path_point.kappa,
                                           init_s, init_d);
}

}