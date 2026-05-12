#include "RayRopeMoveSolverGlobalInternal.h"

namespace RayRopeMoveSolverPrivate
{
namespace
{
bool AreAffectedNodesFree(
	const FMoveSolveContext& SolveContext,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<FVector> NodeLocations,
	float Alpha)
{
	for (const FGlobalMoveNodeState& State : States)
	{
		if (FMath::Abs(State.DeltaParameter * Alpha) < SolveContext.GeometryTolerance)
		{
			continue;
		}

		if (!IsValidViewIndex(NodeLocations, State.NodeIndex) ||
			!FRayRopeTrace::IsValidFreePoint(
				SolveContext.TraceContext,
				NodeLocations[State.NodeIndex]))
		{
			return false;
		}
	}

	return true;
}

bool AreAffectedSpansSeparated(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TConstArrayView<FVector> NodeLocations,
	int32 FirstSpanIndex,
	int32 LastSpanIndex)
{
	const int32 MaxSpanIndex = Segment.Nodes.Num() - 2;
	for (int32 SpanIndex = FMath::Max(0, FirstSpanIndex);
		SpanIndex <= FMath::Min(MaxSpanIndex, LastSpanIndex);
		++SpanIndex)
	{
		if (!IsValidViewIndex(NodeLocations, SpanIndex) ||
			!IsValidViewIndex(NodeLocations, SpanIndex + 1))
		{
			return false;
		}

		const FVector& StartLocation = NodeLocations[SpanIndex];
		const FVector& EndLocation = NodeLocations[SpanIndex + 1];

		if (StartLocation.ContainsNaN() ||
			EndLocation.ContainsNaN() ||
			FVector::DistSquared(StartLocation, EndLocation) <= SolveContext.MinNodeSeparationSquared)
		{
			return false;
		}
	}

	return true;
}

bool IsSpanClear(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TConstArrayView<FVector> NodeLocations,
	int32 SpanIndex)
{
	if (!IsValidViewIndex(NodeLocations, SpanIndex) ||
		!IsValidViewIndex(NodeLocations, SpanIndex + 1))
	{
		return false;
	}

	FRayRopeNode StartNode = Segment.Nodes[SpanIndex];
	FRayRopeNode EndNode = Segment.Nodes[SpanIndex + 1];
	StartNode.WorldLocation = NodeLocations[SpanIndex];
	EndNode.WorldLocation = NodeLocations[SpanIndex + 1];

	const FRayRopeSpan Span{&StartNode, &EndNode};
	return !FRayRopeTrace::HasBlockingSpanHit(SolveContext.TraceContext, Span);
}

bool AreAffectedSpansClear(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TConstArrayView<FVector> NodeLocations,
	int32 FirstSpanIndex,
	int32 LastSpanIndex)
{
	const int32 MaxSpanIndex = Segment.Nodes.Num() - 2;
	for (int32 SpanIndex = FMath::Max(0, FirstSpanIndex);
		SpanIndex <= FMath::Min(MaxSpanIndex, LastSpanIndex);
		++SpanIndex)
	{
		if (!IsSpanClear(
			SolveContext,
			Segment,
			NodeLocations,
			SpanIndex))
		{
			return false;
		}
	}

	return true;
}

bool IsBatchMoveClearForLocations(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<FVector> NodeLocations,
	int32 FirstSpanIndex,
	int32 LastSpanIndex,
	float Alpha)
{
	return AreAffectedNodesFree(
			SolveContext,
			States,
			NodeLocations,
			Alpha) &&
		AreAffectedSpansSeparated(
			SolveContext,
			Segment,
			NodeLocations,
			FirstSpanIndex,
			LastSpanIndex) &&
		AreAffectedSpansClear(
			SolveContext,
			Segment,
			NodeLocations,
			FirstSpanIndex,
			LastSpanIndex);
}
}

bool TryGetAffectedSpanRange(
	TConstArrayView<FGlobalMoveNodeState> States,
	float Alpha,
	float MoveThreshold,
	int32& OutFirstSpanIndex,
	int32& OutLastSpanIndex)
{
	OutFirstSpanIndex = INDEX_NONE;
	OutLastSpanIndex = INDEX_NONE;

	for (const FGlobalMoveNodeState& State : States)
	{
		if (FMath::Abs(State.DeltaParameter * Alpha) < MoveThreshold)
		{
			continue;
		}

		const int32 FirstSpanIndex = State.NodeIndex - 1;
		const int32 LastSpanIndex = State.NodeIndex;
		OutFirstSpanIndex = OutFirstSpanIndex == INDEX_NONE
			? FirstSpanIndex
			: FMath::Min(OutFirstSpanIndex, FirstSpanIndex);
		OutLastSpanIndex = OutLastSpanIndex == INDEX_NONE
			? LastSpanIndex
			: FMath::Max(OutLastSpanIndex, LastSpanIndex);
	}

	return OutFirstSpanIndex != INDEX_NONE && OutFirstSpanIndex <= OutLastSpanIndex;
}

bool IsBatchMoveClearAtAlpha(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	int32 FirstSpanIndex,
	int32 LastSpanIndex,
	float Alpha)
{
	FGlobalMoveLocationBuffer NodeLocations;
	BuildNodeLocationsAtAlpha(
		Segment,
		States,
		NodeToStateIndex,
		Alpha,
		NodeLocations);
	if (!IsBatchMoveClearForLocations(
		SolveContext,
		Segment,
		States,
		NodeLocations,
		FirstSpanIndex,
		LastSpanIndex,
		Alpha))
	{
		return false;
	}

	const int32 SampleCount = SolveContext.MaxEffectivePointSearchIterations;
	for (int32 SampleIndex = 1; SampleIndex <= SampleCount; ++SampleIndex)
	{
		const float SampleAlpha = Alpha * (static_cast<float>(SampleIndex) /
			static_cast<float>(SampleCount + 1));
		BuildNodeLocationsAtAlpha(
			Segment,
			States,
			NodeToStateIndex,
			SampleAlpha,
			NodeLocations);
		if (!IsBatchMoveClearForLocations(
			SolveContext,
			Segment,
			States,
			NodeLocations,
			FirstSpanIndex,
			LastSpanIndex,
			SampleAlpha))
		{
			return false;
		}
	}

	return true;
}
}
