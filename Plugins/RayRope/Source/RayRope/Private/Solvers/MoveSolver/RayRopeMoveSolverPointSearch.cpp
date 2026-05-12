#include "RayRopeMoveSolverInternal.h"

#include "Debug/RayRopeDebugContext.h"

namespace RayRopeMoveSolverPrivate
{
namespace
{
struct FMoveRailSearchContext
{
	const FMoveSolveContext& SolveContext;
	const FMoveNodeWindow& NodeWindow;
	const FMoveRail Rail;
	float CurrentRailParameter = 0.f;
};

FVector GetRailPoint(const FMoveRailSearchContext& SearchContext, float RailParameter)
{
	return SearchContext.Rail.Origin + SearchContext.Rail.Direction * RailParameter;
}

float CalculateRailDistanceSum(const FMoveRailSearchContext& SearchContext, float RailParameter)
{
	const FVector CandidateLocation = GetRailPoint(SearchContext, RailParameter);
	return FVector::Dist(SearchContext.NodeWindow.PrevNode.WorldLocation, CandidateLocation) +
		FVector::Dist(CandidateLocation, SearchContext.NodeWindow.NextNode.WorldLocation);
}

float FindBestRailParameter(
	const FMoveRailSearchContext& SearchContext,
	float LeftRailParameter,
	float RightRailParameter)
{
	float BestRailParameter = SearchContext.CurrentRailParameter;
	float BestDistanceSum = CalculateRailDistanceSum(SearchContext, BestRailParameter);
	const float SearchEpsilon = SearchContext.SolveContext.EffectivePointSearchTolerance;

	const int32 MaxRailPointSearchIterations =
		SearchContext.SolveContext.MaxEffectivePointSearchIterations;
	for (int32 Iteration = 0; Iteration < MaxRailPointSearchIterations; ++Iteration)
	{
		if (FMath::Abs(RightRailParameter - LeftRailParameter) <= SearchEpsilon)
		{
			break;
		}

		const float MidRailParameter = (LeftRailParameter + RightRailParameter) * 0.5f;
		const float LeftMidRailParameter = (LeftRailParameter + MidRailParameter) * 0.5f;
		const float RightMidRailParameter = (MidRailParameter + RightRailParameter) * 0.5f;

		const float LeftMidDistanceSum = CalculateRailDistanceSum(SearchContext, LeftMidRailParameter);
		const float RightMidDistanceSum = CalculateRailDistanceSum(SearchContext, RightMidRailParameter);
		// The local rope length along a rail is convex enough for interval shrinking; sampling both
		// quarter points keeps the best candidate even when the current point is already near optimal.
		if (LeftMidDistanceSum < BestDistanceSum)
		{
			BestRailParameter = LeftMidRailParameter;
			BestDistanceSum = LeftMidDistanceSum;
		}

		if (RightMidDistanceSum < BestDistanceSum)
		{
			BestRailParameter = RightMidRailParameter;
			BestDistanceSum = RightMidDistanceSum;
		}

		if (LeftMidDistanceSum <= RightMidDistanceSum)
		{
			RightRailParameter = MidRailParameter;
		}
		else
		{
			LeftRailParameter = MidRailParameter;
		}
	}

	return BestRailParameter;
}
}

bool TryFindEffectivePointOnRail(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FMoveRail& Rail,
	FVector& OutEffectivePoint)
{
	if (Rail.Origin.ContainsNaN() || Rail.Direction.IsNearlyZero())
	{
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MovePointSearch"),
				FString::Printf(
					TEXT("Node[%d] rejected: invalid rail"),
					NodeWindow.NodeIndex));
		}
		return false;
	}

	const float CurrentRailParameter =
		FVector::DotProduct(NodeWindow.CurrentNode.WorldLocation - Rail.Origin, Rail.Direction);
	const float PrevRailParameter =
		FVector::DotProduct(NodeWindow.PrevNode.WorldLocation - Rail.Origin, Rail.Direction);
	const float NextRailParameter =
		FVector::DotProduct(NodeWindow.NextNode.WorldLocation - Rail.Origin, Rail.Direction);
	if (!FMath::IsFinite(CurrentRailParameter) ||
		!FMath::IsFinite(PrevRailParameter) ||
		!FMath::IsFinite(NextRailParameter))
	{
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MovePointSearch"),
				FString::Printf(
					TEXT("Node[%d] rejected: non-finite rail parameter"),
					NodeWindow.NodeIndex));
		}
		return false;
	}

	const FMoveRailSearchContext SearchContext{
		SolveContext,
		NodeWindow,
		Rail,
		CurrentRailParameter
	};

	const float LeftRailParameter = FMath::Min(
		CurrentRailParameter,
		FMath::Min(PrevRailParameter, NextRailParameter));
	const float RightRailParameter = FMath::Max(
		CurrentRailParameter,
		FMath::Max(PrevRailParameter, NextRailParameter));
	const float TargetRailParameter = FindBestRailParameter(
		SearchContext,
		LeftRailParameter,
		RightRailParameter);

	OutEffectivePoint = GetRailPoint(
		SearchContext,
		TargetRailParameter);
	if (SolveContext.TraceContext.DebugContext != nullptr)
	{
		SolveContext.TraceContext.DebugContext->DrawSolverPoint(
			ERayRopeDebugDrawFlags::MoveCandidates,
			OutEffectivePoint,
			SolveContext.TraceContext.DebugContext->GetSettings().DebugCandidateColor,
			TEXT("IdealMove"));
		SolveContext.TraceContext.DebugContext->RecordSolverEvent(
			TEXT("MovePointSearch"),
			FString::Printf(
				TEXT("Node[%d] ideal target Parameter=%.3f Location=%s"),
				NodeWindow.NodeIndex,
				TargetRailParameter,
				*OutEffectivePoint.ToCompactString()));
	}
	return !OutEffectivePoint.ContainsNaN();
}

bool TryFindValidEffectivePoint(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FVector& TargetPoint,
	FVector& OutEffectivePoint)
{
	OutEffectivePoint = FVector::ZeroVector;
	if (TargetPoint.ContainsNaN())
	{
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MovePointSearch"),
				FString::Printf(
					TEXT("Node[%d] rejected: target contains NaN"),
					NodeWindow.NodeIndex));
		}
		return false;
	}

	if (IsValidMovePoint(
		SolveContext,
		NodeWindow,
		TargetPoint))
	{
		OutEffectivePoint = TargetPoint;
		return true;
	}

	if (!NodeWindow.IsCurrentPointFree(SolveContext.TraceContext))
	{
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MovePointSearch"),
				FString::Printf(
					TEXT("Node[%d] rejected: ideal target invalid and current point is not free"),
					NodeWindow.NodeIndex));
		}
		return false;
	}

	FVector LastValidPoint = NodeWindow.CurrentNode.WorldLocation;
	FVector LastInvalidPoint = TargetPoint;
	bool bFoundValidPoint = false;
	const int32 MaxSearchIterations =
		FMath::Max(1, SolveContext.MaxEffectivePointSearchIterations);
	// Walk back from the ideal target toward the current free point until the transition is valid.
	for (int32 Iteration = 0; Iteration < MaxSearchIterations; ++Iteration)
	{
		const FVector CandidatePoint = (LastValidPoint + LastInvalidPoint) * 0.5f;
		if (FVector::DistSquared(LastValidPoint, LastInvalidPoint) <=
			SolveContext.GeometryToleranceSquared)
		{
			break;
		}

		if (IsValidMovePoint(
			SolveContext,
			NodeWindow,
			CandidatePoint))
		{
			LastValidPoint = CandidatePoint;
			bFoundValidPoint = true;
			continue;
		}

		LastInvalidPoint = CandidatePoint;
	}

	if (!bFoundValidPoint ||
		LastValidPoint.Equals(NodeWindow.CurrentNode.WorldLocation, SolveContext.MinMoveDistance))
	{
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->DrawSolverPoint(
				ERayRopeDebugDrawFlags::MoveCandidates,
				TargetPoint,
				SolveContext.TraceContext.DebugContext->GetSettings().DebugBlockedTraceColor,
				TEXT("MoveReject"));
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MovePointSearch"),
				FString::Printf(
					TEXT("Node[%d] rejected: no valid backtracked target"),
					NodeWindow.NodeIndex));
		}
		return false;
	}

	OutEffectivePoint = LastValidPoint;
	if (SolveContext.TraceContext.DebugContext != nullptr)
	{
		SolveContext.TraceContext.DebugContext->DrawSolverPoint(
			ERayRopeDebugDrawFlags::MoveCandidates,
			OutEffectivePoint,
			SolveContext.TraceContext.DebugContext->GetSettings().DebugAcceptedColor,
			TEXT("BacktrackedMove"));
		SolveContext.TraceContext.DebugContext->RecordSolverEvent(
			TEXT("MovePointSearch"),
			FString::Printf(
				TEXT("Node[%d] accepted backtracked target Location=%s"),
				NodeWindow.NodeIndex,
				*OutEffectivePoint.ToCompactString()));
	}
	return true;
}
}
