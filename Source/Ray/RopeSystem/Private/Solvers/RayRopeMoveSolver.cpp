#include "RayRopeMoveSolver.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Helpers/RayRopeTrace.h"
#include "RayRopeNodeResolver.h"

namespace
{
struct FMoveRailSearchContext
{
	const FRayRopeTraceContext& TraceContext;
	const FRayRopeMoveSettings& MoveSettings;
	const FRayRopeNode& PrevNode;
	const FRayRopeNode& CurrentNode;
	const FRayRopeNode& NextNode;
	const FVector RailDirection;
};

FRayRopeNode CreateTransientMoveNode(const FVector& WorldLocation)
{
	FRayRopeNode Node;
	Node.NodeType = ERayRopeNodeType::Redirect;
	Node.WorldLocation = WorldLocation;
	return Node;
}

FVector GetRailPoint(const FMoveRailSearchContext& SearchContext, float RailParameter)
{
	return SearchContext.CurrentNode.WorldLocation + SearchContext.RailDirection * RailParameter;
}

bool HasBlockingHitBetween(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNode& StartNode,
	const FRayRopeNode& EndNode)
{
	if (!IsValid(TraceContext.World) ||
		StartNode.WorldLocation.ContainsNaN() ||
		EndNode.WorldLocation.ContainsNaN() ||
		StartNode.WorldLocation.Equals(EndNode.WorldLocation, KINDA_SMALL_NUMBER))
	{
		return false;
	}

	FHitResult Hit;
	const FRayRopeSpan TraceSpan{&StartNode, &EndNode};
	return FRayRopeTrace::TryTraceSpan(TraceContext, TraceSpan, Hit);
}

bool IsRailPointBlocked(const FMoveRailSearchContext& SearchContext, float RailParameter)
{
	const FVector CandidateLocation = GetRailPoint(SearchContext, RailParameter);
	if (FRayRopeTrace::IsPointInsideGeometry(SearchContext.TraceContext, CandidateLocation))
	{
		return true;
	}

	const FRayRopeNode CandidateNode = CreateTransientMoveNode(CandidateLocation);

	return HasBlockingHitBetween(SearchContext.TraceContext, SearchContext.PrevNode, CandidateNode) ||
		HasBlockingHitBetween(SearchContext.TraceContext, CandidateNode, SearchContext.NextNode);
}

float CalculateRailDistanceSum(const FMoveRailSearchContext& SearchContext, float RailParameter)
{
	const FVector CandidateLocation = GetRailPoint(SearchContext, RailParameter);
	return FVector::Dist(SearchContext.PrevNode.WorldLocation, CandidateLocation) +
		FVector::Dist(CandidateLocation, SearchContext.NextNode.WorldLocation);
}

float RefineReachableRailLimit(
	const FMoveRailSearchContext& SearchContext,
	float LastValidRailParameter,
	float FirstBlockedRailParameter)
{
	const int32 MaxIterations = FMath::Max(0, SearchContext.MoveSettings.MaxEffectivePointSearchIterations);
	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
	{
		if (FMath::Abs(FirstBlockedRailParameter - LastValidRailParameter) <=
			SearchContext.MoveSettings.MoveSolverTolerance)
		{
			break;
		}

		const float MidRailParameter = (LastValidRailParameter + FirstBlockedRailParameter) * 0.5f;
		if (IsRailPointBlocked(SearchContext, MidRailParameter))
		{
			FirstBlockedRailParameter = MidRailParameter;
		}
		else
		{
			LastValidRailParameter = MidRailParameter;
		}
	}

	return LastValidRailParameter;
}

bool TryClampRailParameterToReachablePath(
	const FMoveRailSearchContext& SearchContext,
	float TargetRailParameter,
	float& OutRailParameter)
{
	OutRailParameter = 0.f;

	if (IsRailPointBlocked(SearchContext, 0.f))
	{
		return false;
	}

	if (FMath::IsNearlyZero(TargetRailParameter, SearchContext.MoveSettings.MoveSolverTolerance))
	{
		return true;
	}

	float LastValidRailParameter = 0.f;
	const int32 StepCount = FMath::Max(1, SearchContext.MoveSettings.MaxEffectivePointSearchIterations);
	for (int32 StepIndex = 1; StepIndex <= StepCount; ++StepIndex)
	{
		const float SampleRailParameter =
			TargetRailParameter * static_cast<float>(StepIndex) / static_cast<float>(StepCount);

		if (IsRailPointBlocked(SearchContext, SampleRailParameter))
		{
			OutRailParameter = RefineReachableRailLimit(
				SearchContext,
				LastValidRailParameter,
				SampleRailParameter);
			return true;
		}

		LastValidRailParameter = SampleRailParameter;
	}

	OutRailParameter = TargetRailParameter;
	return true;
}

float FindBestRailParameter(
	const FMoveRailSearchContext& SearchContext,
	float LeftRailParameter,
	float RightRailParameter)
{
	float BestRailParameter = 0.f;
	float BestDistanceSum = CalculateRailDistanceSum(SearchContext, BestRailParameter);
	const float SearchEpsilon = FMath::Max(
		SearchContext.MoveSettings.EffectivePointSearchTolerance,
		SearchContext.MoveSettings.MoveSolverTolerance);

	const int32 MaxIterations = FMath::Max(0, SearchContext.MoveSettings.MaxEffectivePointSearchIterations);
	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
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

void FRayRopeMoveSolver::MoveSegment(
	const FRayRopeTraceSettings& TraceSettings,
	const FRayRopeMoveSettings& MoveSettings,
	FRayRopeSegment& Segment)
{
	if (Segment.Nodes.Num() < 3)
	{
		return;
	}

	const FRayRopeTraceContext TraceContext = FRayRopeTrace::MakeTraceContext(
		TraceSettings,
		FCollisionQueryParams(SCENE_QUERY_STAT(RayRopeMoveTrace)));

	const int32 MaxMoveIterations = FMath::Max(0, MoveSettings.MaxMoveIterations);
	for (int32 MoveIteration = 0; MoveIteration < MaxMoveIterations; ++MoveIteration)
	{
		const bool bMoveForward = MoveIteration % 2 == 0;
		const int32 FirstNodeIndex = bMoveForward ? 1 : Segment.Nodes.Num() - 2;
		const int32 LastNodeIndex = bMoveForward ? Segment.Nodes.Num() - 1 : 0;
		const int32 NodeIndexStep = bMoveForward ? 1 : -1;

		for (int32 NodeIndex = FirstNodeIndex; NodeIndex != LastNodeIndex; NodeIndex += NodeIndexStep)
		{
			FRayRopeNode& CurrentNode = Segment.Nodes[NodeIndex];
			if (CurrentNode.NodeType == ERayRopeNodeType::Anchor)
			{
				continue;
			}

			const FRayRopeNode& PrevNode = Segment.Nodes[NodeIndex - 1];
			const FRayRopeNode& NextNode = Segment.Nodes[NodeIndex + 1];

			FVector EffectivePoint = FVector::ZeroVector;
			if (!TryFindEffectiveMovePoint(
				TraceContext,
				MoveSettings,
				PrevNode,
				CurrentNode,
				NextNode,
				EffectivePoint))
			{
				continue;
			}

			CurrentNode.WorldLocation = EffectivePoint;
			FRayRopeNodeResolver::CacheAttachedActorOffset(CurrentNode);
		}
	}
}

bool FRayRopeMoveSolver::TryFindEffectiveMovePoint(
	const FRayRopeTraceSettings& TraceSettings,
	const FRayRopeMoveSettings& MoveSettings,
	const FVector& PrevLocation,
	const FVector& CurrentLocation,
	const FVector& NextLocation,
	FVector& OutEffectivePoint)
{
	const FRayRopeTraceContext TraceContext = FRayRopeTrace::MakeTraceContext(
		TraceSettings,
		FCollisionQueryParams(SCENE_QUERY_STAT(RayRopeMoveTrace)));

	const FRayRopeNode PrevNode = CreateMovePointNode(PrevLocation);
	const FRayRopeNode CurrentNode = CreateMovePointNode(CurrentLocation);
	const FRayRopeNode NextNode = CreateMovePointNode(NextLocation);

	return TryFindEffectiveMovePoint(
		TraceContext,
		MoveSettings,
		PrevNode,
		CurrentNode,
		NextNode,
		OutEffectivePoint);
}

bool FRayRopeMoveSolver::TryFindEffectiveMovePoint(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	FVector& OutEffectivePoint)
{
	FVector RailDirection = FVector::ZeroVector;
	if (!TryBuildMoveRailDirection(
		TraceContext,
		MoveSettings,
		PrevNode,
		CurrentNode,
		NextNode,
		RailDirection))
	{
		return false;
	}

	FVector EffectivePoint = FVector::ZeroVector;
	if (!TryFindEffectivePointOnRail(
		TraceContext,
		MoveSettings,
		RailDirection,
		PrevNode,
		CurrentNode,
		NextNode,
		EffectivePoint))
	{
		return false;
	}

	if (EffectivePoint.ContainsNaN())
	{
		return false;
	}

	OutEffectivePoint = EffectivePoint;
	return true;
}

bool FRayRopeMoveSolver::TryBuildMoveRailDirection(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	FVector& OutRailDirection)
{
	FHitResult PrevCurrentToCurrentNextHit;
	FHitResult CurrentNextToPrevCurrentHit;
	if (!TryFindRailDirectionSurfaceHits(
		TraceContext,
		MoveSettings,
		PrevNode,
		CurrentNode,
		NextNode,
		PrevCurrentToCurrentNextHit,
		CurrentNextToPrevCurrentHit))
	{
		return false;
	}

	if (!TryFindPlaneIntersectionRailDirection(
		MoveSettings,
		PrevCurrentToCurrentNextHit,
		CurrentNextToPrevCurrentHit,
		OutRailDirection))
	{
		return false;
	}

	return true;
}

bool FRayRopeMoveSolver::TryFindRailDirectionSurfaceHits(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	FHitResult& OutPrevCurrentToCurrentNextHit,
	FHitResult& OutCurrentNextToPrevCurrentHit)
{
	OutPrevCurrentToCurrentNextHit = FHitResult();
	OutCurrentNextToPrevCurrentHit = FHitResult();

	if (!IsValid(TraceContext.World) ||
		PrevNode.WorldLocation.ContainsNaN() ||
		CurrentNode.WorldLocation.ContainsNaN() ||
		NextNode.WorldLocation.ContainsNaN())
	{
		return false;
	}

	const float SearchEpsilon = FMath::Max(MoveSettings.MoveSolverTolerance, KINDA_SMALL_NUMBER);
	const float SearchEpsilonSquared = FMath::Square(SearchEpsilon);
	if (FVector::DistSquared(PrevNode.WorldLocation, CurrentNode.WorldLocation) <= SearchEpsilonSquared ||
		FVector::DistSquared(CurrentNode.WorldLocation, NextNode.WorldLocation) <= SearchEpsilonSquared)
	{
		return false;
	}

	FVector PrevCurrentNoHitPoint = CurrentNode.WorldLocation;
	FVector PrevCurrentHitPoint = PrevNode.WorldLocation;
	FVector CurrentNextNoHitPoint = CurrentNode.WorldLocation;
	FVector CurrentNextHitPoint = NextNode.WorldLocation;

	FHitResult LastHit;
	FVector LastTraceStart = FVector::ZeroVector;
	FVector LastTraceEnd = FVector::ZeroVector;

	const int32 MaxIterations = FMath::Max(1, MoveSettings.MaxEffectivePointSearchIterations);
	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
	{
		if (FVector::DistSquared(PrevCurrentNoHitPoint, PrevCurrentHitPoint) <= SearchEpsilonSquared ||
			FVector::DistSquared(CurrentNextNoHitPoint, CurrentNextHitPoint) <= SearchEpsilonSquared)
		{
			break;
		}

		const FVector TraceStart = (PrevCurrentNoHitPoint + PrevCurrentHitPoint) * 0.5f;
		const FVector TraceEnd = (CurrentNextNoHitPoint + CurrentNextHitPoint) * 0.5f;
		const FRayRopeNode TraceStartNode = CreateMovePointNode(TraceStart);
		const FRayRopeNode TraceEndNode = CreateMovePointNode(TraceEnd);

		FHitResult TraceHit;
		if (TryTraceMoveHit(
			TraceContext,
			MoveSettings,
			TraceStartNode,
			TraceEndNode,
			TraceHit))
		{
			LastHit = TraceHit;
			LastTraceStart = TraceStart;
			LastTraceEnd = TraceEnd;

			// Keep the search bounded by the last blocking line; farther segment halves cannot improve the hit.
			PrevCurrentHitPoint = TraceStart;
			CurrentNextHitPoint = TraceEnd;
			continue;
		}

		PrevCurrentNoHitPoint = TraceStart;
		CurrentNextNoHitPoint = TraceEnd;
	}

	if (!LastHit.bBlockingHit)
	{
		return false;
	}

	const FRayRopeNode ReverseTraceStartNode = CreateMovePointNode(LastTraceEnd);
	const FRayRopeNode ReverseTraceEndNode = CreateMovePointNode(LastTraceStart);
	FHitResult ReverseHit;
	if (!TryTraceMoveHit(
		TraceContext,
		MoveSettings,
		ReverseTraceStartNode,
		ReverseTraceEndNode,
		ReverseHit))
	{
		return false;
	}

	OutPrevCurrentToCurrentNextHit = LastHit;
	OutCurrentNextToPrevCurrentHit = ReverseHit;
	return true;
}

bool FRayRopeMoveSolver::TryTraceMoveHit(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& StartNode,
	const FRayRopeNode& EndNode,
	FHitResult& OutHit)
{
	OutHit = FHitResult();

	if (!IsValid(TraceContext.World) ||
		StartNode.WorldLocation.ContainsNaN() ||
		EndNode.WorldLocation.ContainsNaN() ||
		StartNode.WorldLocation.Equals(EndNode.WorldLocation, MoveSettings.MoveSolverTolerance))
	{
		return false;
	}

	const FRayRopeSpan TraceSpan{&StartNode, &EndNode};
	if (!FRayRopeTrace::TryTraceSpan(TraceContext, TraceSpan, OutHit))
	{
		return false;
	}

	if (OutHit.ImpactPoint.ContainsNaN() ||
		OutHit.ImpactNormal.GetSafeNormal().IsNearlyZero())
	{
		OutHit = FHitResult();
		return false;
	}

	return true;
}

bool FRayRopeMoveSolver::TryFindPlaneIntersectionRailDirection(
	const FRayRopeMoveSettings& MoveSettings,
	const FHitResult& FirstSurfaceHit,
	const FHitResult& SecondSurfaceHit,
	FVector& OutRailDirection)
{
	if (!FirstSurfaceHit.bBlockingHit || !SecondSurfaceHit.bBlockingHit)
	{
		return false;
	}

	const FVector FirstNormal = FirstSurfaceHit.ImpactNormal.GetSafeNormal();
	const FVector SecondNormal = SecondSurfaceHit.ImpactNormal.GetSafeNormal();
	if (FirstNormal.IsNearlyZero() || SecondNormal.IsNearlyZero())
	{
		return false;
	}

	const FVector RailDirection = FVector::CrossProduct(FirstNormal, SecondNormal);
	const float RailDirectionSizeSquared = RailDirection.SizeSquared();
	if (RailDirectionSizeSquared <= FMath::Square(MoveSettings.PlaneParallelTolerance))
	{
		return false;
	}

	const float FirstPlaneDistance =
		FVector::DotProduct(FirstNormal, FirstSurfaceHit.ImpactPoint);
	const float SecondPlaneDistance =
		FVector::DotProduct(SecondNormal, SecondSurfaceHit.ImpactPoint);
	const FVector LinePoint = FVector::CrossProduct(
		FirstPlaneDistance * SecondNormal - SecondPlaneDistance * FirstNormal,
		RailDirection) / RailDirectionSizeSquared;
	if (LinePoint.ContainsNaN())
	{
		return false;
	}

	OutRailDirection = RailDirection.GetSafeNormal();

	if (OutRailDirection.IsNearlyZero())
	{
		OutRailDirection = FVector::ZeroVector;
		return false;
	}

	return true;
}

bool FRayRopeMoveSolver::TryFindEffectivePointOnRail(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FVector& RailDirection,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	FVector& OutEffectivePoint)
{
	const FVector NormalizedRailDirection = RailDirection.GetSafeNormal();
	if (NormalizedRailDirection.IsNearlyZero() ||
		!IsValid(TraceContext.World))
	{
		return false;
	}

	const float PrevRailParameter =
		FVector::DotProduct(PrevNode.WorldLocation - CurrentNode.WorldLocation, NormalizedRailDirection);
	const float NextRailParameter =
		FVector::DotProduct(NextNode.WorldLocation - CurrentNode.WorldLocation, NormalizedRailDirection);
	if (!FMath::IsFinite(PrevRailParameter) || !FMath::IsFinite(NextRailParameter))
	{
		return false;
	}

	const FMoveRailSearchContext SearchContext{
		TraceContext,
		MoveSettings,
		PrevNode,
		CurrentNode,
		NextNode,
		NormalizedRailDirection
	};

	const float LeftRailParameter = FMath::Min(0.f, FMath::Min(PrevRailParameter, NextRailParameter));
	const float RightRailParameter = FMath::Max(0.f, FMath::Max(PrevRailParameter, NextRailParameter));
	const float TargetRailParameter = FindBestRailParameter(
		SearchContext,
		LeftRailParameter,
		RightRailParameter);

	float ReachableRailParameter = 0.f;
	if (!TryClampRailParameterToReachablePath(SearchContext, TargetRailParameter, ReachableRailParameter))
	{
		return false;
	}

	OutEffectivePoint = GetRailPoint(
		SearchContext,
		ReachableRailParameter);
	return !OutEffectivePoint.ContainsNaN();
}

FRayRopeNode FRayRopeMoveSolver::CreateMovePointNode(const FVector& WorldLocation)
{
	FRayRopeNode Node;
	Node.NodeType = ERayRopeNodeType::Redirect;
	Node.WorldLocation = WorldLocation;
	return Node;
}
