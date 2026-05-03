#include "RayRopePhysicsSolver.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
struct FOwnerTerminalNodes
{
	const FRayRopeNode* OwnerNode = nullptr;
	const FRayRopeNode* AdjacentNode = nullptr;

	bool IsValid() const
	{
		return OwnerNode != nullptr && AdjacentNode != nullptr;
	}
};

bool IsOwnerAnchorNode(AActor* OwnerActor, const FRayRopeNode& Node)
{
	return Node.NodeType == ERayRopeNodeType::Anchor &&
		Node.AttachedActor == OwnerActor;
}

bool TryGetOwnerEndpointInSegment(
	AActor* OwnerActor,
	const FRayRopeSegment& Segment,
	bool bCheckStart,
	FOwnerTerminalNodes& OutTerminalNodes)
{
	OutTerminalNodes = FOwnerTerminalNodes();

	if (Segment.Nodes.Num() < 2)
	{
		return false;
	}

	const int32 OwnerNodeIndex = bCheckStart ? 0 : Segment.Nodes.Num() - 1;
	const int32 AdjacentNodeIndex = bCheckStart ? 1 : OwnerNodeIndex - 1;
	const FRayRopeNode& OwnerNode = Segment.Nodes[OwnerNodeIndex];
	if (!IsOwnerAnchorNode(OwnerActor, OwnerNode))
	{
		return false;
	}

	OutTerminalNodes.OwnerNode = &OwnerNode;
	OutTerminalNodes.AdjacentNode = &Segment.Nodes[AdjacentNodeIndex];
	return true;
}

bool TryGetOwnerTerminalNodes(
	AActor* OwnerActor,
	const TArray<FRayRopeSegment>& Segments,
	FOwnerTerminalNodes& OutTerminalNodes)
{
	OutTerminalNodes = FOwnerTerminalNodes();

	if (!IsValid(OwnerActor) || Segments.Num() == 0)
	{
		return false;
	}

	// The length constraint moves only the component owner, so the owner anchor must stay terminal.
	const FRayRopeSegment& FirstSegment = Segments[0];
	if (TryGetOwnerEndpointInSegment(OwnerActor, FirstSegment, true, OutTerminalNodes))
	{
		return true;
	}

	const FRayRopeSegment& LastSegment = Segments.Last();
	return TryGetOwnerEndpointInSegment(OwnerActor, LastSegment, false, OutTerminalNodes);
}

bool RemoveCharacterOutwardVelocity(AActor* OwnerActor, const FVector& OutwardDirection)
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(OwnerActor);
	if (!IsValid(OwnerCharacter))
	{
		return false;
	}

	UCharacterMovementComponent* CharacterMovement = OwnerCharacter->GetCharacterMovement();
	if (CharacterMovement == nullptr)
	{
		return false;
	}

	const float OutwardSpeed =
		FVector::DotProduct(CharacterMovement->Velocity, OutwardDirection);
	if (OutwardSpeed > 0.f)
	{
		CharacterMovement->Velocity -= OutwardDirection * OutwardSpeed;
	}

	return true;
}

void RemovePrimitiveOutwardVelocity(AActor* OwnerActor, const FVector& OutwardDirection)
{
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

void RemoveOwnerOutwardVelocity(AActor* OwnerActor, const FVector& OutwardDirection)
{
	if (!IsValid(OwnerActor) || OutwardDirection.IsNearlyZero())
	{
		return;
	}

	if (RemoveCharacterOutwardVelocity(OwnerActor, OutwardDirection))
	{
		return;
	}

	RemovePrimitiveOutwardVelocity(OwnerActor, OutwardDirection);
}

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

bool FRayRopePhysicsSolver::Solve(
	AActor* OwnerActor,
	const TArray<FRayRopeSegment>& Segments,
	const FRayRopePhysicsSettings& PhysicsSettings)
{
	if (PhysicsSettings.MaxAllowedRopeLength <= 0.f ||
		PhysicsSettings.CurrentRopeLength <= PhysicsSettings.MaxAllowedRopeLength)
	{
		return false;
	}

	FOwnerTerminalNodes TerminalNodes;
	if (!TryGetOwnerTerminalNodes(OwnerActor, Segments, TerminalNodes) ||
		!TerminalNodes.IsValid())
	{
		return false;
	}

	return ClampOwnerAnchorToMaxRopeLength(
		OwnerActor,
		PhysicsSettings,
		*TerminalNodes.OwnerNode,
		*TerminalNodes.AdjacentNode);
}
