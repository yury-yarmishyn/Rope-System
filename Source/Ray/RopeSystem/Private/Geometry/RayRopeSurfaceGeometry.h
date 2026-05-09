#pragma once

#include "Trace/RayRopeTrace.h"

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
};
