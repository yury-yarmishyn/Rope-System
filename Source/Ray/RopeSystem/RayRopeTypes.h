#pragma once

#include "CoreMinimal.h"
#include "RayRopeTypes.generated.h"

class AActor;

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

struct FRayRopeSpan
{
	const FRayRopeNode* StartNode = nullptr;
	const FRayRopeNode* EndNode = nullptr;

	bool IsValid() const
	{
		return StartNode != nullptr && EndNode != nullptr;
	}

	FVector GetStartLocation() const
	{
		return StartNode != nullptr ? StartNode->WorldLocation : FVector::ZeroVector;
	}

	FVector GetEndLocation() const
	{
		return EndNode != nullptr ? EndNode->WorldLocation : FVector::ZeroVector;
	}

	FVector GetDirection() const
	{
		return IsValid() ? GetEndLocation() - GetStartLocation() : FVector::ZeroVector;
	}

	float GetLengthSquared() const
	{
		return GetDirection().SizeSquared();
	}

	bool IsDegenerate(float Epsilon) const
	{
		return !IsValid() || GetLengthSquared() <= FMath::Square(Epsilon);
	}
};

USTRUCT(BlueprintType)
struct FRayRopeSegment
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Segment")
	TArray<FRayRopeNode> Nodes;
};


