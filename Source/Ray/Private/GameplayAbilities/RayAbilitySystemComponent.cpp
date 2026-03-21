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

void URayAbilitySystemComponent::SetDefaultAttributeValues(const TSubclassOf<UGameplayEffect> InitGameplayEffectClass,
	const URayAttributeInitData* AttributeInitData, const float Level)
{
	if (!InitGameplayEffectClass || !AttributeInitData)
	{
		return;
	}

	/*FGameplayEffectContextHandle EffectContextHandle = MakeEffectContext();
	EffectContextHandle.AddSourceObject(this);*/

	const FGameplayEffectSpecHandle EffectSpecHandle = MakeOutgoingSpec(
		InitGameplayEffectClass,
		Level,
		MakeEffectContext());

	if (!EffectSpecHandle.IsValid() || !EffectSpecHandle.Data.IsValid())
	{
		return;
	}

	for (const FRayAttributeInitConfig& AttributeInitConfig : AttributeInitData->AttributeValues)
	{
		if (!AttributeInitConfig.DataTag.IsValid())
		{
			continue;
		}

		EffectSpecHandle.Data->SetSetByCallerMagnitude(
			AttributeInitConfig.DataTag,
			AttributeInitConfig.Value);
	}

	ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data);
	
}