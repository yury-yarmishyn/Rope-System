#pragma once

#include "RayRopeTrace.h"

struct FHitResult;

struct FRayRopeSurfaceGeometry
{
	static bool AreDirectionsNearlyCollinear(
		const FVector& FirstDirection,
		const FVector& SecondDirection,
		float Epsilon);

	static FVector CalculateRedirectLocation(
		const FRayRopeSpan& ValidSpan,
		const FHitResult& FrontSurfaceHit,
		const FHitResult* BackSurfaceHit);

	static FVector CalculateSurfaceOffsetDirection(
		const FHitResult& FrontSurfaceHit,
		const FHitResult* BackSurfaceHit);

private:
	static FVector CalculateProjectedPointOnHitPlane(
		const FRayRopeSpan& ValidSpan,
		const FHitResult& SurfaceHit);

	static bool TryGetPlaneIntersectionLine(
		const FHitResult& FrontSurfaceHit,
		const FHitResult& BackSurfaceHit,
		FVector& OutLinePoint,
		FVector& OutLineDirection);

	static FVector FindClosestPointOnLineToClampedSegment(
		const FVector& SegmentStart,
		const FVector& SegmentEnd,
		const FVector& LinePoint,
		const FVector& LineDirection);
};
