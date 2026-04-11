#include "RayAttributeSet.h"

#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"

void URayAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(URayAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(URayAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void URayAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	NewValue = ClampAttributeValue(Attribute, NewValue);
	
	BP_PreAttributeChange(Attribute, NewValue);
}

void URayAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

}

void URayAttributeSet::BP_PreAttributeChange_Implementation(const FGameplayAttribute& Attribute, float& NewValue)
{
	
}

float URayAttributeSet::ClampAttributeValue(const FGameplayAttribute& Attribute, const float Value) const
{
	if (Attribute == GetHealthAttribute())
	{
		return FMath::Clamp(Value, 0.0f, GetMaxHealth());
	}

	return Value;
}

void URayAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URayAttributeSet, Health, OldHealth);
}

void URayAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(URayAttributeSet, MaxHealth, OldMaxHealth);
}
