/// \file CAGPUPrimaryVertexContext.cxx
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

#include "CAGPUPrimaryVertexContext.h"

#include <sstream>

namespace {
__device__ void fillIndexTables(CAGPUPrimaryVertexContext &primaryVertexContext, const int layerIndex) {

  const int currentClusterIndex { static_cast<int>(blockDim.x * blockIdx.x + threadIdx.x) };
  const int nextLayerClustersNum { static_cast<int>(primaryVertexContext.getClusters()[layerIndex + 1].size()) };

  if(currentClusterIndex < nextLayerClustersNum) {

    const int currentBinIndex { primaryVertexContext.getClusters()[layerIndex + 1][currentClusterIndex].indexTableBinIndex };
    int previousBinIndex;

    if(currentClusterIndex == 0) {

      primaryVertexContext.getIndexTables()[layerIndex][0] = 0;
      previousBinIndex = 0;

    } else {

      previousBinIndex = primaryVertexContext.getClusters()[layerIndex + 1][currentClusterIndex - 1].indexTableBinIndex;
    }

    if (currentBinIndex > previousBinIndex) {

      for (int iBin { previousBinIndex + 1 }; iBin <= currentBinIndex; ++iBin) {

        primaryVertexContext.getIndexTables()[layerIndex][iBin] = currentClusterIndex;
      }

      previousBinIndex = currentBinIndex;
    }

    if(currentClusterIndex == nextLayerClustersNum - 1) {

      for (int iBin { currentBinIndex + 1 }; iBin <= CAConstants::IndexTable::ZBins * CAConstants::IndexTable::PhiBins;
            iBin++) {

      primaryVertexContext.getIndexTables()[layerIndex][iBin] = nextLayerClustersNum;
    }
  }
  }
}

__device__ void fillTrackletsLookupTables(CAGPUPrimaryVertexContext &primaryVertexContext, const int layerIndex)
{
  const int currentBinIndex { static_cast<int>(blockDim.x * blockIdx.x + threadIdx.x) };
  const int tableSize { static_cast<int>(primaryVertexContext.getClusters()[layerIndex + 1].size()) };

  if(currentBinIndex < tableSize) {

    primaryVertexContext.getTrackletsLookupTable()[layerIndex][currentBinIndex] = CAConstants::ITS::UnusedIndex;
  }
}

__global__ void fillDeviceStructures(CAGPUPrimaryVertexContext &primaryVertexContext, const int layerIndex)
{
  fillIndexTables(primaryVertexContext, layerIndex);
  fillTrackletsLookupTables(primaryVertexContext, layerIndex);
}
}

CAGPUPrimaryVertexContext::CAGPUPrimaryVertexContext(const float3 &primaryVertex,
    const std::array<std::vector<CACluster>, CAConstants::ITS::LayersNumber> &clusters,
    const std::array<std::vector<CACell>, CAConstants::ITS::CellsPerRoad> &cells,
    const std::array<std::vector<int>, CAConstants::ITS::CellsPerRoad - 1> &cellsLookupTable)
    : mPrimaryVertex { primaryVertex }
{
  for (int iLayer { 0 }; iLayer < CAConstants::ITS::LayersNumber; ++iLayer) {

    this->mClusters[iLayer] =
        CAGPUVector<CACluster> { &clusters[iLayer][0], static_cast<int>(clusters[iLayer].size()) };

    if (iLayer < CAConstants::ITS::TrackletsPerRoad) {

      this->mTracklets[iLayer] = CAGPUVector<CATracklet> { static_cast<int>(std::ceil(
          (CAConstants::Memory::TrackletsMemoryCoefficients[iLayer] * clusters[iLayer].size())
              * clusters[iLayer + 1].size())) };
    }

    if (iLayer < CAConstants::ITS::CellsPerRoad) {

      this->mTrackletsLookupTable[iLayer] = CAGPUVector<int> { static_cast<int>(clusters[iLayer + 1].size()) };

      this->mCells[iLayer] = CAGPUVector<CACell> { static_cast<int>(cells[iLayer].capacity()) };
    }

    if (iLayer < CAConstants::ITS::CellsPerRoad - 1) {

      this->mCellsLookupTable[iLayer] = CAGPUVector<int> { &cellsLookupTable[iLayer][0],
          static_cast<int>(cellsLookupTable[iLayer].size()) };
    }
  }
}

GPU_DEVICE const float3& CAGPUPrimaryVertexContext::getPrimaryVertex()
{
  return *mPrimaryVertex;
}

GPU_HOST_DEVICE CAGPUArray<CAGPUVector<CACluster>, CAConstants::ITS::LayersNumber>& CAGPUPrimaryVertexContext::getClusters()
{
  return mClusters;
}

GPU_DEVICE CAGPUArray<CAGPUArray<int, CAConstants::IndexTable::ZBins * CAConstants::IndexTable::PhiBins + 1>,
    CAConstants::ITS::TrackletsPerRoad>& CAGPUPrimaryVertexContext::getIndexTables()
{
  return mIndexTables;
}

GPU_DEVICE CAGPUArray<CAGPUVector<CATracklet>, CAConstants::ITS::TrackletsPerRoad>& CAGPUPrimaryVertexContext::getTracklets()
{
  return mTracklets;
}

GPU_DEVICE CAGPUArray<CAGPUVector<int>, CAConstants::ITS::CellsPerRoad>& CAGPUPrimaryVertexContext::getTrackletsLookupTable()
{
  return mTrackletsLookupTable;
}

GPU_HOST_DEVICE CAGPUArray<CAGPUVector<CACell>, CAConstants::ITS::CellsPerRoad>& CAGPUPrimaryVertexContext::getCells()
{
  return mCells;
}

GPU_HOST_DEVICE CAGPUArray<CAGPUVector<int>, CAConstants::ITS::CellsPerRoad - 1>& CAGPUPrimaryVertexContext::getCellsLookupTable()
{
  return mCellsLookupTable;
}

CAPrimaryVertexContext<true>::CAPrimaryVertexContext(const CAEvent& event, const int primaryVertexIndex)
    : mPrimaryVertex { event.getPrimaryVertex(primaryVertexIndex) }, mClusters {
        CAPrimaryVertexContextInitializer<true>::initClusters(event, primaryVertexIndex) }, mCells {
        CAPrimaryVertexContextInitializer<true>::initCells(event) }, mCellsLookupTable {
        CAPrimaryVertexContextInitializer<true>::initCellsLookupTable(event) }, mGPUContext { mPrimaryVertex, mClusters, mCells,
        mCellsLookupTable }, mGPUContextDevicePointer { mGPUContext }
{
  for(int iLayer { 0 }; iLayer < CAConstants::ITS::TrackletsPerRoad; ++iLayer) {

    const int nextLayerClustersNum = static_cast<int>(mClusters[iLayer + 1].size());

    dim3 threadsPerBlock { CAGPUUtils::Host::getBlockSize(nextLayerClustersNum) };
    dim3 blocksGrid { 1 + nextLayerClustersNum / threadsPerBlock.x };

    cudaStream_t currentStream;
    cudaStreamCreate(&currentStream);

    fillDeviceStructures<<< blocksGrid, threadsPerBlock, 0, currentStream >>>(*mGPUContextDevicePointer, iLayer);

    cudaStreamDestroy(currentStream);

    cudaError_t error = cudaGetLastError();

    if (error != cudaSuccess) {

      std::ostringstream errorString { };
      errorString << __FILE__ << ":" << __LINE__ << " CUDA API returned error [" << cudaGetErrorString(error) << "] (code " << error << ")" << std::endl;

      throw std::runtime_error { errorString.str() };
    }
  }

  cudaDeviceSynchronize();
}

const float3& CAPrimaryVertexContext<true>::getPrimaryVertex()
{
  return mPrimaryVertex;
}

std::array<std::vector<CACluster>, CAConstants::ITS::LayersNumber>& CAPrimaryVertexContext<true>::getClusters()
{
  return mClusters;
}

std::array<std::vector<CATracklet>, CAConstants::ITS::TrackletsPerRoad>& CAPrimaryVertexContext<true>::getTracklets()
{
  return mTracklets;
}

std::array<std::vector<int>, CAConstants::ITS::CellsPerRoad>& CAPrimaryVertexContext<true>::getTrackletsLookupTable()
{
  return mTrackletsLookupTable;
}

std::array<std::vector<CACell>, CAConstants::ITS::CellsPerRoad>& CAPrimaryVertexContext<true>::getCells()
{
  return mCells;
}

std::array<std::vector<int>, CAConstants::ITS::CellsPerRoad - 1>& CAPrimaryVertexContext<true>::getCellsLookupTable()
{
  return mCellsLookupTable;
}

std::array<std::vector<std::vector<int>>, CAConstants::ITS::CellsPerRoad - 1>& CAPrimaryVertexContext<true>::getCellsNeighbours()
{
  return mCellsNeighbours;
}

std::vector<CARoad>& CAPrimaryVertexContext<true>::getRoads()
{
  return mRoads;
}

CAGPUPrimaryVertexContext& CAPrimaryVertexContext<true>::getDeviceContext()
{
  return *mGPUContextDevicePointer;
}

CAGPUArray<CAGPUVector<CACluster>, CAConstants::ITS::LayersNumber>& CAPrimaryVertexContext<true>::getDeviceClusters()
{
  return mGPUContext.getClusters();
}

CAGPUArray<CAGPUVector<CATracklet>, CAConstants::ITS::TrackletsPerRoad>& CAPrimaryVertexContext<true>::getDeviceTracklets()
{
  return mGPUContext.getTracklets();
}

CAGPUArray<CAGPUVector<int>, CAConstants::ITS::CellsPerRoad>& CAPrimaryVertexContext<true>::getDeviceTrackletsLookupTable()
{
  return mGPUContext.getTrackletsLookupTable();
}

CAGPUArray<CAGPUVector<CACell>, CAConstants::ITS::CellsPerRoad>& CAPrimaryVertexContext<true>::getDeviceCells()
{
  return mGPUContext.getCells();
}

CAGPUArray<CAGPUVector<int>, CAConstants::ITS::CellsPerRoad - 1>& CAPrimaryVertexContext<true>::getDeviceCellsLookupTable()
{
  return mGPUContext.getCellsLookupTable();
}
