#include "RayRopeTrace.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
bool IsInitialTraceHit(const FHitResult& Hit)
{
	return Hit.bStartPenetrating ||
		(Hit.Distance <= KINDA_SMALL_NUMBER && Hit.Time <= KINDA_SMALL_NUMBER);
}

const AActor* GetIgnoredEndpointActor(const FRayRopeNode* Node)
{
	return Node != nullptr &&
		Node->NodeType == ERayRopeNodeType::Anchor &&
		IsValid(Node->AttachedActor)
		? Node->AttachedActor
		: nullptr;
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

	const bool bAcceptForwardHit =
		!IsInitialTraceHit(SurfaceHit) ||
		FRayRopeTrace::IsTraceEnteringHitSurface(StartLocation, EndLocation, SurfaceHit);

	if (bAcceptForwardHit)
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
		const bool bAcceptFallbackHit =
			!IsInitialTraceHit(FallbackHit) ||
			FRayRopeTrace::IsTraceEnteringHitSurface(
				EndLocation,
				StartLocation,
				FallbackHit);

		if (bAcceptFallbackHit)
		{
			SurfaceHit = FallbackHit;
			return true;
		}
	}

	SurfaceHit = FHitResult();
	return false;
}

bool IsPointInsideGeometryWithQueryParams(
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
	TraceContext.bTraceComplex = TraceSettings.bTraceComplex;
	TraceContext.QueryParams = MoveTemp(QueryParams);
	BuildTraceQueryParams(TraceSettings, TraceContext.QueryParams);
	return TraceContext;
}

void FRayRopeTrace::BuildTraceQueryParams(
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

bool FRayRopeTrace::TryTraceSpan(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeSpan& Span,
	FHitResult& SurfaceHit)
{
	if (!Span.IsValid() || Span.IsDegenerate(KINDA_SMALL_NUMBER))
	{
		SurfaceHit = FHitResult();
		return false;
	}

	const AActor* StartAttachedActor = GetIgnoredEndpointActor(Span.StartNode);
	const AActor* EndAttachedActor = GetIgnoredEndpointActor(Span.EndNode);
	if (StartAttachedActor == nullptr && EndAttachedActor == nullptr)
	{
		return TryTraceBlockingHitWithQueryParams(
			TraceContext,
			Span.GetStartLocation(),
			Span.GetEndLocation(),
			TraceContext.QueryParams,
			SurfaceHit);
	}

	return TryTraceBlockingHitWithQueryParams(
		TraceContext,
		Span.GetStartLocation(),
		Span.GetEndLocation(),
		BuildSpanQueryParams(TraceContext, StartAttachedActor, EndAttachedActor),
		SurfaceHit);
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

bool FRayRopeTrace::IsPointInsideGeometry(
	const FRayRopeTraceContext& TraceContext,
	const FVector& WorldLocation,
	float ProbeRadius)
{
	return IsPointInsideGeometryWithQueryParams(
		TraceContext,
		WorldLocation,
		TraceContext.QueryParams,
		ProbeRadius);
}

bool FRayRopeTrace::IsNodeInsideGeometry(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNode& Node,
	float ProbeRadius)
{
	const AActor* IgnoredActor = GetIgnoredEndpointActor(&Node);
	if (IgnoredActor == nullptr)
	{
		return IsPointInsideGeometry(TraceContext, Node.WorldLocation, ProbeRadius);
	}

	FCollisionQueryParams QueryParams = TraceContext.QueryParams;
	QueryParams.AddIgnoredActor(IgnoredActor);
	return IsPointInsideGeometryWithQueryParams(
		TraceContext,
		Node.WorldLocation,
		QueryParams,
		ProbeRadius);
}

bool FRayRopeTrace::CanUseHitForRedirectWrap(
	const FHitResult& SurfaceHit,
	bool bAllowWrapOnMovableObjects)
{
	if (!SurfaceHit.bBlockingHit)
	{
		return false;
	}

	if (bAllowWrapOnMovableObjects)
	{
		return true;
	}

	const UPrimitiveComponent* HitComponent = SurfaceHit.GetComponent();
	if (IsValid(HitComponent))
	{
		if (HitComponent->Mobility == EComponentMobility::Movable ||
			HitComponent->IsSimulatingPhysics())
		{
			return false;
		}
	}

	const AActor* HitActor = SurfaceHit.GetActor();
	if (!IsValid(HitActor))
	{
		return HitComponent != nullptr;
	}

	const USceneComponent* RootComponent = HitActor->GetRootComponent();
	return !IsValid(RootComponent) || RootComponent->Mobility != EComponentMobility::Movable;
}

bool FRayRopeTrace::IsTraceEnteringHitSurface(
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
