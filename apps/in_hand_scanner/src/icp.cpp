/*
 * Software License Agreement (BSD License)
 *
 * Point Cloud Library (PCL) - www.pointclouds.org
 * Copyright (c) 2009-2012, Willow Garage, Inc.
 * Copyright (c) 2012-, Open Perception, Inc.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *  * Neither the name of the copyright holder(s) nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: icp.cpp 7151 2012-09-15 20:31:30Z Martin $
 *
 */

#include <pcl/apps/in_hand_scanner/icp.h>

#include <limits>
#include <cstdlib> // EXIT_SUCCESS, EXIT_FAILURE

// #include <iomanip>

#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/common/transforms.h>
#include <pcl/common/impl/centroid.hpp>

////////////////////////////////////////////////////////////////////////////////

pcl::ihs::ICP::ICP ()
  : kd_tree_ (new pcl::KdTreeFLANN <PointNormal> ()),

    epsilon_        (1e-9f),
    max_iterations_ (50),
    min_overlap_    (.75f),
    max_fitness_    (10e-6f),

    squared_distance_threshold_factor_ (9.f),
    normals_threshold_                 (.7f)
{
}

////////////////////////////////////////////////////////////////////////////////

bool
pcl::ihs::ICP::findTransformation (const CloudModelConstPtr&     cloud_model,
                                   const CloudProcessedConstPtr& cloud_data,
                                   const Transformation&         T_init,
                                   Transformation&               T_final)
{
  // Check the input
  // TODO: Double check the minimum number of points necessary for icp
  const size_t n_min = 4;

  if(cloud_model->size () < n_min || cloud_data->size () < n_min)
  {
    std::cerr << "ERROR in icp.cpp: Not enough input points!\n";
    return (false);
  }

  // Convergence and registration failure
  float current_fitness  = 0.f;
  float previous_fitness = std::numeric_limits <float>::max ();
  float delta_fitness    = std::numeric_limits <float>::max ();
  float overlap          = std::numeric_limits <float>::quiet_NaN ();

  // Outlier rejection
  float squared_distance_threshold = std::numeric_limits<float>::max();

  // Transformation
  Transformation T_cur = T_init;

  // Point selection
  const CloudNormalConstPtr cloud_model_selected = this->selectModelPoints (cloud_model, T_init.inverse ());
  const CloudNormalConstPtr cloud_data_selected = this->selectDataPoints (cloud_data);

  const size_t n_data  = cloud_data_selected->size ();
  const size_t n_model = cloud_model_selected->size ();
  if(n_model < n_min || n_data < n_min)
  {
    std::cerr << "ERROR in icp.cpp: Not enough points after selection!\n";
    return (false);
  }

  // Build a kd-tree
  kd_tree_->setInputCloud (cloud_model_selected);

  // ICP main loop
  unsigned int iter = 1;
  while (true)
  {
    // Clouds with one to one correspondences
    CloudNormalPtr cloud_model_corr (new CloudNormal ());
    CloudNormalPtr cloud_data_corr (new CloudNormal ());

    cloud_model_corr->reserve (n_data);
    cloud_data_corr->reserve (n_data);

    // Accumulated error
    float squared_distance_sum = 0.f;

    // NN search
    std::vector <int>   index (1);
    std::vector <float> squared_distance (1);

    CloudNormal::const_iterator it_d = cloud_data_selected->begin ();
    for (; it_d!=cloud_data_selected->end (); ++it_d)
    {
      // Transform the data point
      PointNormal pt_d = *it_d;
      pt_d.getVector4fMap ()       = T_cur * pt_d.getVector4fMap ();
      pt_d.getNormalVector4fMap () = T_cur * pt_d.getNormalVector4fMap ();

      // Find the correspondence to the model points
      if (!kd_tree_->nearestKSearch (pt_d, 1, index, squared_distance))
      {
        std::cerr << "ERROR in icp.cpp: nearestKSearch failed!\n";
        return (false);
      }

      // Check the distance threshold
      if (squared_distance[0] < squared_distance_threshold)
      {
        if (index[0] >= cloud_model_selected->size ())
        {
          std::cerr << "ERROR in icp.cpp: Segfault!\n";
          std::cerr << "  Trying to access index " << index[0] << " >= " << cloud_model_selected->size () << std::endl;
          exit (EXIT_FAILURE);
        }

        const PointNormal& pt_m = cloud_model_selected->operator [] (index[0]);

        // Check the normals threshold
        if (pt_m.getNormalVector4fMap ().dot (pt_d.getNormalVector4fMap ()) > normals_threshold_)
        {
          squared_distance_sum += squared_distance[0];

          cloud_model_corr->push_back (pt_m);
          cloud_data_corr->push_back (pt_d);
        }
      }
    }

    // Shrink to fit ("Scott Meyers swap trick")
    CloudNormal (*cloud_model_corr).swap (*cloud_model_corr);
    CloudNormal (*cloud_data_corr).swap (*cloud_data_corr);

    const size_t n_corr = cloud_data_corr->size ();
    if (n_corr < n_min)
    {
      std::cerr << "ERROR in icp.cpp: Not enough correspondences: " << n_corr << " < " << n_min << std::endl;
      return (false);
    }

    // NOTE: The fitness is calculated with the transformation from the previous iteration (I don't re-calculate it after the transformation estimation). This means that the actual fitness will be one iteration "better" than the calculated fitness suggests. This should be no problem because the difference is small at the state of convergence.
    previous_fitness           = current_fitness;
    current_fitness            = squared_distance_sum / static_cast <float> (n_corr);
    delta_fitness              = std::abs (previous_fitness - current_fitness);
    squared_distance_threshold = squared_distance_threshold_factor_ * current_fitness;
    overlap                    = static_cast <float> (n_corr) / static_cast <float> (n_data);

    //    std::cerr << "Iter: " << std::left << std::setw(3) << iter
    //              << " | Overlap: " << std::setprecision(2) << std::setw(4) << overlap
    //              << " | current fitness: " << std::setprecision(5) << std::setw(10) << current_fitness
    //              << " | delta fitness: " << std::setprecision(5) << std::setw(10) << delta_fitness << std::endl;

    // Minimize the point to plane distance
    Transformation T_delta = Transformation::Identity ();
    if (!this->minimizePointPlane (cloud_data_corr, cloud_model_corr, T_delta))
    {
      std::cerr << "ERROR in icp.cpp: minimizePointPlane failed!\n";
      return (false);
    }

    T_cur = T_delta * T_cur;

    // Convergence
    if (delta_fitness < epsilon_) break;
    ++iter;
    if (iter > max_iterations_)   break;

  } // End ICP main loop

  // Some output
  std::cerr << "\nRegistration:\n"
            << "  - delta fitness / epsilon    : " << delta_fitness   << " / " << epsilon_
            << (delta_fitness   < epsilon_        ? " <-- :-)\n" : "\n")
            << "  - fitness       / max fitness: " << current_fitness << " / " << max_fitness_
            << (current_fitness > max_fitness_    ? " <-- :-(\n" : "\n")
            << "  - iter          / max iter   : " << iter            << " / " << max_iterations_
            << (iter            > max_iterations_ ? " <-- :-(\n" : "\n")
            << "  - overlap       / min overlap: " << overlap         << " / " << min_overlap_
            << (overlap         < min_overlap_    ? " <-- :-(\n" : "\n\n");

  if (iter > max_iterations_ || overlap <  min_overlap_ || current_fitness > max_fitness_)
  {
    return (false);
  }
  else if (delta_fitness <=epsilon_)
  {
    T_final = T_cur;
    return (true);
  }
  else
  {
    std::cerr << "ERROR in icp.cpp: Congratulations! you found a bug.\n";
    exit (EXIT_FAILURE);
  }
}

////////////////////////////////////////////////////////////////////////////////

pcl::ihs::ICP::CloudNormalConstPtr
pcl::ihs::ICP::selectModelPoints (const CloudModelConstPtr& cloud_model,
                                  const Transformation&     T_init_inv) const
{
  const CloudNormalPtr cloud_model_out (new CloudNormal ());
  cloud_model_out->reserve (cloud_model->size ());

  CloudModel::const_iterator it_in = cloud_model->begin ();
  for (; it_in!=cloud_model->end (); ++it_in)
  {
    // Don't consider points that are facing away from the cameara.
    if ((T_init_inv * it_in->getNormalVector4fMap ()).z () < 0.f)
    {
      PointNormal pt;
      pt.getVector4fMap ()       = it_in->getVector4fMap ();
      pt.getNormalVector4fMap () = it_in->getNormalVector4fMap ();

      // NOTE: Not the transformed points!!
      cloud_model_out->push_back (pt);
    }
  }

  // Shrink to fit ("Scott Meyers swap trick")
  CloudNormal (*cloud_model_out).swap (*cloud_model_out);

  return (cloud_model_out);
}

////////////////////////////////////////////////////////////////////////////////

pcl::ihs::ICP::CloudNormalConstPtr
pcl::ihs::ICP::selectDataPoints (const CloudProcessedConstPtr& cloud_data) const
{
  const CloudNormalPtr cloud_data_out (new CloudNormal ());
  cloud_data_out->reserve (cloud_data->size ());

  CloudProcessed::const_iterator it_in = cloud_data->begin ();
  for (; it_in!=cloud_data->end (); ++it_in)
  {
    if (pcl::isFinite (*it_in))
    {
      PointNormal pt;
      pt.getVector4fMap ()       = it_in->getVector4fMap ();
      pt.getNormalVector4fMap () = it_in->getNormalVector4fMap ();

      cloud_data_out->push_back (pt);
    }
  }

  // Shrink to fit ("Scott Meyers swap trick")
  CloudNormal (*cloud_data_out).swap (*cloud_data_out);

  return (cloud_data_out);
}

////////////////////////////////////////////////////////////////////////////////

bool
pcl::ihs::ICP::minimizePointPlane (const CloudNormalConstPtr& cloud_source,
                                   const CloudNormalConstPtr& cloud_target,
                                   Transformation&            T) const
{
  // Check the input
  // n < n_min already checked in the icp main loop
  const size_t n = cloud_source->size ();
  if (cloud_target->size () != n)
  {
    std::cerr << "ERROR in icp.cpp: Input must have the same size!\n";
    return (false);
  }

  // For numerical stability
  // - Low: Linear Least-Squares Optimization for Point-to-Plane ICP Surface Registration (2004), in the discussion: "To improve the numerical stability of the computation, it is important to use a unit of distance that is comparable in magnitude with the rotation angles. The simplest way is to rescale and move the two input surfaces so that they are bounded within a unit sphere or cube centered at the origin."
  // - Gelfand et al.: Geometrically Stable Sampling for the ICP Algorithm (2003), in sec 3.1: "As is common with PCA methods, we will shift the center of mass of the points	to the origin." ... "Therefore, af- ter shifting the center of mass, we will scale the point set so that the average distance of points	from the origin is 1."
  // - Hartley, Zisserman: - Multiple View Geometry (2004), page 109: They normalize to sqrt(2)
  // TODO: Check the resulting C matrix for the conditioning.

  // Subtract the centroid and calculate the scaling factor
  Eigen::Vector4f c_s (0.f, 0.f, 0.f, 1.f);
  Eigen::Vector4f c_t (0.f, 0.f, 0.f, 1.f);
  pcl::compute3DCentroid (*cloud_source, c_s); c_s.w () = 1.f;
  pcl::compute3DCentroid (*cloud_target, c_t); c_t.w () = 1.f;

  // The normals are only needed for the target
  typedef std::vector <Eigen::Vector4f, Eigen::aligned_allocator <Eigen::Vector4f> > Vec4Xf;
  Vec4Xf xyz_s, xyz_t, nor_t;
  xyz_s.reserve (n);
  xyz_t.reserve (n);
  nor_t.reserve (n);

  CloudNormal::const_iterator it_s = cloud_source->begin ();
  CloudNormal::const_iterator it_t = cloud_target->begin ();

  float accum = 0.f;
  for (; it_s!=cloud_source->end (); ++it_s, ++it_t)
  {
    // Subtract the centroid
    const Eigen::Vector4f pt_s = it_s->getVector4fMap () - c_s;
    const Eigen::Vector4f pt_t = it_t->getVector4fMap () - c_t;

    xyz_s.push_back (pt_s);
    xyz_t.push_back (pt_t);
    nor_t.push_back (it_t->getNormalVector4fMap ());

    // Calculate the radius (L2 norm) of the bounding sphere through both shapes and accumulate the average
    // TODO: Change to squared norm and adapt the rest accordingly
    accum += pt_s.head <3> ().norm () + pt_t.head <3> ().norm ();
  }

  // Inverse factor (do a multiplication instead of division later)
  const float factor         = 2.f * static_cast <float> (n) / accum;
  const float factor_squared = factor*factor;

  // Covariance matrix C
  Eigen::Matrix <float, 6, 6> C;

  // Right hand side vector b
  Eigen::Matrix <float, 6, 1> b;

  // For Eigen vectorization: use 4x4 submatrixes instead of 3x3 submatrixes
  // -> top left 3x3 matrix will form the final C
  // Same for b
  Eigen::Matrix4f C_tl    = Eigen::Matrix4f::Zero(); // top left corner
  Eigen::Matrix4f C_tr_bl = Eigen::Matrix4f::Zero(); // top right / bottom left
  Eigen::Matrix4f C_br    = Eigen::Matrix4f::Zero(); // bottom right

  Eigen::Vector4f b_t     = Eigen::Vector4f::Zero(); // top
  Eigen::Vector4f b_b     = Eigen::Vector4f::Zero(); // bottom

  Vec4Xf::const_iterator it_xyz_s = xyz_s.begin ();
  Vec4Xf::const_iterator it_xyz_t = xyz_t.begin ();
  Vec4Xf::const_iterator it_nor_t = nor_t.begin ();

  for (; it_xyz_s!=xyz_s.end (); ++it_xyz_s, ++it_xyz_t, ++it_nor_t)
  {
    const Eigen::Vector4f cross = it_xyz_s->cross3 (*it_nor_t);

    C_tl           += cross     * cross.    transpose ();
    C_tr_bl        += cross     * it_nor_t->transpose ();
    C_br           += *it_nor_t * it_nor_t->transpose ();

    const float dot = (*it_xyz_t-*it_xyz_s).dot (*it_nor_t);

    b_t            += cross     * dot;
    b_b            += *it_nor_t * dot;
  }

  // Scale with the factor and copy the 3x3 submatrixes into C and b
  C_tl    *= factor_squared;
  C_tr_bl *= factor;

  C << C_tl.  topLeftCorner <3, 3> ()            , C_tr_bl.topLeftCorner <3, 3> (),
      C_tr_bl.topLeftCorner <3, 3> ().transpose(), C_br.   topLeftCorner <3, 3> ();

  b << b_t.head <3> () * factor_squared,
      b_b. head <3> () * factor;

  // Solve C * x = b with a Cholesky factorization with pivoting
  // x = [alpha; beta; gamma; trans_x; trans_y; trans_z]
  Eigen::Matrix <float, 6, 1> x = C.selfadjointView <Eigen::Lower> ().ldlt ().solve (b);

  // The calculated transformation in the scaled coordinate system
  const float
      sa = std::sin (x (0)),
      ca = std::cos (x (0)),
      sb = std::sin (x (1)),
      cb = std::cos (x (1)),
      sg = std::sin (x (2)),
      cg = std::cos (x (2)),
      tx = x (3),
      ty = x (4),
      tz = x (5);

  Eigen::Matrix4f TT;
  TT << cg*cb, -sg*ca+cg*sb*sa,  sg*sa+cg*sb*ca, tx,
      sg*cb  ,  cg*ca+sg*sb*sa, -cg*sa+sg*sb*ca, ty,
      -sb    ,  cb*sa         ,  cb*ca         , tz,
      0.f    ,  0.f           ,  0.f           , 1.f;

  // Transformation matrixes into the local coordinate systems of model/data
  Eigen::Matrix4f T_s, T_t;

  T_s << factor, 0.f   , 0.f   , -c_s.x () * factor,
      0.f      , factor, 0.f   , -c_s.y () * factor,
      0.f      , 0.f   , factor, -c_s.z () * factor,
      0.f      , 0.f   , 0.f   ,  1.f;

  T_t << factor, 0.f   , 0.f   , -c_t.x () * factor,
      0.f      , factor, 0.f   , -c_t.y () * factor,
      0.f      , 0.f   , factor, -c_t.z () * factor,
      0.f      , 0.f   , 0.f   ,  1.f;

  // Output transformation T
  T = T_t.inverse () * TT * T_s;

  return (true);
}

////////////////////////////////////////////////////////////////////////////////
