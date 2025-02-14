/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2010, Willow Garage, Inc.
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
 * $Id: nn_classification_example.cpp 6459 2012-07-18 07:50:37Z dpb $
 *
 */

#include <pcl/point_types.h>
#include <pcl/io/pcd_io.h>
#include <pcl/apps/vfh_nn_classifier.h>

int 
main (int, char* argv[])
{
  // Load input file
  char* file_name = argv[1];
  sensor_msgs::PointCloud2 cloud_blob;
  pcl::io::loadPCDFile (file_name, cloud_blob);

  // Declare variable to hold result
  pcl::NNClassification<pcl::VFHSignature308>::ResultPtr result;
  // same as: pcl::VFHClassifierNN::ResultPtr result;

  // Do general classification using NNClassification or use the VHClassiierNN helper class
  if (false)
  {
    // Estimate your favorite feature
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud (new pcl::PointCloud<pcl::PointXYZ> ());
    pcl::fromROSMsg (cloud_blob, *cloud);
    /// NOTE: make sure to use same radius as for training data
    pcl::PointCloud<pcl::VFHSignature308>::Ptr feature = pcl::computeVFH<pcl::PointXYZ> (cloud, 0.03);

    // Nearest neighbors classification
    pcl::NNClassification<pcl::VFHSignature308> nn;
    //nn.setTrainingFeatures(cloud);
    //nn.setTrainingLabels(std::vector<std::string>(cloud->points.size(), "bla"));
    nn.loadTrainingFeatures (argv[2], argv[3]);
    result = nn.classify(feature->points[0], 300, 50);
  }
  else
  {
    pcl::VFHClassifierNN vfh_classifier;
    //vfh_classifier.loadTrainingData ("/home/marton/ros/pcl/trunk/apps/data/can.pcd", "can");
    //vfh_classifier.loadTrainingData ("/home/marton/ros/pcl/trunk/apps/data/salt.pcd", "salt");
    //vfh_classifier.loadTrainingData ("/home/marton/ros/pcl/trunk/apps/data/sugar.pcd", "sugar");
    //vfh_classifier.saveTrainingFeatures ("/tmp/vfhs.pcd", "/tmp/vfhs.labels");
    vfh_classifier.loadTrainingFeatures (argv[2], argv[3]);
    vfh_classifier.finalizeTraining ();
    result = vfh_classifier.classify(cloud_blob);
  }

  // Print results
  for (unsigned i = 0; i < result->first.size(); ++i)
    std::cerr << result->first.at (i) << ": " << result->second.at (i) << std::endl;

  return 0;
}
