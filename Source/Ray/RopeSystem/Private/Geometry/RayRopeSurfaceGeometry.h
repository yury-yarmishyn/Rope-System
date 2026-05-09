#pragma once

#include "Trace/RayRopeTrace.h"

struct FHitResult;

/**
 * Geometry helpers for converting blocking hits into redirect locations.
 */
struct FRayRopeSurfaceGeometry
{
	/**
	 * Compares normalized directions using cross-product magnitude.
	 */
	static bool AreDirectionsNearlyCollinear(
		const FVector& FirstDirection,
		const FVector& SecondDirection,
		float Epsilon);

	/**
	 * Chooses the redirect point implied by one or two hit surfaces.
	 *
	 * With two valid planes, prefers the closest point on their intersection line to the span.
	 * Falls back to the front hit plane projection when the planes are degenerate.
	 */
	static FVector CalculateRedirectLocation(
		const FRayRopeSpan& ValidSpan,
		const FHitResult& FrontSurfaceHit,
		const FHitResult* BackSurfaceHit);

	/**
	 * Returns the direction used to push redirects away from their blocking surface or corner.
	 */
	static FVector CalculateSurfaceOffsetDirection(
		const FHitResult& FrontSurfaceHit,
		const FHitResult* BackSurfaceHit);
};
