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

#include <array>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>

#include "CACell.h"
#include "CAConstants.h"
#include "CADefinitions.h"
#include "CAEvent.h"
#include "CAIndexTableUtils.h"
#include "CALayer.h"
#include "CAMathUtils.h"
#include "CATracklet.h"
#include "CATrackingUtils.h"

#if TRACKINGITSU_GPU_MODE
# include "CAGPUPrimaryVertexContext.h"
#else
# include "CAPrimaryVertexContext.h"
#endif

template<>
void CATrackerTraits<false>::computeLayerTracklets(Context& primaryVertexContext)
{
  for (int iLayer { 0 }; iLayer < CAConstants::ITS::TrackletsPerRoad; ++iLayer) {

    if (primaryVertexContext.getClusters()[iLayer].empty() || primaryVertexContext.getClusters()[iLayer + 1].empty()) {

      return;
    }

    const float3 &primaryVertex = primaryVertexContext.getPrimaryVertex();
    const int currentLayerClustersNum { static_cast<int>(primaryVertexContext.getClusters()[iLayer].size()) };

    for (int iCluster { 0 }; iCluster < currentLayerClustersNum; ++iCluster) {

      const CACluster& currentCluster { primaryVertexContext.getClusters()[iLayer][iCluster] };

      /*if (mUsedClustersTable[currentCluster.clusterId] != CAConstants::ITS::UnusedIndex) {

       continue;
       }*/

      const float tanLambda { (currentCluster.zCoordinate - primaryVertex.z) / currentCluster.rCoordinate };
      const float directionZIntersection { tanLambda
          * (CAConstants::ITS::LayersRCoordinate()[iLayer + 1] - currentCluster.rCoordinate)
          + currentCluster.zCoordinate };

      const int4 selectedBinsRect { CATrackingUtils::getBinsRect(currentCluster, iLayer, directionZIntersection) };

      if (selectedBinsRect.x == 0 && selectedBinsRect.y == 0 && selectedBinsRect.z == 0 && selectedBinsRect.w == 0) {

        continue;
      }

      int phiBinsNum { selectedBinsRect.w - selectedBinsRect.y + 1 };

      if (phiBinsNum < 0) {

        phiBinsNum += CAConstants::IndexTable::PhiBins;
      }

      for (int iPhiBin { selectedBinsRect.y }, iPhiCount { 0 }; iPhiCount < phiBinsNum;
          iPhiBin = ++iPhiBin == CAConstants::IndexTable::PhiBins ? 0 : iPhiBin, iPhiCount++) {

        const int firstBinIndex { CAIndexTableUtils::getBinIndex(selectedBinsRect.x, iPhiBin) };
        const int maxBinIndex { firstBinIndex + selectedBinsRect.z - selectedBinsRect.x + 1 };
        const int firstRowClusterIndex = primaryVertexContext.getIndexTables()[iLayer][firstBinIndex];
        const int maxRowClusterIndex = primaryVertexContext.getIndexTables()[iLayer][maxBinIndex];

        for (int iNextLayerCluster { firstRowClusterIndex }; iNextLayerCluster <= maxRowClusterIndex;
            ++iNextLayerCluster) {

          const CACluster& nextCluster { primaryVertexContext.getClusters()[iLayer + 1][iNextLayerCluster] };

          const float deltaZ { MATH_ABS(
              tanLambda * (nextCluster.rCoordinate - currentCluster.rCoordinate) + currentCluster.zCoordinate
                  - nextCluster.zCoordinate) };
          const float deltaPhi { MATH_ABS(currentCluster.phiCoordinate - nextCluster.phiCoordinate) };

          if (deltaZ < CAConstants::Thresholds::TrackletMaxDeltaZThreshold()[iLayer]
              && (deltaPhi < CAConstants::Thresholds::PhiCoordinateCut
                  || MATH_ABS(deltaPhi - CAConstants::Math::TwoPi) < CAConstants::Thresholds::PhiCoordinateCut)) {

            if (iLayer > 0
                && primaryVertexContext.getTrackletsLookupTable()[iLayer - 1][iCluster]
                    == CAConstants::ITS::UnusedIndex) {

              primaryVertexContext.getTrackletsLookupTable()[iLayer - 1][iCluster] =
                  primaryVertexContext.getTracklets()[iLayer].size();
            }

            primaryVertexContext.getTracklets()[iLayer].emplace_back(iCluster, iNextLayerCluster, currentCluster,
                nextCluster);
          }
        }
      }
    }
  }
}

template<>
void CATrackerTraits<false>::computeLayerCells(Context& primaryVertexContext)
{
  for (int iLayer { 0 }; iLayer < CAConstants::ITS::CellsPerRoad; ++iLayer) {

    if (primaryVertexContext.getTracklets()[iLayer + 1].empty()
        || primaryVertexContext.getTracklets()[iLayer].empty()) {

      return;
    }

    const float3 &primaryVertex = primaryVertexContext.getPrimaryVertex();
    const int currentLayerTrackletsNum { static_cast<int>(primaryVertexContext.getTracklets()[iLayer].size()) };

    for (int iTracklet { 0 }; iTracklet < currentLayerTrackletsNum; ++iTracklet) {

      const CATracklet& currentTracklet { primaryVertexContext.getTracklets()[iLayer][iTracklet] };
      const int nextLayerClusterIndex { currentTracklet.secondClusterIndex };
      const int nextLayerFirstTrackletIndex {
          primaryVertexContext.getTrackletsLookupTable()[iLayer][nextLayerClusterIndex] };

      if (nextLayerFirstTrackletIndex == CAConstants::ITS::UnusedIndex) {

        continue;
      }

      const CACluster& firstCellCluster { primaryVertexContext.getClusters()[iLayer][currentTracklet.firstClusterIndex] };
      const CACluster& secondCellCluster {
          primaryVertexContext.getClusters()[iLayer + 1][currentTracklet.secondClusterIndex] };
      const float firstCellClusterQuadraticRCoordinate { firstCellCluster.rCoordinate * firstCellCluster.rCoordinate };
      const float secondCellClusterQuadraticRCoordinate { secondCellCluster.rCoordinate * secondCellCluster.rCoordinate };
      const float3 firstDeltaVector { secondCellCluster.xCoordinate - firstCellCluster.xCoordinate,
          secondCellCluster.yCoordinate - firstCellCluster.yCoordinate, secondCellClusterQuadraticRCoordinate
              - firstCellClusterQuadraticRCoordinate };
      const int nextLayerTrackletsNum { static_cast<int>(primaryVertexContext.getTracklets()[iLayer + 1].size()) };

      for (int iNextLayerTracklet { nextLayerFirstTrackletIndex };
          iNextLayerTracklet < nextLayerTrackletsNum
              && primaryVertexContext.getTracklets()[iLayer + 1][iNextLayerTracklet].firstClusterIndex
                  == nextLayerClusterIndex; ++iNextLayerTracklet) {

        const CATracklet& nextTracklet { primaryVertexContext.getTracklets()[iLayer + 1][iNextLayerTracklet] };
        const float deltaTanLambda { std::abs(currentTracklet.tanLambda - nextTracklet.tanLambda) };
        const float deltaPhi { std::abs(currentTracklet.phiCoordinate - nextTracklet.phiCoordinate) };

        if (deltaTanLambda < CAConstants::Thresholds::CellMaxDeltaTanLambdaThreshold
            && (deltaPhi < CAConstants::Thresholds::CellMaxDeltaPhiThreshold
                || std::abs(deltaPhi - CAConstants::Math::TwoPi) < CAConstants::Thresholds::CellMaxDeltaPhiThreshold)) {

          const float averageTanLambda { 0.5f * (currentTracklet.tanLambda + nextTracklet.tanLambda) };
          const float directionZIntersection { -averageTanLambda * firstCellCluster.rCoordinate
              + firstCellCluster.zCoordinate };
          const float deltaZ { std::abs(directionZIntersection - primaryVertex.z) };

          if (deltaZ < CAConstants::Thresholds::CellMaxDeltaZThreshold()[iLayer]) {

            const CACluster& thirdCellCluster {
                primaryVertexContext.getClusters()[iLayer + 2][nextTracklet.secondClusterIndex] };

            const float thirdCellClusterQuadraticRCoordinate { thirdCellCluster.rCoordinate
                * thirdCellCluster.rCoordinate };

            const float3 secondDeltaVector { thirdCellCluster.xCoordinate - firstCellCluster.xCoordinate,
                thirdCellCluster.yCoordinate - firstCellCluster.yCoordinate, thirdCellClusterQuadraticRCoordinate
                    - firstCellClusterQuadraticRCoordinate };

            float3 cellPlaneNormalVector { CAMathUtils::crossProduct(firstDeltaVector, secondDeltaVector) };

            const float vectorNorm { std::sqrt(
                cellPlaneNormalVector.x * cellPlaneNormalVector.x + cellPlaneNormalVector.y * cellPlaneNormalVector.y
                    + cellPlaneNormalVector.z * cellPlaneNormalVector.z) };

            if (vectorNorm < CAConstants::Math::FloatMinThreshold
                || std::abs(cellPlaneNormalVector.z) < CAConstants::Math::FloatMinThreshold) {

              continue;
            }

            const float inverseVectorNorm { 1.0f / vectorNorm };
            const float3 normalizedPlaneVector { cellPlaneNormalVector.x * inverseVectorNorm, cellPlaneNormalVector.y
                * inverseVectorNorm, cellPlaneNormalVector.z * inverseVectorNorm };
            const float planeDistance { -normalizedPlaneVector.x * (secondCellCluster.xCoordinate - primaryVertex.x)
                - (normalizedPlaneVector.y * secondCellCluster.yCoordinate - primaryVertex.y)
                - normalizedPlaneVector.z * secondCellClusterQuadraticRCoordinate };
            const float normalizedPlaneVectorQuadraticZCoordinate { normalizedPlaneVector.z * normalizedPlaneVector.z };
            const float cellTrajectoryRadius { std::sqrt(
                (1.0f - normalizedPlaneVectorQuadraticZCoordinate - 4.0f * planeDistance * normalizedPlaneVector.z)
                    / (4.0f * normalizedPlaneVectorQuadraticZCoordinate)) };
            const float2 circleCenter { -0.5f * normalizedPlaneVector.x / normalizedPlaneVector.z, -0.5f
                * normalizedPlaneVector.y / normalizedPlaneVector.z };
            const float distanceOfClosestApproach { std::abs(
                cellTrajectoryRadius - std::sqrt(circleCenter.x * circleCenter.x + circleCenter.y * circleCenter.y)) };

            if (distanceOfClosestApproach
                > CAConstants::Thresholds::CellMaxDistanceOfClosestApproachThreshold()[iLayer]) {

              continue;
            }

            const float cellTrajectoryCurvature { 1.0f / cellTrajectoryRadius };

            if (iLayer > 0
                && primaryVertexContext.getCellsLookupTable()[iLayer - 1][iTracklet] == CAConstants::ITS::UnusedIndex) {

              primaryVertexContext.getCellsLookupTable()[iLayer - 1][iTracklet] =
                  primaryVertexContext.getCells()[iLayer].size();
            }

            primaryVertexContext.getCells()[iLayer].emplace_back(currentTracklet.firstClusterIndex,
                nextTracklet.firstClusterIndex, nextTracklet.secondClusterIndex, iTracklet, iNextLayerTracklet,
                normalizedPlaneVector, cellTrajectoryCurvature);
          }
        }
      }
    }
  }
}

template<bool IsGPU>
CATracker<IsGPU>::CATracker(const CAEvent& event)
    : mEvent(event), mUsedClustersTable(event.getTotalClusters(), CAConstants::ITS::UnusedIndex)
{
  // Nothing to do
}

template<bool IsGPU>
std::vector<std::vector<CARoad>> CATracker<IsGPU>::clustersToTracks()
{
  const int verticesNum { mEvent.getPrimaryVerticesNum() };
  std::vector<std::vector<CARoad>> roads { };
  roads.reserve(verticesNum);

  for (int iVertex { 0 }; iVertex < verticesNum; ++iVertex) {

    TrackerContext primaryVertexContext { mEvent, iVertex };

    computeTracklets(primaryVertexContext);
    computeCells(primaryVertexContext);
    findCellsNeighbours(primaryVertexContext);
    findTracks(primaryVertexContext);
    computeMontecarloLabels(primaryVertexContext);

    roads.emplace_back(primaryVertexContext.getRoads());
  }

  return roads;
}

template<bool IsGPU>
std::vector<std::vector<CARoad>> CATracker<IsGPU>::clustersToTracksVerbose()
{
  const int verticesNum { mEvent.getPrimaryVerticesNum() };
  std::vector<std::vector<CARoad>> roads { };
  roads.reserve(verticesNum);

  for (int iVertex { 0 }; iVertex < verticesNum; ++iVertex) {

    clock_t t1 { }, t2 { };
    float diff { };

    t1 = clock();

    TrackerContext primaryVertexContext { mEvent, iVertex };

    t2 = clock();
    diff = ((float) t2 - (float) t1) / (CLOCKS_PER_SEC / 1000);
    std::cout << std::setw(2) << " - Context initialized in: " << diff << "ms" << std::endl;

    evaluateTask(&CATracker<IsGPU>::computeTracklets, "Tracklets Finding", primaryVertexContext);
    evaluateTask(&CATracker<IsGPU>::computeCells, "Cells Finding", primaryVertexContext);
    evaluateTask(&CATracker<IsGPU>::findCellsNeighbours, "Neighbours Finding", primaryVertexContext);
    evaluateTask(&CATracker<IsGPU>::findTracks, "Tracks Finding", primaryVertexContext);
    evaluateTask(&CATracker<IsGPU>::computeMontecarloLabels, "Computing Montecarlo Labels", primaryVertexContext);

    t2 = clock();
    diff = ((float) t2 - (float) t1) / (CLOCKS_PER_SEC / 1000);
    std::cout << std::setw(2) << " - Vertex " << iVertex + 1 << " completed in: " << diff << "ms" << std::endl;

    roads.emplace_back(primaryVertexContext.getRoads());
  }

  return roads;
}

template<bool IsGPU>
std::vector<std::vector<CARoad>> CATracker<IsGPU>::clustersToTracksMemoryBenchmark(
    std::ofstream & memoryBenchmarkOutputStream)
{
  const int verticesNum { mEvent.getPrimaryVerticesNum() };
  std::vector<std::vector<CARoad>> roads { };
  roads.reserve(verticesNum);

  for (int iVertex { 0 }; iVertex < verticesNum; ++iVertex) {

    TrackerContext primaryVertexContext { mEvent, iVertex };

    for (int iLayer { 0 }; iLayer < CAConstants::ITS::LayersNumber; ++iLayer) {

      memoryBenchmarkOutputStream << primaryVertexContext.getClusters()[iLayer].size() << "\t";
    }

    memoryBenchmarkOutputStream << std::endl;

    for (int iLayer { 0 }; iLayer < CAConstants::ITS::TrackletsPerRoad; ++iLayer) {

      memoryBenchmarkOutputStream << primaryVertexContext.getTracklets()[iLayer].capacity() << "\t";
    }

    memoryBenchmarkOutputStream << std::endl;

    computeTracklets(primaryVertexContext);

    for (int iLayer { 0 }; iLayer < CAConstants::ITS::TrackletsPerRoad; ++iLayer) {

      memoryBenchmarkOutputStream << primaryVertexContext.getTracklets()[iLayer].size() << "\t";
    }

    memoryBenchmarkOutputStream << std::endl;

    for (int iLayer { 0 }; iLayer < CAConstants::ITS::CellsPerRoad; ++iLayer) {

      memoryBenchmarkOutputStream << primaryVertexContext.getCells()[iLayer].capacity() << "\t";
    }

    memoryBenchmarkOutputStream << std::endl;

    computeCells(primaryVertexContext);

    for (int iLayer { 0 }; iLayer < CAConstants::ITS::CellsPerRoad; ++iLayer) {

      memoryBenchmarkOutputStream << primaryVertexContext.getCells()[iLayer].size() << "\t";
    }

    memoryBenchmarkOutputStream << std::endl;

    findCellsNeighbours(primaryVertexContext);
    findTracks(primaryVertexContext);
    computeMontecarloLabels(primaryVertexContext);

    roads.emplace_back(primaryVertexContext.getRoads());

    memoryBenchmarkOutputStream << primaryVertexContext.getRoads().size() << std::endl;
  }

  return roads;
}

template<bool IsGPU>
std::vector<std::vector<CARoad>> CATracker<IsGPU>::clustersToTracksTimeBenchmark(
    std::ofstream& timeBenchmarkOutputStream)
{
  const int verticesNum = mEvent.getPrimaryVerticesNum();
  std::vector<std::vector<CARoad>> roads;
  roads.reserve(verticesNum);

  for (int iVertex = 0; iVertex < verticesNum; ++iVertex) {

    clock_t t1, t2;
    float diff;

    t1 = clock();

    TrackerContext primaryVertexContext { mEvent, iVertex };

    evaluateTask(&CATracker<IsGPU>::computeTracklets, nullptr, primaryVertexContext, timeBenchmarkOutputStream);
    evaluateTask(&CATracker<IsGPU>::computeCells, nullptr, primaryVertexContext, timeBenchmarkOutputStream);
    evaluateTask(&CATracker<IsGPU>::findCellsNeighbours, nullptr, primaryVertexContext, timeBenchmarkOutputStream);
    evaluateTask(&CATracker<IsGPU>::findTracks, nullptr, primaryVertexContext, timeBenchmarkOutputStream);
    evaluateTask(&CATracker<IsGPU>::computeMontecarloLabels, nullptr, primaryVertexContext, timeBenchmarkOutputStream);

    t2 = clock();
    diff = ((float) t2 - (float) t1) / (CLOCKS_PER_SEC / 1000);
    timeBenchmarkOutputStream << diff << std::endl;

    roads.emplace_back(primaryVertexContext.getRoads());
  }

  return roads;
}

template<bool IsGPU>
void CATracker<IsGPU>::computeTracklets(TrackerContext& primaryVertexContext)
{
  TrackerTraits::computeLayerTracklets(primaryVertexContext);
}

template<bool IsGPU>
void CATracker<IsGPU>::computeCells(TrackerContext& primaryVertexContext)
{
  TrackerTraits::computeLayerCells(primaryVertexContext);
}

template<bool IsGPU>
void CATracker<IsGPU>::findCellsNeighbours(TrackerContext& primaryVertexContext)
{
  for (int iLayer { 0 }; iLayer < CAConstants::ITS::CellsPerRoad - 1; ++iLayer) {

    if (primaryVertexContext.getCells()[iLayer + 1].empty()
        || primaryVertexContext.getCellsLookupTable()[iLayer].empty()) {

      continue;
    }

    int layerCellsNum { static_cast<int>(primaryVertexContext.getCells()[iLayer].size()) };

    for (int iCell { 0 }; iCell < layerCellsNum; ++iCell) {

      const CACell& currentCell { primaryVertexContext.getCells()[iLayer][iCell] };
      const int nextLayerTrackletIndex { currentCell.getSecondTrackletIndex() };
      const int nextLayerFirstCellIndex { primaryVertexContext.getCellsLookupTable()[iLayer][nextLayerTrackletIndex] };

      if (nextLayerFirstCellIndex != CAConstants::ITS::UnusedIndex
          && primaryVertexContext.getCells()[iLayer + 1][nextLayerFirstCellIndex].getFirstTrackletIndex()
              == nextLayerTrackletIndex) {

        const int nextLayerCellsNum { static_cast<int>(primaryVertexContext.getCells()[iLayer + 1].size()) };
        primaryVertexContext.getCellsNeighbours()[iLayer].resize(nextLayerCellsNum);

        for (int iNextLayerCell { nextLayerFirstCellIndex };
            iNextLayerCell < nextLayerCellsNum
                && primaryVertexContext.getCells()[iLayer + 1][iNextLayerCell].getFirstTrackletIndex()
                    == nextLayerTrackletIndex; ++iNextLayerCell) {

          CACell& nextCell { primaryVertexContext.getCells()[iLayer + 1][iNextLayerCell] };
          const float3 currentCellNormalVector { currentCell.getNormalVectorCoordinates() };
          const float3 nextCellNormalVector { nextCell.getNormalVectorCoordinates() };
          const float3 normalVectorsDeltaVector { currentCellNormalVector.x - nextCellNormalVector.x,
              currentCellNormalVector.y - nextCellNormalVector.y, currentCellNormalVector.z - nextCellNormalVector.z };

          const float deltaNormalVectorsModulus { (normalVectorsDeltaVector.x * normalVectorsDeltaVector.x)
              + (normalVectorsDeltaVector.y * normalVectorsDeltaVector.y)
              + (normalVectorsDeltaVector.z * normalVectorsDeltaVector.z) };
          const float deltaCurvature { std::abs(currentCell.getCurvature() - nextCell.getCurvature()) };

          if (deltaNormalVectorsModulus < CAConstants::Thresholds::NeighbourCellMaxNormalVectorsDelta[iLayer]
              && deltaCurvature < CAConstants::Thresholds::NeighbourCellMaxCurvaturesDelta[iLayer]) {

            primaryVertexContext.getCellsNeighbours()[iLayer][iNextLayerCell].push_back(iCell);

            const int currentCellLevel { currentCell.getLevel() };

            if (currentCellLevel >= nextCell.getLevel()) {

              nextCell.setLevel(currentCellLevel + 1);
            }
          }
        }
      }
    }
  }
}

template<bool IsGPU>
void CATracker<IsGPU>::findTracks(TrackerContext& primaryVertexContext)
{
  for (int iLevel { CAConstants::ITS::CellsPerRoad }; iLevel >= CAConstants::Thresholds::CellsMinLevel; --iLevel) {

    const int minimumLevel { iLevel - 1 };

    for (int iLayer { CAConstants::ITS::CellsPerRoad - 1 }; iLayer >= minimumLevel; --iLayer) {

      const int levelCellsNum { static_cast<int>(primaryVertexContext.getCells()[iLayer].size()) };

      for (int iCell { 0 }; iCell < levelCellsNum; ++iCell) {

        CACell& currentCell { primaryVertexContext.getCells()[iLayer][iCell] };

        if (currentCell.getLevel() != iLevel) {

          continue;
        }

        primaryVertexContext.getRoads().emplace_back(iLayer, iCell);

        const int cellNeighboursNum {
            static_cast<int>(primaryVertexContext.getCellsNeighbours()[iLayer - 1][iCell].size()) };
        bool isFirstValidNeighbour = true;

        for (int iNeighbourCell { 0 }; iNeighbourCell < cellNeighboursNum; ++iNeighbourCell) {

          const int neighbourCellId = primaryVertexContext.getCellsNeighbours()[iLayer - 1][iCell][iNeighbourCell];
          const CACell& neighbourCell = primaryVertexContext.getCells()[iLayer - 1][neighbourCellId];

          if (iLevel - 1 != neighbourCell.getLevel()) {

            continue;
          }

          if (isFirstValidNeighbour) {

            isFirstValidNeighbour = false;

          } else {

            primaryVertexContext.getRoads().emplace_back(iLayer, iCell);
          }

          traverseCellsTree(primaryVertexContext, neighbourCellId, iLayer - 1);
        }

        currentCell.setLevel(0);
      }
    }
  }
}

template<bool IsGPU>
void CATracker<IsGPU>::traverseCellsTree(TrackerContext& primaryVertexContext, const int currentCellId,
    const int currentLayerId)
{
  CACell& currentCell { primaryVertexContext.getCells()[currentLayerId][currentCellId] };
  const int currentCellLevel = currentCell.getLevel();

  primaryVertexContext.getRoads().back().addCell(currentLayerId, currentCellId);

  if (currentLayerId > 0) {

    const int cellNeighboursNum {
        static_cast<int>(primaryVertexContext.getCellsNeighbours()[currentLayerId - 1][currentCellId].size()) };
    bool isFirstValidNeighbour = true;

    for (int iNeighbourCell { 0 }; iNeighbourCell < cellNeighboursNum; ++iNeighbourCell) {

      const int neighbourCellId =
          primaryVertexContext.getCellsNeighbours()[currentLayerId - 1][currentCellId][iNeighbourCell];
      const CACell& neighbourCell = primaryVertexContext.getCells()[currentLayerId - 1][neighbourCellId];

      if (currentCellLevel - 1 != neighbourCell.getLevel()) {

        continue;
      }

      if (isFirstValidNeighbour) {

        isFirstValidNeighbour = false;

      } else {

        primaryVertexContext.getRoads().push_back(primaryVertexContext.getRoads().back());
      }

      traverseCellsTree(primaryVertexContext, neighbourCellId, currentLayerId - 1);
    }
  }

  //TODO: crosscheck for short track iterations
  //currentCell.setLevel(0);
}

template<bool IsGPU>
void CATracker<IsGPU>::computeMontecarloLabels(TrackerContext& primaryVertexContext)
{
/// Moore’s Voting Algorithm

  int roadsNum { static_cast<int>(primaryVertexContext.getRoads().size()) };

  for (int iRoad { 0 }; iRoad < roadsNum; ++iRoad) {

    CARoad& currentRoad { primaryVertexContext.getRoads()[iRoad] };
    int maxOccurrencesValue { CAConstants::ITS::UnusedIndex };
    int count { 0 };
    bool isFakeRoad { false };
    bool isFirstRoadCell { true };

    for (int iCell { 0 }; iCell < CAConstants::ITS::CellsPerRoad; ++iCell) {

      const int currentCellIndex { currentRoad[iCell] };

      if (currentCellIndex == CAConstants::ITS::UnusedIndex) {

        if (isFirstRoadCell) {

          continue;

        } else {

          break;
        }
      }

      const CACell& currentCell { primaryVertexContext.getCells()[iCell][currentCellIndex] };

      if (isFirstRoadCell) {

        maxOccurrencesValue =
            primaryVertexContext.getClusters()[iCell][currentCell.getFirstClusterIndex()].monteCarloId;
        count = 1;

        const int secondMonteCarlo {
            primaryVertexContext.getClusters()[iCell + 1][currentCell.getSecondClusterIndex()].monteCarloId };

        if (secondMonteCarlo == maxOccurrencesValue) {

          ++count;

        } else {

          maxOccurrencesValue = secondMonteCarlo;
          count = 1;
          isFakeRoad = true;
        }

        isFirstRoadCell = false;
      }

      const int currentMonteCarlo {
          primaryVertexContext.getClusters()[iCell + 2][currentCell.getThirdClusterIndex()].monteCarloId };

      if (currentMonteCarlo == maxOccurrencesValue) {

        ++count;

      } else {

        --count;
        isFakeRoad = true;
      }

      if (count == 0) {

        maxOccurrencesValue = currentMonteCarlo;
        count = 1;
      }
    }

    currentRoad.setLabel(maxOccurrencesValue);
    currentRoad.setFakeRoad(isFakeRoad);
  }
}

template<bool IsGPU>
void CATracker<IsGPU>::evaluateTask(void (CATracker<IsGPU>::*task)(TrackerContext&), const char *taskName,
    TrackerContext& primaryVertexContext)
{
  evaluateTask(task, taskName, primaryVertexContext, std::cout);
}

template<bool IsGPU>
void CATracker<IsGPU>::evaluateTask(void (CATracker<IsGPU>::*task)(TrackerContext&), const char *taskName,
    TrackerContext& primaryVertexContext, std::ostream& ostream)
{
  clock_t t1, t2;
  float diff;

  t1 = clock();

  (this->*task)(primaryVertexContext);

  t2 = clock();
  diff = ((float) t2 - (float) t1) / (CLOCKS_PER_SEC / 1000);

  if (taskName == nullptr) {

    ostream << diff << "\t";

  } else {

    ostream << std::setw(2) << " - " << taskName << " completed in: " << diff << "ms" << std::endl;
  }
}

template class CATracker<TRACKINGITSU_GPU_MODE> ;
