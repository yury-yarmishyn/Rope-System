#pragma once

#include "RayRopeInternalTypes.h"

struct FHitResult;

struct FRayRopeTrace
{
	static FRayRopeTraceContext MakeTraceContext(
		const FRayRopeTraceSettings& TraceSettings,
		FCollisionQueryParams QueryParams);

	static bool TryTraceSpan(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeSpan& Span,
		FHitResult& SurfaceHit);

	static bool TryTraceBlockingHit(
		const FRayRopeTraceContext& TraceContext,
		const FVector& StartLocation,
		const FVector& EndLocation,
		FHitResult& SurfaceHit);

	static bool IsPointInsideGeometry(
		const FRayRopeTraceContext& TraceContext,
		const FVector& WorldLocation,
		float ProbeRadius = KINDA_SMALL_NUMBER);

	static bool IsNodeInsideGeometry(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeNode& Node,
		float ProbeRadius = KINDA_SMALL_NUMBER);

	static bool CanUseHitForRedirectWrap(
		const FHitResult& SurfaceHit,
		bool bAllowWrapOnMovableObjects);

	static bool IsTraceEnteringHitSurface(
		const FVector& StartLocation,
		const FVector& EndLocation,
		const FHitResult& SurfaceHit);

private:
	static void BuildTraceQueryParams(
		const FRayRopeTraceSettings& TraceSettings,
		FCollisionQueryParams& QueryParams);
};
