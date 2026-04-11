#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RayRopeInterface.generated.h"

UINTERFACE(BlueprintType)
class URayRopeInterface : public UInterface
{
	GENERATED_BODY()
};

class RAY_API IRayRopeInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Rope|Getters")
	USceneComponent* GetAnchorComponent() const;
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Rope|Getters")
	FName GetAnchorSocketName() const;
};
