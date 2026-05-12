#include "RayRopeTransitionValidator.h"

#include "Debug/RayRopeDebugContext.h"
#include "Nodes/RayRopeNodeFactory.h"

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
		if (TraceContext.DebugContext != nullptr)
		{
			TraceContext.DebugContext->DrawSolverPoint(
				ERayRopeDebugDrawFlags::TransitionValidation,
				SampleLocation,
				TraceContext.DebugContext->GetSettings().DebugBlockedTraceColor,
				TEXT("FanSampleReject"));
			TraceContext.DebugContext->RecordSolverEvent(
				TEXT("Transition"),
				FString::Printf(
					TEXT("Sample rejected: point overlaps geometry Alpha=%.3f Location=%s"),
					Alpha,
					*SampleLocation.ToCompactString()));
		}
		return false;
	}

	const FRayRopeNode SampleNode = FRayRopeNodeFactory::CreateNodeAtLocation(
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

	const bool bPathClear = !FRayRopeTrace::HasBlockingHit(
		MoveTraceContext,
		Transition.CurrentNode->WorldLocation,
		Transition.TargetLocation);
	if (TraceContext.DebugContext != nullptr)
	{
		TraceContext.DebugContext->DrawSolverLine(
			ERayRopeDebugDrawFlags::TransitionValidation,
			Transition.CurrentNode->WorldLocation,
			Transition.TargetLocation,
			bPathClear
				? TraceContext.DebugContext->GetSettings().DebugSolverGuideColor
				: TraceContext.DebugContext->GetSettings().DebugBlockedTraceColor,
			bPathClear ? TEXT("NodePath") : TEXT("NodePathBlocked"));
	}

	return bPathClear;
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

	// The moving node sweeps a fan of two spans, not just a single point path. Sample interval
	// midpoints breadth-first so narrow blockers are rejected without allocating per-depth arrays.
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
		if (TraceContext.DebugContext != nullptr)
		{
			TraceContext.DebugContext->RecordSolverEvent(
				TEXT("Transition"),
				FString::Printf(
					TEXT("Rejected: invalid transition or target overlaps geometry Target=%s"),
					*Transition.TargetLocation.ToCompactString()));
		}
		return false;
	}

	if (!IsTransitionNodePathClearUnchecked(
		TraceContext,
		Settings,
		Transition))
	{
		if (TraceContext.DebugContext != nullptr)
		{
			TraceContext.DebugContext->RecordSolverEvent(
				TEXT("Transition"),
				TEXT("Rejected: moving node path is blocked"));
		}
		return false;
	}

	const FRayRopeNode CandidateNode = FRayRopeNodeFactory::CreateNodeAtLocation(
		*Transition.CurrentNode,
		Transition.TargetLocation);
	if (!AreResultSpansClear(
		TraceContext,
		*Transition.PrevNode,
		CandidateNode,
		*Transition.NextNode))
	{
		if (TraceContext.DebugContext != nullptr)
		{
			TraceContext.DebugContext->RecordSolverEvent(
				TEXT("Transition"),
				TEXT("Rejected: final adjacent spans are blocked"));
		}
		return false;
	}

	const bool bFanClear = IsContinuousSpanFanClearUnchecked(
		TraceContext,
		Settings,
		Transition);
	if (!bFanClear && TraceContext.DebugContext != nullptr)
	{
		TraceContext.DebugContext->RecordSolverEvent(
			TEXT("Transition"),
			TEXT("Rejected: continuous span fan is blocked"));
	}

	return bFanClear;
}

bool FRayRopeTransitionValidator::IsTransitionNodePathClear(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeTransitionValidationSettings& Settings,
	const FRayRopeNodeTransition& Transition)
{
	if (!IsValidTransition(Transition))
	{
		if (TraceContext.DebugContext != nullptr)
		{
			TraceContext.DebugContext->RecordSolverEvent(
				TEXT("Transition"),
				TEXT("Rejected: invalid node path transition"));
		}
		return false;
	}

	return IsTransitionNodePathClearUnchecked(
		TraceContext,
		Settings,
		Transition);
}
