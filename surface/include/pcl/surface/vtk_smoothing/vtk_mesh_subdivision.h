/*
 * Software License Agreement (BSD License)
 *
 *  Point Cloud Library (PCL) - www.pointclouds.org
 *  Copyright (c) 2011, Willow Garage, Inc.
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
 * $Id: vtk_mesh_subdivision.h 6459 2012-07-18 07:50:37Z dpb $
 *
 */

#ifndef VTK_MESH_SUBDIVISION_H_
#define VTK_MESH_SUBDIVISION_H_

#include <pcl/surface/processing.h>
#include <pcl/surface/vtk_smoothing/vtk.h>

namespace pcl
{
  /** \brief PCL mesh smoothing based on the vtkLinearSubdivisionFilter, vtkLoopSubdivisionFilter, vtkButterflySubdivisionFilter
    * depending on the selected MeshSubdivisionVTKFilterType algorithm from the VTK library.
    * Please check out the original documentation for more details on the inner workings of the algorithm
    * Warning: This wrapper does two fairly computationally expensive conversions from the PCL PolygonMesh
    * data structure to the vtkPolyData data structure and back.
    */
  class PCL_EXPORTS MeshSubdivisionVTK : public MeshProcessing
  {
    public:
      /** \brief Empty constructor */
      MeshSubdivisionVTK ();

      enum MeshSubdivisionVTKFilterType
      { LINEAR, LOOP, BUTTERFLY };

      /** \brief Set the mesh subdivision filter type
        * \param[in] type the filter type
        */
      inline void
      setFilterType (MeshSubdivisionVTKFilterType type)
      {
        filter_type_ = type;
      };

      /** \brief Get the mesh subdivision filter type */
      inline MeshSubdivisionVTKFilterType
      getFilterType ()
      {
        return filter_type_;
      };

    protected:
      void
      performProcessing (pcl::PolygonMesh &output);

    private:
      MeshSubdivisionVTKFilterType filter_type_;

      vtkSmartPointer<vtkPolyData> vtk_polygons_;
  };
}
#endif /* VTK_MESH_SUBDIVISION_H_ */
