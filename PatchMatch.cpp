#include "PatchMatch.h"

// Submodules
#include "Mask/ITKHelpers/ITKHelpers.h"
#include "Mask/MaskOperations.h"
#include "PatchProjection/PatchProjection.h"

// ITK
#include "itkImageRegionReverseIterator.h"

// STL
#include <ctime>

PatchMatch::PatchMatch() : PatchRadius(0)
{
  this->Distance = std::bind(&PatchMatch::PCADistance,this,std::placeholders::_1, std::placeholders::_2,
                             std::placeholders::_3);
  
  this->Output = PMImageType::New();
  this->Image = ImageType::New();
  this->SourceMask = Mask::New();
  this->TargetMask = Mask::New();
}

void PatchMatch::Compute(PMImageType* const initialization)
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
  VectorImageType::Pointer initialOutput = VectorImageType::New();
  GetPatchCentersImage(this->Output, initialOutput);
  ITKHelpers::WriteImage(initialOutput.GetPointer(), "initialization.mha");
  }

  itk::ImageRegion<2> holeBoundingBox = MaskOperations::ComputeHoleBoundingBox(this->SourceMask);

  bool forwardSearch = true;

  for(unsigned int iteration = 0; iteration < this->Iterations; ++iteration)
  {
    std::cout << "PatchMatch iteration " << iteration << std::endl;

    // PROPAGATION
    if (forwardSearch)
    {
      std::cout << "Forward propagation..." << std::endl;
//       itk::ImageRegion<2> internalRegion =
//              ITKHelpers::GetInternalRegion(this->Image->GetLargestPossibleRegion(), this->PatchRadius);
      // Iterate over patch centers
      itk::ImageRegionIteratorWithIndex<PMImageType> outputIterator(this->Output,
                                                                    holeBoundingBox);

      // Forward propagation - compare left (-1, 0), center (0,0) and up (0, -1)

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

        Match currentMatch = outputIterator.Get();

        itk::Index<2> center = outputIterator.GetIndex();
        itk::ImageRegion<2> centerRegion = ITKHelpers::GetRegionInRadiusAroundPixel(center, this->PatchRadius);

        itk::Index<2> leftPixel = outputIterator.GetIndex();
        leftPixel[0] += -1;
        itk::Index<2> leftMatch = ITKHelpers::GetRegionCenter(this->Output->GetPixel(leftPixel).Region);
        leftMatch[0] += 1;

        itk::ImageRegion<2> leftMatchRegion = ITKHelpers::GetRegionInRadiusAroundPixel(leftMatch, this->PatchRadius);

        if(!this->SourceMask->GetLargestPossibleRegion().IsInside(leftMatchRegion) || !this->SourceMask->IsValid(leftMatchRegion))
        {
          // do nothing
        }
        else
        {
          float distLeft = Distance(leftMatchRegion, centerRegion, currentMatch.Score);

          if (distLeft < currentMatch.Score)
          {
            currentMatch.Region = leftMatchRegion;
            currentMatch.Score = distLeft;
          }
        }

        itk::Index<2> upPixel = outputIterator.GetIndex();
        upPixel[1] += -1;
        itk::Index<2> upMatch = ITKHelpers::GetRegionCenter(this->Output->GetPixel(upPixel).Region);
        upMatch[1] += 1;

        itk::ImageRegion<2> upMatchRegion = ITKHelpers::GetRegionInRadiusAroundPixel(upMatch, this->PatchRadius);

        if(!this->SourceMask->GetLargestPossibleRegion().IsInside(upMatchRegion) || !this->SourceMask->IsValid(upMatchRegion))
        {
          // do nothing
        }
        else
        {
          float distUp = Distance(upMatchRegion, centerRegion, currentMatch.Score);

          if (distUp < currentMatch.Score)
          {
            currentMatch.Region = upMatchRegion;
            currentMatch.Score = distUp;
          }
        }

        outputIterator.Set(currentMatch);
        ++outputIterator;
      } // end forward loop

    } // end if (forwardSearch)
    else
    {
      std::cout << "Backward propagation..." << std::endl;
      // Iterate over patch centers in reverse
//       itk::ImageRegion<2> internalRegion =
//              ITKHelpers::GetInternalRegion(this->Image->GetLargestPossibleRegion(), this->PatchRadius);
      itk::ImageRegionReverseIterator<PMImageType> outputIterator(this->Output,
                                                                  holeBoundingBox);

      // Backward propagation - compare right (1, 0) , center (0,0) and down (0, 1)

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

        Match currentMatch = outputIterator.Get();

        itk::Index<2> center = outputIterator.GetIndex();
        itk::ImageRegion<2> currentRegion = ITKHelpers::GetRegionInRadiusAroundPixel(center, this->PatchRadius);

        itk::Index<2> rightPixel = outputIterator.GetIndex();
        rightPixel[0] += 1;
        itk::Index<2> rightMatch = ITKHelpers::GetRegionCenter(this->Output->GetPixel(rightPixel).Region);
        rightMatch[0] += -1;

        itk::ImageRegion<2> rightMatchRegion = ITKHelpers::GetRegionInRadiusAroundPixel(rightMatch, this->PatchRadius);

        if(!this->SourceMask->GetLargestPossibleRegion().IsInside(rightMatchRegion) ||
          !this->SourceMask->IsValid(rightMatchRegion))
        {
          // do nothing
        }
        else
        {
          float distRight = Distance(rightMatchRegion, currentRegion, currentMatch.Score);

          if (distRight < currentMatch.Score)
          {
            currentMatch.Region = rightMatchRegion;
            currentMatch.Score = distRight;
          }
        }

        itk::Index<2> downPixel = outputIterator.GetIndex();
        downPixel[1] += 1;
        itk::Index<2> downMatch = ITKHelpers::GetRegionCenter(this->Output->GetPixel(downPixel).Region);
        downMatch[1] += -1;

        itk::ImageRegion<2> downMatchRegion = ITKHelpers::GetRegionInRadiusAroundPixel(downMatch, this->PatchRadius);

        if(!this->SourceMask->GetLargestPossibleRegion().IsInside(downMatchRegion) ||
          !this->SourceMask->IsValid(downMatchRegion))
        {
          // do nothing
        }
        else
        {
          float distDown = Distance(downMatchRegion, currentRegion, currentMatch.Score);

          if (distDown < currentMatch.Score)
          {
            currentMatch.Region = downMatchRegion;
            currentMatch.Score = distDown;
          }
        }

        outputIterator.Set(currentMatch);

        ++outputIterator;
      } // end backward loop
    } // end else ForwardSearch

    forwardSearch = !forwardSearch;

    // RANDOM SEARCH - try a random region in smaller windows around the current best patch.
    std::cout << "Random search..." << std::endl;
    // Iterate over patch centers
//     itk::ImageRegion<2> internalRegion =
//              ITKHelpers::GetInternalRegion(this->Image->GetLargestPossibleRegion(), this->PatchRadius);
    itk::ImageRegionIteratorWithIndex<PMImageType> outputIterator(this->Output,
                                                                  holeBoundingBox);
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

      unsigned int width = Image->GetLargestPossibleRegion().GetSize()[0];
      unsigned int height = Image->GetLargestPossibleRegion().GetSize()[1];

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

        float dist = Distance(randomValidRegion, centerRegion, currentMatch.Score);

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

    std::stringstream ss;
    ss << "PatchMatch_" << Helpers::ZeroPad(iteration, 2) << ".mha";
    VectorImageType::Pointer temp = VectorImageType::New();
    GetPatchCentersImage(this->Output, temp);
    ITKHelpers::WriteImage(temp.GetPointer(), ss.str());

  } // end for (int i = 0; i < iterations; i++)

}

float PatchMatch::PixelWiseDistance(const itk::ImageRegion<2>& source,
                           const itk::ImageRegion<2>& target,
                           const float prevDist)
{
  assert(this->SourceMask->IsValid(source));

  if(!this->SourceMask->IsValid(source))
  {
    std::cout << "Source: " << source << std::endl;
    throw std::runtime_error("PatchMatch::distance source is not valid!");
  }

  // Do not use patches on boundaries
  if(!this->Output->GetLargestPossibleRegion().IsInside(source) ||
      !this->Output->GetLargestPossibleRegion().IsInside(target))
  {
    return std::numeric_limits<float>::max();
  }

  // Compute distance between patches
  // Average L2 distance in RGB space
  float distance = 0.0f;

  itk::ImageRegionIteratorWithIndex<ImageType> sourceIterator(this->Image, source);
  itk::ImageRegionIteratorWithIndex<ImageType> targetIterator(this->Image, target);

  unsigned int numberOfPixelsCompared = 0;

  while(!sourceIterator.IsAtEnd())
  {
    numberOfPixelsCompared++;

    ImageType::PixelType sourcePixel = sourceIterator.Get();
    ImageType::PixelType targetPixel = targetIterator.Get();

    distance += sqrt( (sourcePixel[0] - targetPixel[0]) * (sourcePixel[0] - targetPixel[0]) +
                      (sourcePixel[1] - targetPixel[1]) * (sourcePixel[1] - targetPixel[1]) +
                      (sourcePixel[2] - targetPixel[2]) * (sourcePixel[2] - targetPixel[2]));

    // Early termination
    if(distance / static_cast<float>(numberOfPixelsCompared) > prevDist)
    {
      return std::numeric_limits<float>::max();
    }

    ++sourceIterator;
    ++targetIterator;
  }

  if(numberOfPixelsCompared == 0)
  {
    return std::numeric_limits<float>::max();
  }

  distance /= static_cast<float>(numberOfPixelsCompared);

  return distance;
}


float PatchMatch::PCADistance(const itk::ImageRegion<2>& source,
                           const itk::ImageRegion<2>& target,
                           const float prevDist)
{
  assert(this->SourceMask->IsValid(source));

  if(!this->SourceMask->IsValid(source))
  {
    std::cout << "Source: " << source << std::endl;
    throw std::runtime_error("PatchMatch::distance source is not valid!");
  }

  // Do not use patches on image boundaries
  if(!this->Output->GetLargestPossibleRegion().IsInside(source) ||
     !this->Output->GetLargestPossibleRegion().IsInside(target))
  {
    return std::numeric_limits<float>::max();
  }

  VectorType vectorizedSource = PatchProjection<MatrixType, VectorType>::VectorizePatch(this->Image.GetPointer(),
                                                               source);

  VectorType vectorizedTarget = PatchProjection<MatrixType, VectorType>::VectorizePatch(this->Image.GetPointer(),
                                                               target);
  // Compute distance between patches in PCA space
  float distance = (vectorizedSource - vectorizedTarget).squaredNorm();
  
  return distance;
}

void PatchMatch::InitKnownRegion()
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

void PatchMatch::BoundaryInit()
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
      match.Score = Distance(closestBoundaryPatchRegion, currentRegion, std::numeric_limits<float>::max());
      outputIterator.Set(match);
    }
    ++outputIterator;
  }

  //std::cout << "Finished initializing." << internalRegion << std::endl;
}

void PatchMatch::RandomInit()
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
      randomMatch.Score = Distance(randomValidRegion, currentRegion, std::numeric_limits<float>::max());
      outputIterator.Set(randomMatch);
    }

    ++outputIterator;
  }

  //std::cout << "Finished initializing." << internalRegion << std::endl;
}

PatchMatch::PMImageType* PatchMatch::GetOutput()
{
  return Output;
}

void PatchMatch::SetIterations(const unsigned int iterations)
{
  this->Iterations = iterations;
}

void PatchMatch::SetPatchRadius(const unsigned int patchRadius)
{
  this->PatchRadius = patchRadius;
}

void PatchMatch::SetImage(ImageType* const image)
{
  ITKHelpers::DeepCopy(image, this->Image.GetPointer());

  this->Output->SetRegions(this->Image->GetLargestPossibleRegion());
  this->Output->Allocate();

  if(this->PatchRadius == 0)
  {
    throw std::runtime_error("Must set PatchRadius before setting image!");
  }

  std::vector<typename VectorType::Scalar> sortedEigenvalues; // unused
  VectorType meanVector; // unused
  this->ProjectionMatrix = PatchProjection<MatrixType, VectorType>::
                              ComputeProjectionMatrix_CovarianceEigen(this->Image.GetPointer(),
                                                                      this->PatchRadius,
                                                                      meanVector,
                                                                      sortedEigenvalues);
}

void PatchMatch::SetSourceMask(Mask* const mask)
{
  this->SourceMask->DeepCopyFrom(mask);
}

void PatchMatch::SetTargetMask(Mask* const mask)
{
  this->TargetMask->DeepCopyFrom(mask);
}

void PatchMatch::GetPatchCentersImage(PMImageType* const pmImage, itk::VectorImage<float, 2>* const output)
{
  output->SetRegions(pmImage->GetLargestPossibleRegion());
  output->SetNumberOfComponentsPerPixel(3);
  output->Allocate();

  itk::ImageRegionIterator<PMImageType> imageIterator(pmImage, pmImage->GetLargestPossibleRegion());

  typedef itk::VectorImage<float, 2> VectorImageType;

  while(!imageIterator.IsAtEnd())
    {
    VectorImageType::PixelType pixel;
    pixel.SetSize(3);

    itk::Index<2> center = ITKHelpers::GetRegionCenter(imageIterator.Get().Region);

    pixel[0] = center[0];
    pixel[1] = center[1];
    pixel[2] = imageIterator.Get().Score;

    output->SetPixel(imageIterator.GetIndex(), pixel);

    ++imageIterator;
    }
}

void PatchMatch::SetDistanceType(const ENUM_DISTANCE_TYPE)
{

}
