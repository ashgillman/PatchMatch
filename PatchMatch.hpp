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
PatchMatch<TImage>::PatchMatch() : PatchRadius(0), PatchDistanceFunctor(NULL),
                                   InitializationStrategy(RANDOM), Random(true)
{
  this->Output = PMImageType::New();
  this->Image = TImage::New();
  this->SourceMask = Mask::New();
  this->TargetMask = Mask::New();
}

template <typename TImage>
void PatchMatch<TImage>::Compute(PMImageType* const initialization)
{
  if(this->Random)
  {
    srand(time(NULL));
  }
  else
  {
    srand(0);
  }

  ComputeTargetRegions();

  // If an initialization is provided, use it. Otherwise, generate one.
  if(initialization)
  {
    ITKHelpers::DeepCopy(initialization, this->Output.GetPointer());
  }
  else
  {
    if(this->InitializationStrategy == RANDOM)
    {
      RandomInit();
    }
    else if(this->InitializationStrategy == BOUNDARY)
    {
      BoundaryInit();
    }
    else
    {
      throw std::runtime_error("PatchMatch::Compute: An invalid initialization strategy was chosen!");
    }
  }

  { // Debug only
  CoordinateImageType::Pointer initialOutput = CoordinateImageType::New();
  GetPatchCentersImage(this->Output, initialOutput);
  ITKHelpers::WriteImage(initialOutput.GetPointer(), "initialization.mha");
  }

  // Initialize this so that we propagate forward first (the propagation direction toggles at each iteration)
  bool forwardPropagation = true;

  // For the number of iterations specified, perform the appropriate propagation and then a random search
  for(unsigned int iteration = 0; iteration < this->Iterations; ++iteration)
  {
    std::cout << "PatchMatch iteration " << iteration << std::endl;

    if(forwardPropagation)
    {
      ForwardPropagation();
    }
    else
    {
      BackwardPropagation();
    }

    // Switch the propagation direction for the next iteration
    forwardPropagation = !forwardPropagation;

    RandomSearch();

    { // Debug only
    // Write the intermediate output
    std::stringstream ss;
    ss << "PatchMatch_" << Helpers::ZeroPad(iteration, 2) << ".mha";
    CoordinateImageType::Pointer temp = CoordinateImageType::New();
    GetPatchCentersImage(this->Output, temp);
    ITKHelpers::WriteImage(temp.GetPointer(), ss.str());
    }
  }

}

template <typename TImage>
void PatchMatch<TImage>::InitKnownRegion()
{
  // Create a zero region
  itk::Index<2> zeroIndex = {{0,0}};
  itk::Size<2> zeroSize = {{0,0}};
  itk::ImageRegion<2> zeroRegion(zeroIndex, zeroSize);

  // Create an invalid match
  Match invalidMatch;
  invalidMatch.Region = zeroRegion;
  invalidMatch.Score = std::numeric_limits<float>::max();

  // Initialize the entire NNfield to be invalid matches
  ITKHelpers::SetImageToConstant(this->Output.GetPointer(), invalidMatch);

  // Get all of the regions that are entirely inside the image
  itk::ImageRegion<2> internalRegion =
             ITKHelpers::GetInternalRegion(this->Image->GetLargestPossibleRegion(), this->PatchRadius);

  // Set all of the patches that are entirely inside the source region to exactly
  // themselves as their nearest neighbor
  itk::ImageRegionIteratorWithIndex<PMImageType> outputIterator(this->Output, internalRegion);

  while(!outputIterator.IsAtEnd())
  {
    // Construct the current region
    itk::Index<2> currentIndex = outputIterator.GetIndex();
    itk::ImageRegion<2> currentRegion =
         ITKHelpers::GetRegionInRadiusAroundPixel(currentIndex, this->PatchRadius);

    if(this->SourceMask->IsValid(currentRegion))
    {
      Match randomMatch;
      randomMatch.Region = currentRegion;
      randomMatch.Score = 0.0f;
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
  std::vector<itk::Index<2> > boundaryIndices =
         ITKHelpers::GetPixelsWithValue(boundaryImage.GetPointer(), outputBoundaryPixelValue);

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
      itk::ImageRegion<2> currentRegion =
            ITKHelpers::GetRegionInRadiusAroundPixel(currentIndex, this->PatchRadius);

      // Find the nearest valid boundary patch
      unsigned int closestIndexId = ITKHelpers::ClosestIndexId(boundaryIndices, currentIndex);
      itk::Index<2> closestBoundaryPatchCenter = boundaryIndices[closestIndexId];
      itk::ImageRegion<2> closestBoundaryPatchRegion =
            ITKHelpers::GetRegionInRadiusAroundPixel(closestBoundaryPatchCenter,
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

  std::vector<itk::ImageRegion<2> > validSourceRegions =
        MaskOperations::GetAllFullyValidRegions(this->SourceMask, internalRegion, this->PatchRadius);

  if(validSourceRegions.size() == 0)
  {
    throw std::runtime_error("PatchMatch: No valid source regions!");
  }

  // std::cout << "Initializing region: " << internalRegion << std::endl;
  for(size_t targetRegionId = 0; targetRegionId < this->TargetRegions.size(); ++targetRegionId)
  {
    itk::ImageRegion<2> targetRegion = this->TargetRegions[targetRegionId];

    unsigned int randomSourceRegionId = Helpers::RandomInt(0, validSourceRegions.size() - 1);
    itk::ImageRegion<2> randomValidRegion = validSourceRegions[randomSourceRegionId];

    Match randomMatch;
    randomMatch.Region = randomValidRegion;
    randomMatch.Score = this->PatchDistanceFunctor->Distance(randomValidRegion, targetRegion);

    itk::Index<2> regionCenter = ITKHelpers::GetRegionCenter(this->TargetRegions[targetRegionId]);
    this->Output->SetPixel(regionCenter, randomMatch);
  }

  //std::cout << "Finished initializing." << internalRegion << std::endl;
}

template <typename TImage>
typename PatchMatch<TImage>::PMImageType* PatchMatch<TImage>::GetOutput()
{
  return this->Output;
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
  this->SourceMaskBoundingBox = MaskOperations::ComputeValidBoundingBox(this->SourceMask);
  std::cout << "SourceMaskBoundingBox: " << this->SourceMaskBoundingBox << std::endl;
}

template <typename TImage>
void PatchMatch<TImage>::SetTargetMask(Mask* const mask)
{
  this->TargetMask->DeepCopyFrom(mask);
  this->TargetMaskBoundingBox = MaskOperations::ComputeValidBoundingBox(this->TargetMask);
  std::cout << "TargetMaskBoundingBox: " << this->TargetMaskBoundingBox << std::endl;
}

template <typename TImage>
void PatchMatch<TImage>::GetPatchCentersImage(PMImageType* const pmImage, CoordinateImageType* const output)
{
  output->SetRegions(pmImage->GetLargestPossibleRegion());
  output->Allocate();

  itk::ImageRegionIterator<PMImageType> imageIterator(pmImage, pmImage->GetLargestPossibleRegion());

  while(!imageIterator.IsAtEnd())
    {
    CoordinateImageType::PixelType pixel;

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
void PatchMatch<TImage>::SetPatchDistanceFunctor(PatchDistance<TImage>* const patchDistanceFunctor)
{
  this->PatchDistanceFunctor = patchDistanceFunctor;
}

template <typename TImage>
void PatchMatch<TImage>::Propagation(const std::vector<itk::Offset<2> >& offsets)
{
  for(size_t targetRegionId = 0; targetRegionId < this->TargetRegions.size(); ++targetRegionId)
  {
    itk::Index<2> targetRegionCenter = ITKHelpers::GetRegionCenter(this->TargetRegions[targetRegionId]);
    // When using PatchMatch for inpainting, most of the NN-field will be an exact match.
    // We don't have to search anymore
    // once the exact match is found.
    if(this->Output->GetPixel(targetRegionCenter).Score == 0)
    {
      continue;
    }

    itk::ImageRegion<2> centerRegion =
          ITKHelpers::GetRegionInRadiusAroundPixel(targetRegionCenter, this->PatchRadius);

    for(size_t potentialPropagationPixelId = 0; potentialPropagationPixelId < offsets.size();
        ++potentialPropagationPixelId)
    {
      itk::Index<2> potentialPropagationPixel = targetRegionCenter +
                                                offsets[potentialPropagationPixelId];

      if(!AllowPropagationFrom(potentialPropagationPixel))
      {
        continue;
      }
      // The potential match is the opposite (hence the " - offsets[...]" in the following line)
      // of the offset of the neighbor. Consider the following case:
      // - We are at (4,4) and potentially propagating from (3,4)
      // - The best match to (3,4) is (10,10)
      // - potentialMatch should be (11,10), because since the current pixel is 1 to the right
      // of the neighbor, we need to consider the patch one to the right of the neighbors best match
      itk::Index<2> potentialMatchPixel =
            ITKHelpers::GetRegionCenter(this->Output->GetPixel(potentialPropagationPixel).Region) -
                                          offsets[potentialPropagationPixelId];

      itk::ImageRegion<2> potentialMatchRegion =
            ITKHelpers::GetRegionInRadiusAroundPixel(potentialMatchPixel, this->PatchRadius);

      if(!this->SourceMask->GetLargestPossibleRegion().IsInside(potentialMatchRegion) ||
         !this->SourceMask->IsValid(potentialMatchRegion))
      {
        // do nothing
      }
      else
      {
        float distance = this->PatchDistanceFunctor->Distance(potentialMatchRegion, centerRegion);

        Match potentialMatch;
        potentialMatch.Region = potentialMatchRegion;
        potentialMatch.Score = distance;

        //float oldScore = this->Output->GetPixel(targetRegionCenter).Score; // For debugging only
        //bool better = AddIfBetter(targetRegionCenter, potentialMatch);

        AddIfBetter(targetRegionCenter, potentialMatch);
//         if(better)
//         {
//           std::cout << "Propagation successful for " << center << " - lowered score from " << oldScore
//                     << " to " << potentialMatch.Score << std::endl;
//         }
//         else
//         {
//           std::cout << "Propagation not successful." << std::endl;
//         }

      }
    } // end loop over offsets

  } // end loop over target patches

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
  for(size_t targetRegionId = 0; targetRegionId < this->TargetRegions.size(); ++targetRegionId)
  {
    itk::ImageRegion<2> targetRegion = this->TargetRegions[targetRegionId];

    itk::Index<2> targetRegionCenter = ITKHelpers::GetRegionCenter(targetRegion);

    // For inpainting, most of the NN-field will be an exact match. We don't have to search anymore
    // once the exact match is found.
    if(this->Output->GetPixel(targetRegionCenter).Score == 0)
    {
      continue;
    }

    unsigned int width = this->Image->GetLargestPossibleRegion().GetSize()[0];
    unsigned int height = this->Image->GetLargestPossibleRegion().GetSize()[1];

    // The maximum (first) search radius, as prescribed in PatchMatch paper section 3.2
    unsigned int radius = std::max(width, height);

    // The fraction by which to reduce the search radius at each iteration,
    // as prescribed in PatchMatch paper section 3.2
    float alpha = 1.0f/2.0f; 

    // Search an exponentially smaller window each time through the loop

    while (radius > this->PatchRadius) // while there is more than just the current patch to search
    {
      itk::ImageRegion<2> searchRegion = ITKHelpers::GetRegionInRadiusAroundPixel(targetRegionCenter, radius);
      searchRegion.Crop(this->Image->GetLargestPossibleRegion());

      unsigned int maxNumberOfAttempts = 5; // How many random patches to test for validity before giving up

      itk::ImageRegion<2> randomValidRegion;
      try
      {
      // This function throws an exception if no valid patch was found
      randomValidRegion =
                MaskOperations::GetRandomValidPatchInRegion(this->SourceMask.GetPointer(),
                                                            searchRegion, this->PatchRadius,
                                                            maxNumberOfAttempts);
      }
      catch (...) // If no suitable region is found, move on
      {
        //std::cout << "No suitable region found." << std::endl;
        radius *= alpha;
        continue;
      }

      // Compute the patch difference
      float dist = this->PatchDistanceFunctor->Distance(randomValidRegion, targetRegion);

      // Construct a match object
      Match potentialMatch;
      potentialMatch.Region = randomValidRegion;
      potentialMatch.Score = dist;

      // Store this match as the best match if it meets the criteria.
      // In this class, the criteria is simply that it is
      // better than the current best patch. In subclasses (i.e. GeneralizedPatchMatch),
      // it must be better than the worst patch currently stored.
//       float oldScore = this->Output->GetPixel(targetRegionCenter).Score; // For debugging only
//       bool better = AddIfBetter(targetRegionCenter, potentialMatch);
      
      AddIfBetter(targetRegionCenter, potentialMatch);
//       if(better)
//       {
//         std::cout << "Random search successful for " << center << " - lowered score from " << oldScore
//                   << " to " << potentialMatch.Score << std::endl;
//       }
//       else
//       {
//         std::cout << "Random search not successful." << std::endl;
//       }

      radius *= alpha;
    } // end decreasing radius loop

  } // end loop over target regions

}

template <typename TImage>
bool PatchMatch<TImage>::AddIfBetter(const itk::Index<2>& index, const Match& potentialMatch)
{
  Match currentMatch = this->Output->GetPixel(index);
  if(potentialMatch.Score < currentMatch.Score)
  {
    this->Output->SetPixel(index, potentialMatch);
    return true;
  }
  return false;
}

template <typename TImage>
void PatchMatch<TImage>::SetInitializationStrategy(const InitializationStrategyEnum initializationStrategy)
{
  this->InitializationStrategy = initializationStrategy;
}

template <typename TImage>
void PatchMatch<TImage>::ComputeTargetRegions()
{
  this->TargetRegions.clear();

  itk::ImageRegion<2> searchRegion = this->TargetMaskBoundingBox;

  itk::Index<2> searchRegionCorner = searchRegion.GetIndex();
  searchRegionCorner[0] -= (this->PatchRadius + 1);
  searchRegionCorner[1] -= (this->PatchRadius + 1);
  searchRegion.SetIndex(searchRegionCorner);

  // 2*PatchRadius is the number of pixels that a patch can be shifted and still
  // touch the original region. The second *2 is because this is possible on both sides.
  itk::Size<2> searchRegionSize = searchRegion.GetSize();
  searchRegionSize[0] += (this->PatchRadius*2 * 2);
  searchRegionSize[1] += (this->PatchRadius*2 * 2);
  searchRegion.SetSize(searchRegionSize);

  // Ensure the search region consists of pixels whose surrounding patches are entirely inside the image
  //searchRegion.Crop(this->Image->GetLargestPossibleRegion());
  itk::ImageRegion<2> internalRegion =
             ITKHelpers::GetInternalRegion(this->Image->GetLargestPossibleRegion(), this->PatchRadius);
  searchRegion.Crop(internalRegion);

  itk::ImageRegionIteratorWithIndex<TImage> imageIterator(this->Image,
                                                          searchRegion);

  while(!imageIterator.IsAtEnd())
  {
    //std::cout << "Testing " << regionCounter << " of " << searchRegion.GetNumberOfPixels() << std::endl;
    // Construct the current region
    itk::Index<2> currentIndex = imageIterator.GetIndex();

    itk::ImageRegion<2> currentRegion =
          ITKHelpers::GetRegionInRadiusAroundPixel(currentIndex, this->PatchRadius);

    if(this->TargetMask->HasValidPixels(currentRegion))
    {
      this->TargetRegions.push_back(currentRegion);
    }
    ++imageIterator;
  }

  std::cout << "There are " << this->TargetRegions.size() << " target regions." << std::endl;

//   { // debug only
//     typedef itk::Image<unsigned char, 2> RegionImageType;
//     RegionImageType::Pointer targetRegionImage = RegionImageType::New();
//     targetRegionImage->SetRegions(this->Image->GetLargestPossibleRegion());
//     targetRegionImage->Allocate();
//     targetRegionImage->FillBuffer(0);
// 
//     for(unsigned int i = 0; i < this->TargetRegions.size(); ++i)
//     {
//       ITKHelpers::SetRegionToConstant(targetRegionImage.GetPointer(), this->TargetRegions[i], 255);
//     }
//     ITKHelpers::WriteImage(targetRegionImage.GetPointer(), "TargetRegionImage.png");
//   }
}

template <typename TImage>
bool PatchMatch<TImage>::AllowPropagationFrom(const itk::Index<2>& potentialPropagationPixel)
{
  return true;
}

template <typename TImage>
void PatchMatch<TImage>::SetRandom(const bool random)
{
  this->Random = random;
}

#endif