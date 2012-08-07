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

#ifndef AcceptanceTestSSD_H
#define AcceptanceTestSSD_H

// Custom
#include "AcceptanceTest.h"

template <typename TImage>
class AcceptanceTestSSD : public AcceptanceTestImage<TImage>
{
  virtual bool IsBetter(const itk::ImageRegion<2>& queryRegion, const Match& currentMatch,
                        const Match& potentialBetterMatch)
  {
    if(potentialBetterMatch.Score < currentMatch.Score)
    {
      return true;
    }
    else
    {
      return false;
    }
  }

};
#endif