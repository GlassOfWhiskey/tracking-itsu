/// \file CAIndexTable.cxx
/// \brief 
///
/// \author Iacopo Colonnelli, Politecnico di Torino
///
/// \copyright Copyright (C) 2017  Iacopo Colonnelli. \n\n
///   This program is free software: you can redistribute it and/or modify
///   it under the terms of the GNU General Public License as published by
///   the Free Software Foundation, either version 3 of the License, or
///   (at your option) any later version. \n\n
///   This program is distributed in the hope that it will be useful,
///   but WITHOUT ANY WARRANTY; without even the implied warranty of
///   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
///   GNU General Public License for more details. \n\n
///   You should have received a copy of the GNU General Public License
///   along with this program.  If not, see <http://www.gnu.org/licenses/>.
///////////////////////////////////////////////////////////////////////////////

#include "CAIndexTable.h"

#include <functional>

#include "CAIndexTableUtils.h"

CAIndexTable::CAIndexTable()
    : mLayerIndex { CAConstants::ITS::UnusedIndex }
{
}

CAIndexTable::CAIndexTable(const CALayer& layer)
    : mLayerIndex { layer.getLayerIndex() }
{
  int layerClustersNum = layer.getClustersSize();

  int previousBinIndex = 0;
  mTableBins[0] = 0;

  for (int iCluster = 1; iCluster < layerClustersNum; ++iCluster) {

    const CACluster& currentCluster = layer.getCluster(iCluster);
    const int currentBinIndex = currentCluster.indexTableBinIndex;

    if(currentBinIndex > previousBinIndex) {

      for(int iBin = previousBinIndex + 1; iBin <= currentBinIndex; iBin++) {

        mTableBins[iBin] = iCluster;
      }

      previousBinIndex = currentBinIndex;
    }
  }

  for(int iBin = previousBinIndex + 1; iBin <= CAConstants::IndexTable::ZBins * CAConstants::IndexTable::PhiBins; iBin++) {

    mTableBins[iBin] = layerClustersNum;
  }

}

const std::vector<int> CAIndexTable::selectBins(const float zRangeMin, const float zRangeMax, const float phiRangeMin,
    const float phiRangeMax)
{
  std::vector<int> filteredBins;

  if (zRangeMax < -CAConstants::ITS::LayersZCoordinate[mLayerIndex]
      || zRangeMin > CAConstants::ITS::LayersZCoordinate[mLayerIndex] || zRangeMin > zRangeMax) {

    return filteredBins;
  }

  const int minZBinIndex = std::max(0, CAIndexTableUtils::getZBinIndex(mLayerIndex, zRangeMin));
  const int maxZBinIndex = std::min(CAConstants::IndexTable::ZBins - 1,
      CAIndexTableUtils::getZBinIndex(mLayerIndex, zRangeMax));
  const int zBinsNum = maxZBinIndex - minZBinIndex + 1;
  const int minPhiBinIndex = CAIndexTableUtils::getPhiBinIndex(CAMathUtils::getNormalizedPhiCoordinate(phiRangeMin));
  const int maxPhiBinIndex = CAIndexTableUtils::getPhiBinIndex(CAMathUtils::getNormalizedPhiCoordinate(phiRangeMax));

  int phiBinsNum = maxPhiBinIndex - minPhiBinIndex + 1;

  if (phiBinsNum < 0) {

    phiBinsNum += CAConstants::IndexTable::PhiBins;
  }

  for (int iPhiBin = minPhiBinIndex, iPhiCount = 0; iPhiCount < phiBinsNum;
      iPhiBin = ++iPhiBin == CAConstants::IndexTable::PhiBins ? 0 : iPhiBin, iPhiCount++) {

    const int firstBinIndex = CAIndexTableUtils::getBinIndex(minZBinIndex, iPhiBin);
    const int maxBinIndex = firstBinIndex + zBinsNum;

    for (int iBinIndex = firstBinIndex; iBinIndex < maxBinIndex; ++iBinIndex) {

      if(mTableBins[iBinIndex] != mTableBins[iBinIndex + 1]) {

        filteredBins.push_back(iBinIndex);
      }
    }
  }

  return filteredBins;
}
