/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2012 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "primitiveMesh.H"
#include "demandDrivenData.H"
#include "faceFunctors.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
defineTypeNameAndDebug(primitiveMesh, 0);
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::primitiveMesh::primitiveMesh()
:
    nInternalPoints_(0),    // note: points are considered ordered on empty mesh
    nPoints_(0),
    nInternal0Edges_(-1),
    nInternal1Edges_(-1),
    nInternalEdges_(-1),
    nEdges_(-1),
    nInternalFaces_(0),
    nFaces_(0),
    nCells_(0),

    cellShapesPtr_(NULL),
    edgesPtr_(NULL),
    gpuEdgesPtr_(NULL),
    ccPtr_(NULL),
    ecPtr_(NULL),
    pcPtr_(NULL),
    pcgpuCellsPtr_(NULL),
    pcgpuStartPtr_(NULL),

    cfPtr_(NULL),
    gpuCellDataPtr_(NULL),
    gpuCellFacesPtr_(NULL),
    efPtr_(NULL),
    pfPtr_(NULL),

    cePtr_(NULL),
    fePtr_(NULL),
    pePtr_(NULL),
    ppPtr_(NULL),
    cpPtr_(NULL),

    labels_(0),

    cellCentresPtr_(NULL),
    gpuCellCentresPtr_(NULL),
    faceCentresPtr_(NULL),
    gpuFaceCentresPtr_(NULL),
    cellVolumesPtr_(NULL),
    gpuCellVolumesPtr_(NULL),
    faceAreasPtr_(NULL),
    gpuFaceAreasPtr_(NULL)
{}


// Construct from components
// WARNING: ASSUMES CORRECT ORDERING OF DATA.
Foam::primitiveMesh::primitiveMesh
(
    const label nPoints,
    const label nInternalFaces,
    const label nFaces,
    const label nCells
)
:
    nInternalPoints_(-1),
    nPoints_(nPoints),
    nEdges_(-1),
    nInternalFaces_(nInternalFaces),
    nFaces_(nFaces),
    nCells_(nCells),

    cellShapesPtr_(NULL),
    edgesPtr_(NULL),
    gpuEdgesPtr_(NULL),
    ccPtr_(NULL),
    ecPtr_(NULL),
    pcPtr_(NULL),
    pcgpuCellsPtr_(NULL),
    pcgpuStartPtr_(NULL),

    cfPtr_(NULL),
    gpuCellDataPtr_(NULL),
    gpuCellFacesPtr_(NULL),
    efPtr_(NULL),
    pfPtr_(NULL),

    cePtr_(NULL),
    fePtr_(NULL),
    pePtr_(NULL),
    ppPtr_(NULL),
    cpPtr_(NULL),

    labels_(0),

    cellCentresPtr_(NULL),
    gpuCellCentresPtr_(NULL),
    faceCentresPtr_(NULL),
    gpuFaceCentresPtr_(NULL),
    cellVolumesPtr_(NULL),
    gpuCellVolumesPtr_(NULL),
    faceAreasPtr_(NULL),
    gpuFaceAreasPtr_(NULL)
{}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::primitiveMesh::~primitiveMesh()
{
    clearOut();
}


// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

bool Foam::primitiveMesh::calcPointOrder
(
    label& nInternalPoints,
    labelList& oldToNew,
    const faceList& faces,
    const label nInternalFaces,
    const label nPoints
)
{
    // Internal points are points that are not used by a boundary face.

    // Map from old to new position
    oldToNew.setSize(nPoints);
    oldToNew = -1;


    // 1. Create compact addressing for boundary points. Start off by indexing
    // from 0 inside oldToNew. (shifted up later on)

    label nBoundaryPoints = 0;
    for (label faceI = nInternalFaces; faceI < faces.size(); faceI++)
    {
        const face& f = faces[faceI];

        forAll(f, fp)
        {
            label pointI = f[fp];

            if (oldToNew[pointI] == -1)
            {
                oldToNew[pointI] = nBoundaryPoints++;
            }
        }
    }

    // Now we know the number of boundary and internal points

    nInternalPoints = nPoints - nBoundaryPoints;

    // Move the boundary addressing up
    forAll(oldToNew, pointI)
    {
        if (oldToNew[pointI] != -1)
        {
            oldToNew[pointI] += nInternalPoints;
        }
    }


    // 2. Compact the internal points. Detect whether internal and boundary
    // points are mixed.

    label internalPointI = 0;

    bool ordered = true;

    for (label faceI = 0; faceI < nInternalFaces; faceI++)
    {
        const face& f = faces[faceI];

        forAll(f, fp)
        {
            label pointI = f[fp];

            if (oldToNew[pointI] == -1)
            {
                if (pointI >= nInternalPoints)
                {
                    ordered = false;
                }
                oldToNew[pointI] = internalPointI++;
            }
        }
    }

    return ordered;
}


void Foam::primitiveMesh::reset
(
    const label nPoints,
    const label nInternalFaces,
    const label nFaces,
    const label nCells
)
{
    clearOut();

    nPoints_ = nPoints;
    nEdges_ = -1;
    nInternal0Edges_ = -1;
    nInternal1Edges_ = -1;
    nInternalEdges_ = -1;

    nInternalFaces_ = nInternalFaces;
    nFaces_ = nFaces;
    nCells_ = nCells;

    // Check if points are ordered
    label nInternalPoints;
    labelList pointMap;

    bool isOrdered = calcPointOrder
    (
        nInternalPoints,
        pointMap,
        faces(),
        nInternalFaces_,
        nPoints_
    );

    if (isOrdered)
    {
        nInternalPoints_ = nInternalPoints;
    }
    else
    {
        nInternalPoints_ = -1;
    }

    if (debug)
    {
        Pout<< "primitiveMesh::reset : mesh reset to"
            << " nInternalPoints:" << nInternalPoints_
            << " nPoints:" << nPoints_
            << " nEdges:" << nEdges_
            << " nInternalFaces:" << nInternalFaces_
            << " nFaces:" << nFaces_
            << " nCells:" << nCells_
            << endl;
    }
}


void Foam::primitiveMesh::reset
(
    const label nPoints,
    const label nInternalFaces,
    const label nFaces,
    const label nCells,
    cellList& clst
)
{
    reset
    (
        nPoints,
        nInternalFaces,
        nFaces,
        nCells
    );

    cfPtr_ = new cellList(clst, true);
}


void Foam::primitiveMesh::reset
(
    const label nPoints,
    const label nInternalFaces,
    const label nFaces,
    const label nCells,
    const Xfer<cellList>& clst
)
{
    reset
    (
        nPoints,
        nInternalFaces,
        nFaces,
        nCells
    );

    cfPtr_ = new cellList(clst);
}


Foam::tmp<Foam::scalargpuField> Foam::primitiveMesh::movePoints
(
    const pointgpuField& newPoints,
    const pointgpuField& oldPoints
)
{
    if (newPoints.size() <  nPoints() || oldPoints.size() < nPoints())
    {
        FatalErrorIn
        (
            "primitiveMesh::movePoints(const pointField& newPoints, "
            "const pointField& oldPoints)"
        )   << "Cannot move points: size of given point list smaller "
            << "than the number of active points"
            << abort(FatalError);
    }

    // Create swept volumes
    const faceDatagpuList& f = getFaces();
    const labelgpuList& nodes = getFaceNodes();

    tmp<scalargpuField> tsweptVols(new scalargpuField(f.size()));
    scalargpuField& sweptVols = tsweptVols();

    thrust::transform
    (
         f.begin(),
         f.end(),
         sweptVols.begin(),
         faceSweptVolFunctor
         (
             nodes.data(),
             oldPoints.data(),
             newPoints.data()
         )          
    );

    // Force recalculation of all geometric data with new points
    clearGeom();

    return tsweptVols;
}


const Foam::cellShapeList& Foam::primitiveMesh::cellShapes() const
{
    if (!cellShapesPtr_)
    {
        calcCellShapes();
    }

    return *cellShapesPtr_;
}


// ************************************************************************* //
