#pragma once
#include "RayRopeTypes.h"

struct FHitResult;

struct FRayRopeSpan
{
	const FRayRopeNode* StartNode = nullptr;
	const FRayRopeNode* EndNode = nullptr;

	bool IsValid() const
	{
		return StartNode != nullptr && EndNode != nullptr;
	}

	FVector GetStartLocation() const
	{
		return StartNode != nullptr ? StartNode->WorldLocation : FVector::ZeroVector;
	}

	FVector GetEndLocation() const
	{
		return EndNode != nullptr ? EndNode->WorldLocation : FVector::ZeroVector;
	}

	FVector GetDirection() const
	{
		return IsValid() ? GetEndLocation() - GetStartLocation() : FVector::ZeroVector;
	}

	float GetLengthSquared() const
	{
		return GetDirection().SizeSquared();
	}

	bool IsDegenerate(float Epsilon) const
	{
		return !IsValid() || GetLengthSquared() <= FMath::Square(Epsilon);
	}
};

struct FRayRopeTraceSettings
{
	UWorld* World = nullptr;
	const AActor* OwnerActor = nullptr;
	ECollisionChannel TraceChannel = ECC_Visibility;
	bool bTraceComplex = false;
};

struct FRayRopeTraceContext
{
	UWorld* World = nullptr;
	ECollisionChannel TraceChannel = ECC_Visibility;
	FCollisionQueryParams QueryParams;
};

struct FRayRopeTrace
{
	static FRayRopeTraceContext MakeTraceContext(
		const FRayRopeTraceSettings& TraceSettings,
		FCollisionQueryParams QueryParams);

	static bool TryTraceSpan(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeSpan& Span,
		FHitResult& SurfaceHit);

	static bool HasBlockingSpanHit(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeSpan& Span);

	static bool TryTraceBlockingHit(
		const FRayRopeTraceContext& TraceContext,
		const FVector& StartLocation,
		const FVector& EndLocation,
		FHitResult& SurfaceHit);

	static bool HasBlockingHit(
		const FRayRopeTraceContext& TraceContext,
		const FVector& StartLocation,
		const FVector& EndLocation);

	static FRayRopeTraceContext MakeTraceContextIgnoringEndpointActors(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeNode* StartNode,
		const FRayRopeNode* EndNode);

	static bool IsPointOverlappingGeometry(
		const FRayRopeTraceContext& TraceContext,
		const FVector& WorldLocation,
		float ProbeRadius = KINDA_SMALL_NUMBER);

	static bool IsNodeOverlappingGeometry(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeNode& Node,
		float ProbeRadius = KINDA_SMALL_NUMBER);

	static bool IsValidFreePoint(
		const FRayRopeTraceContext& TraceContext,
		const FVector& WorldLocation,
		float ProbeRadius = KINDA_SMALL_NUMBER);

	static bool IsValidFreeNode(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeNode& Node,
		float ProbeRadius = KINDA_SMALL_NUMBER);
};
