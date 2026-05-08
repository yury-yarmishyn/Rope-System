#include "RayRopeTransitionValidator.h"

namespace
{
struct FRayRopeTransitionInterval
{
	float StartAlpha = 0.f;
	float EndAlpha = 1.f;
};

bool IsValidTransition(const FRayRopeNodeTransition& Transition)
{
	return Transition.PrevNode != nullptr &&
		Transition.CurrentNode != nullptr &&
		Transition.NextNode != nullptr &&
		!Transition.TargetLocation.ContainsNaN();
}

FRayRopeNode CreateTransitionCandidateNode(
	const FRayRopeNode& SourceNode,
	const FVector& WorldLocation)
{
	FRayRopeNode CandidateNode = SourceNode;
	CandidateNode.WorldLocation = WorldLocation;
	return CandidateNode;
}

bool AreResultSpansClear(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CandidateNode,
	const FRayRopeNode& NextNode)
{
	const FRayRopeSpan PrevCandidateSpan{&PrevNode, &CandidateNode};
	if (FRayRopeTrace::HasBlockingSpanHit(TraceContext, PrevCandidateSpan))
	{
		return false;
	}

	const FRayRopeSpan CandidateNextSpan{&CandidateNode, &NextNode};
	return !FRayRopeTrace::HasBlockingSpanHit(TraceContext, CandidateNextSpan);
}

bool IsTransitionSampleClear(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeTransition& Transition,
	float Alpha)
{
	const FVector SampleLocation = FMath::Lerp(
		Transition.CurrentNode->WorldLocation,
		Transition.TargetLocation,
		Alpha);
	if (!FRayRopeTrace::IsValidFreePoint(TraceContext, SampleLocation))
	{
		return false;
	}

	const FRayRopeNode SampleNode = CreateTransitionCandidateNode(
		*Transition.CurrentNode,
		SampleLocation);
	return AreResultSpansClear(
		TraceContext,
		*Transition.PrevNode,
		SampleNode,
		*Transition.NextNode);
}

bool IsTransitionNodePathClearUnchecked(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeTransitionValidationSettings& Settings,
	const FRayRopeNodeTransition& Transition)
{
	if (Transition.CurrentNode->WorldLocation.Equals(
		Transition.TargetLocation,
		Settings.SolverTolerance))
	{
		return true;
	}

	const FRayRopeTraceContext MoveTraceContext = FRayRopeTrace::MakeTraceContextIgnoringEndpointActors(
		TraceContext,
		Transition.PrevNode,
		Transition.NextNode);

	return !FRayRopeTrace::HasBlockingHit(
		MoveTraceContext,
		Transition.CurrentNode->WorldLocation,
		Transition.TargetLocation);
}

bool IsContinuousSpanFanClearUnchecked(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeTransitionValidationSettings& Settings,
	const FRayRopeNodeTransition& Transition)
{
	const int32 MaxIterations = FMath::Max(0, Settings.MaxTransitionValidationIterations);
	if (MaxIterations == 0 ||
		Transition.CurrentNode->WorldLocation.Equals(
			Transition.TargetLocation,
			Settings.SolverTolerance))
	{
		return true;
	}

	TArray<FRayRopeTransitionInterval, TInlineAllocator<16>> PendingIntervals;
	PendingIntervals.Add(FRayRopeTransitionInterval{0.f, 1.f});

	int32 NextIntervalIndex = 0;
	for (int32 Iteration = 0;
		Iteration < MaxIterations && PendingIntervals.IsValidIndex(NextIntervalIndex);
		++Iteration)
	{
		const FRayRopeTransitionInterval Interval = PendingIntervals[NextIntervalIndex++];
		const float MidAlpha = (Interval.StartAlpha + Interval.EndAlpha) * 0.5f;
		if (!IsTransitionSampleClear(
			TraceContext,
			Transition,
			MidAlpha))
		{
			return false;
		}

		if (MidAlpha - Interval.StartAlpha > KINDA_SMALL_NUMBER)
		{
			PendingIntervals.Add(FRayRopeTransitionInterval{Interval.StartAlpha, MidAlpha});
		}

		if (Interval.EndAlpha - MidAlpha > KINDA_SMALL_NUMBER)
		{
			PendingIntervals.Add(FRayRopeTransitionInterval{MidAlpha, Interval.EndAlpha});
		}
	}

	return true;
}
}

bool FRayRopeTransitionValidator::IsNodeTransitionClear(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeTransitionValidationSettings& Settings,
	const FRayRopeNodeTransition& Transition)
{
	if (!IsValidTransition(Transition) ||
		!FRayRopeTrace::IsValidFreePoint(TraceContext, Transition.TargetLocation))
	{
		return false;
	}

	if (!IsTransitionNodePathClearUnchecked(
		TraceContext,
		Settings,
		Transition))
	{
		return false;
	}

	const FRayRopeNode CandidateNode = CreateTransitionCandidateNode(
		*Transition.CurrentNode,
		Transition.TargetLocation);
	if (!AreResultSpansClear(
		TraceContext,
		*Transition.PrevNode,
		CandidateNode,
		*Transition.NextNode))
	{
		return false;
	}

	return IsContinuousSpanFanClearUnchecked(
		TraceContext,
		Settings,
		Transition);
}

bool FRayRopeTransitionValidator::IsTransitionNodePathClear(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeTransitionValidationSettings& Settings,
	const FRayRopeNodeTransition& Transition)
{
	if (!IsValidTransition(Transition))
	{
		return false;
	}

	return IsTransitionNodePathClearUnchecked(
		TraceContext,
		Settings,
		Transition);
}
