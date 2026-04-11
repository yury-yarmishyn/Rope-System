#pragma once

#include "CoreMinimal.h"
#include "AITypes.h"
#include "RayRopeTypes.generated.h"

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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node", 
		meta = (EditCondition = "NodeType == ENodeType::Anchor", EditConditionHides))
	AActor* AnchorActor = nullptr;
};

USTRUCT(BlueprintType)
struct FRayRopeSegment
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Segment")
	TArray<FRayRopeNode> Nodes;
};