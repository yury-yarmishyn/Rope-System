#include "RayRopeTrace.h"

#include "CollisionQueryParams.h"
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
}

void FRayRopeTrace::BuildTraceQueryParams(
	const AActor* OwnerActor,
	const FRayRopeSegment& Segment,
	FCollisionQueryParams& QueryParams)
{
	QueryParams.bReturnPhysicalMaterial = false;

	if (OwnerActor != nullptr)
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	for (const FRayRopeNode& Node : Segment.Nodes)
	{
		if (Node.NodeType != ENodeType::Anchor || !IsValid(Node.AttachActor))
		{
			continue;
		}

		QueryParams.AddIgnoredActor(Node.AttachActor);
	}
}

bool FRayRopeTrace::TryTraceSpan(
	UWorld* World,
	ECollisionChannel TraceChannel,
	const FRayRopeSpan& Span,
	const FCollisionQueryParams& QueryParams,
	FHitResult& SurfaceHit)
{
	if (!Span.IsValid())
	{
		SurfaceHit = FHitResult();
		return false;
	}

	return TryTraceBlockingHit(
		World,
		TraceChannel,
		QueryParams,
		Span.GetStartLocation(),
		Span.GetEndLocation(),
		SurfaceHit);
}

bool FRayRopeTrace::TryTraceBlockingHit(
	UWorld* World,
	ECollisionChannel TraceChannel,
	const FCollisionQueryParams& QueryParams,
	const FVector& StartLocation,
	const FVector& EndLocation,
	FHitResult& SurfaceHit)
{
	SurfaceHit = FHitResult();

	if (!IsValid(World))
	{
		return false;
	}

	World->LineTraceSingleByChannel(
		SurfaceHit,
		StartLocation,
		EndLocation,
		TraceChannel,
		QueryParams);

	if (!SurfaceHit.bBlockingHit)
	{
		return false;
	}

	const bool bAcceptForwardHit =
		!IsInitialTraceHit(SurfaceHit) ||
		IsTraceEnteringHitSurface(StartLocation, EndLocation, SurfaceHit);

	if (bAcceptForwardHit)
	{
		return true;
	}

	FHitResult FallbackHit;
	World->LineTraceSingleByChannel(
		FallbackHit,
		EndLocation,
		StartLocation,
		TraceChannel,
		QueryParams);

	if (FallbackHit.bBlockingHit)
	{
		const bool bAcceptFallbackHit =
			!IsInitialTraceHit(FallbackHit) ||
			IsTraceEnteringHitSurface(
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
