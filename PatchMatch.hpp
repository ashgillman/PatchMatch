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

#ifndef PATCHMATCH_HPP
#define PATCHMATCH_HPP

#include "PatchMatch.h"

// Submodules
#include "Mask/ITKHelpers/ITKHelpers.h"
#include "Mask/MaskOperations.h"
#include "PatchComparison/SSD.h"

// ITK
#include "itkImageRegionReverseIterator.h"

// STL
#include <ctime>

template <typename TImage>
PatchMatch<TImage>::PatchMatch() : PatchRadius(0), PatchDistanceFunctor(NULL)
{
  this->Output = PMImageType::New();
  this->Image = TImage::New();
  this->SourceMask = Mask::New();
  this->TargetMask = Mask::New();
}

template <typename TImage>
void PatchMatch<TImage>::Compute(PMImageType* const initialization)
{
  srand(time(NULL));

  if(initialization)
  {
    ITKHelpers::DeepCopy(initialization, this->Output.GetPointer());
  }
  else
  {
    //RandomInit();
    BoundaryInit();
  }

  {
  CoordinateImageType::Pointer initialOutput = CoordinateImageType::New();
  GetPatchCentersImage(this->Output, initialOutput);
  ITKHelpers::WriteImage(initialOutput.GetPointer(), "initialization.mha");
  }


  bool forwardSearch = true;

  for(unsigned int iteration = 0; iteration < this->Iterations; ++iteration)
  {
    std::cout << "PatchMatch iteration " << iteration << std::endl;

    if(forwardSearch)
    {
      ForwardPropagation();
    }
    else
    {
      BackwardPropagation();
    }

    // Switch the search direction for the next iteration
    forwardSearch = !forwardSearch;

    RandomSearch();

    // Write the intermediate output
    std::stringstream ss;
    ss << "PatchMatch_" << Helpers::ZeroPad(iteration, 2) << ".mha";
    CoordinateImageType::Pointer temp = CoordinateImageType::New();
    GetPatchCentersImage(this->Output, temp);
    ITKHelpers::WriteImage(temp.GetPointer(), ss.str());
  }

}

template <typename TImage>
void PatchMatch<TImage>::InitKnownRegion()
{
  // Create a zero region
  itk::Index<2> zeroIndex = {{0,0}};
  itk::Size<2> zeroSize = {{0,0}};
  itk::ImageRegion<2> zeroRegion(zeroIndex, zeroSize);
  Match zeroMatch;
  zeroMatch.Region = zeroRegion;
  zeroMatch.Score = 0.0f;

  ITKHelpers::SetImageToConstant(this->Output.GetPointer(), zeroMatch);

  itk::ImageRegion<2> internalRegion =
             ITKHelpers::GetInternalRegion(this->Image->GetLargestPossibleRegion(), this->PatchRadius);

  itk::ImageRegionIteratorWithIndex<PMImageType> outputIterator(this->Output, internalRegion);

  while(!outputIterator.IsAtEnd())
  {
    // Construct the current region
    itk::Index<2> currentIndex = outputIterator.GetIndex();
    itk::ImageRegion<2> currentRegion = ITKHelpers::GetRegionInRadiusAroundPixel(currentIndex, this->PatchRadius);

    if(this->SourceMask->IsValid(currentRegion))
    {
      Match randomMatch;
      randomMatch.Region = currentRegion;
      randomMatch.Score = 0;
      outputIterator.Set(randomMatch);
    }

    ++outputIterator;
  }

}

template <typename TImage>
void PatchMatch<TImage>::BoundaryInit()
{
  //InitKnownRegion();
  RandomInit();

  itk::ImageRegion<2> internalRegion =
             ITKHelpers::GetInternalRegion(this->Image->GetLargestPossibleRegion(), this->PatchRadius);

  // Expand the hole
  Mask::Pointer expandedMask = Mask::New();
  expandedMask->DeepCopyFrom(this->SourceMask);
  expandedMask->ExpandHole(this->PatchRadius);

  // Get the expanded boundary
  Mask::BoundaryImageType::Pointer boundaryImage = Mask::BoundaryImageType::New();
  unsigned char outputBoundaryPixelValue = 255;
  expandedMask->FindBoundary(boundaryImage.GetPointer(), Mask::VALID, outputBoundaryPixelValue);
  ITKHelpers::WriteImage(boundaryImage.GetPointer(), "ExpandedBoundary.png");

  // Get the boundary pixels
  std::vector<itk::Index<2> > boundaryIndices = ITKHelpers::GetPixelsWithValue(boundaryImage.GetPointer(), outputBoundaryPixelValue);

  std::vector<itk::ImageRegion<2> > validSourceRegions =
        MaskOperations::GetAllFullyValidRegions(this->SourceMask, internalRegion, this->PatchRadius);

  if(validSourceRegions.size() == 0)
  {
    throw std::runtime_error("PatchMatch: No valid source regions!");
  }

  // std::cout << "Initializing region: " << internalRegion << std::endl;
  itk::ImageRegionIteratorWithIndex<PMImageType> outputIterator(this->Output, internalRegion);

  while(!outputIterator.IsAtEnd())
  {
    // Construct the current region
    itk::Index<2> currentIndex = outputIterator.GetIndex();

    if(expandedMask->IsHole(currentIndex))
    {
      itk::ImageRegion<2> currentRegion = ITKHelpers::GetRegionInRadiusAroundPixel(currentIndex, this->PatchRadius);

      // Find the nearest valid boundary patch
      unsigned int closestIndexId = ITKHelpers::ClosestIndexId(boundaryIndices, currentIndex);
      itk::Index<2> closestBoundaryPatchCenter = boundaryIndices[closestIndexId];
      itk::ImageRegion<2> closestBoundaryPatchRegion = ITKHelpers::GetRegionInRadiusAroundPixel(closestBoundaryPatchCenter,
                                                                                                this->PatchRadius);

      Match match;
      match.Region = closestBoundaryPatchRegion;
      match.Score = this->PatchDistanceFunctor->Distance(closestBoundaryPatchRegion, currentRegion);
      outputIterator.Set(match);
    }
    ++outputIterator;
  }

  //std::cout << "Finished initializing." << internalRegion << std::endl;
}

template <typename TImage>
void PatchMatch<TImage>::RandomInit()
{
  InitKnownRegion();

  itk::ImageRegion<2> internalRegion =
             ITKHelpers::GetInternalRegion(this->Image->GetLargestPossibleRegion(), this->PatchRadius);

  std::vector<itk::ImageRegion<2> > ValidSourceRegions =
        MaskOperations::GetAllFullyValidRegions(this->SourceMask, internalRegion, this->PatchRadius);

  if(ValidSourceRegions.size() == 0)
  {
    throw std::runtime_error("PatchMatch: No valid source regions!");
  }

  // std::cout << "Initializing region: " << internalRegion << std::endl;
  itk::ImageRegionIteratorWithIndex<PMImageType> outputIterator(this->Output, internalRegion);

  while(!outputIterator.IsAtEnd())
  {
    // Construct the current region
    itk::Index<2> currentIndex = outputIterator.GetIndex();
    itk::ImageRegion<2> currentRegion = ITKHelpers::GetRegionInRadiusAroundPixel(currentIndex, this->PatchRadius);

    if(!this->SourceMask->IsValid(currentRegion))
    {
      itk::ImageRegion<2> randomValidRegion = ValidSourceRegions[Helpers::RandomInt(0, ValidSourceRegions.size() - 1)];

      Match randomMatch;
      randomMatch.Region = randomValidRegion;
      randomMatch.Score = this->PatchDistanceFunctor->Distance(randomValidRegion, currentRegion);
      outputIterator.Set(randomMatch);
    }

    ++outputIterator;
  }

  //std::cout << "Finished initializing." << internalRegion << std::endl;
}

template <typename TImage>
typename PatchMatch<TImage>::PMImageType* PatchMatch<TImage>::GetOutput()
{
  return Output;
}

template <typename TImage>
void PatchMatch<TImage>::SetIterations(const unsigned int iterations)
{
  this->Iterations = iterations;
}

template <typename TImage>
void PatchMatch<TImage>::SetPatchRadius(const unsigned int patchRadius)
{
  this->PatchRadius = patchRadius;
}


template <typename TImage>
void PatchMatch<TImage>::SetImage(TImage* const image)
{
  ITKHelpers::DeepCopy(image, this->Image.GetPointer());

  this->Output->SetRegions(this->Image->GetLargestPossibleRegion());
  this->Output->Allocate();
}

template <typename TImage>
void PatchMatch<TImage>::SetSourceMask(Mask* const mask)
{
  this->SourceMask->DeepCopyFrom(mask);
  this->SourceMaskBoundingBox = MaskOperations::ComputeHoleBoundingBox(this->SourceMask);
}

template <typename TImage>
void PatchMatch<TImage>::SetTargetMask(Mask* const mask)
{
  this->TargetMask->DeepCopyFrom(mask);
  this->TargetMaskBoundingBox = MaskOperations::ComputeHoleBoundingBox(this->TargetMask);
}

template <typename TImage>
void PatchMatch<TImage>::GetPatchCentersImage(PMImageType* const pmImage, CoordinateImageType* const output)
{
  output->SetRegions(pmImage->GetLargestPossibleRegion());
  output->SetNumberOfComponentsPerPixel(3);
  output->Allocate();

  itk::ImageRegionIterator<PMImageType> imageIterator(pmImage, pmImage->GetLargestPossibleRegion());

  while(!imageIterator.IsAtEnd())
    {
    CoordinateImageType::PixelType pixel;
    pixel.SetSize(3);

    Match match = imageIterator.Get();
    itk::Index<2> center = ITKHelpers::GetRegionCenter(match.Region);

    pixel[0] = center[0];
    pixel[1] = center[1];
    pixel[2] = match.Score;

    output->SetPixel(imageIterator.GetIndex(), pixel);

    ++imageIterator;
    }
}

template <typename TImage>
void PatchMatch<TImage>::SetPatchDistanceFunctor(PatchDistance* const patchDistanceFunctor)
{
  this->PatchDistanceFunctor = patchDistanceFunctor;
}


template <typename TImage>
void PatchMatch<TImage>::Propagation(const std::vector<itk::Offset<2> >& offsets)
{
  // Iterate over patch centers
  itk::ImageRegionIteratorWithIndex<PMImageType> outputIterator(this->Output,
                                                                this->SourceMaskBoundingBox);

  while(!outputIterator.IsAtEnd())
  {
    // Only compute the NN-field where the target mask is valid
    if(!this->TargetMask->IsValid(outputIterator.GetIndex()))
    {
      ++outputIterator;
      continue;
    }

    // When using PatchMatch for inpainting, most of the NN-field will be an exact match. We don't have to search anymore
    // once the exact match is found.
    if(outputIterator.Get().Score == 0)
    {
      ++outputIterator;
      continue;
    }

    Match currentMatch = outputIterator.Get();

    itk::Index<2> center = outputIterator.GetIndex();
    itk::ImageRegion<2> centerRegion = ITKHelpers::GetRegionInRadiusAroundPixel(center, this->PatchRadius);
    for(size_t potentialPropagationPixelId = 0; potentialPropagationPixelId < offsets.size(); ++potentialPropagationPixelId)
    {
      itk::Index<2> potentialPropagationPixel = outputIterator.GetIndex() + offsets[potentialPropagationPixelId];

      // The potential match is the opposite (hence the " - offsets[...]" in the following line)
      // of the offset of the neighbor. Consider the following case:
      // - We are at (4,4) and potentially propagating from (3,4)
      // - The best match to (3,4) is (10,10)
      // - potentialMatch should be (11,10), because since the current pixel is 1 to the right of the neighbor,
      //   we need to consider the patch one to the right of the neighbors best match
      itk::Index<2> potentialMatchPixel = ITKHelpers::GetRegionCenter(this->Output->GetPixel(potentialPropagationPixel).Region) -
                                          offsets[potentialPropagationPixelId];

      itk::ImageRegion<2> potentialMatchRegion = ITKHelpers::GetRegionInRadiusAroundPixel(potentialMatchPixel, this->PatchRadius);

      if(!this->SourceMask->GetLargestPossibleRegion().IsInside(potentialMatchRegion) ||
         !this->SourceMask->IsValid(potentialMatchRegion))
      {
        // do nothing
      }
      else
      {
        float distance = this->PatchDistanceFunctor->Distance(potentialMatchRegion, centerRegion);

        if (distance < currentMatch.Score)
        {
          currentMatch.Region = potentialMatchRegion;
          currentMatch.Score = distance;
        }
      }
    } // end loop over offsets

    outputIterator.Set(currentMatch);
    ++outputIterator;
  } // end forward loop

}

template <typename TImage>
void PatchMatch<TImage>::ForwardPropagation()
{
  std::vector<itk::Offset<2> > offsets;

  // Setup the indices of the "left" pixel
  itk::Offset<2> leftPixelOffset = {{-1, 0}};
  offsets.push_back(leftPixelOffset);

  // Setup the indices of the "up" pixel
  itk::Offset<2> upPixelOffset = {{0, -1}};
  offsets.push_back(upPixelOffset);

  Propagation(offsets);
}

template <typename TImage>
void PatchMatch<TImage>::BackwardPropagation()
{
  std::vector<itk::Offset<2> > offsets;
  // Setup the indices of the "right" pixel
  itk::Offset<2> rightPixel = {{1, 0}};
  offsets.push_back(rightPixel);

  // Setup the indices of the "down" pixel
  itk::Offset<2> downPixel = {{0,1}};
  offsets.push_back(downPixel);

  Propagation(offsets);
}

template <typename TImage>
void PatchMatch<TImage>::RandomSearch()
{
  // RANDOM SEARCH - try a random region in smaller windows around the current best patch.
  std::cout << "Random search..." << std::endl;

  // Iterate over patch centers
  itk::ImageRegionIteratorWithIndex<PMImageType> outputIterator(this->Output,
                                                                this->SourceMaskBoundingBox);
  while(!outputIterator.IsAtEnd())
  {
    // Only compute the NN-field where the target mask is valid
    if(!this->TargetMask->IsValid(outputIterator.GetIndex()))
    {
      ++outputIterator;
      continue;
    }

    // For inpainting, most of the NN-field will be an exact match. We don't have to search anymore
    // once the exact match is found.
    if(outputIterator.Get().Score == 0)
    {
      ++outputIterator;
      continue;
    }
    itk::Index<2> center = outputIterator.GetIndex();

    itk::ImageRegion<2> centerRegion = ITKHelpers::GetRegionInRadiusAroundPixel(center, this->PatchRadius);

    Match currentMatch = outputIterator.Get();

    unsigned int width = this->Image->GetLargestPossibleRegion().GetSize()[0];
    unsigned int height = this->Image->GetLargestPossibleRegion().GetSize()[1];

    unsigned int radius = std::max(width, height);
    //radius /= 2; // Only search half of the image
    radius /= 8; // Only search a small window

    // Search an exponentially smaller window each time through the loop
    itk::Index<2> searchRegionCenter = ITKHelpers::GetRegionCenter(outputIterator.Get().Region);

    while (radius > this->PatchRadius * 2) // the *2 is arbitrary, just want a small-ish region
    {
      itk::ImageRegion<2> searchRegion = ITKHelpers::GetRegionInRadiusAroundPixel(searchRegionCenter, radius);
      searchRegion.Crop(this->Image->GetLargestPossibleRegion());

      unsigned int maxNumberOfAttempts = 5; // How many random patches to test for validity before giving up
      itk::ImageRegion<2> randomValidRegion =
                MaskOperations::GetRandomValidPatchInRegion(this->SourceMask.GetPointer(),
                                                            searchRegion, this->PatchRadius, maxNumberOfAttempts);

      // If no suitable region is found, move on
      if(randomValidRegion.GetSize()[0] == 0)
      {
        radius /= 2;
        continue;
      }

      float dist = this->PatchDistanceFunctor->Distance(randomValidRegion, centerRegion);

      if (dist < currentMatch.Score)
      {
        currentMatch.Region = randomValidRegion;
        currentMatch.Score = dist;
      }

      outputIterator.Set(currentMatch);

      radius /= 2;
    } // end while radius

    ++outputIterator;
  } // end random search loop

}

#endif
