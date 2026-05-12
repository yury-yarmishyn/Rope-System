#include "RayRopePhysicsSolverInternal.h"

namespace RayRopePhysicsSolverPrivate
{
bool ClampOwnerAnchorToMaxRopeLength(
	AActor* OwnerActor,
	const FRayRopePhysicsSettings& PhysicsSettings,
	const FRayRopeNode& OwnerNode,
	const FRayRopeNode& AdjacentNode,
	FOwnerClampDebugResult* OutDebugResult)
{
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	const FVector OwnerAnchorLocation = OwnerNode.WorldLocation;
	const FVector AdjacentLocation = AdjacentNode.WorldLocation;
	if (OutDebugResult != nullptr)
	{
		OutDebugResult->OwnerAnchorLocation = OwnerAnchorLocation;
		OutDebugResult->AdjacentLocation = AdjacentLocation;
	}

	const FVector TowardAdjacent = AdjacentLocation - OwnerAnchorLocation;
	const float TerminalSpanLength = TowardAdjacent.Size();
	if (TerminalSpanLength <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float ExcessLength =
		PhysicsSettings.CurrentRopeLength - PhysicsSettings.MaxAllowedRopeLength;
	if (OutDebugResult != nullptr)
	{
		OutDebugResult->ExcessLength = ExcessLength;
	}

	if (ExcessLength <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector TowardAdjacentDirection = TowardAdjacent / TerminalSpanLength;
	const FVector OutwardDirection = -TowardAdjacentDirection;
	RemoveOwnerOutwardVelocity(OwnerActor, OutwardDirection);

	const float ClampDistance = FMath::Min(ExcessLength, TerminalSpanLength);
	if (OutDebugResult != nullptr)
	{
		OutDebugResult->ClampDistance = ClampDistance;
	}

	const FVector TargetAnchorLocation =
		OwnerAnchorLocation + TowardAdjacentDirection * ClampDistance;
	const FVector ActorDelta = TargetAnchorLocation - OwnerAnchorLocation;
	if (OutDebugResult != nullptr)
	{
		OutDebugResult->ActorDelta = ActorDelta;
	}

	if (ActorDelta.IsNearlyZero())
	{
		return false;
	}

	// Move the actor with sweep enabled so the length clamp respects world collision.
	const FVector StartActorLocation = OwnerActor->GetActorLocation();
	FHitResult SweepHit;
	OwnerActor->SetActorLocation(
		StartActorLocation + ActorDelta,
		true,
		&SweepHit,
		ETeleportType::None);
	if (OutDebugResult != nullptr)
	{
		OutDebugResult->SweepHit = SweepHit;
		OutDebugResult->bSweepBlocked = SweepHit.bBlockingHit;
	}

	return !OwnerActor->GetActorLocation().Equals(
		StartActorLocation,
		KINDA_SMALL_NUMBER);
}
}

