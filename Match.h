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

#ifndef Match_H
#define Match_H

// ITK
#include "itkImageRegion.h"

// STL
#include <limits>

// Submodules
#include <Helpers/Helpers.h>

/** A simple container to pair a region with its patch difference value/score. */
class Match
{
public:

  Match() : SSDScore(InvalidScore), VerificationScore(InvalidScore), Verified(false)
  {
    itk::Index<2> index = {{0,0}};
    itk::Size<2> size = {{0,0}};
    this->Region.SetIndex(index);
    this->Region.SetSize(size);
  }

  /** Determine if the score is valid. */
  bool IsValid() const
  {
    if(Helpers::IsNaN(this->SSDScore))
    {
      return false;
    }

    if(this->Region.GetSize()[0] == 0 || this->Region.GetSize()[1] == 0)
    {
      return false;
    }

    return true;
  }

  /** Determine if the score is valid. */
  bool IsVerified() const
  {
    return this->Verified;
  }

  void SetVerified(const bool verified)
  {
    this->Verified = verified;
  }

  /** Set the Match to be invalid. */
  void MakeInvalid()
  {
    this->SSDScore = this->InvalidScore;
    this->VerificationScore = this->InvalidScore;

    itk::Index<2> invalidIndex = {{0,0}};
    itk::Size<2> invalidSize = {{0,0}};
    itk::ImageRegion<2> invalidRegion(invalidIndex, invalidSize);

    this->Region = invalidRegion;

    this->Verified = false;
  }

  void SetRegion(const itk::ImageRegion<2>& region)
  {
    this->Region = region;
  }

  itk::ImageRegion<2> GetRegion() const
  {
    return this->Region;
  }

  void SetSSDScore(const float& ssdScore)
  {
    this->SSDScore = ssdScore;
  }

  float GetSSDScore() const
  {
    return this->SSDScore;
  }

  float GetVerificationScore() const
  {
    return this->VerificationScore;
  }

  void SetVerificationScore(const float& verificationScore)
  {
    this->VerificationScore = verificationScore;
  }

  bool operator==(const Match &other) const
  {
    // Take special care in comparing scores because they could be NaN (which doesn't compare as expected).

    // If one score is NaN and the other is not
    if(Helpers::IsNaN(this->SSDScore) != Helpers::IsNaN(other.SSDScore))
    {
      return false;
    }

    // If one score is not NaN, the other must also not be NaN (or the previous test would have failed), so we can compare them normally
    if(!Helpers::IsNaN(this->SSDScore))
    {
      if(this->SSDScore != other.SSDScore)
      {
        return false;
      }
    }

    // If one score is NaN and the other is not
    if(Helpers::IsNaN(this->VerificationScore) != Helpers::IsNaN(other.VerificationScore))
    {
      return false;
    }

    // If one score is not NaN, the other must also not be NaN (or the previous test would have failed), so we can compare them normally
    if(!Helpers::IsNaN(this->VerificationScore))
    {
      if(this->VerificationScore != other.VerificationScore)
      {
        return false;
      }
    }

    // Compare normal members
    if((this->Verified != other.Verified) ||
       (this->Region == other.Region))
    {
      return false;
    }

    return true;
  }

private:
  /** The region/patch that describes the 'source' of the match. */
  itk::ImageRegion<2> Region;

  /** The score according to which ever PatchDistanceFunctor is being used. */
  float SSDScore;

  /** The score according to a verification test being used. */
  float VerificationScore;

  /** A constant making it easier to define an invalid score.*/
  static constexpr float InvalidScore = std::numeric_limits< float >::quiet_NaN();

  /** A flag to determine if the Match has passed some sort of test (histogram, etc). */
  bool Verified;

};

#endif
