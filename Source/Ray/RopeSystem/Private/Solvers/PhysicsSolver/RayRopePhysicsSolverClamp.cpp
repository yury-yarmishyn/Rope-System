#include "RayRopePhysicsSolverInternal.h"

namespace RayRopePhysicsSolverPrivate
{
bool ClampOwnerAnchorToMaxRopeLength(
	AActor* OwnerActor,
	const FRayRopePhysicsSettings& PhysicsSettings,
	const FRayRopeNode& OwnerNode,
	const FRayRopeNode& AdjacentNode)
{
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	const FVector OwnerAnchorLocation = OwnerNode.WorldLocation;
	const FVector AdjacentLocation = AdjacentNode.WorldLocation;
	const FVector TowardAdjacent = AdjacentLocation - OwnerAnchorLocation;
	const float TerminalSpanLength = TowardAdjacent.Size();
	if (TerminalSpanLength <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float ExcessLength =
		PhysicsSettings.CurrentRopeLength - PhysicsSettings.MaxAllowedRopeLength;
	if (ExcessLength <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector TowardAdjacentDirection = TowardAdjacent / TerminalSpanLength;
	const FVector OutwardDirection = -TowardAdjacentDirection;
	RemoveOwnerOutwardVelocity(OwnerActor, OutwardDirection);

	const float ClampDistance = FMath::Min(ExcessLength, TerminalSpanLength);
	const FVector TargetAnchorLocation =
		OwnerAnchorLocation + TowardAdjacentDirection * ClampDistance;
	const FVector ActorDelta = TargetAnchorLocation - OwnerAnchorLocation;
	if (ActorDelta.IsNearlyZero())
	{
		return false;
	}

	const FVector StartActorLocation = OwnerActor->GetActorLocation();
	FHitResult SweepHit;
	OwnerActor->SetActorLocation(
		StartActorLocation + ActorDelta,
		true,
		&SweepHit,
		ETeleportType::None);

	return !OwnerActor->GetActorLocation().Equals(
		StartActorLocation,
		KINDA_SMALL_NUMBER);
}
}

