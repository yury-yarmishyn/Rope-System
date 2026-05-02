#pragma once

#include "RayRopeInternalTypes.h"

class AActor;

struct FRayRopeRelaxSettings
{
	float RelaxSolverTolerance = 0.f;
	float RelaxCollinearityTolerance = 0.f;
	int32 MaxRelaxCollapseIterations = 8;
};

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

	static bool TryGetNodeSpan(
		TConstArrayView<FRayRopeNode> Nodes,
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

	static bool CanContinuouslyCollapseRelaxNode(
		const FRayRopeTraceContext& TraceContext,
		const FRayRopeRelaxSettings& RelaxSettings,
		const FRayRopeNode& PrevNode,
		const FRayRopeNode& CurrentNode,
		const FRayRopeNode& NextNode,
		const FVector& CollapseTarget);
};
