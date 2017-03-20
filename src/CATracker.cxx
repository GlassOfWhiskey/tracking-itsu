/// \file CATracker.cxx
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

#include "CATracker.h"

CATracker::CATracker(const CAEvent& event)
    : mEvent { event }, mUsedClustersTable(event.getTotalClusters(), false)
{
}

int CATracker::clustersToTracks(CAEvent& event)
{
  //TODO: Implement
  return 0;
}

void CATracker::makeCells(int int1)
{
  std::vector<int> trackletsLookUpTable[ITSConstants::CellsPerRoad];

  for (int iLayer = 0; iLayer < ITSConstants::TrackletsPerRoad; ++iLayer) {

    if (mEvent.getLayer(iLayer).getClusters().empty()) {

      continue;
    }

    if((iLayer + 1) < ITSConstants::TrackletsPerRoad) {

      trackletsLookUpTable[iLayer].resize(mEvent.getLayer(iLayer + 1).getClusters().size(), -1);
    }

    // FIXME: Non ne capisco il significato: in piu' potrebbe dare errore nel caso iL == 0
    //if (trackletsLookUpTable[iLayer - 1].size() == 0u) continue;

    int currentLayerClustersNum = mEvent.getLayer(iLayer).getClusters().size();

    for(int iCluster = 0; iCluster < currentLayerClustersNum; ++iCluster) {

      const CACluster& currentCluster = mEvent.getLayer(iLayer).getClusters()[iCluster];

      if(mUsedClustersTable[currentCluster.clusterId]) {

        continue;
      }

      // TODO: variables
      //const float lambdaTan = (currentCluster.zCoordinate - mEvent.getPrimaryVertexZCoordinate()) / currentCluster.rCoordinate;
      //const float extZ = lambdaTan * ();
      //const int


      //for(int iNextLevelClusters = 0; iNextCluster <)
    }
  }
}

void CATracker::selectClusters(const float zRangeMin, const float zRangeMax, const float phiRangeMin,
  const float phiRangeMax)
{

}
