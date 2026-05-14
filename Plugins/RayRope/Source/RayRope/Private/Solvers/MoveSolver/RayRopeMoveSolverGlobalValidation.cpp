#include "RayRopeMoveSolverGlobalInternal.h"

namespace RayRopeMoveSolverPrivate
{
namespace
{
bool AreAffectedNodesFree(
	const FMoveSolveContext& SolveContext,
	TConstArrayView<FGlobalMoveNodeState> States,
	float Alpha)
{
	for (const FGlobalMoveNodeState& State : States)
	{
		if (FMath::Abs(State.DeltaParameter * Alpha) < SolveContext.GeometryTolerance)
		{
			continue;
		}

		const FVector NodeLocation =
			State.StartLocation + State.Rail.Direction * (State.DeltaParameter * Alpha);
		if (NodeLocation.ContainsNaN() ||
			!FRayRopeTrace::IsValidFreePoint(
				SolveContext.TraceContext,
				NodeLocation))
		{
			return false;
		}
	}

	return true;
}

bool TryGetSpanLocationsAtAlpha(
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	int32 SpanIndex,
	float Alpha,
	FVector& OutStartLocation,
	FVector& OutEndLocation)
{
	if (!Segment.Nodes.IsValidIndex(SpanIndex) ||
		!Segment.Nodes.IsValidIndex(SpanIndex + 1))
	{
		return false;
	}

	OutStartLocation = GetNodeLocationAtAlpha(
		Segment,
		States,
		NodeToStateIndex,
		SpanIndex,
		Alpha);
	OutEndLocation = GetNodeLocationAtAlpha(
		Segment,
		States,
		NodeToStateIndex,
		SpanIndex + 1,
		Alpha);
	return !OutStartLocation.ContainsNaN() && !OutEndLocation.ContainsNaN();
}

bool AreAffectedSpansValid(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	TConstArrayView<FRayRopeSpanIndexRange> AffectedSpanRanges,
	float Alpha)
{
	const int32 MaxSpanIndex = Segment.Nodes.Num() - 2;
	for (const FRayRopeSpanIndexRange& Range : AffectedSpanRanges)
	{
		for (int32 SpanIndex = FMath::Max(0, Range.FirstSpanIndex);
			SpanIndex <= FMath::Min(MaxSpanIndex, Range.LastSpanIndex);
			++SpanIndex)
		{
			FVector StartLocation = FVector::ZeroVector;
			FVector EndLocation = FVector::ZeroVector;
			if (!TryGetSpanLocationsAtAlpha(
					Segment,
					States,
					NodeToStateIndex,
					SpanIndex,
					Alpha,
					StartLocation,
					EndLocation))
			{
				return false;
			}

			if (FVector::DistSquared(StartLocation, EndLocation) <=
				SolveContext.MinNodeSeparationSquared)
			{
				return false;
			}

			FRayRopeNode StartNode = Segment.Nodes[SpanIndex];
			FRayRopeNode EndNode = Segment.Nodes[SpanIndex + 1];
			StartNode.WorldLocation = StartLocation;
			EndNode.WorldLocation = EndLocation;

			const FRayRopeSpan Span{&StartNode, &EndNode};
			if (FRayRopeTrace::HasBlockingSpanHit(SolveContext.TraceContext, Span))
			{
				return false;
			}
		}
	}

	return true;
}

bool IsBatchMoveClearForAlpha(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	TConstArrayView<FRayRopeSpanIndexRange> AffectedSpanRanges,
	float Alpha)
{
	return AreAffectedNodesFree(
			SolveContext,
			States,
			Alpha) &&
		AreAffectedSpansValid(
			SolveContext,
			Segment,
			States,
			NodeToStateIndex,
			AffectedSpanRanges,
			Alpha);
}
}

bool BuildAffectedSpanRanges(
	TConstArrayView<FGlobalMoveNodeState> States,
	float Alpha,
	float MoveThreshold,
	int32 MaxSpanIndex,
	FRayRopeAffectedSpanRangeBuffer& OutRanges)
{
	OutRanges.Reset();

	for (const FGlobalMoveNodeState& State : States)
	{
		if (FMath::Abs(State.DeltaParameter * Alpha) < MoveThreshold)
		{
			continue;
		}

		FRayRopeSpanIndexRange Range;
		Range.FirstSpanIndex = State.NodeIndex - 1;
		Range.LastSpanIndex = State.NodeIndex;
		FRayRopeSpanIndexRange ClampedRange;
		if (FRayRopeSpanIndexRangeUtils::TryClampRange(
				Range,
				MaxSpanIndex,
				ClampedRange))
		{
			if (OutRanges.Num() > 0 &&
				ClampedRange.FirstSpanIndex <= OutRanges.Last().LastSpanIndex + 1)
			{
				OutRanges.Last().LastSpanIndex = FMath::Max(
					OutRanges.Last().LastSpanIndex,
					ClampedRange.LastSpanIndex);
				continue;
			}

			OutRanges.Add(ClampedRange);
		}
	}

	return OutRanges.Num() > 0;
}

bool IsBatchMoveClearAtAlpha(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	TConstArrayView<FRayRopeSpanIndexRange> AffectedSpanRanges,
	float Alpha)
{
	if (AffectedSpanRanges.Num() == 0)
	{
		return false;
	}

	if (!IsBatchMoveClearForAlpha(
		SolveContext,
		Segment,
		States,
		NodeToStateIndex,
		AffectedSpanRanges,
		Alpha))
	{
		return false;
	}

	const int32 SampleCount = SolveContext.MaxGlobalValidationSamples;
	for (int32 SampleIndex = 1; SampleIndex <= SampleCount; ++SampleIndex)
	{
		const float SampleAlpha = Alpha * (static_cast<float>(SampleIndex) /
			static_cast<float>(SampleCount + 1));
		if (!IsBatchMoveClearForAlpha(
			SolveContext,
			Segment,
			States,
			NodeToStateIndex,
			AffectedSpanRanges,
			SampleAlpha))
		{
			return false;
		}
	}

	return true;
}
}
