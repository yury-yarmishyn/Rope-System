#pragma once

#include "CoreMinimal.h"
#include "RayRopeTypes.generated.h"

class AActor;
class USceneComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogRayRope, Log, All);

UENUM(Blueprintable)
enum class ERayRopeNodeType : uint8
{
	Anchor,
	Redirect
};

USTRUCT(BlueprintType)
struct FRayRopeNode
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node")
	ERayRopeNodeType NodeType = ERayRopeNodeType::Redirect;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node")
	FVector WorldLocation = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node")
	AActor* AttachActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node")
	bool bUseAttachActorOffset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node")
	FVector AttachActorOffset = FVector::ZeroVector;

	UPROPERTY(Transient)
	USceneComponent* CachedAnchorComponent = nullptr;

	UPROPERTY(Transient)
	FName CachedAnchorSocketName = NAME_None;
};

USTRUCT(BlueprintType)
struct FRayRopeSegment
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Segment")
	TArray<FRayRopeNode> Nodes;
};
