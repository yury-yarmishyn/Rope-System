#include "RayRopeMoveSolverGlobalInternal.h"

#include "Debug/RayRopeDebugContext.h"

namespace RayRopeMoveSolverPrivate
{
float CalculateInitialLineSearchAlpha(
	const FRayRopeMoveSettings& MoveSettings,
	float MaxAbsDelta)
{
	if (MoveSettings.MaxMoveDistancePerIteration <= 0.f ||
		MaxAbsDelta <= MoveSettings.MaxMoveDistancePerIteration)
	{
		return 1.f;
	}

	return MoveSettings.MaxMoveDistancePerIteration / MaxAbsDelta;
}

float CalculateMaxMoveDistanceAtAlpha(
	TConstArrayView<FGlobalMoveNodeState> States,
	float Alpha)
{
	float MaxMoveDistance = 0.f;
	for (const FGlobalMoveNodeState& State : States)
	{
		MaxMoveDistance = FMath::Max(
			MaxMoveDistance,
			FMath::Abs(State.DeltaParameter * Alpha));
	}

	return MaxMoveDistance;
}

bool TryFindAcceptedAlpha(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	float InitialAlpha,
	float CurrentLength,
	float& OutAcceptedAlpha,
	int32& OutFirstSpanIndex,
	int32& OutLastSpanIndex)
{
	OutAcceptedAlpha = 0.f;
	OutFirstSpanIndex = INDEX_NONE;
	OutLastSpanIndex = INDEX_NONE;

	const int32 MaxLineSearchSteps =
		FMath::Max(1, SolveContext.MaxGlobalMoveLineSearchSteps);
	for (int32 LineSearchStep = 0; LineSearchStep < MaxLineSearchSteps; ++LineSearchStep)
	{
		const float Alpha = InitialAlpha * FMath::Pow(0.5f, LineSearchStep);
		if (Alpha <= KINDA_SMALL_NUMBER)
		{
			break;
		}

		if (CalculateMaxMoveDistanceAtAlpha(States, Alpha) < SolveContext.MinMoveDistance)
		{
			if (SolveContext.TraceContext.DebugContext != nullptr)
			{
				SolveContext.TraceContext.DebugContext->RecordSolverEvent(
					TEXT("MoveGlobal"),
					FString::Printf(
						TEXT("LineSearch step %d rejected: max move below threshold Alpha=%.3f"),
						LineSearchStep,
						Alpha));
			}
			break;
		}

		int32 FirstSpanIndex = INDEX_NONE;
		int32 LastSpanIndex = INDEX_NONE;
		if (!TryGetAffectedSpanRange(
			States,
			Alpha,
			SolveContext.GeometryTolerance,
			FirstSpanIndex,
			LastSpanIndex))
		{
			if (SolveContext.TraceContext.DebugContext != nullptr)
			{
				SolveContext.TraceContext.DebugContext->RecordSolverEvent(
					TEXT("MoveGlobal"),
					FString::Printf(
						TEXT("LineSearch step %d rejected: no affected spans Alpha=%.3f"),
						LineSearchStep,
						Alpha));
			}
			continue;
		}

		const float CandidateLength = CalculateSegmentLengthAtAlpha(
			Segment,
			States,
			NodeToStateIndex,
			Alpha);
		if (!FMath::IsFinite(CandidateLength) ||
			CandidateLength + SolveContext.MinLengthImprovement >= CurrentLength)
		{
			if (SolveContext.TraceContext.DebugContext != nullptr)
			{
				SolveContext.TraceContext.DebugContext->RecordSolverEvent(
					TEXT("MoveGlobal"),
					FString::Printf(
						TEXT("LineSearch step %d rejected: length %.3f does not improve current %.3f Alpha=%.3f"),
						LineSearchStep,
						CandidateLength,
						CurrentLength,
						Alpha));
			}
			continue;
		}

		if (!IsBatchMoveClearAtAlpha(
			SolveContext,
			Segment,
			States,
			NodeToStateIndex,
			FirstSpanIndex,
			LastSpanIndex,
			Alpha))
		{
			if (SolveContext.TraceContext.DebugContext != nullptr)
			{
				SolveContext.TraceContext.DebugContext->RecordSolverEvent(
					TEXT("MoveGlobal"),
					FString::Printf(
						TEXT("LineSearch step %d rejected: batch move blocked Alpha=%.3f AffectedSpans=%d..%d"),
						LineSearchStep,
						Alpha,
						FirstSpanIndex,
						LastSpanIndex));
			}
			continue;
		}

		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			for (const FGlobalMoveNodeState& State : States)
			{
				const FVector TargetLocation = GetNodeLocationAtAlpha(
					Segment,
					States,
					NodeToStateIndex,
					State.NodeIndex,
					Alpha);
				SolveContext.TraceContext.DebugContext->DrawSolverLine(
					ERayRopeDebugDrawFlags::GlobalMove,
					State.StartLocation,
					TargetLocation,
					SolveContext.TraceContext.DebugContext->GetSettings().DebugAcceptedColor,
					TEXT("GlobalMove"));
				SolveContext.TraceContext.DebugContext->DrawSolverPoint(
					ERayRopeDebugDrawFlags::GlobalMove,
					TargetLocation,
					SolveContext.TraceContext.DebugContext->GetSettings().DebugAcceptedColor,
					TEXT("GlobalTarget"));
			}
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveGlobal"),
				FString::Printf(
					TEXT("LineSearch step %d accepted Alpha=%.3f Length=%.3f->%.3f AffectedSpans=%d..%d"),
					LineSearchStep,
					Alpha,
					CurrentLength,
					CandidateLength,
					FirstSpanIndex,
					LastSpanIndex));
		}

		OutAcceptedAlpha = Alpha;
		OutFirstSpanIndex = FirstSpanIndex;
		OutLastSpanIndex = LastSpanIndex;
		return true;
	}

	return false;
}
}
