#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RayAttributeInitData.h"
#include "RayAbilitySystemComponent.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class RAY_API URayAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	void InitAbilitySystem(AActor* InOwnerActor, AActor* InAvatarActor);
	
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	void InitAttributeSet(TSubclassOf<UAttributeSet> AttributeSetClass);
	
	UFUNCTION(BlueprintCallable, Category = "AbilitySystem")
	void SetDefaultAttributeValues(const TSubclassOf<UGameplayEffect> InitGameplayEffectClass, const URayAttributeInitData* AttributeInitData, const float Level);
};
