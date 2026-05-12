#pragma once
#include "Types/RayRopeTypes.h"

struct FHitResult;
struct FRayRopeDebugContext;

/**
 * Non-owning view of two adjacent rope nodes.
 *
 * Callers must keep the referenced nodes alive for the duration of the trace or geometry operation.
 */
struct FRayRopeSpan
{
	/** First node in the span; never owned by the span. */
	const FRayRopeNode* StartNode = nullptr;

	/** Second node in the span; never owned by the span. */
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

	/** Treats invalid spans and spans shorter than Epsilon as unusable geometry. */
	bool IsDegenerate(float Epsilon) const
	{
		return !IsValid() || GetLengthSquared() <= FMath::Square(Epsilon);
	}
};

/**
 * External trace policy used to build reusable rope trace contexts.
 */
struct FRayRopeTraceSettings
{
	/** World used for all line traces and overlap probes. */
	UWorld* World = nullptr;

	/** Component owner ignored by default so the rope does not collide with its controller. */
	const AActor* OwnerActor = nullptr;

	/** Collision channel used for rope queries. */
	ECollisionChannel TraceChannel = ECC_Visibility;

	/** Whether rope queries should use complex collision. */
	bool bTraceComplex = false;

	/** Optional tick-local diagnostics sink. */
	FRayRopeDebugContext* DebugContext = nullptr;
};

/**
 * Prepared query state shared by a solver pass.
 */
struct FRayRopeTraceContext
{
	/** World used for all line traces and overlap probes. */
	UWorld* World = nullptr;

	/** Collision channel used for rope queries. */
	ECollisionChannel TraceChannel = ECC_Visibility;

	/** Query params with the owner and any pass-specific ignored actors already applied. */
	FCollisionQueryParams QueryParams;

	/** Optional tick-local diagnostics sink. */
	FRayRopeDebugContext* DebugContext = nullptr;
};

/**
 * Collision helpers for rope traces and free-point probes.
 *
 * Span traces ignore anchor endpoint actors so anchors do not block the rope attached to them.
 */
struct FRayRopeTrace
{
	/**
	 * Builds a reusable context and appends the owner ignore and trace complexity policy.
	 */
	static FRayRopeTraceContext MakeTraceContext(
		const FRayRopeTraceSettings& TraceSettings,
		FCollisionQueryParams QueryParams);

	/**
	 * Traces a span while ignoring valid anchor actors at the span endpoints.
	 *
	 * Returns false for invalid spans, degenerate traces, missing worlds, and unusable initial hits.
	 */
	static bool TryTraceSpan(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeSpan& Span,
		FHitResult& SurfaceHit);

	static bool HasBlockingSpanHit(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeSpan& Span);

	/**
	 * Traces between two world locations using the context query params.
	 *
	 * Initial hits are accepted only when the trace is entering the surface; otherwise a reverse trace
	 * is attempted to avoid treating an exit from penetration as a valid wrap surface.
	 */
	static bool TryTraceBlockingHit(
		const FRayRopeTraceContext& TraceContext,
		const FVector& StartLocation,
		const FVector& EndLocation,
		FHitResult& SurfaceHit);

	static bool HasBlockingHit(
		const FRayRopeTraceContext& TraceContext,
		const FVector& StartLocation,
		const FVector& EndLocation);

	/**
	 * Copies a trace context and adds valid anchor endpoint actors to its ignore list.
	 */
	static FRayRopeTraceContext MakeTraceContextIgnoringEndpointActors(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeNode* StartNode,
		const FRayRopeNode* EndNode);

	static bool IsPointOverlappingGeometry(
		const FRayRopeTraceContext& TraceContext,
		const FVector& WorldLocation,
		float ProbeRadius = KINDA_SMALL_NUMBER);

	/**
	 * Returns true when a node overlaps blocking geometry, ignoring its valid anchor actor when needed.
	 */
	static bool IsNodeOverlappingGeometry(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeNode& Node,
		float ProbeRadius = KINDA_SMALL_NUMBER);

	static bool IsValidFreePoint(
		const FRayRopeTraceContext& TraceContext,
		const FVector& WorldLocation,
		float ProbeRadius = KINDA_SMALL_NUMBER);

	/**
	 * Returns true when the node location is finite and does not overlap blocking geometry.
	 */
	static bool IsValidFreeNode(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeNode& Node,
		float ProbeRadius = KINDA_SMALL_NUMBER);
};
