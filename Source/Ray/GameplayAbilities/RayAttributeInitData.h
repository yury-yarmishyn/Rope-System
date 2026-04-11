// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "RayAttributeInitData.generated.h"

USTRUCT(BlueprintType)
struct FRayAttributeInitConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayAttribute Attribute;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float Value = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag DataTag;
};

/**
 * 
 */
UCLASS(Blueprintable)
class RAY_API URayAttributeInitData : public UDataAsset
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Attributes")
	TArray<FRayAttributeInitConfig> AttributeValues;
};
