/*
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2012, Willow Garage, Inc.
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
 *   * Neither the name of the copyright holder(s) nor the names of its
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
 * $Id: poisson_reconstruction.cpp 6877 2012-08-20 04:26:39Z aichim $
 *
 */

#include <sensor_msgs/PointCloud2.h>
#include <pcl/io/pcd_io.h>
#include <pcl/io/vtk_io.h>
#include <pcl/surface/poisson.h>
#include <pcl/console/print.h>
#include <pcl/console/parse.h>
#include <pcl/console/time.h>

using namespace pcl;
using namespace pcl::io;
using namespace pcl::console;

#include <fstream>
using namespace std;

int default_depth = 8;
int default_solver_divide = 8;
int default_iso_divide = 8;

void
printHelp (int, char **argv)
{
  print_error ("Syntax is: %s input.pcd output.vtk <options>\n", argv[0]);
  print_info ("  where options are:\n");
  print_info ("                     -depth X          = set the maximum depth of the tree that will be used for surface reconstruction (default: ");
  print_value ("%d", default_depth); print_info (")\n");
  print_info ("                     -solver_divide X  = set the the depth at which a block Gauss-Seidel solver is used to solve the Laplacian equation (default: ");
  print_value ("%d", default_solver_divide); print_info (")\n");
  print_info ("                     -iso_divide X     = Set the depth at which a block iso-surface extractor should be used to extract the iso-surface (default: ");
  print_value ("%d", default_iso_divide); print_info (")\n");
}

bool
loadCloud (const std::string &filename, sensor_msgs::PointCloud2 &cloud)
{
  TicToc tt;
  print_highlight ("Loading "); print_value ("%s ", filename.c_str ());

  tt.tic ();
  if (loadPCDFile (filename, cloud) < 0)
    return (false);
  print_info ("[done, "); print_value ("%g", tt.toc ()); print_info (" ms : "); print_value ("%d", cloud.width * cloud.height); print_info (" points]\n");
  print_info ("Available dimensions: "); print_value ("%s\n", pcl::getFieldsList (cloud).c_str ());

  return (true);
}

void
compute (const sensor_msgs::PointCloud2::ConstPtr &input, PolygonMesh &output,
         int depth, int solver_divide, int iso_divide)
{
  PointCloud<PointNormal>::Ptr xyz_cloud (new pcl::PointCloud<PointNormal> ());
  fromROSMsg (*input, *xyz_cloud);

	Poisson<PointNormal> poisson;
	poisson.setDepth (depth);
	poisson.setSolverDivide (solver_divide);
	poisson.setIsoDivide (iso_divide);
  poisson.setInputCloud (xyz_cloud);


  TicToc tt;
  tt.tic ();

  print_highlight ("Computing ");
  poisson.reconstruct (output);

  print_info ("[done, "); print_value ("%g", tt.toc ()); print_info (" ms]\n");
}

void
saveCloud (const std::string &filename, const PolygonMesh &output)
{
  TicToc tt;
  tt.tic ();

  print_highlight ("Saving "); print_value ("%s ", filename.c_str ());
  saveVTKFile (filename, output);

  print_info ("[done, "); print_value ("%g", tt.toc ()); print_info (" ms]\n");
}

/* ---[ */
int
main (int argc, char** argv)
{
  print_info ("Compute the surface reconstruction of a point cloud using the marching cubes algorithm (pcl::surface::MarchingCubesGreedy or pcl::surface::MarchingCubesGreedyDot. For more information, use: %s -h\n", argv[0]);

  if (argc < 3)
  {
    printHelp (argc, argv);
    return (-1);
  }

  // Parse the command line arguments for .pcd files
  std::vector<int> pcd_file_indices;
  pcd_file_indices = parse_file_extension_argument (argc, argv, ".pcd");
  if (pcd_file_indices.size () != 1)
  {
    print_error ("Need one input PCD file and one output PCD file to continue.\n");
    return (-1);
  }

  std::vector<int> vtk_file_indices = parse_file_extension_argument (argc, argv, ".vtk");
  if (vtk_file_indices.size () != 1)
  {
    print_error ("Need one output VTK file to continue.\n");
    return (-1);
  }

  // Command line parsing
  int depth = default_depth;
  parse_argument (argc, argv, "-depth", depth);
  print_info ("Using a depth of: "); print_value ("%d\n", depth);

  int solver_divide = default_solver_divide;
  parse_argument (argc, argv, "-solver_divide", solver_divide);
  print_info ("Setting solver_divide to: "); print_value ("%d\n", solver_divide);

  int iso_divide = default_iso_divide;
  parse_argument (argc, argv, "-iso_divide", iso_divide);
  print_info ("Setting iso_divide to: "); print_value ("%d\n", iso_divide);


  // Load the first file
  sensor_msgs::PointCloud2::Ptr cloud (new sensor_msgs::PointCloud2);
  if (!loadCloud (argv[pcd_file_indices[0]], *cloud))
    return (-1);

  // Apply the marching cubes algorithm
  PolygonMesh output;
  compute (cloud, output, depth, solver_divide, iso_divide);

  // Save into the second file
  saveCloud (argv[vtk_file_indices[0]], output);
}

