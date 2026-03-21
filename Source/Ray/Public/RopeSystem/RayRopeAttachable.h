#pragma once

#include "CoreMinimal.h"
#include "RayRopeComponent.h"
#include "UObject/Interface.h"
#include "RayRopeAttachable.generated.h"

UINTERFACE(BlueprintType)
class URayRopeAttachable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class RAY_API IRayRopeAttachable
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Rope")
	URayRopeComponent* GetRayCableComponent() const;
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Rope")
	float GetRopeResistance() const;
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Rope")
	FName GetRopeAttachSocket() const;
};
