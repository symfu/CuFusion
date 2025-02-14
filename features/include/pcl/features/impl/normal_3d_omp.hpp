/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2010-2011, Willow Garage, Inc.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage, Inc. nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: normal_3d_omp.hpp 6968 2012-08-26 18:46:25Z jspricke $
 *
 */

#ifndef PCL_FEATURES_IMPL_NORMAL_3D_OMP_H_
#define PCL_FEATURES_IMPL_NORMAL_3D_OMP_H_

#include <pcl/features/normal_3d_omp.h>

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT> void
pcl::NormalEstimationOMP<PointInT, Eigen::MatrixXf>::computeFeatureEigen (pcl::PointCloud<Eigen::MatrixXf> &output)
{
  float vpx, vpy, vpz;
  getViewPoint (vpx, vpy, vpz);
  output.is_dense = true;

  // Resize the output dataset
  output.points.resize (indices_->size (), 4);

  // Allocate enough space to hold the results
  // \note This resize is irrelevant for a radiusSearch ().
  std::vector<int> nn_indices (k_);
  std::vector<float> nn_dists (k_);

#ifdef _OPENMP
#pragma omp parallel for shared (output) private (nn_indices, nn_dists) num_threads(threads_)
#endif
  // Iterating over the entire index vector
  for (int idx = 0; idx < static_cast<int> (indices_->size ()); ++idx)
  {
    if (!isFinite ((*input_)[(*indices_)[idx]]) ||
        this->searchForNeighbors ((*indices_)[idx], search_parameter_, nn_indices, nn_dists) == 0)
    {
      output.points (idx, 0) = output.points (idx, 1) = output.points (idx, 2) = output.points (idx, 3) = std::numeric_limits<float>::quiet_NaN ();
      output.is_dense = false;
      continue;
    }

    // 16-bytes aligned placeholder for the XYZ centroid of a surface patch
    Eigen::Vector4f xyz_centroid;
    // Estimate the XYZ centroid
    compute3DCentroid (*surface_, nn_indices, xyz_centroid);

    // Placeholder for the 3x3 covariance matrix at each surface patch
    EIGEN_ALIGN16 Eigen::Matrix3f covariance_matrix;
    // Compute the 3x3 covariance matrix
    computeCovarianceMatrix (*surface_, nn_indices, xyz_centroid, covariance_matrix);

    // Get the plane normal and surface curvature
    solvePlaneParameters (covariance_matrix,
                          output.points (idx, 0), output.points (idx, 1), output.points (idx, 2), output.points (idx, 3));

    flipNormalTowardsViewpoint (input_->points[(*indices_)[idx]], vpx, vpy, vpz,
                                output.points (idx, 0), output.points (idx, 1), output.points (idx, 2));
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////
template <typename PointInT, typename PointOutT> void
pcl::NormalEstimationOMP<PointInT, PointOutT>::computeFeature (PointCloudOut &output)
{
  float vpx, vpy, vpz;
  getViewPoint (vpx, vpy, vpz);

  output.is_dense = true;

  // Allocate enough space to hold the results
  // \note This resize is irrelevant for a radiusSearch ().
  std::vector<int> nn_indices (k_);
  std::vector<float> nn_dists (k_);

  // Iterating over the entire index vector
#ifdef _OPENMP
#pragma omp parallel for shared (output) private (nn_indices, nn_dists) num_threads(threads_)
#endif
  for (int idx = 0; idx < static_cast<int> (indices_->size ()); ++idx)
  {
    if (!isFinite ((*input_)[(*indices_)[idx]]) ||
        this->searchForNeighbors ((*indices_)[idx], search_parameter_, nn_indices, nn_dists) == 0)
    {
      output.points[idx].normal[0] = output.points[idx].normal[1] = output.points[idx].normal[2] = output.points[idx].curvature = std::numeric_limits<float>::quiet_NaN ();
  
      output.is_dense = false;
      continue;
    }

    // 16-bytes aligned placeholder for the XYZ centroid of a surface patch
    Eigen::Vector4f xyz_centroid;
    // Estimate the XYZ centroid
    compute3DCentroid (*surface_, nn_indices, xyz_centroid);

    // Placeholder for the 3x3 covariance matrix at each surface patch
    EIGEN_ALIGN16 Eigen::Matrix3f covariance_matrix;
    // Compute the 3x3 covariance matrix
    computeCovarianceMatrix (*surface_, nn_indices, xyz_centroid, covariance_matrix);

    // Get the plane normal and surface curvature
    solvePlaneParameters (covariance_matrix,
                          output.points[idx].normal[0], output.points[idx].normal[1], output.points[idx].normal[2], output.points[idx].curvature);

    flipNormalTowardsViewpoint (input_->points[(*indices_)[idx]], vpx, vpy, vpz,
                                output.points[idx].normal[0], output.points[idx].normal[1], output.points[idx].normal[2]);
  }
}

#define PCL_INSTANTIATE_NormalEstimationOMP(T,NT) template class PCL_EXPORTS pcl::NormalEstimationOMP<T,NT>;

#endif    // PCL_FEATURES_IMPL_NORMAL_3D_OMP_H_

