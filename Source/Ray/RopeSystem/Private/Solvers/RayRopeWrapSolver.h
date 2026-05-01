#pragma once

#include "RayRopeInternalTypes.h"

class AActor;

struct FRayWrapRedirectInput
{
	FRayRopeSpan ValidSpan;
	FHitResult FrontSurfaceHit;
	FHitResult BackSurfaceHit;
	bool bHasBackSurfaceHit = false;

	const FHitResult* GetBackSurfaceHitPtr() const
	{
		return bHasBackSurfaceHit ? &BackSurfaceHit : nullptr;
	}
};

using FRayRopeWrapNodeBuffer = TArray<FRayRopeNode, TInlineAllocator<2>>;
using FRayRopePendingInsertionBuffer = TArray<TPair<int32, FRayRopeNode>, TInlineAllocator<8>>;

struct FRayRopeWrapSolver
{
	static void WrapSegment(
		const FRayRopeTraceSettings& TraceSettings,
		const FRayRopeWrapSettings& WrapSettings,
		FRayRopeSegment& Segment,
		const FRayRopeSegment& ReferenceSegment);

private:
	static bool BuildWrapNodes(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeWrapSettings& WrapSettings,
		int32 NodeIndex,
		TConstArrayView<FRayRopeNode> CurrentNodes,
		TConstArrayView<FRayRopeNode> ReferenceNodes,
		FRayRopeWrapNodeBuffer& OutNodes);

	static bool TryBuildWrapRedirectInputs(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeWrapSettings& WrapSettings,
		const FRayRopeSpan& CurrentSpan,
		const FRayRopeSpan& ReferenceSpan,
		const FHitResult& FrontSurfaceHit,
		FRayWrapRedirectInput& OutInput);

	static bool TryFindBoundaryHit(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeWrapSettings& WrapSettings,
		const FRayRopeSpan& ValidSpan,
		const FRayRopeSpan& InvalidSpan,
		FHitResult& SurfaceHit);

	static FRayRopeNode CreateRedirectNode(
		const FRayRopeWrapSettings& WrapSettings,
		const FRayRopeSpan& ValidSpan,
		const FHitResult& FrontSurfaceHit,
		const FHitResult* BackSurfaceHit);

	static void AppendRedirectNodes(
		const FRayRopeWrapSettings& WrapSettings,
		const FRayWrapRedirectInput& RedirectInput,
		FRayRopeWrapNodeBuffer& OutNodes);

	static AActor* ResolveRedirectAttachActor(
		const FHitResult& FrontSurfaceHit,
		const FHitResult* BackSurfaceHit);

	static bool CanInsertWrapNode(
		const FRayRopeWrapSettings& WrapSettings,
		int32 InsertIndex,
		const FRayRopeSegment& Segment,
		const FRayRopeNode& Candidate,
		const FRayRopePendingInsertionBuffer& PendingInsertions);

	static bool AreEquivalentWrapNodes(
		const FRayRopeWrapSettings& WrapSettings,
		const FRayRopeNode& FirstNode,
		const FRayRopeNode& SecondNode);
};
