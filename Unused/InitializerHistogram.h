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

#ifndef InitializerHistogram_H
#define InitializerHistogram_H

#include "Initializer.h"

/** Assign random nearest neighbors, as long as the difference between the histogram of the random patch and the histogram of the query
  * is less than a specified threshold.*/
class InitializerHistogram : public Initializer
{
public:
  virtual void Initialize(itk::Image<Match, 2>* const initialization)
  {
    itk::ImageRegion<2> internalRegion =
             ITKHelpers::GetInternalRegion(this->Image->GetLargestPossibleRegion(), this->PatchRadius);

    std::vector<itk::ImageRegion<2> > validSourceRegions =
          MaskOperations::GetAllFullyValidRegions(this->SourceMask, internalRegion, this->PatchRadius);

    if(validSourceRegions.size() == 0)
    {
      throw std::runtime_error("PatchMatch::RandomInitWithHistogramTest() No valid source regions!");
    }

    std::vector<itk::Index<2> > targetPixels = this->TargetMask->GetValidPixels();
    std::cout << "RandomInitWithHistogramTest: There are : " << targetPixels.size() << " target pixels." << std::endl;
    for(size_t targetPixelId = 0; targetPixelId < targetPixels.size(); ++targetPixelId)
    {
      itk::ImageRegion<2> targetRegion = ITKHelpers::GetRegionInRadiusAroundPixel(targetPixels[targetPixelId], this->PatchRadius);
      if(!this->Image->GetLargestPossibleRegion().IsInside(targetRegion))
      {
        continue;
      }

      itk::Index<2> targetPixel = targetPixels[targetPixelId];
      if(initialization->GetPixel(targetPixel).IsValid())
      {
        continue;
      }

      unsigned int numberOfBinsPerDimension = 20;

      Histogram<int>::HistogramType queryHistogram = Histogram<int>::ComputeImageHistogram1D(this->HSVImage.GetPointer(),
                                                                                            targetRegion, numberOfBinsPerDimension, this->RangeMin, this->RangeMax);

      float histogramDifference;
      itk::ImageRegion<2> randomValidRegion;
      Histogram<int>::HistogramType randomPatchHistogram;
      unsigned int attempts = 0;
      bool acceptableMatchFound = true;
      unsigned int maxAttemps = 10;
      do
      {
        unsigned int randomSourceRegionId = Helpers::RandomInt(0, validSourceRegions.size() - 1);
        randomValidRegion = validSourceRegions[randomSourceRegionId];
        randomPatchHistogram = Histogram<int>::ComputeImageHistogram1D(this->Image.GetPointer(),
                                                                      randomValidRegion, numberOfBinsPerDimension, this->RangeMin, this->RangeMax);
        histogramDifference = Histogram<int>::HistogramDifference(queryHistogram, randomPatchHistogram);
        //std::cout << "histogramDifference: " << histogramDifference << std::endl;
        attempts++;
        if(attempts > maxAttemps)
        {
  //         std::stringstream ss;
  //         ss << "Too many attempts to find a good random match for " << targetPixel;
  //         throw std::runtime_error(ss.str());
          acceptableMatchFound = false;
          break;
        }
      }
      while(histogramDifference > this->HistogramAcceptanceThreshold);

      //std::cout << "Attempts: " << attempts << std::endl;
      Match randomMatch;
      randomMatch.Region = randomValidRegion;

      if(acceptableMatchFound)
      {
        randomMatch.Score = this->PatchDistanceFunctor->Distance(randomValidRegion, targetRegion);
      }
      else
      {
        //std::cout << "Random initialization failed for " << targetPixel << std::endl;
        randomMatch.Score = std::numeric_limits<float>::max();
      }

      initialization->SetPixel(targetPixel, randomMatch);
    }


    { // Debug only
    CoordinateImageType::Pointer initialOutput = CoordinateImageType::New();
    GetPatchCentersImage(this->Output, initialOutput);
    ITKHelpers::WriteImage(initialOutput.GetPointer(), "RandomInit.mha");
    }
    //std::cout << "Finished RandomInit." << internalRegion << std::endl;
  }

  void SetRangeMin(const typename TypeTraits<typename TImage::PixelType>::ComponentType rangeMin)
  {
    this->RangeMin = rangeMin;
  }

  void SetRangeMax(const typename TypeTraits<typename TImage::PixelType>::ComponentType rangeMax)
  {
    this->RangeMax = rangeMax;
  }

protected:
  typename TypeTraits<typename TImage::PixelType>::ComponentType RangeMin;
  typename TypeTraits<typename TImage::PixelType>::ComponentType RangeMax;
};

#endif
