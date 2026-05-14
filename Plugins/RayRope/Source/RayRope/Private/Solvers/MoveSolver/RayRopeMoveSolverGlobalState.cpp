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

FGlobalMoveStateBuildResult BuildGlobalMoveNodeStates(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TArray<FGlobalMoveNodeState, TInlineAllocator<32>>& OutStates,
	TArray<int32, TInlineAllocator<64>>& OutNodeToStateIndex)
{
	FGlobalMoveStateBuildResult Result;
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
		++Result.RedirectCount;

		const FMoveNodeWindow NodeWindow(
			Segment.Nodes[NodeIndex - 1],
			CurrentNode,
			Segment.Nodes[NodeIndex + 1],
			NodeIndex);

		FMoveRail Rail;
		if (!TryBuildMoveRail(SolveContext, NodeWindow, Rail) ||
			!IsUsableRail(Rail))
		{
			Result.bSkippedAnyRedirect = true;
			continue;
		}

		FGlobalMoveNodeState State;
		State.NodeIndex = NodeIndex;
		State.StartLocation = CurrentNode.WorldLocation;
		State.Rail = Rail;
		State.Rail.Direction = Rail.Direction.GetSafeNormal();
		OutNodeToStateIndex[NodeIndex] = OutStates.Add(State);
	}

	Result.StateCount = OutStates.Num();
	return Result;
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

float CalculateSegmentLengthAtAlpha(
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	float Alpha)
{
	float SegmentLength = 0.f;
	if (Segment.Nodes.Num() < 2)
	{
		return SegmentLength;
	}

	FVector PrevLocation = GetNodeLocationAtAlpha(
		Segment,
		States,
		NodeToStateIndex,
		0,
		Alpha);
	for (int32 NodeIndex = 1; NodeIndex < Segment.Nodes.Num(); ++NodeIndex)
	{
		const FVector NextLocation = GetNodeLocationAtAlpha(
			Segment,
			States,
			NodeToStateIndex,
			NodeIndex,
			Alpha);
		SegmentLength += FVector::Dist(PrevLocation, NextLocation);
		PrevLocation = NextLocation;
	}

	return SegmentLength;
}
}
