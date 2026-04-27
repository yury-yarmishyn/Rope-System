#pragma once

#include "CoreMinimal.h"
#include "RayRopeTypes.h"

struct FHitResult;

struct FRayRopeWrapGeometry
{
	static bool AreDirectionsNearlyCollinear(
		const FVector& FirstDirection,
		const FVector& SecondDirection,
		float Epsilon);

	static FVector CalculateRedirectLocation(
		const FRayRopeSpan& ValidSpan,
		const FHitResult& FrontSurfaceHit,
		const FHitResult* BackSurfaceHit);

	static FVector CalculateRedirectOffset(
		const FRayRopeWrapSettings& WrapSettings,
		const FHitResult& FrontSurfaceHit,
		const FHitResult* BackSurfaceHit = nullptr);

private:
	static FVector CalculateProjectedPointOnHitPlane(
		const FRayRopeSpan& ValidSpan,
		const FHitResult& SurfaceHit);

	static bool TryGetPlaneIntersectionLine(
		const FHitResult& FrontSurfaceHit,
		const FHitResult& BackSurfaceHit,
		FVector& OutLinePoint,
		FVector& OutLineDirection);

	static void FindClosestPointsOnSegmentToLine(
		const FVector& SegmentStart,
		const FVector& SegmentEnd,
		const FVector& LinePoint,
		const FVector& LineDirection,
		FVector& OutPointOnSegment,
		FVector& OutPointOnLine,
		float& OutDistanceSquared);
};
