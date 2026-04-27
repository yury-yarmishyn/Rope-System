#pragma once

#include "CoreMinimal.h"
#include "RayRopeTypes.h"

class AActor;
class UWorld;
struct FCollisionQueryParams;

struct FRayRopeTopology
{
	static bool TryBuildBaseSegments(
		const FRayRopeTraceSettings& TraceSettings,
		const TArray<AActor*>& AnchorActors,
		TArray<FRayRopeSegment>& OutSegments);

	static bool TryGetSegmentSpan(
		const FRayRopeSegment& Segment,
		int32 NodeIndex,
		FRayRopeSpan& OutSpan);

	static float CalculateRopeLength(const TArray<FRayRopeSegment>& Segments);

	static void RelaxSegment(
		const FRayRopeTraceSettings& TraceSettings,
		const FRayRopeRelaxSettings& RelaxSettings,
		FRayRopeSegment& Segment);

	static void SplitSegmentsOnAnchors(TArray<FRayRopeSegment>& Segments);
	static void SplitSegmentOnAnchors(TArray<FRayRopeSegment>& Segments, int32 SegmentIndex);

private:
	static bool CanRemoveRelaxNode(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeRelaxSettings& RelaxSettings,
		const FRayRopeNode& PrevNode,
		const FRayRopeNode& CurrentNode,
		const FRayRopeNode& NextNode);
};
