#include "RayRopeMoveSolverInternal.h"

#include "Debug/RayRopeDebugConfig.h"

#if RAYROPE_WITH_DEBUG
#include "Debug/RayRopeDebugContext.h"
#endif

namespace RayRopeMoveSolverPrivate
{
namespace
{
#if RAYROPE_WITH_DEBUG
#define RAYROPE_MAKE_MOVE_VALIDATION(Type, TargetPoint, Reason) \
	MakeMoveValidation(Type, TargetPoint, Reason)
#else
#define RAYROPE_MAKE_MOVE_VALIDATION(Type, TargetPoint, Reason) \
	MakeMoveValidation(Type, TargetPoint)
#endif

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

FMoveValidation MakeMoveValidation(
	EMoveDecisionType Type,
	const FVector& TargetPoint
#if RAYROPE_WITH_DEBUG
	,
	const TCHAR* DebugReason
#endif
)
{
	FMoveValidation Validation;
	Validation.Type = Type;
	Validation.TargetPoint = TargetPoint;
#if RAYROPE_WITH_DEBUG
	Validation.DebugReason = DebugReason;
#endif
	return Validation;
}

void RecordCandidateEvaluation(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FMoveValidation& Validation)
{
#if RAYROPE_WITH_DEBUG
	if (SolveContext.TraceContext.DebugContext == nullptr)
	{
		return;
	}

	const TCHAR* ActionName = TEXT("Reject");
	if (Validation.Type == EMoveDecisionType::Move)
	{
		ActionName = TEXT("Move");
	}
	else if (Validation.Type == EMoveDecisionType::Insert)
	{
		ActionName = TEXT("Insert");
	}
	else if (Validation.Type == EMoveDecisionType::MoveAndInsert)
	{
		ActionName = TEXT("MoveAndInsert");
	}

	SolveContext.TraceContext.DebugContext->RecordSolverEvent(
		TEXT("MoveValidation"),
		FString::Printf(
			TEXT("Node[%d] %s Candidate=%s PrevBlocked=%s NextBlocked=%s Reason=%s"),
			NodeWindow.NodeIndex,
			ActionName,
			*Validation.TargetPoint.ToCompactString(),
			Validation.bPrevSpanBlocked ? TEXT("true") : TEXT("false"),
			Validation.bNextSpanBlocked ? TEXT("true") : TEXT("false"),
			Validation.DebugReason));
#endif
}

void SetResultSpanBlocks(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint,
	FMoveValidation& Validation)
{
	const FRayRopeNode CandidateNode = FRayRopeNodeFactory::CreateNodeAtLocation(
		NodeWindow.CurrentNode,
		CandidatePoint);
	const FRayRopeSpan PrevCandidateSpan{&NodeWindow.PrevNode, &CandidateNode};
	const FRayRopeSpan CandidateNextSpan{&CandidateNode, &NodeWindow.NextNode};
	Validation.bPrevSpanBlocked =
		FRayRopeTrace::HasBlockingSpanHit(SolveContext.TraceContext, PrevCandidateSpan);
	Validation.bNextSpanBlocked =
		FRayRopeTrace::HasBlockingSpanHit(SolveContext.TraceContext, CandidateNextSpan);
}

FMoveValidation FinishValidation(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	FMoveValidation Validation)
{
	RecordCandidateEvaluation(SolveContext, NodeWindow, Validation);
	return Validation;
}
}

FMoveValidation EvaluateCurrentSpans(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow)
{
	FMoveValidation Validation = RAYROPE_MAKE_MOVE_VALIDATION(
		EMoveDecisionType::None,
		NodeWindow.CurrentNode.WorldLocation,
		TEXT("Current spans are clear"));

	const FRayRopeSpan PrevCurrentSpan{&NodeWindow.PrevNode, &NodeWindow.CurrentNode};
	const FRayRopeSpan CurrentNextSpan{&NodeWindow.CurrentNode, &NodeWindow.NextNode};
	Validation.bPrevSpanBlocked =
		FRayRopeTrace::HasBlockingSpanHit(SolveContext.TraceContext, PrevCurrentSpan);
	Validation.bNextSpanBlocked =
		FRayRopeTrace::HasBlockingSpanHit(SolveContext.TraceContext, CurrentNextSpan);

	if (Validation.bPrevSpanBlocked || Validation.bNextSpanBlocked)
	{
		Validation.Type = EMoveDecisionType::Insert;
#if RAYROPE_WITH_DEBUG
		Validation.DebugReason = TEXT("Current spans are blocked");
#endif
	}

	return FinishValidation(SolveContext, NodeWindow, Validation);
}

FMoveValidation EvaluateMoveCandidate(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& CandidatePoint)
{
	if (!HasValidCandidateGeometry(SolveContext, NodeWindow, CandidatePoint))
	{
		return FinishValidation(
			SolveContext,
			NodeWindow,
			RAYROPE_MAKE_MOVE_VALIDATION(
				EMoveDecisionType::None,
				CandidatePoint,
				TEXT("Invalid geometry")));
	}

	if (!IsMoveImprovementSignificant(
		SolveContext,
		NodeWindow,
		CandidatePoint))
	{
		return FinishValidation(
			SolveContext,
			NodeWindow,
			RAYROPE_MAKE_MOVE_VALIDATION(
				EMoveDecisionType::None,
				CandidatePoint,
				TEXT("Insufficient length improvement")));
	}

	if (!FRayRopeTrace::IsValidFreePoint(SolveContext.TraceContext, CandidatePoint))
	{
		return FinishValidation(
			SolveContext,
			NodeWindow,
			RAYROPE_MAKE_MOVE_VALIDATION(
				EMoveDecisionType::None,
				CandidatePoint,
				TEXT("Target overlaps geometry")));
	}

	const FRayRopeNodeTransition Transition = FRayRopeNodeTransition::Make(
		NodeWindow.PrevNode,
		NodeWindow.CurrentNode,
		NodeWindow.NextNode,
		CandidatePoint);

	const bool bNodePathClear = FRayRopeTransitionValidator::IsTransitionNodePathClear(
		SolveContext.TraceContext,
		SolveContext.TransitionValidationSettings,
		Transition);
	if (!bNodePathClear)
	{
		return FinishValidation(
			SolveContext,
			NodeWindow,
			RAYROPE_MAKE_MOVE_VALIDATION(
				EMoveDecisionType::None,
				CandidatePoint,
				TEXT("Node path blocked")));
	}

	FMoveValidation Validation = RAYROPE_MAKE_MOVE_VALIDATION(
		EMoveDecisionType::Move,
		CandidatePoint,
		TEXT("Clear direct move"));
	SetResultSpanBlocks(SolveContext, NodeWindow, CandidatePoint, Validation);
	if (Validation.bPrevSpanBlocked || Validation.bNextSpanBlocked)
	{
		Validation.Type = EMoveDecisionType::MoveAndInsert;
#if RAYROPE_WITH_DEBUG
		Validation.DebugReason = TEXT("Final spans are blocked");
#endif
		return FinishValidation(SolveContext, NodeWindow, Validation);
	}

	if (!FRayRopeTransitionValidator::IsTransitionSpanFanClear(
		SolveContext.TraceContext,
		SolveContext.TransitionValidationSettings,
		Transition))
	{
		return FinishValidation(
			SolveContext,
			NodeWindow,
			RAYROPE_MAKE_MOVE_VALIDATION(
				EMoveDecisionType::None,
				CandidatePoint,
				TEXT("Span fan blocked")));
	}

	return FinishValidation(SolveContext, NodeWindow, Validation);
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

#undef RAYROPE_MAKE_MOVE_VALIDATION
}
