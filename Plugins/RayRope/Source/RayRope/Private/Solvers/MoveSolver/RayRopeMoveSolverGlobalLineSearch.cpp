#include "RayRopeMoveSolverGlobalInternal.h"

#include "Debug/RayRopeDebugConfig.h"

#if RAYROPE_WITH_DEBUG
#include "Debug/RayRopeDebugContext.h"
#endif

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

bool TryFindAcceptedAlpha(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	float InitialAlpha,
	float MaxAbsDelta,
	float CurrentLength,
	float& OutAcceptedAlpha,
	FRayRopeAffectedSpanRangeBuffer& OutAffectedSpanRanges)
{
	OutAcceptedAlpha = 0.f;
	OutAffectedSpanRanges.Reset();

	const int32 MaxLineSearchSteps = SolveContext.MaxGlobalMoveLineSearchSteps;
	float Alpha = InitialAlpha;
	for (int32 LineSearchStep = 0;
		LineSearchStep < MaxLineSearchSteps;
		++LineSearchStep, Alpha *= 0.5f)
	{
		if (Alpha <= KINDA_SMALL_NUMBER)
		{
			break;
		}

		if (MaxAbsDelta * Alpha < SolveContext.MinMoveDistance)
		{
#if RAYROPE_WITH_DEBUG
			if (SolveContext.TraceContext.DebugContext != nullptr)
			{
				SolveContext.TraceContext.DebugContext->RecordSolverEvent(
					TEXT("MoveGlobal"),
					FString::Printf(
						TEXT("LineSearch step %d rejected: max move below threshold Alpha=%.3f"),
						LineSearchStep,
						Alpha));
			}
#endif
			break;
		}

		FRayRopeAffectedSpanRangeBuffer AffectedSpanRanges;
		if (!BuildAffectedSpanRanges(
			States,
			Alpha,
			SolveContext.GeometryTolerance,
			Segment.Nodes.Num() - 2,
			AffectedSpanRanges))
		{
#if RAYROPE_WITH_DEBUG
			if (SolveContext.TraceContext.DebugContext != nullptr)
			{
				SolveContext.TraceContext.DebugContext->RecordSolverEvent(
					TEXT("MoveGlobal"),
					FString::Printf(
						TEXT("LineSearch step %d rejected: no affected spans Alpha=%.3f"),
						LineSearchStep,
						Alpha));
			}
#endif
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
#if RAYROPE_WITH_DEBUG
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
#endif
			continue;
		}

		if (!IsBatchMoveClearAtAlpha(
			SolveContext,
			Segment,
			States,
			NodeToStateIndex,
			AffectedSpanRanges,
			Alpha))
		{
#if RAYROPE_WITH_DEBUG
			if (SolveContext.TraceContext.DebugContext != nullptr)
			{
				SolveContext.TraceContext.DebugContext->RecordSolverEvent(
					TEXT("MoveGlobal"),
					FString::Printf(
						TEXT("LineSearch step %d rejected: batch move blocked Alpha=%.3f AffectedRanges=%d"),
						LineSearchStep,
						Alpha,
						AffectedSpanRanges.Num()));
			}
#endif
			continue;
		}

#if RAYROPE_WITH_DEBUG
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
					TEXT("LineSearch step %d accepted Alpha=%.3f Length=%.3f->%.3f AffectedRanges=%d"),
					LineSearchStep,
					Alpha,
					CurrentLength,
					CandidateLength,
					AffectedSpanRanges.Num()));
		}
#endif

		OutAcceptedAlpha = Alpha;
		OutAffectedSpanRanges = MoveTemp(AffectedSpanRanges);
		return true;
	}

	return false;
}
}
