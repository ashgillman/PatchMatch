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

#ifndef Propagator_HPP
#define Propagator_HPP

#include "Propagator.h"

template <typename TPatchDistanceFunctor,
          typename TAcceptanceTest>
Propagator<TPatchDistanceFunctor, TAcceptanceTest>::Propagator() :
PropagatorInterface<TPatchDistanceFunctor, TAcceptanceTest>(), NeighborFunctor(NULL)
{
}

template <typename TPatchDistanceFunctor,
          typename TAcceptanceTest>
unsigned int Propagator<TPatchDistanceFunctor, TAcceptanceTest>::
Propagate(PatchMatchHelpers::NNFieldType* const nnField, const bool force)
{
  assert(this->NeighborFunctor);
  assert(this->ProcessFunctor);
  assert(this->AcceptanceTest);
  assert(this->PatchDistanceFunctor);

  itk::ImageRegion<2> fullRegion = nnField->GetLargestPossibleRegion();

  assert(nnField->GetLargestPossibleRegion().GetSize()[0] > 0); // An initialization must be provided

  std::vector<itk::Index<2> > targetPixels = this->ProcessFunctor->GetPixelsToProcess();

//  std::cout << "Propagation(): There are " << targetPixels.size()
//            << " pixels that would like to be processed." << std::endl;

  unsigned int propagatedPixels = 0;
  unsigned int acceptanceTestFailed = 0;

  for(size_t targetPixelId = 0; targetPixelId < targetPixels.size(); ++targetPixelId)
  {
    itk::Index<2> targetPixel = targetPixels[targetPixelId];
    ProcessPixelSignal(targetPixel);

    itk::ImageRegion<2> targetRegion =
          ITKHelpers::GetRegionInRadiusAroundPixel(targetPixel, this->PatchRadius);

    if(!fullRegion.IsInside(targetRegion))
      {
        std::cerr << "targetRegion " << targetRegion << " is outside of the image." << std::endl;
        continue;
      }

    std::vector<itk::Index<2> > potentialPropagationPixels = this->NeighborFunctor->GetNeighbors(targetPixel);
    // std::cout << "There are " << potentialPropagationPixels.size() << " potentialPropagationPixels." << std::endl;

    bool propagated = false;
    for(size_t potentialPropagationPixelId = 0;
        potentialPropagationPixelId < potentialPropagationPixels.size();
        ++potentialPropagationPixelId)
    {
      itk::Index<2> potentialPropagationPixel = potentialPropagationPixels[potentialPropagationPixelId];

      itk::Offset<2> potentialPropagationPixelOffset = potentialPropagationPixel - targetPixel;

      //assert(this->Image->GetLargestPossibleRegion().IsInside(potentialPropagationPixel));
      if(!fullRegion.IsInside(potentialPropagationPixel))
      {
        // This check should be done in the NeighborFunctor
        //std::cerr << "Pixel " << potentialPropagationPixel << " is outside of the image." << std::endl;
        continue;
      }

      // The potential match is the opposite (hence the " - offset" in the following line)
      // of the offset of the neighbor. Consider the following case:
      // - We are at (4,4) and potentially propagating from (3,4)
      // - The best match to (3,4) is (10,10)
      // - potentialMatch should be (11,10), because since the current pixel is 1 to the right
      // of the neighbor, we need to consider the patch one to the right of the neighbors best match
      if(nnField->GetPixel(potentialPropagationPixel).GetNumberOfMatches() == 0)
      {
        //throw std::runtime_error("potentialPropagationPixel has 0 matches!");
        // This check should really be done in the NeighborFunctor, but the Forwards/BackwardsPropagationNeighbors
        // classes do not have the target mask or the source mask, which they would need to check their hard-coded
        // offsets for validity, so it is easier to do here for now.
        continue;
      }
      itk::Index<2> potentialPropagationPixelNN =
        ITKHelpers::GetRegionCenter(nnField->GetPixel(potentialPropagationPixel).GetMatch(0).GetRegion());
      itk::Index<2> potentialMatchPixel = potentialPropagationPixelNN - potentialPropagationPixelOffset;

      itk::ImageRegion<2> potentialMatchRegion =
            ITKHelpers::GetRegionInRadiusAroundPixel(potentialMatchPixel, this->PatchRadius);

      if(!fullRegion.IsInside(potentialMatchRegion))
      {
        // do nothing - we don't want to propagate information that is not originally valid
        //std::cerr << "Cannot propagate from this source region " << potentialMatchRegion
        //          << " - it is outside of the image!" << std::endl;
      }
      else
      {
        float distance = this->PatchDistanceFunctor->Distance(potentialMatchRegion, targetRegion);

        Match potentialMatch;
        potentialMatch.SetRegion(potentialMatchRegion);
        potentialMatch.SetSSDScore(distance);

        // If there were previous matches, add this one if it is better
        Match currentMatch = nnField->GetPixel(targetPixel).GetMatch(0);

        float verificationScore = 0.0f;

        MatchSet matchSet = nnField->GetPixel(targetPixel);

        if(this->AcceptanceTest->IsBetterWithScore(targetRegion, currentMatch, potentialMatch, verificationScore))
        {
          //std::cout << "Accepted new match for " << targetPixel << std::endl;
          AcceptedSignal(targetPixel, potentialMatchPixel, verificationScore);

          potentialMatch.SetVerified(true);
          potentialMatch.SetVerificationScore(verificationScore);
        }
        else
        {
          acceptanceTestFailed++;
          // DEBUG ONLY: Run this again so we can see what happened
          //this->AcceptanceTest->IsBetterWithScore(targetRegion, currentMatch, potentialMatch, verificationScore); 
          //std::cerr << "Acceptance test failed!" << std::endl;
        }

        // This function handles adding or not adding the match based on the scores
        matchSet.AddMatch(potentialMatch);

        nnField->SetPixel(targetPixel, matchSet);
        PropagatedSignal(nnField);
        propagated = true;

      } // end else source region valid
    } // end loop over potentialPropagationPixels

    if(propagated)
    {
      propagatedPixels++;
    }
    {
      //std::cerr << "Failed to propagate to " << targetPixel << std::endl;
    }

  } // end loop over target pixels

  //std::cout << "Propagation() propagated " << propagatedPixels << " pixels." << std::endl;
  //std::cout << "AcceptanceTest failed " << acceptanceTestFailed << std::endl;
  return propagatedPixels;
}

#endif
