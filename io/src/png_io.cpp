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
 * $Id$
 *
 */

#include <pcl/io/png_io.h>
#include <vtkImageImport.h>
#include <vtkPNGWriter.h>
#include <vtkSmartPointer.h>
#include <vtkImageFlip.h>

namespace 
{
  void flipAndWritePng(const std::string &file_name, vtkSmartPointer<vtkImageImport>& importer)
  {
    vtkSmartPointer<vtkImageFlip> flipYFilter = vtkSmartPointer<vtkImageFlip>::New();
    flipYFilter->SetFilteredAxis(1); // flip y axis
    flipYFilter->SetInputConnection(importer->GetOutputPort());
    flipYFilter->Update();

    vtkSmartPointer<vtkPNGWriter> writer = vtkSmartPointer<vtkPNGWriter>::New();
    writer->SetFileName(file_name.c_str());
    writer->SetInputConnection(flipYFilter->GetOutputPort());
    writer->Write();
  }
};




void 
pcl::io::saveCharPNGFile (const std::string &file_name, const unsigned char *char_data, int width, int height, int channels)
{
  vtkSmartPointer<vtkImageImport> importer = vtkSmartPointer<vtkImageImport>::New ();
  importer->SetNumberOfScalarComponents (channels);
  importer->SetDataScalarTypeToUnsignedChar ();
  importer->SetWholeExtent (0, width - 1, 0, height - 1, 0, 0);
  importer->SetDataExtentToWholeExtent ();

  void* data = const_cast<void*> (reinterpret_cast<const void*> (char_data));
  importer->SetImportVoidPointer (data, 1);
  importer->Update ();

  flipAndWritePng(file_name, importer);
}

void 
pcl::io::saveShortPNGFile (const std::string &file_name, const unsigned short *short_image, int width, int height, int channels)
{
  vtkSmartPointer<vtkImageImport> importer = vtkSmartPointer<vtkImageImport>::New ();
  importer->SetNumberOfScalarComponents (channels);
  importer->SetDataScalarTypeToUnsignedShort ();
  importer->SetWholeExtent (0, width - 1, 0, height - 1, 0, 0);
  importer->SetDataExtentToWholeExtent ();

  void* data = const_cast<void*> (reinterpret_cast<const void*> (short_image));
  importer->SetImportVoidPointer (data, 1);
  importer->Update ();

  flipAndWritePng(file_name, importer);
}
