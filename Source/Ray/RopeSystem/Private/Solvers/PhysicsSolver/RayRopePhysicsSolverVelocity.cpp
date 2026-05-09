#include "RayRopePhysicsSolverInternal.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace RayRopePhysicsSolverPrivate
{
namespace
{
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
}

