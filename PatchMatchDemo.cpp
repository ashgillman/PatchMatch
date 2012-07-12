/*=========================================================================
 *
 *  Copyright David Doria 2012 daviddoria@gmail.com
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/

/** This program computes the NN field of an image. */

#include <iostream>

#include "itkImage.h"
#include "itkImageFileReader.h"
#include "itkImageFileWriter.h"
#include "itkCovariantVector.h"

#include "Mask/Mask.h"
#include "Mask/ITKHelpers/ITKHelpers.h"

#include "PatchMatch.h"

typedef itk::Image<itk::CovariantVector<float, 3>, 2> ImageType;

int main(int argc, char*argv[])
{
  if(argc < 4)
  {
    std::cerr << "Required arguments: image mask output" << std::endl;
    return EXIT_FAILURE;
  }

  std::string imageFilename = argv[1];
  std::string maskFilename = argv[2];
  std::string outputFilename = argv[3];

  std::cout << "imageFilename: " << imageFilename << std::endl;
  std::cout << "maskFilename: " << maskFilename << std::endl;
  std::cout << "outputFilename: " << outputFilename << std::endl;

  typedef itk::ImageFileReader<ImageType> ImageReaderType;
  ImageReaderType::Pointer imageReader = ImageReaderType::New();
  imageReader->SetFileName(imageFilename);
  imageReader->Update();

  Mask::Pointer sourceMask = Mask::New();
  sourceMask->Read(maskFilename);

  Mask::Pointer targetMask = Mask::New();
  targetMask->SetRegions(sourceMask->GetLargestPossibleRegion());
  targetMask->Allocate();
  ITKHelpers::SetImageToConstant(targetMask.GetPointer(), targetMask->GetValidValue());

  PatchMatch patchMatch;
  patchMatch.SetImage(imageReader->GetOutput());
  patchMatch.SetPatchRadius(3);
  
  patchMatch.SetDistanceType(PatchMatch::PIXELWISE);

//   patchMatch.SetDistanceType(PatchMatch::PCA);
//   patchMatch.ComputeProjectionMatrix();

  patchMatch.SetTargetMask(targetMask);
  patchMatch.SetSourceMask(sourceMask);
  patchMatch.SetIterations(10);

  patchMatch.Compute(NULL);

  typedef itk::VectorImage<float, 2> VectorImageType;
  VectorImageType::Pointer output = VectorImageType::New();

  patchMatch.GetPatchCentersImage(patchMatch.GetOutput(), output.GetPointer());

  typedef itk::ImageFileWriter<VectorImageType> WriterType;
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(outputFilename);
  writer->SetInput(output);
  writer->Update();

  return EXIT_SUCCESS;
}
