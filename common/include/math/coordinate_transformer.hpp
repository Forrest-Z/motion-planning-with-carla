
#ifndef CATKIN_WS_SRC_MOTION_PLANNING_WITH_CARLA_COMMON_INCLUDE_MATH_COORDINATE_TRANSFORMER_HPP_
#define CATKIN_WS_SRC_MOTION_PLANNING_WITH_CARLA_COMMON_INCLUDE_MATH_COORDINATE_TRANSFORMER_HPP_

#include <array>
#include <Eigen/src/Core/Matrix.h>
namespace common {


// apollo and the reference paper

class CoordinateTransformer {

 public:
  CoordinateTransformer() = default;
  ~CoordinateTransformer() = default;
  static void CartesianToFrenet(double rs, double rx,
                                double ry, double rtheta,
                                double rkappa, double rdkappa,
                                double x, double y,
                                double v, double a,
                                double theta, double kappa,
                                std::array<double, 3> *const ptr_s_condition,
                                std::array<double, 3> *const ptr_d_condition);

  static void CartesianToFrenet(double rs, double rx,
                                double ry, double rtheta,
                                double x, double y,
                                double *const ptr_s,
                                double *const ptr_d);

  static void FrenetToCartesian(double rs, double rx,
                                double ry, double rtheta,
                                double rkappa, double rdkappa,
                                const std::array<double, 3> &s_condition,
                                const std::array<double, 3> &d_condition,
                                double *ptr_x, double *ptr_y,
                                double *ptr_theta, double *ptr_kappa,
                                double *ptr_v, double *ptr_a);

  static double CalcTheta(double rtheta, double rkappa,
                          double l, double dl);

  static double CalcKappa(double rkappa, double rdkappa,
                          double l, double dl, double ddl);

  static Eigen::Vector2d CalcCatesianPoint(double rtheta,
                                           double rx, double ry, double l);

  static double CalcLateralDerivative(double rtheta,
                                      double theta, double l,
                                      double rkappa);

  static double CalcSecondOrderLateralDerivative(double rtheta, double theta,
                                                 double rkappa, double kappa,
                                                 double rdkappa, double l);

};

}
#endif //CATKIN_WS_SRC_LOCAL_PLANNER_INCLUDE_COORDINATE_TRANSFORMER_HPP_
