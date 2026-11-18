/*
 * Copyright 2026 Uber Technologies, Inc.
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
/** @file
 * @brief Benchmarks for geoPolygonAreaRads2 on many-vertex polygons.
 */

#include <math.h>

#include "area.h"
#include "benchmark.h"
#include "h3api.h"

// The existing area benchmarks only exercise six-vertex cells, but the
// per-vertex work in the area computation scales with the number of
// polygon vertices. These fixtures use densified circles of increasing
// vertex counts, a wide polygon that crosses the antimeridian, and a
// polygon with a hole.
#define SMALL_NUM_VERTS 100
#define MID_NUM_VERTS 1000
#define LARGE_NUM_VERTS 4000

static void makeCircle(LatLng *verts, int numVerts, double centerLat,
                       double centerLng, double radius) {
    for (int i = 0; i < numVerts; i++) {
        double theta = 2.0 * M_PI * i / numVerts;
        verts[i].lat = centerLat + radius * sin(theta);
        verts[i].lng = centerLng + radius * cos(theta) / cos(centerLat);
    }
}

LatLng smallVerts[SMALL_NUM_VERTS];
LatLng midVerts[MID_NUM_VERTS];
LatLng largeVerts[LARGE_NUM_VERTS];
LatLng holeVerts[MID_NUM_VERTS];
LatLng wideVerts[LARGE_NUM_VERTS];

GeoLoop smallGeoLoop;
GeoLoop midGeoLoop;
GeoLoop largeGeoLoop;
GeoLoop holeGeoLoop;
GeoLoop wideGeoLoop;

GeoPolygon smallGeoPolygon;
GeoPolygon midGeoPolygon;
GeoPolygon largeGeoPolygon;
GeoPolygon holeGeoPolygon;
GeoPolygon wideGeoPolygon;

BEGIN_BENCHMARKS();

// All circles are centered on San Francisco with the same geographic
// extent, so only the vertex count differs between fixtures.
{
    const double centerLat = 0.659966917655;
    const double centerLng = -2.1364398519396;
    const double radius = 300.0 / 6371.0;
    makeCircle(smallVerts, SMALL_NUM_VERTS, centerLat, centerLng, radius);
    makeCircle(midVerts, MID_NUM_VERTS, centerLat, centerLng, radius);
    makeCircle(largeVerts, LARGE_NUM_VERTS, centerLat, centerLng, radius);
    makeCircle(holeVerts, MID_NUM_VERTS, centerLat, centerLng, radius / 2.0);
}

// A wide "stadium" spanning more than 180 degrees of longitude, so the
// vertex chain crosses the antimeridian.
{
    for (int i = 0; i < LARGE_NUM_VERTS; i++) {
        double theta = 2.0 * M_PI * i / LARGE_NUM_VERTS;
        wideVerts[i].lat = 0.5 + 0.5 * sin(theta);
        wideVerts[i].lng = 3.2 * cos(theta);
    }
}

smallGeoLoop.numVerts = SMALL_NUM_VERTS;
smallGeoLoop.verts = smallVerts;
smallGeoPolygon.geoloop = smallGeoLoop;

midGeoLoop.numVerts = MID_NUM_VERTS;
midGeoLoop.verts = midVerts;
midGeoPolygon.geoloop = midGeoLoop;

largeGeoLoop.numVerts = LARGE_NUM_VERTS;
largeGeoLoop.verts = largeVerts;
largeGeoPolygon.geoloop = largeGeoLoop;

holeGeoLoop.numVerts = MID_NUM_VERTS;
holeGeoLoop.verts = holeVerts;
holeGeoPolygon.geoloop = largeGeoLoop;
holeGeoPolygon.numHoles = 1;
holeGeoPolygon.holes = &holeGeoLoop;

wideGeoLoop.numVerts = LARGE_NUM_VERTS;
wideGeoLoop.verts = wideVerts;
wideGeoPolygon.geoloop = wideGeoLoop;

double area;

BENCHMARK(polygonAreaCircle100Verts, 100000, {
    geoPolygonAreaRads2(smallGeoPolygon, &area);
});

BENCHMARK(polygonAreaCircle1000Verts, 10000, {
    geoPolygonAreaRads2(midGeoPolygon, &area);
});

BENCHMARK(polygonAreaCircle4000Verts, 5000, {
    geoPolygonAreaRads2(largeGeoPolygon, &area);
});

BENCHMARK(polygonAreaWide4000Verts, 5000, {
    geoPolygonAreaRads2(wideGeoPolygon, &area);
});

BENCHMARK(polygonAreaCircle4000WithHole1000, 5000, {
    geoPolygonAreaRads2(holeGeoPolygon, &area);
});

END_BENCHMARKS();
