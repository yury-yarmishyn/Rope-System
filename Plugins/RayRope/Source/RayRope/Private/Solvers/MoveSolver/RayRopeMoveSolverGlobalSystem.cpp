#include "RayRopeMoveSolverGlobalInternal.h"

namespace RayRopeMoveSolverPrivate
{
namespace
{
void AddSpanLengthDerivatives(
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	int32 SpanIndex,
	FGlobalMoveSystem& System)
{
	const int32 StartStateIndex = NodeToStateIndex[SpanIndex];
	const int32 EndStateIndex = NodeToStateIndex[SpanIndex + 1];
	if (!IsValidViewIndex(States, StartStateIndex) &&
		!IsValidViewIndex(States, EndStateIndex))
	{
		return;
	}

	const FVector StartLocation = Segment.Nodes[SpanIndex].WorldLocation;
	const FVector EndLocation = Segment.Nodes[SpanIndex + 1].WorldLocation;
	const FVector SpanDelta = EndLocation - StartLocation;
	const float SpanLength = SpanDelta.Size();
	if (SpanLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector UnitSpan = SpanDelta / SpanLength;
	if (IsValidViewIndex(States, StartStateIndex))
	{
		const FVector& StartDirection = States[StartStateIndex].Rail.Direction;
		System.Gradient[StartStateIndex] += FVector::DotProduct(-UnitSpan, StartDirection);

		const float StartDirectionAlongSpan =
			FVector::DotProduct(StartDirection, UnitSpan);
		System.Diagonal[StartStateIndex] +=
			(FVector::DotProduct(StartDirection, StartDirection) -
				StartDirectionAlongSpan * StartDirectionAlongSpan) / SpanLength;
	}

	if (IsValidViewIndex(States, EndStateIndex))
	{
		const FVector& EndDirection = States[EndStateIndex].Rail.Direction;
		System.Gradient[EndStateIndex] += FVector::DotProduct(UnitSpan, EndDirection);

		const float EndDirectionAlongSpan =
			FVector::DotProduct(EndDirection, UnitSpan);
		System.Diagonal[EndStateIndex] +=
			(FVector::DotProduct(EndDirection, EndDirection) -
				EndDirectionAlongSpan * EndDirectionAlongSpan) / SpanLength;
	}

	if (IsValidViewIndex(States, StartStateIndex) &&
		IsValidViewIndex(States, EndStateIndex) &&
		FMath::Abs(StartStateIndex - EndStateIndex) == 1)
	{
		const FVector& StartDirection = States[StartStateIndex].Rail.Direction;
		const FVector& EndDirection = States[EndStateIndex].Rail.Direction;
		const float StartDirectionAlongSpan =
			FVector::DotProduct(StartDirection, UnitSpan);
		const float EndDirectionAlongSpan =
			FVector::DotProduct(EndDirection, UnitSpan);
		const float ScalarHessian =
			(FVector::DotProduct(StartDirection, EndDirection) -
				StartDirectionAlongSpan * EndDirectionAlongSpan) / SpanLength;
		const int32 UpperIndex = FMath::Min(StartStateIndex, EndStateIndex);
		System.Upper[UpperIndex] += -ScalarHessian;
	}
}
}

bool BuildGlobalMoveSystem(
	const FMoveSolveContext& SolveContext,
	const FRayRopeSegment& Segment,
	TConstArrayView<FGlobalMoveNodeState> States,
	TConstArrayView<int32> NodeToStateIndex,
	FGlobalMoveSystem& OutSystem)
{
	const int32 StateCount = States.Num();
	if (StateCount == 0)
	{
		return false;
	}

	OutSystem.Gradient.Reset(StateCount);
	OutSystem.Diagonal.Reset(StateCount);
	OutSystem.Upper.Reset(FMath::Max(0, StateCount - 1));
	OutSystem.Delta.Reset(StateCount);

	OutSystem.Gradient.Init(0.f, StateCount);
	OutSystem.Diagonal.Init(SolveContext.GlobalMoveDamping, StateCount);
	OutSystem.Upper.Init(0.f, FMath::Max(0, StateCount - 1));
	OutSystem.Delta.Init(0.f, StateCount);

	// Each span contributes first and second derivatives of polyline length in rail coordinates.
	for (int32 SpanIndex = 0; SpanIndex < Segment.Nodes.Num() - 1; ++SpanIndex)
	{
		AddSpanLengthDerivatives(
			Segment,
			States,
			NodeToStateIndex,
			SpanIndex,
			OutSystem);
	}

	return true;
}

bool SolveTridiagonalSystem(FGlobalMoveSystem& System)
{
	const int32 StateCount = System.Diagonal.Num();
	if (StateCount == 0 || System.Gradient.Num() != StateCount)
	{
		return false;
	}

	TArray<float, TInlineAllocator<32>> UpperPrime;
	TArray<float, TInlineAllocator<32>> DeltaPrime;
	UpperPrime.Init(0.f, StateCount);
	DeltaPrime.Init(0.f, StateCount);

	float Denominator = System.Diagonal[0];
	if (FMath::Abs(Denominator) <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	UpperPrime[0] = StateCount > 1 ? System.Upper[0] / Denominator : 0.f;
	DeltaPrime[0] = -System.Gradient[0] / Denominator;

	for (int32 StateIndex = 1; StateIndex < StateCount; ++StateIndex)
	{
		const float Lower = System.Upper[StateIndex - 1];
		Denominator = System.Diagonal[StateIndex] - Lower * UpperPrime[StateIndex - 1];
		if (FMath::Abs(Denominator) <= KINDA_SMALL_NUMBER)
		{
			return false;
		}

		UpperPrime[StateIndex] = StateIndex + 1 < StateCount
			? System.Upper[StateIndex] / Denominator
			: 0.f;
		DeltaPrime[StateIndex] =
			(-System.Gradient[StateIndex] - Lower * DeltaPrime[StateIndex - 1]) / Denominator;
	}

	System.Delta[StateCount - 1] = DeltaPrime[StateCount - 1];
	for (int32 StateIndex = StateCount - 2; StateIndex >= 0; --StateIndex)
	{
		System.Delta[StateIndex] =
			DeltaPrime[StateIndex] - UpperPrime[StateIndex] * System.Delta[StateIndex + 1];
	}

	return true;
}

float CopyDeltaToStates(
	const FGlobalMoveSystem& System,
	TArray<FGlobalMoveNodeState, TInlineAllocator<32>>& States)
{
	float MaxAbsDelta = 0.f;
	for (int32 StateIndex = 0; StateIndex < States.Num(); ++StateIndex)
	{
		const float Delta = System.Delta[StateIndex];
		if (!FMath::IsFinite(Delta))
		{
			States[StateIndex].DeltaParameter = 0.f;
			continue;
		}

		States[StateIndex].DeltaParameter = Delta;
		MaxAbsDelta = FMath::Max(MaxAbsDelta, FMath::Abs(Delta));
	}

	return MaxAbsDelta;
}
}
