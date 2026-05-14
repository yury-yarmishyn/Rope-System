#include "RayRopeMoveSolverInternal.h"

#include "Debug/RayRopeDebugConfig.h"
#include "Geometry/RayRopeSurfaceGeometry.h"

#if RAYROPE_WITH_DEBUG
#include "Debug/RayRopeDebugContext.h"
#endif

namespace RayRopeMoveSolverPrivate
{
namespace
{
struct FMoveSurfaceHits
{
	FHitResult ForwardSurfaceHit;
	FHitResult ReverseSurfaceHit;
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

bool TryTraceMoveHit(
	const FRayRopeTraceContext& TraceContext,
	const FMoveSolveContext& SolveContext,
	const FVector& StartLocation,
	const FVector& EndLocation,
	FHitResult& OutHit)
{
	OutHit = FHitResult();

	if (!IsValid(TraceContext.World) ||
		StartLocation.ContainsNaN() ||
		EndLocation.ContainsNaN() ||
		StartLocation.Equals(EndLocation, SolveContext.GeometryTolerance))
	{
		return false;
	}

	if (!FRayRopeTrace::TryTraceBlockingHit(
		TraceContext,
		StartLocation,
		EndLocation,
		OutHit))
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

bool TryFindRailDirectionSurfaceHits(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	FMoveSurfaceHits& OutHits)
{
	OutHits = FMoveSurfaceHits();

	if (!IsValid(SolveContext.TraceContext.World) ||
		NodeWindow.PrevNode.WorldLocation.ContainsNaN() ||
		NodeWindow.CurrentNode.WorldLocation.ContainsNaN() ||
		NodeWindow.NextNode.WorldLocation.ContainsNaN())
	{
#if RAYROPE_WITH_DEBUG
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveRail"),
				FString::Printf(
					TEXT("Node[%d] rejected: invalid world or NaN node"),
					NodeWindow.NodeIndex));
		}
#endif
		return false;
	}

	if (FVector::DistSquared(
			NodeWindow.PrevNode.WorldLocation,
			NodeWindow.CurrentNode.WorldLocation) <= SolveContext.MinNodeSeparationSquared ||
		FVector::DistSquared(
			NodeWindow.CurrentNode.WorldLocation,
			NodeWindow.NextNode.WorldLocation) <= SolveContext.MinNodeSeparationSquared)
	{
#if RAYROPE_WITH_DEBUG
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveRail"),
				FString::Printf(
					TEXT("Node[%d] rejected: below min node separation"),
					NodeWindow.NodeIndex));
		}
#endif
		return false;
	}

	FMoveSurfaceSearchBounds SearchBounds{
		NodeWindow.CurrentNode.WorldLocation,
		NodeWindow.PrevNode.WorldLocation,
		NodeWindow.CurrentNode.WorldLocation,
		NodeWindow.NextNode.WorldLocation
	};

	// Search from the current redirect toward the neighbor span to recover the two surfaces that
	// constrain the redirect. Endpoint actors are ignored so anchors do not hide nearby blockers.
	FHitResult LastForwardSurfaceHit;
	FVector LastBlockedTraceStart = FVector::ZeroVector;
	FVector LastBlockedTraceEnd = FVector::ZeroVector;

	const FRayRopeTraceContext MoveTraceContext = FRayRopeTrace::MakeTraceContextIgnoringEndpointActors(
		SolveContext.TraceContext,
		&NodeWindow.PrevNode,
		&NodeWindow.NextNode);

	const int32 MaxSurfaceHitSearchIterations = SolveContext.MaxRailSurfaceSearchIterations;
	for (int32 Iteration = 0; Iteration < MaxSurfaceHitSearchIterations; ++Iteration)
	{
		if (SearchBounds.IsResolved(SolveContext.GeometryToleranceSquared))
		{
			break;
		}

		const FVector TraceStart = SearchBounds.GetTraceStart();
		const FVector TraceEnd = SearchBounds.GetTraceEnd();

		FHitResult TraceHit;
		if (TryTraceMoveHit(
			MoveTraceContext,
			SolveContext,
			TraceStart,
			TraceEnd,
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
#if RAYROPE_WITH_DEBUG
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveRail"),
				FString::Printf(
					TEXT("Node[%d] rejected: no forward surface hit"),
					NodeWindow.NodeIndex));
		}
#endif
		return false;
	}

	FHitResult ReverseSurfaceHit;
	if (!TryTraceMoveHit(
		MoveTraceContext,
		SolveContext,
		LastBlockedTraceEnd,
		LastBlockedTraceStart,
		ReverseSurfaceHit))
	{
#if RAYROPE_WITH_DEBUG
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveRail"),
				FString::Printf(
					TEXT("Node[%d] rejected: no reverse surface hit"),
					NodeWindow.NodeIndex));
		}
#endif
		return false;
	}

	OutHits.ForwardSurfaceHit = LastForwardSurfaceHit;
	OutHits.ReverseSurfaceHit = ReverseSurfaceHit;
	return true;
}

bool TryBuildRailFromSurfaceHits(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	const FHitResult& FirstSurfaceHit,
	const FHitResult& SecondSurfaceHit,
	FMoveRail& OutRail)
{
	OutRail = FMoveRail();

	if (!FirstSurfaceHit.bBlockingHit || !SecondSurfaceHit.bBlockingHit)
	{
#if RAYROPE_WITH_DEBUG
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveRail"),
				FString::Printf(
					TEXT("Node[%d] rejected: missing blocking hit pair"),
					NodeWindow.NodeIndex));
		}
#endif
		return false;
	}

	const FVector FirstNormal = FirstSurfaceHit.ImpactNormal.GetSafeNormal();
	const FVector SecondNormal = SecondSurfaceHit.ImpactNormal.GetSafeNormal();
	if (FirstNormal.IsNearlyZero() || SecondNormal.IsNearlyZero())
	{
#if RAYROPE_WITH_DEBUG
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveRail"),
				FString::Printf(
					TEXT("Node[%d] rejected: invalid surface normal"),
					NodeWindow.NodeIndex));
		}
#endif
		return false;
	}

	FVector RailDirection = FVector::CrossProduct(FirstNormal, SecondNormal);
	if (RailDirection.SizeSquared() <= SolveContext.PlaneParallelToleranceSquared)
	{
		// Parallel or nearly parallel planes do not define a stable intersection line. Fall back to
		// the rope direction projected onto the surface so the node can still slide along the contact.
		const FVector SpanDirection =
			(NodeWindow.NextNode.WorldLocation - NodeWindow.PrevNode.WorldLocation).GetSafeNormal();
		RailDirection = FVector::VectorPlaneProject(SpanDirection, FirstNormal);
		if (RailDirection.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
			const FVector PrevToCurrentDirection =
				(NodeWindow.CurrentNode.WorldLocation - NodeWindow.PrevNode.WorldLocation).GetSafeNormal();
			RailDirection = FVector::VectorPlaneProject(PrevToCurrentDirection, FirstNormal);
		}

		if (RailDirection.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
			const FVector CurrentToNextDirection =
				(NodeWindow.NextNode.WorldLocation - NodeWindow.CurrentNode.WorldLocation).GetSafeNormal();
			RailDirection = FVector::VectorPlaneProject(CurrentToNextDirection, FirstNormal);
		}

		if (RailDirection.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
#if RAYROPE_WITH_DEBUG
			if (SolveContext.TraceContext.DebugContext != nullptr)
			{
				SolveContext.TraceContext.DebugContext->RecordSolverEvent(
					TEXT("MoveRail"),
					FString::Printf(
						TEXT("Node[%d] rejected: parallel surfaces did not produce fallback rail"),
						NodeWindow.NodeIndex));
			}
#endif
			return false;
		}
	}

	const FVector RailDirectionNormal = RailDirection.GetSafeNormal();
	if (RailDirectionNormal.IsNearlyZero())
	{
#if RAYROPE_WITH_DEBUG
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveRail"),
				FString::Printf(
					TEXT("Node[%d] rejected: zero rail direction"),
					NodeWindow.NodeIndex));
		}
#endif
		return false;
	}

	const FRayRopeSpan CurrentPointSpan{&NodeWindow.CurrentNode, &NodeWindow.CurrentNode};
	const FVector SurfaceRailPoint = FRayRopeSurfaceGeometry::CalculateRedirectLocation(
		CurrentPointSpan,
		FirstSurfaceHit,
		&SecondSurfaceHit);
	const FVector OffsetDirection = FRayRopeSurfaceGeometry::CalculateSurfaceOffsetDirection(
		FirstSurfaceHit,
		&SecondSurfaceHit);

	OutRail.Origin = SurfaceRailPoint + OffsetDirection * SolveContext.SurfaceOffset;
	OutRail.Direction = RailDirectionNormal;
	if (OutRail.Origin.ContainsNaN())
	{
#if RAYROPE_WITH_DEBUG
		if (SolveContext.TraceContext.DebugContext != nullptr)
		{
			SolveContext.TraceContext.DebugContext->RecordSolverEvent(
				TEXT("MoveRail"),
				FString::Printf(
					TEXT("Node[%d] rejected: rail origin is NaN"),
					NodeWindow.NodeIndex));
		}
#endif
		return false;
	}

#if RAYROPE_WITH_DEBUG
	if (SolveContext.TraceContext.DebugContext != nullptr)
	{
		SolveContext.TraceContext.DebugContext->DrawSolverRail(
			ERayRopeDebugDrawFlags::MoveRails,
			OutRail.Origin,
			OutRail.Direction,
			FVector::Dist(NodeWindow.PrevNode.WorldLocation, NodeWindow.NextNode.WorldLocation),
			TEXT("MoveRail"));
		SolveContext.TraceContext.DebugContext->RecordSolverEvent(
			TEXT("MoveRail"),
			FString::Printf(
				TEXT("Node[%d] rail Origin=%s Direction=%s"),
				NodeWindow.NodeIndex,
			*OutRail.Origin.ToCompactString(),
			*OutRail.Direction.ToCompactString()));
	}
#endif

	return true;
}
}

bool TryBuildMoveRail(
	const FMoveSolveContext& SolveContext,
	const FMoveNodeWindow& NodeWindow,
	FMoveRail& OutRail)
{
	FMoveSurfaceHits SurfaceHits;
	if (!TryFindRailDirectionSurfaceHits(
		SolveContext,
		NodeWindow,
		SurfaceHits))
	{
		return false;
	}

	if (!TryBuildRailFromSurfaceHits(
		SolveContext,
		NodeWindow,
		SurfaceHits.ForwardSurfaceHit,
		SurfaceHits.ReverseSurfaceHit,
		OutRail))
	{
		return false;
	}

	return true;
}
}
