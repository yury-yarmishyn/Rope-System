#include "RayRopeMoveSolver.h"

#include "Helpers/RayRopeNodeSynchronizer.h"
#include "Helpers/RayRopeSurfaceGeometry.h"
#include "Helpers/RayRopeTransitionValidator.h"

namespace
{
struct FMoveSurfaceHits
{
	FHitResult ForwardSurfaceHit;
	FHitResult ReverseSurfaceHit;
};

struct FMoveRail
{
	FVector Origin = FVector::ZeroVector;
	FVector Direction = FVector::ZeroVector;
};

struct FMoveRailSearchContext
{
	const FRayRopeMoveSettings& MoveSettings;
	const FRayRopeNode& PrevNode;
	const FRayRopeNode& NextNode;
	const FMoveRail Rail;
	float CurrentRailParameter = 0.f;
};

struct FMoveResult
{
	FVector EffectivePoint = FVector::ZeroVector;
	FRayRopeBuiltNodeBuffer BeforeCurrentNodes;
	FRayRopeBuiltNodeBuffer AfterCurrentNodes;
};

struct FMoveSurfaceSearchBounds
{
	FVector ClearStartPoint;
	FVector BlockedStartPoint;
	FVector ClearEndPoint;
	FVector BlockedEndPoint;

	bool IsResolved(float SearchEpsilonSquared) const
	{
		return FVector::DistSquared(ClearStartPoint, BlockedStartPoint) <= SearchEpsilonSquared &&
			FVector::DistSquared(ClearEndPoint, BlockedEndPoint) <= SearchEpsilonSquared;
	}

	FVector GetTraceStart() const
	{
		return (ClearStartPoint + BlockedStartPoint) * 0.5f;
	}

	FVector GetTraceEnd() const
	{
		return (ClearEndPoint + BlockedEndPoint) * 0.5f;
	}

	void MarkBlocked(const FVector& TraceStart, const FVector& TraceEnd)
	{
		BlockedStartPoint = TraceStart;
		BlockedEndPoint = TraceEnd;
	}

	void MarkClear(const FVector& TraceStart, const FVector& TraceEnd)
	{
		ClearStartPoint = TraceStart;
		ClearEndPoint = TraceEnd;
	}
};

bool TryFindEffectiveMove(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	FMoveResult& OutResult);

bool TryBuildMoveRail(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	FMoveRail& OutRail);

bool TryFindRailDirectionSurfaceHits(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	FMoveSurfaceHits& OutHits);

bool TryTraceMoveHit(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& StartNode,
	const FRayRopeNode& EndNode,
	FHitResult& OutHit);

bool TryBuildRailFromSurfaceHits(
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& CurrentNode,
	const FHitResult& FirstSurfaceHit,
	const FHitResult& SecondSurfaceHit,
	FMoveRail& OutRail);

bool TryFindEffectivePointOnRail(
	const FRayRopeMoveSettings& MoveSettings,
	const FMoveRail& Rail,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	FVector& OutEffectivePoint);

bool TryFindValidEffectivePoint(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FVector& TargetPoint,
	FVector& OutEffectivePoint);

bool TryBuildMoveWithNewNodes(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FVector& TargetPoint,
	FMoveResult& OutResult);

bool IsReachableMoveTarget(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FVector& CandidatePoint);

bool IsValidMovePoint(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FVector& CandidatePoint);

bool TryQueueMoveInsertions(
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& MovedNode,
	const FRayRopeNode& NextNode,
	int32 NodeIndex,
	FMoveResult& MoveResult,
	FRayRopePendingNodeInsertionBuffer& PendingInsertions);

FRayRopeTransitionValidationSettings BuildMoveTransitionValidationSettings(
	const FRayRopeMoveSettings& MoveSettings);

float CalculateMoveDistanceSum(
	const FRayRopeNode& PrevNode,
	const FVector& MiddleLocation,
	const FRayRopeNode& NextNode);

bool IsMoveImprovementSignificant(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FVector& CandidatePoint);

FRayRopeNode CreateMovePointNode(const FVector& WorldLocation);

FRayRopeNode CreateMoveCandidateNode(
	const FRayRopeNode& SourceNode,
	const FVector& WorldLocation);

FVector GetRailPoint(const FMoveRailSearchContext& SearchContext, float RailParameter)
{
	return SearchContext.Rail.Origin + SearchContext.Rail.Direction * RailParameter;
}

float CalculateRailDistanceSum(const FMoveRailSearchContext& SearchContext, float RailParameter)
{
	const FVector CandidateLocation = GetRailPoint(SearchContext, RailParameter);
	return FVector::Dist(SearchContext.PrevNode.WorldLocation, CandidateLocation) +
		FVector::Dist(CandidateLocation, SearchContext.NextNode.WorldLocation);
}

float FindBestRailParameter(
	const FMoveRailSearchContext& SearchContext,
	float LeftRailParameter,
	float RightRailParameter)
{
	float BestRailParameter = SearchContext.CurrentRailParameter;
	float BestDistanceSum = CalculateRailDistanceSum(SearchContext, BestRailParameter);
	const float SearchEpsilon = FMath::Max(
		SearchContext.MoveSettings.EffectivePointSearchTolerance,
		SearchContext.MoveSettings.MoveSolverTolerance);

	const int32 MaxRailPointSearchIterations =
		FMath::Max(0, SearchContext.MoveSettings.MaxEffectivePointSearchIterations);
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

		FRayRopePendingNodeInsertionBuffer PendingInsertions;
		PendingInsertions.Reserve(Segment.Nodes.Num() * 2);

		for (int32 NodeIndex = FirstNodeIndex; NodeIndex != LastNodeIndex; NodeIndex += NodeIndexStep)
		{
			FRayRopeNode& CurrentNode = Segment.Nodes[NodeIndex];
			if (CurrentNode.NodeType == ERayRopeNodeType::Anchor)
			{
				continue;
			}

			const FRayRopeNode& PrevNode = Segment.Nodes[NodeIndex - 1];
			const FRayRopeNode& NextNode = Segment.Nodes[NodeIndex + 1];

			FMoveResult MoveResult;
			if (!TryFindEffectiveMove(
				TraceContext,
				MoveSettings,
				PrevNode,
				CurrentNode,
				NextNode,
				MoveResult))
			{
				continue;
			}

			FRayRopeNode MovedNode = CreateMoveCandidateNode(CurrentNode, MoveResult.EffectivePoint);
			FRayRopeNodeSynchronizer::CacheAttachedActorOffset(MovedNode);
			if (!TryQueueMoveInsertions(
				MoveSettings,
				PrevNode,
				MovedNode,
				NextNode,
				NodeIndex,
				MoveResult,
				PendingInsertions))
			{
				continue;
			}

			CurrentNode = MoveTemp(MovedNode);
		}

		FRayRopeNodeBuilder::ApplyPendingInsertions(Segment, PendingInsertions);
	}
}

namespace
{
bool TryFindEffectiveMove(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	FMoveResult& OutResult)
{
	OutResult = FMoveResult();

	FMoveRail Rail;
	if (!TryBuildMoveRail(
		TraceContext,
		MoveSettings,
		PrevNode,
		CurrentNode,
		NextNode,
		Rail))
	{
		return false;
	}

	FVector TargetPoint = FVector::ZeroVector;
	if (!TryFindEffectivePointOnRail(
		MoveSettings,
		Rail,
		PrevNode,
		CurrentNode,
		NextNode,
		TargetPoint))
	{
		return false;
	}

	if (!TryFindValidEffectivePoint(
		TraceContext,
		MoveSettings,
		PrevNode,
		CurrentNode,
		NextNode,
		TargetPoint,
		OutResult.EffectivePoint))
	{
		return TryBuildMoveWithNewNodes(
			TraceContext,
			MoveSettings,
			PrevNode,
			CurrentNode,
			NextNode,
			TargetPoint,
			OutResult);
	}

	return true;
}

bool TryBuildMoveRail(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	FMoveRail& OutRail)
{
	FMoveSurfaceHits SurfaceHits;
	if (!TryFindRailDirectionSurfaceHits(
		TraceContext,
		MoveSettings,
		PrevNode,
		CurrentNode,
		NextNode,
		SurfaceHits))
	{
		return false;
	}

	if (!TryBuildRailFromSurfaceHits(
		MoveSettings,
		CurrentNode,
		SurfaceHits.ForwardSurfaceHit,
		SurfaceHits.ReverseSurfaceHit,
		OutRail))
	{
		return false;
	}

	return true;
}

bool TryFindRailDirectionSurfaceHits(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	FMoveSurfaceHits& OutHits)
{
	OutHits = FMoveSurfaceHits();

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

	FMoveSurfaceSearchBounds SearchBounds{
		CurrentNode.WorldLocation,
		PrevNode.WorldLocation,
		CurrentNode.WorldLocation,
		NextNode.WorldLocation
	};

	FHitResult LastForwardSurfaceHit;
	FVector LastBlockedTraceStart = FVector::ZeroVector;
	FVector LastBlockedTraceEnd = FVector::ZeroVector;

	const FRayRopeTraceContext MoveTraceContext = FRayRopeTrace::MakeTraceContextIgnoringEndpointActors(
		TraceContext,
		&PrevNode,
		&NextNode);

	const int32 MaxSurfaceHitSearchIterations =
		FMath::Max(1, MoveSettings.MaxEffectivePointSearchIterations);
	for (int32 Iteration = 0; Iteration < MaxSurfaceHitSearchIterations; ++Iteration)
	{
		if (SearchBounds.IsResolved(SearchEpsilonSquared))
		{
			break;
		}

		const FVector TraceStart = SearchBounds.GetTraceStart();
		const FVector TraceEnd = SearchBounds.GetTraceEnd();
		const FRayRopeNode TraceStartNode = CreateMovePointNode(TraceStart);
		const FRayRopeNode TraceEndNode = CreateMovePointNode(TraceEnd);

		FHitResult TraceHit;
		if (TryTraceMoveHit(
			MoveTraceContext,
			MoveSettings,
			TraceStartNode,
			TraceEndNode,
			TraceHit))
		{
			LastForwardSurfaceHit = TraceHit;
			LastBlockedTraceStart = TraceStart;
			LastBlockedTraceEnd = TraceEnd;

			// Keep the search bounded by the last blocking line; farther segment halves cannot improve the hit.
			SearchBounds.MarkBlocked(TraceStart, TraceEnd);
			continue;
		}

		SearchBounds.MarkClear(TraceStart, TraceEnd);
	}

	if (!LastForwardSurfaceHit.bBlockingHit)
	{
		return false;
	}

	const FRayRopeNode ReverseTraceStartNode = CreateMovePointNode(LastBlockedTraceEnd);
	const FRayRopeNode ReverseTraceEndNode = CreateMovePointNode(LastBlockedTraceStart);
	FHitResult ReverseSurfaceHit;
	if (!TryTraceMoveHit(
		MoveTraceContext,
		MoveSettings,
		ReverseTraceStartNode,
		ReverseTraceEndNode,
		ReverseSurfaceHit))
	{
		return false;
	}

	OutHits.ForwardSurfaceHit = LastForwardSurfaceHit;
	OutHits.ReverseSurfaceHit = ReverseSurfaceHit;
	return true;
}

bool TryTraceMoveHit(
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

bool TryBuildRailFromSurfaceHits(
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& CurrentNode,
	const FHitResult& FirstSurfaceHit,
	const FHitResult& SecondSurfaceHit,
	FMoveRail& OutRail)
{
	OutRail = FMoveRail();

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

	const FVector RailDirectionNormal = RailDirection.GetSafeNormal();
	if (RailDirectionNormal.IsNearlyZero())
	{
		return false;
	}

	FVector OffsetDirection = FirstNormal + SecondNormal;
	if (OffsetDirection.IsNearlyZero())
	{
		return false;
	}

	OffsetDirection = OffsetDirection.GetSafeNormal();
	const FRayRopeSpan CurrentPointSpan{&CurrentNode, &CurrentNode};
	const FVector SurfaceRailPoint = FRayRopeSurfaceGeometry::CalculateRedirectLocation(
		CurrentPointSpan,
		FirstSurfaceHit,
		&SecondSurfaceHit);
	const float SurfaceOffset = FMath::Max(MoveSettings.SurfaceOffset, KINDA_SMALL_NUMBER);

	OutRail.Origin = SurfaceRailPoint + OffsetDirection * SurfaceOffset;
	OutRail.Direction = RailDirectionNormal;
	return !OutRail.Origin.ContainsNaN();
}

bool TryFindEffectivePointOnRail(
	const FRayRopeMoveSettings& MoveSettings,
	const FMoveRail& Rail,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	FVector& OutEffectivePoint)
{
	if (Rail.Origin.ContainsNaN() || Rail.Direction.IsNearlyZero())
	{
		return false;
	}

	const float CurrentRailParameter =
		FVector::DotProduct(CurrentNode.WorldLocation - Rail.Origin, Rail.Direction);
	const float PrevRailParameter =
		FVector::DotProduct(PrevNode.WorldLocation - Rail.Origin, Rail.Direction);
	const float NextRailParameter =
		FVector::DotProduct(NextNode.WorldLocation - Rail.Origin, Rail.Direction);
	if (!FMath::IsFinite(CurrentRailParameter) ||
		!FMath::IsFinite(PrevRailParameter) ||
		!FMath::IsFinite(NextRailParameter))
	{
		return false;
	}

	const FMoveRailSearchContext SearchContext{
		MoveSettings,
		PrevNode,
		NextNode,
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
	return !OutEffectivePoint.ContainsNaN();
}

bool TryFindValidEffectivePoint(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FVector& TargetPoint,
	FVector& OutEffectivePoint)
{
	OutEffectivePoint = FVector::ZeroVector;
	if (TargetPoint.ContainsNaN())
	{
		return false;
	}

	if (IsValidMovePoint(
		TraceContext,
		MoveSettings,
		PrevNode,
		CurrentNode,
		NextNode,
		TargetPoint))
	{
		OutEffectivePoint = TargetPoint;
		return true;
	}

	if (!FRayRopeTrace::IsValidFreePoint(
		TraceContext,
		CurrentNode.WorldLocation))
	{
		return false;
	}

	FVector LastValidPoint = CurrentNode.WorldLocation;
	FVector LastInvalidPoint = TargetPoint;
	bool bFoundValidPoint = false;
	const int32 MaxSearchIterations =
		FMath::Max(1, MoveSettings.MaxEffectivePointSearchIterations);
	for (int32 Iteration = 0; Iteration < MaxSearchIterations; ++Iteration)
	{
		const FVector CandidatePoint = (LastValidPoint + LastInvalidPoint) * 0.5f;
		if (FVector::DistSquared(LastValidPoint, LastInvalidPoint) <=
			FMath::Square(MoveSettings.MoveSolverTolerance))
		{
			break;
		}

		if (IsValidMovePoint(
			TraceContext,
			MoveSettings,
			PrevNode,
			CurrentNode,
			NextNode,
			CandidatePoint))
		{
			LastValidPoint = CandidatePoint;
			bFoundValidPoint = true;
			continue;
		}

		LastInvalidPoint = CandidatePoint;
	}

	if (!bFoundValidPoint ||
		LastValidPoint.Equals(CurrentNode.WorldLocation, MoveSettings.MoveSolverTolerance))
	{
		return false;
	}

	OutEffectivePoint = LastValidPoint;
	return true;
}

bool TryBuildMoveWithNewNodes(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FVector& TargetPoint,
	FMoveResult& OutResult)
{
	OutResult = FMoveResult();

	if (!IsReachableMoveTarget(
		TraceContext,
		MoveSettings,
		PrevNode,
		CurrentNode,
		NextNode,
		TargetPoint))
	{
		return false;
	}

	const FRayRopeNode CandidateNode = CreateMoveCandidateNode(CurrentNode, TargetPoint);
	const FRayRopeSpan PrevCurrentSpan{&PrevNode, &CurrentNode};
	const FRayRopeSpan CurrentNextSpan{&CurrentNode, &NextNode};
	const FRayRopeSpan PrevCandidateSpan{&PrevNode, &CandidateNode};
	const FRayRopeSpan CandidateNextSpan{&CandidateNode, &NextNode};

	const bool bPrevCandidateBlocked =
		FRayRopeTrace::HasBlockingSpanHit(TraceContext, PrevCandidateSpan);
	const bool bCandidateNextBlocked =
		FRayRopeTrace::HasBlockingSpanHit(TraceContext, CandidateNextSpan);
	if (!bPrevCandidateBlocked && !bCandidateNextBlocked)
	{
		return false;
	}

	if (bPrevCandidateBlocked &&
		!FRayRopeNodeBuilder::BuildNodesForSpanTransition(
			TraceContext,
			MoveSettings.NodeBuildSettings,
			PrevCandidateSpan,
			PrevCurrentSpan,
			OutResult.BeforeCurrentNodes))
	{
		return false;
	}

	if (bCandidateNextBlocked &&
		!FRayRopeNodeBuilder::BuildNodesForSpanTransition(
			TraceContext,
			MoveSettings.NodeBuildSettings,
			CandidateNextSpan,
			CurrentNextSpan,
			OutResult.AfterCurrentNodes))
	{
		return false;
	}

	if (OutResult.BeforeCurrentNodes.Num() == 0 &&
		OutResult.AfterCurrentNodes.Num() == 0)
	{
		return false;
	}

	OutResult.EffectivePoint = TargetPoint;
	return true;
}

bool IsReachableMoveTarget(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FVector& CandidatePoint)
{
	if (CandidatePoint.ContainsNaN() ||
		!FRayRopeTrace::IsValidFreePoint(TraceContext, CandidatePoint))
	{
		return false;
	}

	const float MinNodeDistanceSquared =
		FMath::Square(FMath::Max(MoveSettings.MoveSolverTolerance, KINDA_SMALL_NUMBER));
	if (FVector::DistSquared(CandidatePoint, PrevNode.WorldLocation) <= MinNodeDistanceSquared ||
		FVector::DistSquared(CandidatePoint, NextNode.WorldLocation) <= MinNodeDistanceSquared)
	{
		return false;
	}

	if (!IsMoveImprovementSignificant(
		TraceContext,
		MoveSettings,
		PrevNode,
		CurrentNode,
		NextNode,
		CandidatePoint))
	{
		return false;
	}

	const FRayRopeNodeTransition Transition{
		&PrevNode,
		&CurrentNode,
		&NextNode,
		CandidatePoint
	};
	return FRayRopeTransitionValidator::IsTransitionNodePathClear(
		TraceContext,
		BuildMoveTransitionValidationSettings(MoveSettings),
		Transition);
}

bool IsValidMovePoint(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FVector& CandidatePoint)
{
	if (CandidatePoint.ContainsNaN())
	{
		return false;
	}

	const float MinNodeDistanceSquared =
		FMath::Square(FMath::Max(MoveSettings.MoveSolverTolerance, KINDA_SMALL_NUMBER));
	if (FVector::DistSquared(CandidatePoint, PrevNode.WorldLocation) <= MinNodeDistanceSquared ||
		FVector::DistSquared(CandidatePoint, NextNode.WorldLocation) <= MinNodeDistanceSquared)
	{
		return false;
	}

	if (!IsMoveImprovementSignificant(
		TraceContext,
		MoveSettings,
		PrevNode,
		CurrentNode,
		NextNode,
		CandidatePoint))
	{
		return false;
	}

	const FRayRopeNodeTransition Transition{
		&PrevNode,
		&CurrentNode,
		&NextNode,
		CandidatePoint
	};
	return FRayRopeTransitionValidator::IsNodeTransitionClear(
		TraceContext,
		BuildMoveTransitionValidationSettings(MoveSettings),
		Transition);
}

bool TryQueueMoveInsertions(
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& MovedNode,
	const FRayRopeNode& NextNode,
	int32 NodeIndex,
	FMoveResult& MoveResult,
	FRayRopePendingNodeInsertionBuffer& PendingInsertions)
{
	if (MoveResult.BeforeCurrentNodes.Num() == 0 &&
		MoveResult.AfterCurrentNodes.Num() == 0)
	{
		return true;
	}

	FRayRopePendingNodeInsertionBuffer UpdatedPendingInsertions = PendingInsertions;

	if (MoveResult.BeforeCurrentNodes.Num() > 0 &&
		!FRayRopeNodeBuilder::CanInsertNodes(
			MoveSettings.NodeBuildSettings,
			PrevNode,
			MovedNode,
			NodeIndex,
			MoveResult.BeforeCurrentNodes,
			UpdatedPendingInsertions))
	{
		return false;
	}

	FRayRopeNodeBuilder::AppendPendingInsertions(
		NodeIndex,
		MoveResult.BeforeCurrentNodes,
		UpdatedPendingInsertions);

	if (MoveResult.AfterCurrentNodes.Num() > 0 &&
		!FRayRopeNodeBuilder::CanInsertNodes(
			MoveSettings.NodeBuildSettings,
			MovedNode,
			NextNode,
			NodeIndex + 1,
			MoveResult.AfterCurrentNodes,
			UpdatedPendingInsertions))
	{
		return false;
	}

	FRayRopeNodeBuilder::AppendPendingInsertions(
		NodeIndex + 1,
		MoveResult.AfterCurrentNodes,
		UpdatedPendingInsertions);

	PendingInsertions = MoveTemp(UpdatedPendingInsertions);
	return true;
}

FRayRopeTransitionValidationSettings BuildMoveTransitionValidationSettings(
	const FRayRopeMoveSettings& MoveSettings)
{
	FRayRopeTransitionValidationSettings ValidationSettings;
	ValidationSettings.SolverTolerance = MoveSettings.MoveSolverTolerance;
	ValidationSettings.MaxTransitionValidationIterations =
		FMath::Max(0, MoveSettings.MaxEffectivePointSearchIterations);
	return ValidationSettings;
}

float CalculateMoveDistanceSum(
	const FRayRopeNode& PrevNode,
	const FVector& MiddleLocation,
	const FRayRopeNode& NextNode)
{
	return FVector::Dist(PrevNode.WorldLocation, MiddleLocation) +
		FVector::Dist(MiddleLocation, NextNode.WorldLocation);
}

bool IsMoveImprovementSignificant(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FVector& CandidatePoint)
{
	if (!FRayRopeTrace::IsValidFreePoint(TraceContext, CurrentNode.WorldLocation))
	{
		return true;
	}

	const float CurrentDistanceSum = CalculateMoveDistanceSum(
		PrevNode,
		CurrentNode.WorldLocation,
		NextNode);
	const float CandidateDistanceSum = CalculateMoveDistanceSum(
		PrevNode,
		CandidatePoint,
		NextNode);
	const float RequiredImprovement =
		FMath::Max(MoveSettings.MoveSolverTolerance, KINDA_SMALL_NUMBER);
	return CandidateDistanceSum + RequiredImprovement < CurrentDistanceSum;
}

FRayRopeNode CreateMovePointNode(const FVector& WorldLocation)
{
	FRayRopeNode Node;
	Node.NodeType = ERayRopeNodeType::Redirect;
	Node.WorldLocation = WorldLocation;
	return Node;
}

FRayRopeNode CreateMoveCandidateNode(
	const FRayRopeNode& SourceNode,
	const FVector& WorldLocation)
{
	FRayRopeNode CandidateNode = SourceNode;
	CandidateNode.WorldLocation = WorldLocation;
	return CandidateNode;
}
}
