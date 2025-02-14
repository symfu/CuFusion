/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2011, Willow Garage, Inc.
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
 */

#include <pcl/gpu/kinfu_large_scale/tsdf_volume.h>
#include "internal.h"
#include <algorithm>
#include <Eigen/Core>
#include <pcl/common/time.h>

#include <iostream>

using namespace pcl;
using namespace pcl::gpu;
using namespace Eigen;
using pcl::device::device_cast;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

pcl::gpu::TsdfVolume::TsdfVolume(const Vector3i& resolution) : resolution_(resolution)
{
  int volume_x = resolution_(0);
  int volume_y = resolution_(1);
  int volume_z = resolution_(2);

  volume_.create (volume_y * volume_z, volume_x);

  const Vector3f default_volume_size = Vector3f::Constant (3.f); //meters
  const float    default_tranc_dist  = 0.03f; //meters

  setSize(default_volume_size);
  setTsdfTruncDist(default_tranc_dist);

  reset();
}

void
pcl::gpu::TsdfVolume::create_init_cu_volume(){
	printf("in create_init_cu_volume()\n");
	int volume_x = resolution_(0);
	int volume_y = resolution_(1);
	int volume_z = resolution_(2);

	volume2nd_.create(volume_y * volume_z, volume_x);
	flagVolume_.create(volume_y * volume_z, volume_x);
	vrayPrevVolume_.create(volume_y * volume_z, volume_x);
	surfNormPrev_.create(volume_y * volume_z, volume_x);
	//volumeUpper_.create(volume_y * volume_z, volume_x);

	device::initFlagVolume(flagVolume_);
	device::initVrayPrevVolume(vrayPrevVolume_);
	device::initVrayPrevVolume(volume2nd_);
	device::initVrayPrevVolume(surfNormPrev_);
	//device::initVolume(volumeUpper_);
}

void
pcl::gpu::TsdfVolume::create_init_s2s_volume(){
	int volume_x = resolution_(0);
	int volume_y = resolution_(1);
	int volume_z = resolution_(2);

	volume2nd_.create(volume_y * volume_z, volume_x);
	volume3rd_proxy_.create(volume_y * volume_z, volume_x);

	device::initVrayPrevVolume(volume2nd_);
	device::initVrayPrevVolume(volume3rd_proxy_);
	printf("device::initVrayPrevVolume(volume3rd_proxy_)\n");
}//create_init_s2s_volume

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
pcl::gpu::TsdfVolume::setSize(const Vector3f& size)
{  
  size_ = size;
  setTsdfTruncDist(tranc_dist_);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
pcl::gpu::TsdfVolume::setTsdfTruncDist (float distance)
{
  float cx = size_(0) / resolution_(0);
  float cy = size_(1) / resolution_(1);
  float cz = size_(2) / resolution_(2);

  //tranc_dist_ = std::max (distance, 1.111f * std::max (cx, std::max (cy, cz)));  
  tranc_dist_ = std::max (0.f, distance); //zc: 试试完全不设最小tdist 的话
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

pcl::gpu::DeviceArray2D<int> 
pcl::gpu::TsdfVolume::data() const
{
  return volume_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const Eigen::Vector3f&
pcl::gpu::TsdfVolume::getSize() const
{
    return size_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const Eigen::Vector3i&
pcl::gpu::TsdfVolume::getResolution() const
{
  return resolution_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const Eigen::Vector3f
pcl::gpu::TsdfVolume::getVoxelSize() const
{    
  return size_.array () / resolution_.array().cast<float>();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float
pcl::gpu::TsdfVolume::getTsdfTruncDist () const
{
  return tranc_dist_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void 
pcl::gpu::TsdfVolume::reset()
{
  device::initVolume(volume_);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
pcl::gpu::TsdfVolume::fetchCloudHost (PointCloud<PointXYZI>& cloud, bool connected26) const
{
  PointCloud<PointXYZ>::Ptr cloud_ptr_ = PointCloud<PointXYZ>::Ptr (new PointCloud<PointXYZ>);
  PointCloud<PointIntensity>::Ptr cloud_i_ptr_ = PointCloud<PointIntensity>::Ptr (new PointCloud<PointIntensity>);
  fetchCloudHost(*cloud_ptr_);
  pcl::concatenateFields (*cloud_ptr_, *cloud_i_ptr_, cloud);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
pcl::gpu::TsdfVolume::fetchCloudHost (PointCloud<PointType>& cloud, bool connected26) const
{
  int volume_x = resolution_(0);
  int volume_y = resolution_(1);
  int volume_z = resolution_(2);

  int cols;
  std::vector<int> volume_host;
  volume_.download (volume_host, cols);

  cloud.points.clear ();
  cloud.points.reserve (10000);

  const int DIVISOR = device::DIVISOR; // SHRT_MAX;

#define FETCH(x, y, z) volume_host[(x) + (y) * volume_x + (z) * volume_y * volume_x]

  Array3f cell_size = getVoxelSize();

  for (int x = 1; x < volume_x-1; ++x)
  {
    for (int y = 1; y < volume_y-1; ++y)
    {
      for (int z = 0; z < volume_z-1; ++z)
      {
        int tmp = FETCH (x, y, z);
        int W = reinterpret_cast<short2*>(&tmp)->y;
        int F = reinterpret_cast<short2*>(&tmp)->x;

        if (W == 0 || F == DIVISOR)
          continue;

        Vector3f V = ((Array3f(x, y, z) + 0.5f) * cell_size).matrix ();

        if (connected26)
        {
          int dz = 1;
          for (int dy = -1; dy < 2; ++dy)
            for (int dx = -1; dx < 2; ++dx)
            {
              int tmp = FETCH (x+dx, y+dy, z+dz);

              int Wn = reinterpret_cast<short2*>(&tmp)->y;
              int Fn = reinterpret_cast<short2*>(&tmp)->x;
              if (Wn == 0 || Fn == DIVISOR)
                continue;

              if ((F > 0 && Fn < 0) || (F < 0 && Fn > 0))
              {
                Vector3f Vn = ((Array3f (x+dx, y+dy, z+dz) + 0.5f) * cell_size).matrix ();
                Vector3f point = (V * abs (Fn) + Vn * abs (F)) / (abs (F) + abs (Fn));

                pcl::PointXYZ xyz;
                xyz.x = point (0);
                xyz.y = point (1);
                xyz.z = point (2);

                cloud.points.push_back (xyz);
              }
            }
          dz = 0;
          for (int dy = 0; dy < 2; ++dy)
            for (int dx = -1; dx < dy * 2; ++dx)
            {
              int tmp = FETCH (x+dx, y+dy, z+dz);

              int Wn = reinterpret_cast<short2*>(&tmp)->y;
              int Fn = reinterpret_cast<short2*>(&tmp)->x;
              if (Wn == 0 || Fn == DIVISOR)
                continue;

              if ((F > 0 && Fn < 0) || (F < 0 && Fn > 0))
              {
                Vector3f Vn = ((Array3f (x+dx, y+dy, z+dz) + 0.5f) * cell_size).matrix ();
                Vector3f point = (V * abs(Fn) + Vn * abs(F))/(abs(F) + abs (Fn));

                pcl::PointXYZ xyz;
                xyz.x = point (0);
                xyz.y = point (1);
                xyz.z = point (2);

                cloud.points.push_back (xyz);
              }
            }
        }
        else /* if (connected26) */
        {
          for (int i = 0; i < 3; ++i)
          {
            int ds[] = {0, 0, 0};
            ds[i] = 1;

            int dx = ds[0];
            int dy = ds[1];
            int dz = ds[2];

            int tmp = FETCH (x+dx, y+dy, z+dz);

            int Wn = reinterpret_cast<short2*>(&tmp)->y;
            int Fn = reinterpret_cast<short2*>(&tmp)->x;
            if (Wn == 0 || Fn == DIVISOR)
              continue;

            if ((F > 0 && Fn < 0) || (F < 0 && Fn > 0))
            {
              Vector3f Vn = ((Array3f (x+dx, y+dy, z+dz) + 0.5f) * cell_size).matrix ();
              Vector3f point = (V * abs (Fn) + Vn * abs (F)) / (abs (F) + abs (Fn));

              pcl::PointXYZ xyz;
              xyz.x = point (0);
              xyz.y = point (1);
              xyz.z = point (2);

              cloud.points.push_back (xyz);
            }
          }
        } /* if (connected26) */
      }
    }
  }
#undef FETCH
  cloud.width  = (int)cloud.points.size ();
  cloud.height = 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
pcl::gpu::DeviceArray<pcl::gpu::TsdfVolume::PointType>
pcl::gpu::TsdfVolume::fetchCloud (DeviceArray<PointType>& cloud_buffer, const pcl::gpu::tsdf_buffer* buffer) const
{
  if (cloud_buffer.empty ())
    cloud_buffer.create (DEFAULT_CLOUD_BUFFER_SIZE);

  float3 device_volume_size = device_cast<const float3> (size_);
  size_t size = device::extractCloud (volume_, buffer, device_volume_size, cloud_buffer);
  return (DeviceArray<PointType> (cloud_buffer.ptr (), size));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
pcl::gpu::TsdfVolume::fetchNormals (const DeviceArray<PointType>& cloud, DeviceArray<PointType>& normals) const
{
  normals.create (cloud.size ());
  const float3 device_volume_size = device_cast<const float3> (size_);
  device::extractNormals (volume_, device_volume_size, cloud, (device::PointType*)normals.ptr ());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
pcl::gpu::TsdfVolume::fetchNormalsInSpace (const DeviceArray<PointType>& cloud, const pcl::gpu::tsdf_buffer* buffer) const
{
  const float3 device_volume_size = device_cast<const float3> (size_);
  device::extractNormalsInSpace (volume_, buffer, device_volume_size, cloud);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void 
pcl::gpu::TsdfVolume::pushSlice (PointCloud<PointXYZI>::Ptr existing_data_cloud, const pcl::gpu::tsdf_buffer* buffer) const
{
  size_t gpu_array_size = existing_data_cloud->points.size ();

  if(gpu_array_size == 0)
  {
    //std::cout << "[KinfuTracker](pushSlice) Existing data cloud has no points\n";//CREATE AS PCL MESSAGE
    return;
  }

  const pcl::PointXYZI *first_point_ptr = &(existing_data_cloud->points[0]);

  pcl::gpu::DeviceArray<pcl::PointXYZI> cloud_gpu;
  cloud_gpu.upload (first_point_ptr, gpu_array_size);

  DeviceArray<float4>& cloud_cast = (DeviceArray<float4>&) cloud_gpu;
  //volume().pushCloudAsSlice (cloud_cast, &buffer_);
  pcl::device::pushCloudAsSliceGPU (volume_, cloud_cast, buffer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

size_t
pcl::gpu::TsdfVolume::fetchSliceAsCloud (DeviceArray<PointType>& cloud_buffer_xyz, DeviceArray<float>& cloud_buffer_intensity, const pcl::gpu::tsdf_buffer* buffer, int shiftX, int shiftY, int shiftZ ) const
{
  if (cloud_buffer_xyz.empty ())
    cloud_buffer_xyz.create (DEFAULT_CLOUD_BUFFER_SIZE);

  if (cloud_buffer_intensity.empty ()) {
    cloud_buffer_intensity.create (DEFAULT_CLOUD_BUFFER_SIZE);  
  }

  float3 device_volume_size = device_cast<const float3> (size_);

  {
	  int newX = buffer->origin_GRID.x + shiftX;
	  int newY = buffer->origin_GRID.y + shiftY;
	  int newZ = buffer->origin_GRID.z + shiftZ;

	  int minBounds[ 3 ], maxBounds[ 3 ];

	  //X
	  if (newX >= 0)
	  {
		minBounds[ 0 ] = buffer->origin_GRID.x;
		maxBounds[ 0 ] = newX;    
	  }
	  else
	  {
		minBounds[ 0 ] = newX + buffer->voxels_size.x - 1;
		maxBounds[ 0 ] = buffer->origin_GRID.x + buffer->voxels_size.x - 1;
	  }
  
	  if (minBounds[ 0 ] > maxBounds[ 0 ])
		std::swap (minBounds[ 0 ], maxBounds[ 0 ]);

	  //Y
	  if (newY >= 0)
	  {
		 minBounds[ 1 ] = buffer->origin_GRID.y;
		 maxBounds[ 1 ] = newY;
	  }
	  else
	  {
		minBounds[ 1 ] = newY + buffer->voxels_size.y - 1;
		maxBounds[ 1 ] = buffer->origin_GRID.y + buffer->voxels_size.y - 1;
	  }
  
	  if(minBounds[ 1 ] > maxBounds[ 1 ])
		std::swap (minBounds[ 1 ], maxBounds[ 1 ]);

	  //Z
	  if (newZ >= 0)
	  {
	   minBounds[ 2 ] = buffer->origin_GRID.z;
	   maxBounds[ 2 ] = newZ;
	  }
	  else
	  {
		minBounds[ 2 ] = newZ + buffer->voxels_size.z - 1;
		maxBounds[ 2 ] = buffer->origin_GRID.z + buffer->voxels_size.z - 1;
	  }

	  if (minBounds[ 2 ] > maxBounds[ 2 ])
		std::swap(minBounds[ 2 ], maxBounds[ 2 ]);

	  minBounds[ 0 ] -= buffer->origin_GRID.x;
	  maxBounds[ 0 ] -= buffer->origin_GRID.x;

	  minBounds[ 1 ] -= buffer->origin_GRID.y;
	  maxBounds[ 1 ] -= buffer->origin_GRID.y;

	  minBounds[ 2 ] -= buffer->origin_GRID.z;
	  maxBounds[ 2 ] -= buffer->origin_GRID.z;

	  if (minBounds[ 0 ] < 0) // We are shifting Left
	  {
		minBounds[ 0 ] += ( buffer->voxels_size.x - 1 );
		maxBounds[ 0 ] += ( buffer->voxels_size.x - 1 );
	  }

	  if (minBounds[ 1 ] < 0) // We are shifting up
	  {
		minBounds[ 1 ] += ( buffer->voxels_size.y - 1 );
		maxBounds[ 1 ] += ( buffer->voxels_size.y - 1 );
	  }

	  if (minBounds[ 2 ] < 0) // We are shifting back
	  {
		minBounds[ 2 ] += ( buffer->voxels_size.z - 1 );
		maxBounds[ 2 ] += ( buffer->voxels_size.z - 1 );
	  }
	  PCL_DEBUG( "In fetchSliceAsCloud:\n" );
	  PCL_DEBUG("Origin : %d, %d %d\n", buffer->origin_GRID.x, buffer->origin_GRID.y, buffer->origin_GRID.z );
	  PCL_DEBUG("Origin global : %f, %f %f\n", buffer->origin_GRID_global.x, buffer->origin_GRID_global.y, buffer->origin_GRID_global.z );
	  PCL_DEBUG( "Offset : %d, %d, %d\n", shiftX, shiftY, shiftZ );
      PCL_DEBUG ("X bound: [%d - %d]\n", minBounds[ 0 ], maxBounds[ 0 ]);
      PCL_DEBUG ("Y bound: [%d - %d]\n", minBounds[ 1 ], maxBounds[ 1 ]);
      PCL_DEBUG ("Z bound: [%d - %d]\n", minBounds[ 2 ], maxBounds[ 2 ]);

  }
  
  size_t size = pcl::device::extractSliceAsCloud (volume_, device_volume_size, buffer, shiftX, shiftY, shiftZ, cloud_buffer_xyz, cloud_buffer_intensity);
  
  std::cout << "SIZE IS " << size << " (maximum: " << DEFAULT_CLOUD_BUFFER_SIZE <<")" << std::endl;
  
  return (size);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
pcl::gpu::TsdfVolume::fetchNormals (const DeviceArray<PointType>& cloud, DeviceArray<NormalType>& normals) const
{
  normals.create (cloud.size ());
  const float3 device_volume_size = device_cast<const float3> (size_);
  device::extractNormals (volume_, device_volume_size, cloud, (device::float8*)normals.ptr ());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
pcl::gpu::TsdfVolume::downloadTsdf (std::vector<float>& tsdf) const
{
  tsdf.resize (volume_.cols() * volume_.rows());
  volume_.download(&tsdf[0], volume_.cols() * sizeof(int));

#pragma omp parallel for
  for(int i = 0; i < (int) tsdf.size(); ++i)
  {
    float tmp = reinterpret_cast<short2*>(&tsdf[i])->x;
    tsdf[i] = tmp/device::DIVISOR;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void
pcl::gpu::TsdfVolume::downloadTsdfAndWeighs (std::vector<float>& tsdf, std::vector<short>& weights) const
{
  int volumeSize = volume_.cols() * volume_.rows();
  tsdf.resize (volumeSize);
  weights.resize (volumeSize);
  {
    std::cout << std::endl;
    ScopeTime t("CUDA download");
    volume_.download(&tsdf[0], volume_.cols() * sizeof(int));
  }
 // {
	//std::cout << std::endl;
	//ScopeTime t("CUDA upload");
	//volume_.upload (&tsdf[0], volume_.cols() * sizeof(int), volume_.rows(), volume_.cols() );
 //   //volume_.download(&tsdf[0], volume_.cols() * sizeof(int));
 // }

#pragma omp parallel for
  for(int i = 0; i < (int) tsdf.size(); ++i)
  {
    short2 elem = *reinterpret_cast<short2*>(&tsdf[i]);
    tsdf[i] = (float)(elem.x)/device::DIVISOR;    
    weights[i] = (short)(elem.y);    
  }
}
