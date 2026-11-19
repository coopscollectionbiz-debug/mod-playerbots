/*
 * Pure 2D geometry helpers for Ulduar bot strategies.
 * Dependency-free (only <cmath>) so it can be unit-tested standalone.
 */
#ifndef PLAYERBOTS_ULDGEOMETRY_H
#define PLAYERBOTS_ULDGEOMETRY_H

#include <cmath>

// Perpendicular distance (2D, ignoring Z) from point P to the segment AB, clamped to the segment
// (not the infinite line). Writes the closest point on the segment into footX/footY.
inline float UldDistancePointToSegment2D(float px, float py, float ax, float ay, float bx, float by,
                                         float& footX, float& footY)
{
    float const abx = bx - ax;
    float const aby = by - ay;
    float const apx = px - ax;
    float const apy = py - ay;
    float const abLenSq = abx * abx + aby * aby;
    float t = abLenSq > 0.0f ? (apx * abx + apy * aby) / abLenSq : 0.0f;
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;
    footX = ax + t * abx;
    footY = ay + t * aby;
    float const dx = px - footX;
    float const dy = py - footY;
    return std::sqrt(dx * dx + dy * dy);
}

// Inclusive axis-aligned 2D rectangle containment.
inline bool UldPointInRect2D(float x, float y, float minX, float maxX, float minY, float maxY)
{
    return x >= minX && x <= maxX && y >= minY && y <= maxY;
}

#endif  // PLAYERBOTS_ULDGEOMETRY_H
