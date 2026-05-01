#include "RayRopePhysicsSolver.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

bool FRayRopePhysicsSolver::Solve(
	AActor* OwnerActor,
	const TArray<FRayRopeSegment>& Segments,
	const FRayRopePhysicsSettings& PhysicsSettings)
{
	if (PhysicsSettings.MaxRopeLength <= 0.f ||
		PhysicsSettings.RopeLength <= PhysicsSettings.MaxRopeLength)
	{
		return false;
	}

	const FRayRopeNode* OwnerNode = nullptr;
	const FRayRopeNode* AdjacentNode = nullptr;
	if (!TryGetOwnerTerminalNodes(
			OwnerActor,
			Segments,
			OwnerNode,
			AdjacentNode) ||
		OwnerNode == nullptr ||
		AdjacentNode == nullptr)
	{
		return false;
	}

	return ClampOwnerAnchorToMaxRopeLength(
		OwnerActor,
		PhysicsSettings,
		*OwnerNode,
		*AdjacentNode);
}

bool FRayRopePhysicsSolver::TryGetOwnerTerminalNodes(
	AActor* OwnerActor,
	const TArray<FRayRopeSegment>& Segments,
	const FRayRopeNode*& OutOwnerNode,
	const FRayRopeNode*& OutAdjacentNode)
{
	OutOwnerNode = nullptr;
	OutAdjacentNode = nullptr;

	if (!IsValid(OwnerActor) || Segments.Num() == 0)
	{
		return false;
	}

	const FRayRopeSegment& FirstSegment = Segments[0];
	if (FirstSegment.Nodes.Num() >= 2)
	{
		const FRayRopeNode& FirstNode = FirstSegment.Nodes[0];
		if (FirstNode.NodeType == ERayRopeNodeType::Anchor &&
			FirstNode.AttachActor == OwnerActor)
		{
			OutOwnerNode = &FirstNode;
			OutAdjacentNode = &FirstSegment.Nodes[1];
			return true;
		}
	}

	const FRayRopeSegment& LastSegment = Segments.Last();
	if (LastSegment.Nodes.Num() >= 2)
	{
		const int32 LastNodeIndex = LastSegment.Nodes.Num() - 1;
		const FRayRopeNode& LastNode = LastSegment.Nodes[LastNodeIndex];
		if (LastNode.NodeType == ERayRopeNodeType::Anchor &&
			LastNode.AttachActor == OwnerActor)
		{
			OutOwnerNode = &LastNode;
			OutAdjacentNode = &LastSegment.Nodes[LastNodeIndex - 1];
			return true;
		}
	}

	return false;
}

bool FRayRopePhysicsSolver::ClampOwnerAnchorToMaxRopeLength(
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
		PhysicsSettings.RopeLength - PhysicsSettings.MaxRopeLength;
	if (ExcessLength <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector OutwardDirection = -TowardAdjacent / TerminalSpanLength;
	RemoveOwnerOutwardVelocity(OwnerActor, OutwardDirection);

	const float ClampDistance = FMath::Min(ExcessLength, TerminalSpanLength);
	const FVector TargetAnchorLocation =
		OwnerAnchorLocation + TowardAdjacent.GetSafeNormal() * ClampDistance;
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

void FRayRopePhysicsSolver::RemoveOwnerOutwardVelocity(
	AActor* OwnerActor,
	const FVector& OutwardDirection)
{
	if (!IsValid(OwnerActor) || OutwardDirection.IsNearlyZero())
	{
		return;
	}

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(OwnerActor))
	{
		if (UCharacterMovementComponent* CharacterMovement =
				OwnerCharacter->GetCharacterMovement())
		{
			const float OutwardSpeed =
				FVector::DotProduct(CharacterMovement->Velocity, OutwardDirection);
			if (OutwardSpeed > 0.f)
			{
				CharacterMovement->Velocity -= OutwardDirection * OutwardSpeed;
			}
		}
	}

	UPrimitiveComponent* RootPrimitive =
		Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
	if (!IsValid(RootPrimitive))
	{
		return;
	}

	if (RootPrimitive->IsSimulatingPhysics())
	{
		const FVector LinearVelocity = RootPrimitive->GetPhysicsLinearVelocity();
		const float OutwardSpeed =
			FVector::DotProduct(LinearVelocity, OutwardDirection);
		if (OutwardSpeed > 0.f)
		{
			RootPrimitive->AddImpulse(
				-OutwardDirection * OutwardSpeed * RootPrimitive->GetMass(),
				NAME_None,
				true);
		}

		return;
	}

	const FVector LinearVelocity = RootPrimitive->GetComponentVelocity();
	const float OutwardSpeed = FVector::DotProduct(LinearVelocity, OutwardDirection);
	if (OutwardSpeed > 0.f)
	{
		RootPrimitive->ComponentVelocity =
			LinearVelocity - OutwardDirection * OutwardSpeed;
	}
}
