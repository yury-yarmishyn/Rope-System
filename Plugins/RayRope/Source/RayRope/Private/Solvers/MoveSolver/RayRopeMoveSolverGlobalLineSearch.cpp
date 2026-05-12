#include "RayRopeMoveSolverGlobalInternal.h"

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
			continue;
		}

		OutAcceptedAlpha = Alpha;
		OutFirstSpanIndex = FirstSpanIndex;
		OutLastSpanIndex = LastSpanIndex;
		return true;
	}

	return false;
}
}
