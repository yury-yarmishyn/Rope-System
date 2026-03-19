#include "GameplayAbilities/RayAbilitySystemComponent.h"

void URayAbilitySystemComponent::InitAbilitySystem(AActor* InOwnerActor, AActor* InAvatarActor)
{
	if (InOwnerActor != nullptr && InAvatarActor != nullptr)
	{
		InitAbilityActorInfo(InOwnerActor, InAvatarActor);
	}
}

void URayAbilitySystemComponent::InitAttributeSet(TSubclassOf<UAttributeSet> AttributeSetClass)
{
	if (!AttributeSetClass) return;
	
	UAttributeSet* AttributeSet = NewObject<UAttributeSet>(this, AttributeSetClass);
	AddAttributeSetSubobject(AttributeSet);
}
