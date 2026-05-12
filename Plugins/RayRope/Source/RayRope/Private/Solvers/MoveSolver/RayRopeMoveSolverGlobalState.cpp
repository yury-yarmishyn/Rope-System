#include "RayRopeMoveSolverGlobalInternal.h"

namespace RayRopeMoveSolverPrivate
{
namespace
{
bool IsUsableRail(const FMoveRail& Rail)
{
	return !Rail.Origin.ContainsNaN() && !Rail.Direction.GetSafeNormal().IsNearlyZero();
}
}

void BuildGlobalMoveNodeStates(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TArray<FGlobalMoveNodeState, TInlineAllocator<32>>& OutStates,
	TArray<int32, TInlineAllocator<64>>& OutNodeToStateIndex)
{
	const int32 NodeCount = Segment.Nodes.Num();
	OutStates.Reset();
	OutNodeToStateIndex.Reset();
	OutNodeToStateIndex.Init(INDEX_NONE, NodeCount);

	for (int32 NodeIndex = 1; NodeIndex < NodeCount - 1; ++NodeIndex)
	{
		const FRayRopeNode& CurrentNode = Segment.Nodes[NodeIndex];
		if (CurrentNode.NodeType != ERayRopeNodeType::Redirect)
		{
			continue;
		}

		const FMoveNodeWindow NodeWindow(
			Segment.Nodes[NodeIndex - 1],
			CurrentNode,
			Segment.Nodes[NodeIndex + 1],
			NodeIndex);

		FMoveRail Rail;
		if (!TryBuildMoveRail(SolveContext, NodeWindow, Rail) ||
			!IsUsableRail(Rail))
		{
			continue;
		}

		FGlobalMoveNodeState State;
		State.NodeIndex = NodeIndex;
		State.StartLocation = CurrentNode.WorldLocation;
		State.Rail = Rail;
		State.Rail.Direction = Rail.Direction.GetSafeNormal();
		OutNodeToStateIndex[NodeIndex] = OutStates.Add(State);
	}
}

FVector GetNodeLocationAtAlpha(
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	int32 NodeIndex,
	float Alpha)
{
	const int32 StateIndex = IsValidViewIndex(NodeToStateIndex, NodeIndex)
		? NodeToStateIndex[NodeIndex]
		: INDEX_NONE;
	if (!IsValidViewIndex(States, StateIndex))
	{
		return Segment.Nodes[NodeIndex].WorldLocation;
	}

	const FGlobalMoveNodeState& State = States[StateIndex];
	return State.StartLocation + State.Rail.Direction * (State.DeltaParameter * Alpha);
}

void BuildNodeLocationsAtAlpha(
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	float Alpha,
	FGlobalMoveLocationBuffer& OutNodeLocations)
{
	OutNodeLocations.Reset(Segment.Nodes.Num());
	OutNodeLocations.Reserve(Segment.Nodes.Num());
	for (int32 NodeIndex = 0; NodeIndex < Segment.Nodes.Num(); ++NodeIndex)
	{
		OutNodeLocations.Add(GetNodeLocationAtAlpha(
			Segment,
			States,
			NodeToStateIndex,
			NodeIndex,
			Alpha));
	}
}

float CalculateSegmentLength(TConstArrayView<FVector> NodeLocations)
{
	float SegmentLength = 0.f;
	for (int32 NodeIndex = 1; NodeIndex < NodeLocations.Num(); ++NodeIndex)
	{
		SegmentLength += FVector::Dist(
			NodeLocations[NodeIndex - 1],
			NodeLocations[NodeIndex]);
	}

	return SegmentLength;
}

float CalculateSegmentLengthAtAlpha(
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	float Alpha)
{
	FGlobalMoveLocationBuffer NodeLocations;
	BuildNodeLocationsAtAlpha(
		Segment,
		States,
		NodeToStateIndex,
		Alpha,
		NodeLocations);
	return CalculateSegmentLength(NodeLocations);
}
}
