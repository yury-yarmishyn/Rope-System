#include "RayRopeMoveSolver.h"

#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "Helpers/RayRopeTrace.h"
#include "RayRopeNodeResolver.h"

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
			FRayRopeNodeResolver::CacheAttachActorOffset(CurrentNode);
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

	const float SearchEpsilon = FMath::Max(MoveSettings.MoveSolverEpsilon, KINDA_SMALL_NUMBER);
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
		StartNode.WorldLocation.Equals(EndNode.WorldLocation, MoveSettings.MoveSolverEpsilon))
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
	if (RailDirectionSizeSquared <= FMath::Square(MoveSettings.PlaneParallelEpsilon))
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

	FEffectivePointSearchState SearchState;
	SearchState.LeftRailParameter = FMath::Min(PrevRailParameter, NextRailParameter);
	SearchState.RightRailParameter = FMath::Max(PrevRailParameter, NextRailParameter);
	SearchState.BestCandidate.RailParameter = 0.f;
	SearchState.BestCandidate.Location = CurrentNode.WorldLocation;
	SearchState.BestCandidate.DistanceSum = CalculateEffectivePointDistanceSum(
		PrevNode.WorldLocation,
		CurrentNode.WorldLocation,
		NextNode.WorldLocation);

	const float InitialMidRailParameter =
		(SearchState.LeftRailParameter + SearchState.RightRailParameter) * 0.5f;

	FEffectivePointCandidate InitialMidCandidate;
	bool bHasBlockingHit = false;
	if (!TryEvaluateEffectivePointCandidate(
		TraceContext,
		NormalizedRailDirection,
		PrevNode,
		CurrentNode,
		NextNode,
		InitialMidRailParameter,
		InitialMidCandidate,
		bHasBlockingHit))
	{
		return false;
	}

	if (bHasBlockingHit)
	{
		OutEffectivePoint = CalculateRailPoint(
			CurrentNode.WorldLocation,
			NormalizedRailDirection,
			SearchState.BestCandidate.RailParameter);
		return true;
	}

	SaveBetterEffectivePointCandidate(InitialMidCandidate, SearchState);

	const float SearchEpsilon = FMath::Max(
		MoveSettings.EffectivePointSearchEpsilon,
		MoveSettings.MoveSolverEpsilon);
	const int32 MaxIterations = FMath::Max(0, MoveSettings.MaxEffectivePointSearchIterations);

	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
	{
		const float SegmentLength =
			FMath::Abs(SearchState.RightRailParameter - SearchState.LeftRailParameter);
		if (SegmentLength <= SearchEpsilon)
		{
			break;
		}

		const float MidRailParameter =
			(SearchState.LeftRailParameter + SearchState.RightRailParameter) * 0.5f;
		const float LeftMidRailParameter =
			(SearchState.LeftRailParameter + MidRailParameter) * 0.5f;
		const float RightMidRailParameter =
			(MidRailParameter + SearchState.RightRailParameter) * 0.5f;

		FEffectivePointCandidate LeftMidCandidate;
		if (!TryEvaluateEffectivePointCandidate(
			TraceContext,
			NormalizedRailDirection,
			PrevNode,
			CurrentNode,
			NextNode,
			LeftMidRailParameter,
			LeftMidCandidate,
			bHasBlockingHit))
		{
			return false;
		}

		if (bHasBlockingHit)
		{
			OutEffectivePoint = CalculateRailPoint(
				CurrentNode.WorldLocation,
				NormalizedRailDirection,
				SearchState.BestCandidate.RailParameter);
			return true;
		}

		SaveBetterEffectivePointCandidate(LeftMidCandidate, SearchState);

		FEffectivePointCandidate RightMidCandidate;
		if (!TryEvaluateEffectivePointCandidate(
			TraceContext,
			NormalizedRailDirection,
			PrevNode,
			CurrentNode,
			NextNode,
			RightMidRailParameter,
			RightMidCandidate,
			bHasBlockingHit))
		{
			return false;
		}

		if (bHasBlockingHit)
		{
			OutEffectivePoint = CalculateRailPoint(
				CurrentNode.WorldLocation,
				NormalizedRailDirection,
				SearchState.BestCandidate.RailParameter);
			return true;
		}

		SaveBetterEffectivePointCandidate(RightMidCandidate, SearchState);

		if (LeftMidCandidate.DistanceSum <= RightMidCandidate.DistanceSum)
		{
			SearchState.RightRailParameter = MidRailParameter;
		}
		else
		{
			SearchState.LeftRailParameter = MidRailParameter;
		}
	}

	OutEffectivePoint = CalculateRailPoint(
		CurrentNode.WorldLocation,
		NormalizedRailDirection,
		SearchState.BestCandidate.RailParameter);

	return !OutEffectivePoint.ContainsNaN();
}

FVector FRayRopeMoveSolver::CalculateRailPoint(
	const FVector& CurrentLocation,
	const FVector& NormalizedRailDirection,
	float RailParameter)
{
	return CurrentLocation + NormalizedRailDirection * RailParameter;
}

float FRayRopeMoveSolver::CalculateEffectivePointDistanceSum(
	const FVector& PrevLocation,
	const FVector& CandidateLocation,
	const FVector& NextLocation)
{
	return FVector::Dist(PrevLocation, CandidateLocation) +
		FVector::Dist(CandidateLocation, NextLocation);
}

bool FRayRopeMoveSolver::TryEvaluateEffectivePointCandidate(
	const FRayRopeTraceContext& TraceContext,
	const FVector& NormalizedRailDirection,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	float RailParameter,
	FEffectivePointCandidate& OutCandidate,
	bool& bOutHasBlockingHit)
{
	OutCandidate = FEffectivePointCandidate();
	bOutHasBlockingHit = false;

	if (!FMath::IsFinite(RailParameter) ||
		NormalizedRailDirection.ContainsNaN() ||
		NormalizedRailDirection.IsNearlyZero())
	{
		return false;
	}

	OutCandidate.RailParameter = RailParameter;
	OutCandidate.Location = CalculateRailPoint(
		CurrentNode.WorldLocation,
		NormalizedRailDirection,
		RailParameter);

	if (OutCandidate.Location.ContainsNaN())
	{
		return false;
	}

	const FRayRopeNode CandidateNode = CreateMovePointNode(OutCandidate.Location);
	bOutHasBlockingHit = HasEffectivePointBlockingHit(
		TraceContext,
		PrevNode,
		CandidateNode,
		NextNode);

	if (bOutHasBlockingHit)
	{
		return true;
	}

	OutCandidate.DistanceSum = CalculateEffectivePointDistanceSum(
		PrevNode.WorldLocation,
		OutCandidate.Location,
		NextNode.WorldLocation);

	return FMath::IsFinite(OutCandidate.DistanceSum);
}

bool FRayRopeMoveSolver::HasEffectivePointBlockingHit(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CandidateNode,
	const FRayRopeNode& NextNode)
{
	return HasBlockingHitBetween(TraceContext, PrevNode, CandidateNode) ||
		HasBlockingHitBetween(TraceContext, CandidateNode, NextNode);
}

bool FRayRopeMoveSolver::HasBlockingHitBetween(
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

void FRayRopeMoveSolver::SaveBetterEffectivePointCandidate(
	const FEffectivePointCandidate& Candidate,
	FEffectivePointSearchState& SearchState)
{
	if (Candidate.DistanceSum < SearchState.BestCandidate.DistanceSum)
	{
		SearchState.BestCandidate = Candidate;
	}
}

FRayRopeNode FRayRopeMoveSolver::CreateMovePointNode(const FVector& WorldLocation)
{
	FRayRopeNode Node;
	Node.NodeType = ERayRopeNodeType::Redirect;
	Node.WorldLocation = WorldLocation;
	return Node;
}
