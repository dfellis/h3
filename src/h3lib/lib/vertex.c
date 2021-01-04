/*
 * Copyright 2020 Uber Technologies, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/** @file  vertex.h
 *  @brief Functions for working with cell vertexes.
 */

#include "vertex.h"

#include <assert.h>
#include <stdbool.h>

#include "algos.h"
#include "baseCells.h"
#include "faceijk.h"
#include "geoCoord.h"
#include "h3Index.h"

#define DIRECTION_INDEX_OFFSET 2

/** @brief Table of direction-to-face mapping for each pentagon
 *
 * Note that faces are in directional order, starting at J_AXES_DIGIT.
 * This table is generated by the generatePentagonDirectionFaces script.
 */
static const PentagonDirectionFaces pentagonDirectionFaces[NUM_PENTAGONS] = {
    {4, {4, 0, 2, 1, 3}},       {14, {6, 11, 2, 7, 1}},
    {24, {5, 10, 1, 6, 0}},     {38, {7, 12, 3, 8, 2}},
    {49, {9, 14, 0, 5, 4}},     {58, {8, 13, 4, 9, 3}},
    {63, {11, 6, 15, 10, 16}},  {72, {12, 7, 16, 11, 17}},
    {83, {10, 5, 19, 14, 15}},  {97, {13, 8, 17, 12, 18}},
    {107, {14, 9, 18, 13, 19}}, {117, {15, 19, 17, 18, 16}},
};

/**
 * Get the number of CCW rotations of the cell's vertex numbers
 * compared to the directional layout of its neighbors.
 * @return Number of CCW rotations for the cell
 */
static int vertexRotations(H3Index cell, bool adjustForPentagon) {
    // Get the face and other info for the origin
    FaceIJK fijk;
    _h3ToFaceIjk(cell, &fijk);
    int baseCell = H3_EXPORT(h3GetBaseCell)(cell);
    int cellLeadingDigit = _h3LeadingNonZeroDigit(cell);

    // get the base cell face
    FaceIJK baseFijk;
    _baseCellToFaceIjk(baseCell, &baseFijk);

    int ccwRot60 = _baseCellToCCWrot60(baseCell, fijk.face);

    if (adjustForPentagon && _isBaseCellPentagon(baseCell)) {
        // Find the appropriate direction-to-face mapping
        PentagonDirectionFaces dirFaces;
        for (int p = 0; p < NUM_PENTAGONS; p++) {
            if (pentagonDirectionFaces[p].baseCell == baseCell) {
                dirFaces = pentagonDirectionFaces[p];
                break;
            }
        }

        // additional CCW rotation for polar neighbors or IK neighbors
        if (fijk.face != baseFijk.face &&
            (_isBaseCellPolarPentagon(baseCell) ||
             fijk.face ==
                 dirFaces.faces[IK_AXES_DIGIT - DIRECTION_INDEX_OFFSET])) {
            ccwRot60 = (ccwRot60 + 1) % 6;
        }

        // Check whether the cell crosses a deleted pentagon subsequence
        if (cellLeadingDigit == JK_AXES_DIGIT &&
            fijk.face ==
                dirFaces.faces[IK_AXES_DIGIT - DIRECTION_INDEX_OFFSET]) {
            // Crosses from JK to IK: Rotate CW
            ccwRot60 = (ccwRot60 + 5) % 6;
        } else if (cellLeadingDigit == IK_AXES_DIGIT &&
                   fijk.face ==
                       dirFaces.faces[JK_AXES_DIGIT - DIRECTION_INDEX_OFFSET]) {
            // Crosses from IK to JK: Rotate CCW
            ccwRot60 = (ccwRot60 + 1) % 6;
        }
    }
    return ccwRot60;
}

/** @brief Hexagon direction to vertex number relationships (same face).
 *         Note that we don't use direction 0 (center).
 */
static const int directionToVertexNumHex[NUM_DIGITS] = {
    INVALID_DIGIT, 3, 1, 2, 5, 4, 0};

/** @brief Pentagon direction to vertex number relationships (same face).
 *         Note that we don't use directions 0 (center) or 1 (deleted K axis).
 */
static const int directionToVertexNumPent[NUM_DIGITS] = {
    INVALID_DIGIT, INVALID_DIGIT, 1, 2, 4, 3, 0};

/**
 * Get the first vertex number for a given direction. The neighbor in this
 * direction is located between this vertex number and the next number in
 * sequence.
 * @returns The number for the first topological vertex, or INVALID_VERTEX_NUM
 *          if the direction is not valid for this cell
 */
int vertexNumForDirection(const H3Index origin, const Direction direction) {
    int isPentagon = H3_EXPORT(h3IsPentagon)(origin);
    // Check for invalid directions
    if (direction == CENTER_DIGIT || direction >= INVALID_DIGIT ||
        (isPentagon && direction == K_AXES_DIGIT))
        return INVALID_VERTEX_NUM;

    // Determine the vertex rotations for this cell
    int rotations = vertexRotations(origin, true);

    // Find the appropriate vertex, rotating CCW if necessary
    if (isPentagon) {
        return (directionToVertexNumPent[direction] + NUM_PENT_VERTS -
                rotations) %
               NUM_PENT_VERTS;
    } else {
        return (directionToVertexNumHex[direction] + NUM_HEX_VERTS -
                rotations) %
               NUM_HEX_VERTS;
    }
}

/** @brief Vertex number to hexagon direction relationships (same face).
 */
static const Direction vertexNumToDirectionHex[NUM_HEX_VERTS] = {
    IJ_AXES_DIGIT, J_AXES_DIGIT,  JK_AXES_DIGIT,
    K_AXES_DIGIT,  IK_AXES_DIGIT, I_AXES_DIGIT};

/** @brief Vertex number to pentagon direction relationships (same face).
 */
static const Direction vertexNumToDirectionPent[NUM_PENT_VERTS] = {
    IJ_AXES_DIGIT, J_AXES_DIGIT, JK_AXES_DIGIT, IK_AXES_DIGIT, I_AXES_DIGIT};

/**
 * Get the direction for a given vertex number. This returns the direction for
 * the neighbor between the given vertex number and the next number in sequence.
 * @returns The direction for this vertex, or INVALID_DIGIT if the vertex
 * number is invalid.
 */
Direction directionForVertexNum(const H3Index origin, const int vertexNum) {
    int isPentagon = H3_EXPORT(h3IsPentagon)(origin);
    // Check for invalid vertexes
    if (vertexNum < 0 ||
        vertexNum > (isPentagon ? NUM_PENT_VERTS : NUM_HEX_VERTS) - 1)
        return INVALID_DIGIT;

    // Determine the vertex rotations for this cell
    int rotations = vertexRotations(origin, true);

    // Find the appropriate direction, rotating CW if necessary
    return isPentagon ? vertexNumToDirectionPent[(vertexNum + rotations) %
                                                 NUM_PENT_VERTS]
                      : vertexNumToDirectionHex[(vertexNum + rotations) %
                                                NUM_HEX_VERTS];
}

/** @brief Lookup table from origin vertex to [leftVertex, rightVertex] (same
 * orientation)
 */
static const int neighborVertexes[NUM_HEX_VERTS][2] = {{4, 2}, {5, 3}, {0, 4},
                                                       {1, 5}, {2, 0}, {3, 1}};

/**
 * Get a single vertex for a given cell, as an H3 index, or
 * H3_NULL if the vertex is invalid
 * @param origin    Cell to get the vertex for
 * @param vertexNum Number (index) of the vertex to calculate
 */
H3Index getCellVertex(H3Index origin, int vertexNum) {
    int originIsPentagon = H3_EXPORT(h3IsPentagon)(origin);
    int originNumVerts = originIsPentagon ? NUM_PENT_VERTS : NUM_HEX_VERTS;

    // Get the left neighbor of the vertex, with its rotations
    Direction left = directionForVertexNum(origin, vertexNum);
    if (left == INVALID_DIGIT) return H3_NULL;
    int rRotations = 0;
    H3Index leftNeighbor = h3NeighborRotations(origin, left, &rRotations);

    // Get the right neighbor of the vertex, with its rotations
    // (Note that vertex - 1 is the left neighbor, because vertex numbers are
    // CCW)
    Direction right = directionForVertexNum(
        origin, (vertexNum - 1 + originNumVerts) % originNumVerts);
    if (right == INVALID_DIGIT) return H3_NULL;
    int lRotations = 0;
    H3Index rightNeighbor = h3NeighborRotations(origin, right, &lRotations);

    // Determine the owner. By convention, this is the cell with the
    // lowest numerical index.
    H3Index owner = origin;
    if (leftNeighbor < owner) owner = leftNeighbor;
    if (rightNeighbor < owner) owner = rightNeighbor;

    // Determine the vertex number for the owner cell
    int ownerVertexNum = vertexNum;

    if (owner != origin) {
        // Test whether we can use the simpler, faster logic
        // TODO: Benchmark whether this actually improves perf
        bool ownerIsPentagon = H3_EXPORT(h3IsPentagon)(owner);
        FaceIJK fijk;
        _h3ToFaceIjk(origin, &fijk);
        int originFace = fijk.face;
        _h3ToFaceIjk(owner, &fijk);
        int ownerFace = fijk.face;
        bool isSimpleCase =
            !ownerIsPentagon && !originIsPentagon && originFace == ownerFace;

        if (owner == leftNeighbor) {
            if (isSimpleCase) {
                ownerVertexNum = neighborVertexes[vertexNum][0];
            } else {
                Direction dir = directionForNeighbor(owner, origin);
                // For the left neighbor, we need the second vertex of the
                // edge, which may involve looping around the vertex nums
                ownerVertexNum = vertexNumForDirection(owner, dir) + 1;
                if ((ownerIsPentagon && ownerVertexNum == NUM_PENT_VERTS) ||
                    ownerVertexNum == NUM_HEX_VERTS) {
                    ownerVertexNum = 0;
                }
            }
        } else if (owner == rightNeighbor) {
            if (isSimpleCase) {
                ownerVertexNum = neighborVertexes[vertexNum][1];
            } else {
                Direction dir = directionForNeighbor(owner, origin);
                ownerVertexNum = vertexNumForDirection(owner, dir);
            }
        }
    }

    // Create the vertex index
    H3Index vertex = owner;
    H3_SET_MODE(vertex, H3_VERTEX_MODE);
    H3_SET_RESERVED_BITS(vertex, ownerVertexNum);

    return vertex;
}

/**
 * Get all vertexes for the given cell
 * @param origin    Cell to get the vertexes for
 * @param vertexes  Array to hold vertex output. Must have length >= 6.
 */
void getCellVertexes(H3Index origin, H3Index* vertexes) {
    // Get all vertexes. If the origin is a pentagon, will fill the final slot
    // with H3_NULL.
    for (int i = 0; i < NUM_HEX_VERTS; i++) {
        vertexes[i] = getCellVertex(origin, i);
    }
}

/**
 * Get the geocoordinates of an H3 vertex
 * @param vertex H3 index describing a vertex
 * @param coord  Output geo coordinate
 */
void vertexToPoint(H3Index vertex, GeoCoord* coord) {
    // Get the vertex number and owner from the vertex
    int vertexNum = H3_GET_RESERVED_BITS(vertex);
    H3Index owner = vertex;
    H3_SET_MODE(owner, H3_HEXAGON_MODE);
    H3_SET_RESERVED_BITS(owner, 0);

    // Get the single vertex from the boundary
    GeoBoundary gb;
    FaceIJK fijk;
    _h3ToFaceIjk(owner, &fijk);
    int res = H3_GET_RESOLUTION(owner);

    if (H3_EXPORT(h3IsPentagon)(owner)) {
        _faceIjkPentToGeoBoundary(&fijk, res, vertexNum, 1, &gb);
    } else {
        _faceIjkToGeoBoundary(&fijk, res, vertexNum, 1, &gb);
    }

    // Copy from boundary to output coord
    *coord = gb.verts[0];
}
