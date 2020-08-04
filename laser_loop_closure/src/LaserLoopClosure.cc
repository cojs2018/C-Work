/*
 * Copyright (c) 2016, The Regents of the University of California (Regents).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *
 *    3. Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS AS IS
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Please contact the author(s) of this library if you have any questions.
 * Author: Erik Nelson            ( eanelson@eecs.berkeley.edu )
 */
#include <iostream>
#include <laser_loop_closure/LaserLoopClosure.h>
#include <Eigen/Dense>

#include <geometry_utils/GeometryUtilsROS.h>
#include <parameter_utils/ParameterUtils.h>
#include <pose_graph_msgs/KeyedScan.h>
#include <pose_graph_msgs/PoseGraph.h>
#include <std_msgs/Empty.h>
#include <visualization_msgs/Marker.h>

#include <pcl/registration/gicp.h>
#include <pcl/common/projection_matrix.h>
#include <pcl/point_cloud.h>
#include <pcl_conversions/pcl_conversions.h>
#include <pcl/io/ply_io.h>
#include <ros/console.h>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>

#include <fstream>
#include <ros/ros.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <string>
#include <sstream>
#include <iterator>


//#include "/home/lunet/cojs9/Documents/code/blam/internal/src/wavelet_transform/main_patch_2.h"
//#include "/home/lunet/cojs9/Documents/code/blam/internal/src/wavelet_transform/wavelet_transform.h"

#define Pi 3.14159265359

namespace gu = geometry_utils;
namespace gr = gu::ros;
namespace pu = parameter_utils;

using namespace wavelet;
using namespace std;

using gtsam::BetweenFactor;
using gtsam::ISAM2;
using gtsam::ISAM2Params;
using gtsam::NonlinearFactorGraph;
using gtsam::Pose3;
using gtsam::PriorFactor;
using gtsam::Rot3;
using gtsam::Values;
using gtsam::Vector3;
using gtsam::Vector6;

LaserLoopClosure::LaserLoopClosure()
    : key_(0), last_closure_key_(std::numeric_limits<int>::min()) {}

LaserLoopClosure::~LaserLoopClosure() {}

bool LaserLoopClosure::Initialize(const ros::NodeHandle& n) {
  name_ = ros::names::append(n.getNamespace(), "LaserLoopClosure");

  if (!filter_.Initialize(n)) {
    ROS_ERROR("%s: Failed to initialize point cloud filter.", name_.c_str());
    return false;
  }

  if (!LoadParameters(n)) {
    ROS_ERROR("%s: Failed to load parameters.", name_.c_str());
    return false;
  }

  if (!RegisterCallbacks(n)) {
    ROS_ERROR("%s: Failed to register callbacks.", name_.c_str());
    return false;
  }

  return true;
}

bool LaserLoopClosure::LoadParameters(const ros::NodeHandle& n) {

  // Load frame ids.
  if (!pu::Get("frame_id/fixed", fixed_frame_id_)) return false;
  if (!pu::Get("frame_id/base", base_frame_id_)) return false;

  // Should we turn loop closure checking on or off?
  if (!pu::Get("check_for_loop_closures", check_for_loop_closures_)) return false;

  // Load ISAM2 parameters.
  unsigned int relinearize_skip = 1;
  double relinearize_threshold = 0.01;
  if (!pu::Get("relinearize_skip", relinearize_skip)) return false;
  if (!pu::Get("relinearize_threshold", relinearize_threshold)) return false;

  // Load loop closing parameters.
  if (!pu::Get("translation_threshold", translation_threshold_)) return false;
  if (!pu::Get("proximity_threshold", proximity_threshold_)) return false;
  if (!pu::Get("max_tolerable_fitness", max_tolerable_fitness_)) return false;
  if (!pu::Get("skip_recent_poses", skip_recent_poses_)) return false;
  if (!pu::Get("poses_before_reclosing", poses_before_reclosing_)) return false;

  // Load ICP parameters.
  if (!pu::Get("icp/tf_epsilon", icp_tf_epsilon_)) return false;
  if (!pu::Get("icp/corr_dist", icp_corr_dist_)) return false;
  if (!pu::Get("icp/iterations", icp_iterations_)) return false;

  // Load initial position and orientation.
  double init_x = 0.0, init_y = 0.0, init_z = 0.0;
  double init_roll = 0.0, init_pitch = 0.0, init_yaw = 0.0;
  if (!pu::Get("init/position/x", init_x)) return false;
  if (!pu::Get("init/position/y", init_y)) return false;
  if (!pu::Get("init/position/z", init_z)) return false;
  if (!pu::Get("init/orientation/roll", init_roll)) return false;
  if (!pu::Get("init/orientation/pitch", init_pitch)) return false;
  if (!pu::Get("init/orientation/yaw", init_yaw)) return false;

  // Load initial position and orientation noise.
  double sigma_x = 0.0, sigma_y = 0.0, sigma_z = 0.0;
  double sigma_roll = 0.0, sigma_pitch = 0.0, sigma_yaw = 0.0;
  if (!pu::Get("init/position_sigma/x", sigma_x)) return false;
  if (!pu::Get("init/position_sigma/y", sigma_y)) return false;
  if (!pu::Get("init/position_sigma/z", sigma_z)) return false;
  if (!pu::Get("init/orientation_sigma/roll", sigma_roll)) return false;
  if (!pu::Get("init/orientation_sigma/pitch", sigma_pitch)) return false;
  if (!pu::Get("init/orientation_sigma/yaw", sigma_yaw)) return false;

  // Create the ISAM2 solver.
  ISAM2Params parameters;
  parameters.relinearizeSkip = relinearize_skip;
  parameters.relinearizeThreshold = relinearize_threshold;
  isam_.reset(new ISAM2(parameters));
	
  // Set the initial position.
  Vector3 translation(init_x, init_y, init_z);
  Rot3 rotation(Rot3::RzRyRx(init_roll, init_pitch, init_yaw));
  Pose3 pose(rotation, translation);

  // Set the covariance on initial position.
  Vector6 noise;
  noise << sigma_x, sigma_y, sigma_z, sigma_roll, sigma_pitch, sigma_yaw;
  LaserLoopClosure::Diagonal::shared_ptr covariance(
      LaserLoopClosure::Diagonal::Sigmas(noise));

  // Initialize ISAM2.
  NonlinearFactorGraph new_factor;
  Values new_value;
  new_factor.add(MakePriorFactor(pose, covariance));
  new_value.insert(key_, pose);

  isam_->update(new_factor, new_value);
  values_ = isam_->calculateEstimate();
  key_++;

  // Set the initial odometry.
  odometry_ = Pose3::identity();

  return true;
}

bool LaserLoopClosure::RegisterCallbacks(const ros::NodeHandle& n) {
  // Create a local nodehandle to manage callback subscriptions.
  ros::NodeHandle nl(n);

  odometry_edge_pub_ =
      nl.advertise<visualization_msgs::Marker>("odometry_edges", 10, false);
  loop_edge_pub_ =
      nl.advertise<visualization_msgs::Marker>("loop_edges", 10, false);
  graph_node_pub_ =
      nl.advertise<visualization_msgs::Marker>("graph_nodes", 10, false);
  keyframe_node_pub_ =
      nl.advertise<visualization_msgs::Marker>("keyframe_nodes", 10, false);
  closure_area_pub_ =
      nl.advertise<visualization_msgs::Marker>("closure_area", 10, false);

  scan1_pub_ = nl.advertise<PointCloud>("loop_closure_scan1", 10, false);
  scan2_pub_ = nl.advertise<PointCloud>("loop_closure_scan2", 10, false);

  pose_graph_pub_ =
      nl.advertise<pose_graph_msgs::PoseGraph>("pose_graph", 10, false);
  keyed_scan_pub_ =
      nl.advertise<pose_graph_msgs::KeyedScan>("keyed_scans", 10, false);
  loop_closure_notifier_pub_ =
      nl.advertise<std_msgs::Empty>("loop_closure", 10, false);

  return true;
}

bool LaserLoopClosure::AddBetweenFactor(
    const gu::Transform3& delta, const LaserLoopClosure::Mat66& covariance,
    const ros::Time& stamp, unsigned int* key) {
  if (key == NULL) {
    ROS_ERROR("%s: Output key is null.", name_.c_str());
    return false;
  }

  // Append the new odometry.
  Pose3 new_odometry = ToGtsam(delta);

  NonlinearFactorGraph new_factor;
  Values new_value;
  new_factor.add(MakeBetweenFactor(new_odometry, ToGtsam(covariance)));

  Pose3 last_pose = values_.at<Pose3>(key_-1);
  new_value.insert(key_, last_pose.compose(new_odometry));

  // Store this timestamp so that we can publish the pose graph later.
  keyed_stamps_.insert(std::pair<unsigned int, ros::Time>(key_, stamp));

  // Update ISAM2.
  isam_->update(new_factor, new_value);
  values_ = isam_->calculateEstimate();

  // Assign output and get ready to go again!
  *key = key_++;

  // We always add new poses, but only return true if the pose is far enough
  // away from the last one (keyframes). This lets the caller know when they
  // can add a laser scan.

  // Is the odometry translation large enough to add a new keyframe to the graph?
  odometry_ = odometry_.compose(new_odometry);
  if (odometry_.translation().norm() > translation_threshold_) {
    odometry_ = Pose3::identity();
    return true;
  }else {
    return false;    
  }

  return false;
}

bool LaserLoopClosure::AddKeyScanPair(unsigned int key,
                                      const PointCloud::ConstPtr& scan) {
  if (keyed_scans_.count(key)) {
    ROS_ERROR("%s: Key %u already has a laser scan.", name_.c_str(), key);
    return false;
  }

  // The first key should be treated differently; we need to use the laser
  // scan's timestamp for pose zero.
  if (key == 0) {
    const ros::Time stamp = pcl_conversions::fromPCL(scan->header.stamp);
    keyed_stamps_.insert(std::pair<unsigned int, ros::Time>(key, stamp));
  }

  // Add the key and scan.
  keyed_scans_.insert(std::pair<unsigned int, PointCloud::ConstPtr>(key, scan));

  // Publish the inserted laser scan.
  if (keyed_scan_pub_.getNumSubscribers() > 0) {
    pose_graph_msgs::KeyedScan keyed_scan;
    keyed_scan.key = key;

    pcl::toROSMsg(*scan, keyed_scan.scan);
    keyed_scan_pub_.publish(keyed_scan);
  }

  return true;
}

bool LaserLoopClosure::FindLoopClosures(
    unsigned int key, std::vector<unsigned int>* closure_keys) {

  // If loop closure checking is off, don't do this step. This will save some
  // computation time.
  if (!check_for_loop_closures_)
    return false;

  // Check arguments.
  if (closure_keys == NULL) {
    ROS_ERROR("%s: Output pointer is null.", name_.c_str());
    return false;
  }
  closure_keys->clear();

  // If a loop has already been closed recently, don't try to close a new one.
  if (std::fabs(key - last_closure_key_) < poses_before_reclosing_)
    return false;

  // Get pose and scan for the provided key.
  const gu::Transform3 pose1 = ToGu(values_.at<Pose3>(key));
  const PointCloud::ConstPtr scan1 = keyed_scans_[key];

  // Iterate through past poses and find those that lie close to the most
  // recently added one.
  bool closed_loop = false;
  for (const auto& keyed_pose : values_) {
    const unsigned int other_key = keyed_pose.key;

    // Don't self-check.
    if (other_key == key)
      continue;

    // Don't compare against poses that were recently collected.
    if (std::fabs(key - other_key) < skip_recent_poses_)
      continue;

    // Don't check for loop closures against poses that are not keyframes.
    if (!keyed_scans_.count(other_key))
      continue;

    const gu::Transform3 pose2 = ToGu(values_.at<Pose3>(other_key));
    const gu::Transform3 difference = gu::PoseDelta(pose1, pose2);
    if ((difference.translation.Norm() < proximity_threshold_) && (difference.translation.Norm() > 0)) {
      // Found a potential loop closure! Perform ICP between the two scans to
      // determine if there really is a loop to close.
      const PointCloud::ConstPtr scan2 = keyed_scans_[other_key];

      gu::Transform3 delta;
      LaserLoopClosure::Mat66 covariance;
      if (PerformICP(scan1, scan2, pose1, pose2, &delta, &covariance)) {
        // We found a loop closure. Add it to the pose graph.
        NonlinearFactorGraph new_factor;
        new_factor.add(BetweenFactor<Pose3>(key, other_key, ToGtsam(delta),
                                            ToGtsam(covariance)));
        isam_->update(new_factor, Values());
        closed_loop = true;
        last_closure_key_ = key;

        // Store for visualization and output.
        loop_edges_.push_back(std::make_pair(key, other_key));
        closure_keys->push_back(other_key);

        // Send an empty message notifying any subscribers that we found a loop
        // closure.
        loop_closure_notifier_pub_.publish(std_msgs::Empty());
      } else {
	return false;
      }
    } else {
      return false;
    }
  }
  values_ = isam_->calculateEstimate();

  return closed_loop;
}

void LaserLoopClosure::GetMaximumLikelihoodPoints(PointCloud* points) {
  if (points == NULL) {
    ROS_ERROR("%s: Output point cloud container is null.", name_.c_str());
    return;
  }
  points->points.clear();

  // Iterate over poses in the graph, transforming their corresponding laser
  // scans into world frame and appending them to the output.
  for (const auto& keyed_pose : values_) {
    const unsigned int key = keyed_pose.key;

    // Check if this pose is a keyframe. If it's not, it won't have a scan
    // associated to it and we should continue.
    if (!keyed_scans_.count(key))
      continue;

    const gu::Transform3 pose = ToGu(values_.at<Pose3>(key));
    Eigen::Matrix4d b2w;
    b2w.block(0, 0, 3, 3) = pose.rotation.Eigen();
    b2w.block(0, 3, 3, 1) = pose.translation.Eigen();

    // Transform the body-frame scan into world frame.
    PointCloud scan_world;
    pcl::transformPointCloud(*keyed_scans_[key], scan_world, b2w);

    // Append the world-frame point cloud to the output.
    *points += scan_world;
  }
}

gu::Transform3 LaserLoopClosure::GetLastPose() const {
  if (key_ > 1) {
    return ToGu(values_.at<Pose3>(key_-1));
  } else {
    ROS_WARN("%s: The graph only contains its initial pose.", name_.c_str());
    return ToGu(values_.at<Pose3>(0));
  }
}

gu::Transform3 LaserLoopClosure::ToGu(const Pose3& pose) const {
  gu::Transform3 out;
  out.translation(0) = pose.translation().x();
  out.translation(1) = pose.translation().y();
  out.translation(2) = pose.translation().z();

  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j)
      out.rotation(i, j) = pose.rotation().matrix()(i, j);
  }

  return out;
}

Pose3 LaserLoopClosure::ToGtsam(const gu::Transform3& pose) const {
  Vector3 t;
  t(0) = pose.translation(0);
  t(1) = pose.translation(1);
  t(2) = pose.translation(2);

  Rot3 r(pose.rotation(0, 0), pose.rotation(0, 1), pose.rotation(0, 2),
         pose.rotation(1, 0), pose.rotation(1, 1), pose.rotation(1, 2),
         pose.rotation(2, 0), pose.rotation(2, 1), pose.rotation(2, 2));

  return Pose3(r, t);
}

LaserLoopClosure::Mat66 LaserLoopClosure::ToGu(
    const LaserLoopClosure::Gaussian::shared_ptr& covariance) const {
  gtsam::Matrix66 gtsam_covariance = covariance->covariance();

  LaserLoopClosure::Mat66 out;
  for (int i = 0; i < 6; ++i)
    for (int j = 0; j < 6; ++j)
      out(i, j) = gtsam_covariance(i, j);

  return out;
}

LaserLoopClosure::Gaussian::shared_ptr LaserLoopClosure::ToGtsam(
    const LaserLoopClosure::Mat66& covariance) const {
  gtsam::Matrix66 gtsam_covariance;

  for (int i = 0; i < 6; ++i)
    for (int j = 0; j < 6; ++j)
      gtsam_covariance(i, j) = covariance(i, j);

  return Gaussian::Covariance(gtsam_covariance);
}

PriorFactor<Pose3> LaserLoopClosure::MakePriorFactor(
    const Pose3& pose,
    const LaserLoopClosure::Diagonal::shared_ptr& covariance) {
  return PriorFactor<Pose3>(key_, pose, covariance);
}

BetweenFactor<Pose3> LaserLoopClosure::MakeBetweenFactor(
    const Pose3& delta,
    const LaserLoopClosure::Gaussian::shared_ptr& covariance) {
  odometry_edges_.push_back(std::make_pair(key_-1, key_));
  return BetweenFactor<Pose3>(key_-1, key_, delta, covariance);
}

bool LaserLoopClosure::PerformICP(const PointCloud::ConstPtr& scan1,
                                  const PointCloud::ConstPtr& scan2,
                                  const gu::Transform3& pose1,
                                  const gu::Transform3& pose2,
                                  gu::Transform3* delta,
                                  LaserLoopClosure::Mat66* covariance) {
  if (delta == NULL || covariance == NULL) {
    ROS_ERROR("%s: Output pointers are null.", name_.c_str());
    return false;
  }

  // Set up ICP.
  pcl::GeneralizedIterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> icp;
  icp.setTransformationEpsilon(icp_tf_epsilon_);
  icp.setMaxCorrespondenceDistance(icp_corr_dist_);
  icp.setMaximumIterations(icp_iterations_);
  icp.setRANSACIterations(0);

  // Filter the two scans. They are stored in the pose graph as dense scans for
  // visualization.
  PointCloud::Ptr scan1_filtered(new PointCloud);
  PointCloud::Ptr scan2_filtered(new PointCloud);
  filter_.Filter(scan1, scan1_filtered);
  filter_.Filter(scan2, scan2_filtered);

  // Set source point cloud. Transform it to pose 2 frame to get a delta.
  const Eigen::Matrix<double, 3, 3> R1 = pose1.rotation.Eigen();
  const Eigen::Matrix<double, 3, 1> t1 = pose1.translation.Eigen();
  Eigen::Matrix4d body1_to_world;
  body1_to_world.block(0, 0, 3, 3) = R1;
  body1_to_world.block(0, 3, 3, 1) = t1;

  const Eigen::Matrix<double, 3, 3> R2 = pose2.rotation.Eigen();
  const Eigen::Matrix<double, 3, 1> t2 = pose2.translation.Eigen();
  Eigen::Matrix4d body2_to_world;
  body2_to_world.block(0, 0, 3, 3) = R2;
  body2_to_world.block(0, 3, 3, 1) = t2;

  PointCloud::Ptr source(new PointCloud);
  pcl::transformPointCloud(*scan1_filtered, *source, body1_to_world);
  icp.setInputSource(source);

  // Set target point cloud in its own frame.
  PointCloud::Ptr target(new PointCloud);
  pcl::transformPointCloud(*scan2_filtered, *target, body2_to_world);
  icp.setInputTarget(target);

  // Perform ICP.
  PointCloud unused_result;
  icp.align(unused_result);

  // Get resulting transform.
  const Eigen::Matrix4f T = icp.getFinalTransformation();
  gu::Transform3 delta_icp;
  delta_icp.translation = gu::Vec3(T(0, 3), T(1, 3), T(2, 3));
  delta_icp.rotation = gu::Rot3(T(0, 0), T(0, 1), T(0, 2),
                                T(1, 0), T(1, 1), T(1, 2),
                                T(2, 0), T(2, 1), T(2, 2));

  // Is the transform good?
  if (!icp.hasConverged())
    return false;

  if (icp.getFitnessScore() > max_tolerable_fitness_) {
    return false;
  }

  // Update the pose-to-pose odometry estimate using the output of ICP.
  const gu::Transform3 update =
      gu::PoseUpdate(gu::PoseUpdate(pose1, gu::PoseInverse(delta_icp)), gu::PoseInverse(pose1));

  *delta = gu::PoseUpdate(gu::PoseDelta(pose1, pose2), update);
  
  try
  {
    //Modification by Joseph Stevens
    //Compute norm from here.
    Eigen::Matrix<double, 3, 1> fT;
    fT << T(0, 3), 
         T(1, 3), 
         T(2, 3);
    double kappaT = fT.norm();
    Eigen::Matrix<double,3,1> tauT;
    for(int jj = 0; jj < 3; jj++)
    {
      double tT = t1(jj);
      double ttT = t2(jj);
      tauT(jj) = (tT - kappaT)*(ttT - kappaT);
    }
    double psiT = tauT.norm();

    wavelet::transform jwtT;

    double cov = jwtT.W(0, 0.1, 2, 1);

    std::cout << cov;

    if(psiT > 0.1) //Normalise the coveriance between [0, 0.1]
    {
      while(psiT > 0.1)
      {
	psiT = psiT*cov;
      }
    }
    
    Eigen::Matrix<double, 3, 1> fR;
    fR << T(0, 0), T(0, 1), T(0, 2),
          T(1, 0), T(1, 1), T(1, 2),
          T(2, 0), T(2, 1), T(2, 2);
    double kappaR = fR.norm();
    Eigen::Matrix<double,3,3> tauR;
    for(int jj = 0; jj < 3; jj++)
    {
      for(int kk = 0; kk < 3; kk++)
      {
	//Assuming radians
        tauR(jj,kk) = (R1(jj,kk) - kappaR)*(R2(jj,kk) - kappaR);
      }
    }
    double psiR = tauR.norm();

    wavelet::transform jwtR;

    cov = jwtR.W(0, 0.04, 2, 1);

    if(psiR > 0.04) //Normalise the coveriance between [0, 0.04]
    {
      while(psiR > 0.04)
      {
	psiR = psiR*cov;
      }
    }

    // TODO: Use real ICP covariance.
    covariance->Zeros();
    for (int i = 0; i < 3; ++i)
      (*covariance)(i, i) = psiT;
    for (int i = 3; i < 6; ++i)
      (*covariance)(i, i) = psiR;
  }catch(std::exception ex)
  {
    ROS_ERROR("Error: %s", ex.what());
  }

  // If the loop closure was a success, publish the two scans.
  source->header.frame_id = fixed_frame_id_;
  target->header.frame_id = base_frame_id_;
  //frame_id = fixed_frame_id_;
  scan1_pub_.publish(*source);
  scan2_pub_.publish(*target);

  return true;
}

void LaserLoopClosure::PublishPoseGraph() {
  PointCloud Scloud;
  double x;
  double y;
  const double z = 0;

  try
  {
    // Publish odometry edges.
    if (odometry_edge_pub_.getNumSubscribers() > 0) {
      visualization_msgs::Marker m;
      m.header.frame_id = fixed_frame_id_;
      m.ns = fixed_frame_id_;
      m.id = 0;
      m.action = visualization_msgs::Marker::ADD;
      m.type = visualization_msgs::Marker::LINE_LIST;
      m.color.r = 1.0;
      m.color.g = 0.0;
      m.color.b = 0.0;
      m.color.a = 0.8;
      m.scale.x = 0.02;

      for (size_t ii = 0; ii < odometry_edges_.size(); ++ii) {
        unsigned int key1 = odometry_edges_[ii].first;
        unsigned int key2 = odometry_edges_[ii].second;

        gu::Vec3 p1 = ToGu(values_.at<Pose3>(key1)).translation;
        gu::Vec3 p2 = ToGu(values_.at<Pose3>(key2)).translation;

        gu::Rot3 r1 = ToGu(values_.at<Pose3>(key1)).rotation;
        gu::Rot3 r2 = ToGu(values_.at<Pose3>(key2)).rotation;

        p1(2) = 0;
        p2(2) = 0;

        Eigen::Matrix3d rot_limit; //Get specific ration matrix format
        rot_limit << 1, 0, 0,
      	       	     0, 1, 0,
     		     0, 0, 1;
      
        for(int k = 0; k < 3; k++) //Ensure rotation matrix stays in correct format
        {
	  if(r1(k,2) != rot_limit(k,2))
          {
	    r1(k,2) = rot_limit(k,2);
          }

	  if(r2(2,k) != rot_limit(2,k))
          {
	    r2(2,k) = rot_limit(2,k);
          }

	  if(r2(k,2) != rot_limit(k,2))
          {
	    r2(k,2) = rot_limit(k,2);
          }

	  if(r1(2,k) != rot_limit(2,k))
          {
	    r1(2,k) = rot_limit(2,k);
          }

          double yaw_r = acos(r2(1, 1)) - acos(r1(1, 1)); //Get the yaw angle
	  if((yaw_r != 0) && (k < 2)){
	    p2(k) = p1(k) + (p2(k) - p1(k))*cos(yaw_r); //Set approprately for correct raw angle
          } else if(k < 2) {
            p2(k) = p1(k); //else rotation is identity and no translation exists
          }
        }

	//bool check = isinf(p1(1)) || isinf(p2(1)) || isinf(p1(2)) || isinf(p2(2)) || isnan(p1(1)) || isnan(p2(1)) || isnan(p1(2)) || isnan(p2(2));

	//if(check == true){
	  //ROS_ERROR("Invalid odometry value");
	//}
	//else{
	m.points.push_back(gr::ToRosPoint(p1));
        m.points.push_back(gr::ToRosPoint(p2));

        Scloud.push_back(pcl::PointXYZ(float(p1(0)), float(p1(1)), float(p1(2))));
        Scloud.push_back(pcl::PointXYZ(float(p2(0)), float(p2(1)), float(p2(2))));
  
        //Get location and save with map to a specified directory

        x = double(p2(0));
        y = double(p2(1));
	//}

      }
      odometry_edge_pub_.publish(m);
    }
  }catch(std::exception& ex2)
  {
    ROS_ERROR("%s\n", ex2.what());
    LaserLoopClosure::PublishPoseGraph();
  }

  // Publish loop closure edges.
  if (loop_edge_pub_.getNumSubscribers() > 0) {
    visualization_msgs::Marker m;
    m.header.frame_id = fixed_frame_id_;
    m.ns = fixed_frame_id_;
    m.id = 1;
    m.action = visualization_msgs::Marker::ADD;
    m.type = visualization_msgs::Marker::LINE_LIST;
    m.color.r = 0.0;
    m.color.g = 0.2;
    m.color.b = 1.0;
    m.color.a = 0.8;
    m.scale.x = 0.02;

    for (size_t ii = 0; ii < loop_edges_.size(); ++ii) {
      unsigned int key1 = loop_edges_[ii].first;
      unsigned int key2 = loop_edges_[ii].second;

      gu::Vec3 p1 = ToGu(values_.at<Pose3>(key1)).translation;
      gu::Vec3 p2 = ToGu(values_.at<Pose3>(key2)).translation;
      

      gu::Rot3 r1 = ToGu(values_.at<Pose3>(key1)).rotation;
      gu::Rot3 r2 = ToGu(values_.at<Pose3>(key2)).rotation;

      Eigen::Matrix3d rot_limit;
      rot_limit << 1, 0, 0,
      		   0, 1, 0,
     		   0, 0, 1;
      
      p1(2) = 0;
      p2(2) = 0;

       for(int k = 0; k < 3; k++) //Ensure rotation matrix stays in correct format
        {
	  if(r1(k,2) != rot_limit(k,2))
          {
	    r1(k,2) = rot_limit(k,2);
          }

	  if(r2(2,k) != rot_limit(2,k))
          {
	    r2(2,k) = rot_limit(2,k);
          }

 	 /* if(r2(k,2) != rot_limit(k,2))
          {
	    r2(k,2) = rot_limit(k,2);
          }

	  if(r1(2,k) != rot_limit(2,k))
          {
	    r1(2,k) = rot_limit(2,k);
          }*/
	  
          double yaw_r = acos(r2(1, 1)) - acos(r1(1, 1)); //Get the yaw angle
	  if(yaw_r != 0){
	    p2(k) = p1(k) + (p2(k) - p1(k))*cos(yaw_r); //Set approprately for correct raw angle
          } else {
            p2(k) = p1(k); //else rotation is identity and no translation exists
          }
        }

      
      m.points.push_back(gr::ToRosPoint(p1));
      m.points.push_back(gr::ToRosPoint(p2));

      Scloud.push_back(pcl::PointXYZ(float(p1(0)), float(p1(1)), float(p1(2))));
      Scloud.push_back(pcl::PointXYZ(float(p2(0)), float(p2(1)), float(p2(2))));
    }
    loop_edge_pub_.publish(m);
  }

  // Publish nodes in the pose graph.
  if (graph_node_pub_.getNumSubscribers() > 0) {
    visualization_msgs::Marker m;
    m.header.frame_id = fixed_frame_id_;
    m.ns = fixed_frame_id_;
    m.id = 2;
    m.action = visualization_msgs::Marker::ADD;
    m.type = visualization_msgs::Marker::SPHERE_LIST;
    m.color.r = 0.3;
    m.color.g = 0.0;
    m.color.b = 1.0;
    m.color.a = 0.8;
    m.scale.x = 0.1;
    m.scale.y = 0.1;
    m.scale.z = 0.1;

    for (const auto& keyed_pose : values_) {
      gu::Vec3 p = ToGu(values_.at<Pose3>(keyed_pose.key)).translation;
      p(2) = 0;
      
 /*     gu::Rot3 r = ToGu(values_.at<Pose3>(keyed_pose.key)).rotation;

      Eigen::Matrix3d rot_limit;
      rot_limit << 1, 0, 0,
		   0, 1, 0,
		   0, 0, 1;
      
      
      for(int k = 0; k < 3; k++) //Ensure rotation matrix stays in correct format
      {
	if(r(k,2) != rot_limit(k,2))
        {
	  r(k,2) = rot_limit(k,2);
        }

	if(r(2,k) != rot_limit(2,k))
        {
	  r(2,k) = rot_limit(2,k);
        }
      }*/

      //gu::Vec3 q = apply(Keyed_pose.key, p);
      m.points.push_back(gr::ToRosPoint(p));
      Scloud.push_back(pcl::PointXYZ(float(p(0)), float(p(1)), float(p(2))));
    }
    graph_node_pub_.publish(m);
  }

  // Publish keyframe nodes in the pose graph.
  if (keyframe_node_pub_.getNumSubscribers() > 0) {
    visualization_msgs::Marker m;
    m.header.frame_id = fixed_frame_id_;
    m.ns = fixed_frame_id_;
    m.id = 3;
    m.action = visualization_msgs::Marker::ADD;
    m.type = visualization_msgs::Marker::SPHERE_LIST;
    m.color.r = 0.0;
    m.color.g = 1.0;
    m.color.b = 0.3;
    m.color.a = 0.8;
    m.scale.x = 0.25;
    m.scale.y = 0.25;
    m.scale.z = 0.25;

    for (const auto& keyed_pose : values_) {
      if (keyed_scans_.count(keyed_pose.key)) {
        gu::Vec3 p = ToGu(values_.at<Pose3>(keyed_pose.key)).translation;
        p(2) = 0;
	//Modification by Joseph Stevens
	gu::Rot3 r = ToGu(values_.at<Pose3>(keyed_pose.key)).rotation;
	Eigen::Matrix3d rot_limit;
        rot_limit << 1, 0, 0,
		     0, 1, 0,
		     0, 0, 1;
      
      
   /*     for(int k = 0; k < 3; k++) //Ensure rotation matrix stays in correct format
        {
	  if(r(k,2) != rot_limit(k,2))
          {
	    r(k,2) = rot_limit(k,2);
          }

	  if(r(2,k) != rot_limit(2,k))
          {
	    r(2,k) = rot_limit(2,k);
          }
        }*/
        m.points.push_back(gr::ToRosPoint(p));
        Scloud.push_back(pcl::PointXYZ(float(p(0)), float(p(1)), float(p(2))));
      }
    }
    graph_node_pub_.publish(m);
  }

  // Draw a sphere around the current sensor frame to show the area in which we
  // are checking for loop closures.
  if (closure_area_pub_.getNumSubscribers() > 0) {
    visualization_msgs::Marker m;
    m.header.frame_id = base_frame_id_;
    m.ns = base_frame_id_;
    m.id = 4;
    m.action = visualization_msgs::Marker::ADD;
    m.type = visualization_msgs::Marker::SPHERE;
    m.color.r = 0.0;
    m.color.g = 0.4;
    m.color.b = 0.8;
    m.color.a = 0.4;
    m.scale.x = proximity_threshold_ * 2.0;
    m.scale.y = proximity_threshold_ * 2.0;
    m.scale.z = proximity_threshold_ * 2.0;
    m.pose = gr::ToRosPose(gu::Transform3::Identity());
    closure_area_pub_.publish(m);
  }

  // Construct and send the pose graph.
  if (pose_graph_pub_.getNumSubscribers() > 0) {
    pose_graph_msgs::PoseGraph g;
    g.header.frame_id = fixed_frame_id_;

    for (const auto& keyed_pose : values_) {
      gu::Transform3 t = ToGu(values_.at<Pose3>(keyed_pose.key));

      // Populate the message with the pose's data.
      pose_graph_msgs::PoseGraphNode node;
      node.key = keyed_pose.key;
      node.header.frame_id = fixed_frame_id_;
      node.pose = gr::ToRosPose(t);
      if (keyed_stamps_.count(keyed_pose.key)) {
        node.header.stamp = keyed_stamps_[keyed_pose.key];
      } else {
        ROS_WARN("%s: Couldn't find timestamp for key %lu", name_.c_str(),
                 keyed_pose.key);
      }
      //pose_graph_msgs::PoseGraphNode fixed_node = pose_graph_msgs::PoseGraphNode(apply(node.key, gu:Vec3(node)));
      g.nodes.push_back(node);
    }

    pose_graph_msgs::PoseGraphEdge edge;
    //pose_graph_msgs::PoseGraphEdge edge2;
    for (size_t ii = 0; ii < odometry_edges_.size(); ++ii) {
      edge.key_from = odometry_edges_[ii].first;
      edge.key_to = odometry_edges_[ii].second;
      //edge2 = apply(edge.key_from, ToGu(edge));
      g.edges.push_back(edge);
    }

    for (size_t ii = 0; ii < loop_edges_.size(); ++ii) {
      edge.key_from = loop_edges_[ii].first;
      edge.key_to = loop_edges_[ii].second;
      //edge2 = apply(edge.key_from, ToGu(edge));
      g.edges.push_back(edge);
    }

    // Publish.
    pose_graph_pub_.publish(g);
  }

  try{
    time_t t = time(0); //Get current local time
    struct tm* now = std::localtime(&t);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d-%H-%M-%S", now);

    std::string now_str = buffer;

    globe::loc gl;
    PointCloud::Ptr cloud_test(new PointCloud);
    bool emp = gl.xyz_empty(*cloud_test);
      
    //Save file will be in format "saveMap.ply"
    std::string f_str = "saveMap.ply";
    gl.xyz_save(f_str, Scloud, emp);
    //Store location
    gl.xyz_location(x,y,z,now_str);
  }catch(std::exception& ex3){
    ROS_ERROR("%s", ex3.what());
  }
}

/////////////////////////////////////////////////////////////////////////////////

double lambda = 20*sqrt(5);//Default: Range divided by the maximum dilation of k_correspondences

double transform::Simpsons3_8rule(double V[], int n, double s) //Conduct integration
{
  double Integral = V[0];
  for(int i = i; i < n-1; i++)
  {
    double v = V[i];
    Integral = Integral + 3*v;
  }
  Integral = Integral + V[n-1];
  Integral = (3/8)*s*Integral;
}

double transform::W(double z_b, double z_t, double a, double b)
{ //Find wavelet covariance transform between amplitudes z_b and z_t, dilation a, of magnitude distance from origin b.
  double z0 = z_b;
  transform tf;
  double Y0[100]; //Store in array
  int j = 0;
  if((a > 1) && (a < 20))
  {
    lambda = a*b;
  }
  while(z0 < z_t)
  {
    double g = tf.f(z0)*tf.h(z0,b,a);
    Y0[j] = g;
    z0 = z0 + z_t*0.01;
    j++;
  }
  int n0 = j; //number of elements in the array
  double integ = tf.Simpsons3_8rule(Y0, n0, 0.1); //Get integral
  double ans = (1/a)*integ; //Divide by number of points in sample
  return ans;
}

double transform::h(double amp, double loc, double dil)
{ //Imagenary exponentail function h, h(w) = Integral(f(t)*e^(-jwt), dt). for some function f(t). Note: Only returns real values in {-1, 0, 1}.
  if ((amp <= dil) && (amp >= loc - dil / 2))
  {
    return 1;
  }
  else if ((amp >= dil) && (amp <= loc - dil / 2))
  {
    return -1;
  }
  else
  {
    return 0;
  }
}

double transform::f(double z)
{
  /* Estimate equation of wavelet signal as Lambda*cos(z*t + phi) where:
    * Lambda is the minimum possible frequency equal to 20*sqrt(5) Hertz
    * z is altitude
    * t is a time increment
    * phi is a random variable between -Pi and Pi (radians)
    */
    double t = double(clock());

    double phi = double(rand()) / 2*Pi + (-1)*Pi;

    double ans = lambda * cos(z*t + phi);

    return ans;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

using namespace globe;

bool loc::xyz_empty(pcl::PointCloud<pcl::PointXYZ> cloud)
{
  //File for map
  if(pcl::io::loadPLYFile(loc::dir_str + "saveMap.ply", cloud) == -1)
  {
    ROS_WARN("Warning: no files of type *.ply in path %s.", loc::dir_str);
    return true;
  }
  
  return false;
}

bool loc::xyz_save(std::string mfile, pcl::PointCloud<pcl::PointXYZ> cloud, bool emp)
{
  //Save file will be in format "saveMap.ply"

  //Now add a new file
  if(!cloud.empty())
  {
    pcl::io::savePLYFile(loc::dir_str + mfile, cloud, true);
    return true;
  }
  return false;
}

void loc::xyz_location(double x, double y, double z, std::string t) //save location of lidar
{

  if(abs(x) > 9999)
  {
    loc::multi = 0;
    while(abs(x) > 9999)
    {
      x = round(x/10);
      y = round(y/10);
      z = 0;
      loc::multi++;
    }
  }

  std::string locat_str = "[New Map] location:" + to_string(x) + "," + to_string(y) + "," + to_string(z) + "; at time:" + t + ".\n"; //Find way keeping point at 4sf
  
  std::ofstream out(loc::dir_str + "saveStream.txt");
  out << locat_str;
  out.flush();
  out.close();

}

Eigen::Matrix<double, 3, 1> loc::xyz_get(std::string mfile, pcl::PointCloud<pcl::PointXYZ> cloud) //Get map file and last location, work out vector between last and new location and get new location from that
{
  //File for map
  pcl::io::loadPLYFile(loc::dir_str + mfile, cloud);

  //Read text file for location
  std::ifstream inp("saveStream.txt");
  std::string loc_str;
  for(std::string line; getline(inp, line);)
  {
    if(line != "")
    {
      loc_str = line;
    }
  }
  
  double x; double y; double z;
  Eigen::Matrix<double, 3, 1> last_location;
  
  //Numbers should exist at points 20, 26 and 32 and are of 4sf
  
  std::string x_str = loc_str.substr(20, 4);
  std::string y_str = loc_str.substr(26, 4);
  std::string z_str = loc_str.substr(32, 4);
  
  std::stringstream x_stream(x_str);
  std::stringstream y_stream(y_str);
  std::stringstream z_stream(z_str);
  
  x_stream >> x;
  y_stream >> y;
  z_stream >> z;

  ROS_INFO("Last position, x: %s, y: %s, z: %s \n", x_str.c_str(), y_str.c_str(), z_str.c_str());

  last_location << x * pow(10,double(loc::multi)),
		   y * pow(10,double(loc::multi)),
		   z * pow(10,double(loc::multi));
  
  return last_location;
}

