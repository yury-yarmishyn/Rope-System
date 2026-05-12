#include "RayRopeMoveSolverInternal.h"

#include "Debug/RayRopeDebugContext.h"

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
}

bool IsReachableMoveTarget(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint)
{
	if (!HasValidCandidateGeometry(SolveContext, NodeWindow, CandidatePoint))
	{
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveValidation"),
				FString::Printf(
					TEXT("Node[%d] reachable target rejected: invalid geometry Candidate=%s"),
					NodeWindow.NodeIndex,
					*CandidatePoint.ToCompactString()));
		}
		return false;
	}

	if (!FRayRopeTrace::IsValidFreePoint(SolveContext.TraceContext, CandidatePoint))
	{
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->DrawSolverPoint(
				ERayRopeDebugDrawFlags::MoveCandidates,
				CandidatePoint,
				SolveContext.TraceContext.DebugContext->GetSettings().DebugBlockedTraceColor,
				TEXT("OverlapReject"));
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveValidation"),
				FString::Printf(
					TEXT("Node[%d] reachable target rejected: overlaps geometry Candidate=%s"),
					NodeWindow.NodeIndex,
					*CandidatePoint.ToCompactString()));
		}
		return false;
	}

	if (!IsMoveImprovementSignificant(
		SolveContext,
		NodeWindow,
		CandidatePoint))
	{
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveValidation"),
				FString::Printf(
					TEXT("Node[%d] reachable target rejected: insufficient length improvement"),
					NodeWindow.NodeIndex));
		}
		return false;
	}

	const bool bPathClear = FRayRopeTransitionValidator::IsTransitionNodePathClear(
		SolveContext.TraceContext,
		SolveContext.TransitionValidationSettings,
		FRayRopeNodeTransition::Make(
			NodeWindow.PrevNode,
			NodeWindow.CurrentNode,
			NodeWindow.NextNode,
			CandidatePoint));
	if (!bPathClear && SolveContext.TraceContext.DebugContext != nullptr)
	{
		SolveContext.TraceContext.DebugContext->DrawSolverLine(
			ERayRopeDebugDrawFlags::MoveCandidates,
			NodeWindow.CurrentNode.WorldLocation,
			CandidatePoint,
			SolveContext.TraceContext.DebugContext->GetSettings().DebugBlockedTraceColor,
			TEXT("PathBlocked"));
		SolveContext.TraceContext.DebugContext->RecordSolverEvent(
			TEXT("MoveValidation"),
			FString::Printf(
				TEXT("Node[%d] reachable target rejected: node path blocked"),
				NodeWindow.NodeIndex));
	}

	return bPathClear;
}

bool IsValidMovePoint(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint)
{
	if (!HasValidCandidateGeometry(SolveContext, NodeWindow, CandidatePoint))
	{
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveValidation"),
				FString::Printf(
					TEXT("Node[%d] move point rejected: invalid geometry Candidate=%s"),
					NodeWindow.NodeIndex,
					*CandidatePoint.ToCompactString()));
		}
		return false;
	}

	if (!IsMoveImprovementSignificant(
		SolveContext,
		NodeWindow,
		CandidatePoint))
	{
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveValidation"),
				FString::Printf(
					TEXT("Node[%d] move point rejected: insufficient length improvement"),
					NodeWindow.NodeIndex));
		}
		return false;
	}

	const bool bTransitionClear = FRayRopeTransitionValidator::IsNodeTransitionClear(
		SolveContext.TraceContext,
		SolveContext.TransitionValidationSettings,
		FRayRopeNodeTransition::Make(
			NodeWindow.PrevNode,
			NodeWindow.CurrentNode,
			NodeWindow.NextNode,
			CandidatePoint));
	if (!bTransitionClear && SolveContext.TraceContext.DebugContext != nullptr)
	{
		SolveContext.TraceContext.DebugContext->DrawSolverLine(
			ERayRopeDebugDrawFlags::MoveCandidates,
			NodeWindow.CurrentNode.WorldLocation,
			CandidatePoint,
			SolveContext.TraceContext.DebugContext->GetSettings().DebugBlockedTraceColor,
			TEXT("TransitionReject"));
		SolveContext.TraceContext.DebugContext->RecordSolverEvent(
			TEXT("MoveValidation"),
			FString::Printf(
				TEXT("Node[%d] move point rejected: transition blocked Candidate=%s"),
				NodeWindow.NodeIndex,
				*CandidatePoint.ToCompactString()));
	}

	return bTransitionClear;
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
