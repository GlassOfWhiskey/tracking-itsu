/// \file CALabel.cxx
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

#include "CALabel.h"

CALabel::CALabel(const int monteCarloId, const float transverseMomentum, const float phiCoordinate,
    const float pseudorapidity, const int pdgCode, const int numberOfClusters)
    : monteCarloId { monteCarloId }, transverseMomentum { transverseMomentum }, phiCoordinate { phiCoordinate }, pseudorapidity {
        pseudorapidity }, pdgCode { pdgCode }, numberOfClusters { numberOfClusters }
{
  // TNothing to do
}

std::ostream& operator<<(std::ostream& outputStream, const CALabel& label)
{

  outputStream << label.monteCarloId << "\t" << label.transverseMomentum << "\t" << label.phiCoordinate << "\t"
      << label.pseudorapidity << "\t" << label.pdgCode << "\t" << label.numberOfClusters;

  return outputStream;
}
