#include <utility>
#include "motion_planner/frenet_lattice_planner/st_graph.hpp"
namespace planning {

STGraph::STGraph(const std::vector<std::shared_ptr<Obstacle>> &obstacles,
                 std::shared_ptr<ReferenceLine> reference_line,
                 double s_start,
                 double s_end,
                 double t_start,
                 double t_end,
                 const std::array<double, 3> &init_d) {
  ROS_ASSERT(s_end >= s_start);
  ROS_ASSERT(t_end >= t_start);
  s_range_.first = s_start;
  s_range_.second = s_end;
  time_range_.first = t_start;
  time_range_.second = t_end;
  reference_line_ = std::move(reference_line);
  init_d_ = init_d;
  SetUp(obstacles, reference_line_);
}
void STGraph::SetUp(const std::vector<std::shared_ptr<Obstacle>> &obstacles,
                    std::shared_ptr<ReferenceLine> ref_line) {
  for (const auto &obstacle : obstacles) {
    if (!obstacle->HasTrajectory()) {
      SetUpStaticObstacle(obstacle, ref_line);
    } else {
      SetUpDynamicObstacle(obstacle, ref_line);
    }
  }

  std::sort(obstacles_sl_boundary_.begin(), obstacles_sl_boundary_.end(),
            [](const SLBoundary &sl0, const SLBoundary &sl1) {
              return sl0.start_s < sl1.start_s;
            });

  for (auto &obstacle_st : obstacle_st_map_) {
    obstacles_st_boundary_.push_back(obstacle_st.second);
  }
}

void STGraph::SetUpStaticObstacle(std::shared_ptr<Obstacle> obstacle,
                                  std::shared_ptr<ReferenceLine> ref_line) {
  auto box = obstacle->BoundingBox();
  SLBoundary sl_boundary;
  if (!ref_line->GetSLBoundary(box, &sl_boundary)) {
    ROS_DEBUG("[STGraph::SetUpStaticObstacle] Failed to GetSLBoundary.");
    return;
  }
  double left_width;
  double right_width;
  int obstacle_id = obstacle->Id();
  ref_line->GetLaneWidth(sl_boundary.start_s, &left_width, &right_width);
  if (sl_boundary.start_s > s_range_.second ||
      sl_boundary.end_s < s_range_.first ||
      sl_boundary.start_l > left_width ||
      sl_boundary.end_l < -right_width) {
    ROS_DEBUG("[STGraph::SetUpStaticObstacle], obstacle[%i] is out of range. ", obstacle_id);
    return;
  }

  obstacle_st_map_[obstacle_id].set_id(obstacle_id);
  obstacle_st_map_[obstacle_id].set_lower_left_point(SetSTPoint(sl_boundary.start_s, 0.0));
  obstacle_st_map_[obstacle_id].set_lower_right_point(SetSTPoint(sl_boundary.start_s,
                                                                 PlanningConfig::Instance().max_lookahead_time()));
  obstacle_st_map_[obstacle_id].set_upper_left_point(SetSTPoint(sl_boundary.end_s, 0.0));
  obstacle_st_map_[obstacle_id].set_upper_right_point(SetSTPoint(sl_boundary.end_s,
                                                                 PlanningConfig::Instance().max_lookahead_time()));
  obstacles_sl_boundary_.push_back(std::move(sl_boundary));

}
void STGraph::SetUpDynamicObstacle(std::shared_ptr<Obstacle> obstacle,
                                   std::shared_ptr<ReferenceLine> ref_line) {

  double relative_time = time_range_.first;
  while (relative_time < time_range_.second) {
    planning_msgs::TrajectoryPoint point = obstacle->GetPointAtTime(relative_time);
    Box2d box = obstacle->GetBoundingBoxAtPoint(point);
    SLBoundary sl_boundary;
    if (!ref_line->GetSLBoundary(box, &sl_boundary)) {
      ROS_DEBUG("[STGraph::SetUpDynamicObstacle]: failed to get sl_boundary");
      return;
    }
    double left_width;
    double right_width;
    ref_line->GetLaneWidth(sl_boundary.start_s, &left_width, &right_width);

    // The obstacle is not shown on the region to be considered.
    if (sl_boundary.start_s > s_range_.second ||
        sl_boundary.end_s < s_range_.first ||
        sl_boundary.start_l > left_width ||
        sl_boundary.end_l < -right_width) {
      if (obstacle_st_map_.find(obstacle->Id()) !=
          obstacle_st_map_.end()) {
        break;
      }
      relative_time += PlanningConfig::Instance().delta_t();
      continue;
    }

    if (obstacle_st_map_.find(obstacle->Id()) ==
        obstacle_st_map_.end()) {
      obstacle_st_map_[obstacle->Id()].set_id(obstacle->Id());
      obstacle_st_map_[obstacle->Id()].set_lower_left_point(SetSTPoint(sl_boundary.start_s, relative_time));
      obstacle_st_map_[obstacle->Id()].set_upper_left_point(SetSTPoint(sl_boundary.end_s, relative_time));
    }

    obstacle_st_map_[obstacle->Id()].set_lower_right_point(SetSTPoint(sl_boundary.start_s, relative_time));
    obstacle_st_map_[obstacle->Id()].set_upper_right_point(SetSTPoint(sl_boundary.end_s, relative_time));
    relative_time += PlanningConfig::Instance().delta_t();
  }
}
STPoint STGraph::SetSTPoint(double s, double t) {
  STPoint st_point(s, t);
  return st_point;
}
bool STGraph::GetSTObstacle(int id, STBoundary *st_boundary) {
  if (st_boundary == nullptr) {
    return false;
  }
  if (obstacle_st_map_.find(id) == obstacle_st_map_.end()) {
    return false;
  } else {
    *st_boundary = obstacle_st_map_[id];
    return true;
  }

}
const std::vector<STBoundary> &STGraph::GetObstaclesSTBoundary() const { return obstacles_st_boundary_; }

std::vector<std::pair<double, double>> STGraph::GetPathBlockingIntervals(const double t) const {
  ROS_ASSERT(time_range_.first <= t && t <= time_range_.second);
  std::vector<std::pair<double, double>> intervals;
  for (const auto &pt_obstacle : obstacles_st_boundary_) {
    if (t > pt_obstacle.max_t() || t < pt_obstacle.min_t()) {
      continue;
    }
    double s_upper = MathUtil::lerp(pt_obstacle.upper_left_point().s(),
                                    pt_obstacle.upper_left_point().t(),
                                    pt_obstacle.upper_right_point().s(),
                                    pt_obstacle.upper_right_point().t(), t);
    double s_lower = MathUtil::lerp(pt_obstacle.lower_left_point().s(),
                                    pt_obstacle.lower_left_point().t(),
                                    pt_obstacle.lower_right_point().s(),
                                    pt_obstacle.lower_right_point().t(), t);
    intervals.emplace_back(s_lower, s_upper);
  }
  return intervals;
}
std::vector<std::vector<std::pair<double, double>>> STGraph::GetPathBlockingInterval(const double t_start,
                                                                                     const double t_end,
                                                                                     const double t_resolution) const {
  std::vector<std::vector<std::pair<double, double>>> intervals;
  double t = t_start;
  while (t <= t_end) {
    intervals.push_back(GetPathBlockingIntervals(t));
    t += t_resolution;
  }
  return intervals;
}

std::vector<STPoint> STGraph::GetObstacleSurroundingPoints(int obstacle_id, double s_dist, double t_density) const {
  ROS_ASSERT(t_density > 0.0);
  std::vector<STPoint> pt_pairs;
  if (obstacle_st_map_.find(obstacle_id) == obstacle_st_map_.end()) {
    return pt_pairs;
  }
  const auto &pt_obstacle = obstacle_st_map_.at(obstacle_id);
  double s0;
  double s1;
  double t0;
  double t1;

  if (s_dist > 0.0) {
    s0 = pt_obstacle.upper_left_point().s();
    s1 = pt_obstacle.upper_right_point().s();
    t0 = pt_obstacle.upper_left_point().t();
    t1 = pt_obstacle.upper_right_point().t();
  } else {
    s0 = pt_obstacle.lower_left_point().s();
    s1 = pt_obstacle.lower_right_point().s();
    t0 = pt_obstacle.lower_left_point().t();
    t1 = pt_obstacle.lower_right_point().t();
  }

  double time_gap = t1 - t0;
  ROS_ASSERT(time_gap > -1e-5);
  time_gap = std::fabs(time_gap);

  auto num_sections = static_cast<size_t>(time_gap / t_density + 1);
  double t_interval = time_gap / static_cast<double>(num_sections);

  for (size_t i = 0; i <= num_sections; ++i) {
    double t = t_interval * static_cast<double>(i) + t0;
    double s = MathUtil::lerp(s0, t0, s1, t1, t) + s_dist;

    STPoint ptt;
    ptt.set_t(t);
    ptt.set_s(s);
    pt_pairs.push_back(ptt);
  }

  return pt_pairs;

}
bool STGraph::IsObstacleInGraph(int obstacle_id) {
  return obstacle_st_map_.find(obstacle_id) != obstacle_st_map_.end();
}

}