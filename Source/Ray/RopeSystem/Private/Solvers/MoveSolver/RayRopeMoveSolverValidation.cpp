#include "RayRopeMoveSolverInternal.h"

namespace RayRopeMoveSolverPrivate
{
namespace
{
bool IsCandidateSeparatedFromNeighbors(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint)
{
	return FVector::DistSquared(CandidatePoint, NodeWindow.PrevNode.WorldLocation) >
			SolveContext.MinNodeSeparationSquared &&
		FVector::DistSquared(CandidatePoint, NodeWindow.NextNode.WorldLocation) >
			SolveContext.MinNodeSeparationSquared;
}

bool HasValidCandidateGeometry(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint)
{
	return !CandidatePoint.ContainsNaN() &&
		IsCandidateSeparatedFromNeighbors(SolveContext, NodeWindow, CandidatePoint);
}

FRayRopeNodeTransition MakeNodeTransition(
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint)
{
	return FRayRopeNodeTransition{
		&NodeWindow.PrevNode,
		&NodeWindow.CurrentNode,
		&NodeWindow.NextNode,
		CandidatePoint
	};
}
}

bool IsReachableMoveTarget(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint)
{
	if (!HasValidCandidateGeometry(SolveContext, NodeWindow, CandidatePoint))
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

	return FRayRopeTransitionValidator::IsTransitionNodePathClear(
		SolveContext.TraceContext,
		SolveContext.TransitionValidationSettings,
		MakeNodeTransition(NodeWindow, CandidatePoint));
}

bool IsValidMovePoint(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint)
{
	if (!HasValidCandidateGeometry(SolveContext, NodeWindow, CandidatePoint))
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

	return FRayRopeTransitionValidator::IsNodeTransitionClear(
		SolveContext.TraceContext,
		SolveContext.TransitionValidationSettings,
		MakeNodeTransition(NodeWindow, CandidatePoint));
}

bool IsMoveImprovementSignificant(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint)
{
	if (!NodeWindow.IsCurrentPointFree(SolveContext.TraceContext))
	{
		// Escaping penetration is more important than shortening the local rope path.
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
