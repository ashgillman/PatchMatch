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

#include "PatchMatchHelpers.h"


namespace PatchMatchHelpers
{

itk::Offset<2> RandomNeighborNonZeroOffset()
{
  int randomOffsetX = 0;
  int randomOffsetY = 0;
  while((randomOffsetX == 0) && (randomOffsetY == 0) ) // We don't want the random offset to be (0,0), because the histogram difference would be zero!
  {
    // Generate random numbers in the set (-1,0,1) by generating a number in (0,1,2) and then subtracting 1
    randomOffsetX = rand()%3 - 1;
    randomOffsetY = rand()%3 - 1;
  }

  itk::Offset<2> randomNeighborNonZeroOffset = {{randomOffsetX, randomOffsetY}};

  return randomNeighborNonZeroOffset;
}


void WriteValidPixels(const NNFieldType* const nnField, const std::string& fileName)
{
  // This function writes the validity of the first Match in the MatchSet at every pixel
  // to an image.

  typedef itk::Image<unsigned char> ImageType;
  ImageType::Pointer image = ImageType::New();
  image->SetRegions(nnField->GetLargestPossibleRegion());
  image->Allocate();
  image->FillBuffer(0);

  itk::ImageRegionConstIterator<NNFieldType> imageIterator(nnField,
                                                           nnField->GetLargestPossibleRegion());

  while(!imageIterator.IsAtEnd())
  {
    if(imageIterator.Get().GetMatch(0).IsValid())
    {
      image->SetPixel(imageIterator.GetIndex(), 255);
    }

    ++imageIterator;
  }

  ITKHelpers::WriteImage(image.GetPointer(), fileName);
}

void WriteConsistentRegions(const NNFieldType* const nnField, const Mask* const regionMask, const std::string& fileName)
{
  Mask::Pointer usedMask = Mask::New();
  usedMask->DeepCopyFrom(regionMask);

//   std::vector<itk::Index<2> > unusedPixels;
//   do
//   {
//     unusedPixels = usedMask->GetValidPixels();
//
//     itk::ImageRegionConstIterator<NNFieldType> imageIterator(nnField,
//                                                              nnField->GetLargestPossibleRegion());
//
//     while(!imageIterator.IsAtEnd())
//     {
//       if(imageIterator.Get().GetMatch(0).IsValid())
//       {
//         image->SetPixel(imageIterator.GetIndex(), 255);
//       }
//
//       ++imageIterator;
//     }
//   } while (unusedPixels.size() > 0);

}

} // namespace PatchMatchHelpers