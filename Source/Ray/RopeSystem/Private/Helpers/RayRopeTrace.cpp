#include "RayRopeTrace.h"

#include "CollisionQueryParams.h"

namespace
{
bool IsInitialTraceHit(const FHitResult& Hit)
{
	return Hit.bStartPenetrating ||
		(Hit.Distance <= KINDA_SMALL_NUMBER && Hit.Time <= KINDA_SMALL_NUMBER);
}

bool IsTraceEnteringHitSurface(
	const FVector& StartLocation,
	const FVector& EndLocation,
	const FHitResult& SurfaceHit)
{
	const FVector TraceDirection = (EndLocation - StartLocation).GetSafeNormal();
	const FVector SurfaceNormal = SurfaceHit.ImpactNormal.GetSafeNormal();
	if (TraceDirection.IsNearlyZero() || SurfaceNormal.IsNearlyZero())
	{
		return false;
	}

	return FVector::DotProduct(TraceDirection, SurfaceNormal) < -KINDA_SMALL_NUMBER;
}

void BuildTraceQueryParams(
	const FRayRopeTraceSettings& TraceSettings,
	FCollisionQueryParams& QueryParams)
{
	QueryParams.bReturnPhysicalMaterial = false;
	QueryParams.bTraceComplex = TraceSettings.bTraceComplex;

	if (TraceSettings.OwnerActor != nullptr)
	{
		QueryParams.AddIgnoredActor(TraceSettings.OwnerActor);
	}
}

const AActor* GetIgnoredEndpointActor(const FRayRopeNode* Node)
{
	if (Node == nullptr)
	{
		return nullptr;
	}

	if (Node->NodeType != ERayRopeNodeType::Anchor)
	{
		return nullptr;
	}

	if (!IsValid(Node->AttachedActor))
	{
		return nullptr;
	}

	return Node->AttachedActor;
}

bool TryGetIgnoredEndpointActors(
	const FRayRopeNode* StartNode,
	const FRayRopeNode* EndNode,
	const AActor*& OutStartAttachedActor,
	const AActor*& OutEndAttachedActor)
{
	OutStartAttachedActor = GetIgnoredEndpointActor(StartNode);
	OutEndAttachedActor = GetIgnoredEndpointActor(EndNode);
	return OutStartAttachedActor != nullptr || OutEndAttachedActor != nullptr;
}

FCollisionQueryParams BuildSpanQueryParams(
	const FRayRopeTraceContext& TraceContext,
	const AActor* StartAttachedActor,
	const AActor* EndAttachedActor)
{
	FCollisionQueryParams QueryParams = TraceContext.QueryParams;

	if (StartAttachedActor != nullptr)
	{
		QueryParams.AddIgnoredActor(StartAttachedActor);
	}

	if (EndAttachedActor != nullptr && EndAttachedActor != StartAttachedActor)
	{
		QueryParams.AddIgnoredActor(EndAttachedActor);
	}

	return QueryParams;
}

bool TryTraceBlockingHitWithQueryParams(
	const FRayRopeTraceContext& TraceContext,
	const FVector& StartLocation,
	const FVector& EndLocation,
	const FCollisionQueryParams& QueryParams,
	FHitResult& SurfaceHit)
{
	SurfaceHit = FHitResult();

	if (!IsValid(TraceContext.World) ||
		StartLocation.Equals(EndLocation, KINDA_SMALL_NUMBER))
	{
		return false;
	}

	TraceContext.World->LineTraceSingleByChannel(
		SurfaceHit,
		StartLocation,
		EndLocation,
		TraceContext.TraceChannel,
		QueryParams);

	if (!SurfaceHit.bBlockingHit)
	{
		return false;
	}

	const bool bForwardHitUsable =
		!IsInitialTraceHit(SurfaceHit) ||
		IsTraceEnteringHitSurface(StartLocation, EndLocation, SurfaceHit);

	if (bForwardHitUsable)
	{
		return true;
	}

	FHitResult FallbackHit;
	TraceContext.World->LineTraceSingleByChannel(
		FallbackHit,
		EndLocation,
		StartLocation,
		TraceContext.TraceChannel,
		QueryParams);

	if (FallbackHit.bBlockingHit)
	{
		const bool bReverseHitUsable =
			!IsInitialTraceHit(FallbackHit) ||
			IsTraceEnteringHitSurface(
				EndLocation,
				StartLocation,
				FallbackHit);

		if (bReverseHitUsable)
		{
			SurfaceHit = FallbackHit;
			return true;
		}
	}

	SurfaceHit = FHitResult();
	return false;
}

bool IsPointOverlappingGeometryWithQueryParams(
	const FRayRopeTraceContext& TraceContext,
	const FVector& WorldLocation,
	const FCollisionQueryParams& QueryParams,
	float ProbeRadius)
{
	if (!IsValid(TraceContext.World) || WorldLocation.ContainsNaN())
	{
		return false;
	}

	return TraceContext.World->OverlapBlockingTestByChannel(
		WorldLocation,
		FQuat::Identity,
		TraceContext.TraceChannel,
		FCollisionShape::MakeSphere(FMath::Max(ProbeRadius, KINDA_SMALL_NUMBER)),
		QueryParams);
}
}

FRayRopeTraceContext FRayRopeTrace::MakeTraceContext(
	const FRayRopeTraceSettings& TraceSettings,
	FCollisionQueryParams QueryParams)
{
	FRayRopeTraceContext TraceContext;
	TraceContext.World = TraceSettings.World;
	TraceContext.TraceChannel = TraceSettings.TraceChannel;
	TraceContext.QueryParams = MoveTemp(QueryParams);
	BuildTraceQueryParams(TraceSettings, TraceContext.QueryParams);
	return TraceContext;
}

bool FRayRopeTrace::TryTraceSpan(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeSpan& Span,
	FHitResult& SurfaceHit)
{
	if (!Span.IsValid())
	{
		SurfaceHit = FHitResult();
		return false;
	}

	const FVector StartLocation = Span.GetStartLocation();
	const FVector EndLocation = Span.GetEndLocation();
	if (StartLocation.Equals(EndLocation, KINDA_SMALL_NUMBER))
	{
		SurfaceHit = FHitResult();
		return false;
	}

	const AActor* StartAttachedActor = nullptr;
	const AActor* EndAttachedActor = nullptr;
	if (!TryGetIgnoredEndpointActors(
		Span.StartNode,
		Span.EndNode,
		StartAttachedActor,
		EndAttachedActor))
	{
		return TryTraceBlockingHitWithQueryParams(
			TraceContext,
			StartLocation,
			EndLocation,
			TraceContext.QueryParams,
			SurfaceHit);
	}

	return TryTraceBlockingHitWithQueryParams(
		TraceContext,
		StartLocation,
		EndLocation,
		BuildSpanQueryParams(TraceContext, StartAttachedActor, EndAttachedActor),
		SurfaceHit);
}

bool FRayRopeTrace::HasBlockingSpanHit(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeSpan& Span)
{
	FHitResult SurfaceHit;
	return TryTraceSpan(TraceContext, Span, SurfaceHit);
}

bool FRayRopeTrace::TryTraceBlockingHit(
	const FRayRopeTraceContext& TraceContext,
	const FVector& StartLocation,
	const FVector& EndLocation,
	FHitResult& SurfaceHit)
{
	return TryTraceBlockingHitWithQueryParams(
		TraceContext,
		StartLocation,
		EndLocation,
		TraceContext.QueryParams,
		SurfaceHit);
}

bool FRayRopeTrace::HasBlockingHit(
	const FRayRopeTraceContext& TraceContext,
	const FVector& StartLocation,
	const FVector& EndLocation)
{
	FHitResult SurfaceHit;
	return TryTraceBlockingHit(
		TraceContext,
		StartLocation,
		EndLocation,
		SurfaceHit);
}

FRayRopeTraceContext FRayRopeTrace::MakeTraceContextIgnoringEndpointActors(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNode* StartNode,
	const FRayRopeNode* EndNode)
{
	FRayRopeTraceContext EndpointTraceContext = TraceContext;
	const AActor* StartAttachedActor = nullptr;
	const AActor* EndAttachedActor = nullptr;
	TryGetIgnoredEndpointActors(
		StartNode,
		EndNode,
		StartAttachedActor,
		EndAttachedActor);
	EndpointTraceContext.QueryParams = BuildSpanQueryParams(
		TraceContext,
		StartAttachedActor,
		EndAttachedActor);
	return EndpointTraceContext;
}

bool FRayRopeTrace::IsPointOverlappingGeometry(
	const FRayRopeTraceContext& TraceContext,
	const FVector& WorldLocation,
	float ProbeRadius)
{
	return IsPointOverlappingGeometryWithQueryParams(
		TraceContext,
		WorldLocation,
		TraceContext.QueryParams,
		ProbeRadius);
}

bool FRayRopeTrace::IsNodeOverlappingGeometry(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNode& Node,
	float ProbeRadius)
{
	const AActor* IgnoredActor = GetIgnoredEndpointActor(&Node);
	if (IgnoredActor == nullptr)
	{
		return IsPointOverlappingGeometry(TraceContext, Node.WorldLocation, ProbeRadius);
	}

	FCollisionQueryParams QueryParams = TraceContext.QueryParams;
	QueryParams.AddIgnoredActor(IgnoredActor);
	return IsPointOverlappingGeometryWithQueryParams(
		TraceContext,
		Node.WorldLocation,
		QueryParams,
		ProbeRadius);
}

bool FRayRopeTrace::IsValidFreePoint(
	const FRayRopeTraceContext& TraceContext,
	const FVector& WorldLocation,
	float ProbeRadius)
{
	return !WorldLocation.ContainsNaN() &&
		!IsPointOverlappingGeometry(TraceContext, WorldLocation, ProbeRadius);
}

bool FRayRopeTrace::IsValidFreeNode(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNode& Node,
	float ProbeRadius)
{
	return !Node.WorldLocation.ContainsNaN() &&
		!IsNodeOverlappingGeometry(TraceContext, Node, ProbeRadius);
}
