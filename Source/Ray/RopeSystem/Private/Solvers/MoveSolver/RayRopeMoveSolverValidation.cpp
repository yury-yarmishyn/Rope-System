#include "RayRopeMoveSolverInternal.h"

namespace RayRopeMoveSolverPrivate
{
bool IsReachableMoveTarget(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint)
{
	if (CandidatePoint.ContainsNaN())
	{
		return false;
	}

	if (FVector::DistSquared(CandidatePoint, NodeWindow.PrevNode.WorldLocation) <=
			SolveContext.MinNodeSeparationSquared ||
		FVector::DistSquared(CandidatePoint, NodeWindow.NextNode.WorldLocation) <=
			SolveContext.MinNodeSeparationSquared)
	{
		return false;
	}

	if (!FRayRopeTrace::IsValidFreePoint(SolveContext.TraceContext, CandidatePoint))
	{
		return false;
	}

	if (!IsMoveImprovementSignificant(
		SolveContext,
		NodeWindow,
		CandidatePoint))
	{
		return false;
	}

	const FRayRopeNodeTransition Transition{
		&NodeWindow.PrevNode,
		&NodeWindow.CurrentNode,
		&NodeWindow.NextNode,
		CandidatePoint
	};
	return FRayRopeTransitionValidator::IsTransitionNodePathClear(
		SolveContext.TraceContext,
		SolveContext.TransitionValidationSettings,
		Transition);
}

bool IsValidMovePoint(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint)
{
	if (CandidatePoint.ContainsNaN())
	{
		return false;
	}

	if (FVector::DistSquared(CandidatePoint, NodeWindow.PrevNode.WorldLocation) <=
			SolveContext.MinNodeSeparationSquared ||
		FVector::DistSquared(CandidatePoint, NodeWindow.NextNode.WorldLocation) <=
			SolveContext.MinNodeSeparationSquared)
	{
		return false;
	}

	if (!IsMoveImprovementSignificant(
		SolveContext,
		NodeWindow,
		CandidatePoint))
	{
		return false;
	}

	const FRayRopeNodeTransition Transition{
		&NodeWindow.PrevNode,
		&NodeWindow.CurrentNode,
		&NodeWindow.NextNode,
		CandidatePoint
	};
	return FRayRopeTransitionValidator::IsNodeTransitionClear(
		SolveContext.TraceContext,
		SolveContext.TransitionValidationSettings,
		Transition);
}

bool IsMoveImprovementSignificant(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint)
{
	if (!NodeWindow.IsCurrentPointFree(SolveContext.TraceContext))
	{
		return true;
	}

	if (NodeWindow.CurrentNode.WorldLocation.Equals(CandidatePoint, SolveContext.MinMoveDistance))
	{
		return false;
	}

	const float CurrentDistanceSum = CalculateMoveDistanceSum(
		NodeWindow.PrevNode,
		NodeWindow.CurrentNode.WorldLocation,
		NodeWindow.NextNode);
	const float CandidateDistanceSum = CalculateMoveDistanceSum(
		NodeWindow.PrevNode,
		CandidatePoint,
		NodeWindow.NextNode);
	return CandidateDistanceSum + SolveContext.MinLengthImprovement < CurrentDistanceSum;
}
}
