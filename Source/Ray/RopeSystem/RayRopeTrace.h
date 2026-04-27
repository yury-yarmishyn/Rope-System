#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "RayRopeTypes.h"

class AActor;
class UWorld;
struct FCollisionQueryParams;
struct FHitResult;

struct FRayRopeTrace
{
	static void BuildTraceQueryParams(
		const AActor* OwnerActor,
		const FRayRopeSegment& Segment,
		FCollisionQueryParams& QueryParams);

	static bool TryTraceSpan(
		UWorld* World,
		ECollisionChannel TraceChannel,
		const FRayRopeSpan& Span,
		const FCollisionQueryParams& QueryParams,
		FHitResult& SurfaceHit);

	static bool TryTraceBlockingHit(
		UWorld* World,
		ECollisionChannel TraceChannel,
		const FCollisionQueryParams& QueryParams,
		const FVector& StartLocation,
		const FVector& EndLocation,
		FHitResult& SurfaceHit);

	static bool CanUseHitForRedirectWrap(
		const FHitResult& SurfaceHit,
		bool bAllowWrapOnMovableObjects);

	static bool IsTraceEnteringHitSurface(
		const FVector& StartLocation,
		const FVector& EndLocation,
		const FHitResult& SurfaceHit);
};
