#pragma once

#include "CoreMinimal.h"
#include "RayRopeTypes.generated.h"

class AActor;

DECLARE_LOG_CATEGORY_EXTERN(LogRayRope, Log, All);

UENUM(Blueprintable)
enum class ENodeType : uint8
{
	Anchor,
	Redirect
};

USTRUCT(BlueprintType)
struct FRayRopeNode
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node")
	ENodeType NodeType = ENodeType::Redirect;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node")
	FVector WorldLocation = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node")
	AActor* AttachActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node")
	bool bUseAttachActorOffset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node")
	FVector AttachActorOffset = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FRayRopeSegment
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Segment")
	TArray<FRayRopeNode> Nodes;
};
