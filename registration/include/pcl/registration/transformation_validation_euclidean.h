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
 * $Id: transformation_validation_euclidean.h 6648 2012-07-31 04:37:16Z rusu $
 *
 */
#ifndef PCL_REGISTRATION_TRANSFORMATION_VALIDATION_EUCLIDEAN_H_
#define PCL_REGISTRATION_TRANSFORMATION_VALIDATION_EUCLIDEAN_H_

#include <pcl/point_representation.h>
#include <pcl/kdtree/kdtree.h>
#include <pcl/kdtree/kdtree_flann.h>
#include <pcl/registration/transformation_validation.h>

namespace pcl
{
  namespace registration
  {
    /** \brief TransformationValidationEuclidean computes an L2SQR norm between a source and target
      * dataset.
      * 
      * To prevent points with bad correspondences to contribute to the overall score, the class also 
      * accepts a maximum_range parameter given via \ref setMaxRange that is used as a cutoff value for
      * nearest neighbor distance comparisons.
      * 
      * The output score is normalized with respect to the number of valid correspondences found.
      *
      * Usage example:
      * \code
      * pcl::TransformationValidationEuclidean<pcl::PointXYZ, pcl::PointXYZ> tve;
      * tve.setMaxRange (0.01);  // 1cm
      * double score = tve.validateTransformation (source, target, transformation);
      * \endcode
      *
      * \note The class is templated on the source and target point types as well as on the output scalar of the transformation matrix (i.e., float or double). Default: float.
      * \author Radu B. Rusu
      * \ingroup registration
      */
    template <typename PointSource, typename PointTarget, typename Scalar = float>
    class TransformationValidationEuclidean
    {
      public:
        typedef typename TransformationValidation<PointSource, PointTarget, Scalar>::Matrix4 Matrix4;
        
        typedef boost::shared_ptr<TransformationValidation<PointSource, PointTarget, Scalar> > Ptr;
        typedef boost::shared_ptr<const TransformationValidation<PointSource, PointTarget, Scalar> > ConstPtr;

        typedef typename pcl::KdTree<PointTarget> KdTree;
        typedef typename pcl::KdTree<PointTarget>::Ptr KdTreePtr;

        typedef typename KdTree::PointRepresentationConstPtr PointRepresentationConstPtr;

        typedef typename TransformationValidation<PointSource, PointTarget>::PointCloudSourceConstPtr PointCloudSourceConstPtr;
        typedef typename TransformationValidation<PointSource, PointTarget>::PointCloudTargetConstPtr PointCloudTargetConstPtr;

        /** \brief Constructor.
          * Sets the \a max_range parameter to double::max, \a threshold_ to NaN
          * and initializes the internal search \a tree to a FLANN kd-tree.
          */
        TransformationValidationEuclidean () : 
          max_range_ (std::numeric_limits<double>::max ()),
          threshold_ (std::numeric_limits<double>::quiet_NaN ()),
          tree_ (new pcl::KdTreeFLANN<PointTarget>)
        {
        }

        virtual ~TransformationValidationEuclidean () {};

        /** \brief Set the maximum allowable distance between a point and its correspondence in the 
          * target in order for a correspondence to be considered \a valid. Default: double::max.
          * \param[in] max_range the new maximum allowable distance
          */
        inline void
        setMaxRange (double max_range)
        {
          max_range_ = max_range;
        }

        /** \brief Get the maximum allowable distance between a point and its 
          * correspondence, as set by the user.
          */
        inline double
        getMaxRange ()
        {
          return (max_range_);
        }

        /** \brief Set a threshold for which a specific transformation is considered valid.
          *
          * \note Since we're using MSE (Mean Squared Error) as a metric, the threshold
          * represents the mean Euclidean distance threshold over all nearest neighbors
          * up to max_range.
          *
          * \param[in] threshold the threshold for which a transformation is vali
          */
        inline void
        setThreshold (double threshold)
        {
          threshold_ = threshold;
        }

        /** \brief Get the threshold for which a specific transformation is valid. */
        inline double
        getThreshold ()
        {
          return (threshold_);
        }

        /** \brief Validate the given transformation with respect to the input cloud data, and return a score.
          *
          * \param[in] cloud_src the source point cloud dataset
          * \param[in] cloud_tgt the target point cloud dataset
          * \param[out] transformation_matrix the resultant transformation matrix
          *
          * \return the score or confidence measure for the given
          * transformation_matrix with respect to the input data
          */
        double
        validateTransformation (
            const PointCloudSourceConstPtr &cloud_src,
            const PointCloudTargetConstPtr &cloud_tgt,
            const Matrix4 &transformation_matrix) const;

        /** \brief Comparator function for deciding which score is better after running the 
          * validation on multiple transforms.
          *
          * \param[in] score1 the first value
          * \param[in] score2 the second value
          *
          * \return true if score1 is better than score2
          */
        virtual bool
        operator() (const double &score1, const double &score2) const
        {
          return (score1 < score2);
        }

        /** \brief Check if the score is valid for a specific transformation.
          *
          * \param[in] cloud_src the source point cloud dataset
          * \param[in] cloud_tgt the target point cloud dataset
          * \param[out] transformation_matrix the transformation matrix
          *
          * \return true if the transformation is valid, false otherwise.
          */
        virtual bool
        isValid (
            const PointCloudSourceConstPtr &cloud_src,
            const PointCloudTargetConstPtr &cloud_tgt,
            const Matrix4 &transformation_matrix) const
        {
          if (pcl_isnan (threshold_))
          {
            PCL_ERROR ("[pcl::TransformationValidationEuclidean::isValid] Threshold not set! Please use setThreshold () before continuing.");
            return (false);
          }

          return (validateTransformation (cloud_src, cloud_tgt, transformation_matrix) < threshold_);
        }

      protected:
        /** \brief The maximum allowable distance between a point and its correspondence in the target 
          * in order for a correspondence to be considered \a valid. Default: double::max.
          */
        double max_range_;

        /** \brief The threshold for which a specific transformation is valid. 
          * Set to NaN by default, as we must require the user to set it.
          */
        double threshold_;

        /** \brief A pointer to the spatial search object. */
        KdTreePtr tree_;

        /** \brief Internal point representation uses only 3D coordinates for L2 */
        class MyPointRepresentation: public pcl::PointRepresentation<PointTarget>
        {
          using pcl::PointRepresentation<PointTarget>::nr_dimensions_;
          using pcl::PointRepresentation<PointTarget>::trivial_;
          public:
            typedef boost::shared_ptr<MyPointRepresentation> Ptr;
            typedef boost::shared_ptr<const MyPointRepresentation> ConstPtr;
            
            MyPointRepresentation ()
            {
              nr_dimensions_ = 3;
              trivial_ = true;
            }

            virtual void
            copyToFloatArray (const PointTarget &p, float * out) const
            {
              out[0] = p.x;
              out[1] = p.y;
              out[2] = p.z;
            }
        };

      public:
        EIGEN_MAKE_ALIGNED_OPERATOR_NEW
    };
  }
}

#include <pcl/registration/impl/transformation_validation_euclidean.hpp>

#endif    // PCL_REGISTRATION_TRANSFORMATION_VALIDATION_EUCLIDEAN_H_

