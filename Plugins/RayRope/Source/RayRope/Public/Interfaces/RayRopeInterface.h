#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RayRopeInterface.generated.h"

class USceneComponent;

/**
 * Implemented by actors that can be used as rope anchors.
 *
 * The rope component queries this interface while synchronizing anchor nodes so an actor can expose
 * a specific component/socket instead of using the actor origin.
 */
UINTERFACE(BlueprintType)
class RAYROPE_API URayRopeInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Blueprint-facing anchor contract for the rope system.
 */
class RAYROPE_API IRayRopeInterface
{
	GENERATED_BODY()

public:
	/**
	 * Returns the component that defines the rope anchor transform.
	 *
	 * Returning null falls back to the actor location.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Rope|Getters")
	USceneComponent* GetAnchorComponent() const;
	
	/**
	 * Returns the optional socket used on the anchor component.
	 *
	 * NAME_None or a missing socket falls back to the component location.
	 */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Rope|Getters")
	FName GetAnchorSocketName() const;
};
