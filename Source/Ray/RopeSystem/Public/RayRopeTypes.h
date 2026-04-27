#pragma once

#include "CollisionQueryParams.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "RayRopeTypes.generated.h"

class AActor;
class UWorld;

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

struct FRayRopeTraceSettings
{
	UWorld* World = nullptr;
	const AActor* OwnerActor = nullptr;
	ECollisionChannel TraceChannel = ECC_Visibility;
};

struct FRayRopeTraceContext
{
	UWorld* World = nullptr;
	ECollisionChannel TraceChannel = ECC_Visibility;
	FCollisionQueryParams QueryParams;
};

struct FRayRopeRelaxSettings
{
	float RelaxSolverEpsilon = 0.f;
	float RelaxCollinearEpsilon = 0.f;
};

struct FRayRopeWrapSettings
{
	bool bAllowWrapOnMovableObjects = false;
	int32 MaxBinarySearchIteration = 0;
	float WrapSolverEpsilon = 0.f;
	float GeometryCollinearEpsilon = 0.f;
	float RopePhysicalRadius = 0.f;
};

struct FRayWrapRedirectInput
{
	FRayRopeSpan ValidSpan;
	FHitResult FrontSurfaceHit;
	FHitResult BackSurfaceHit;
	bool bHasBackSurfaceHit = false;

	const FHitResult* GetBackSurfaceHitPtr() const
	{
		return bHasBackSurfaceHit ? &BackSurfaceHit : nullptr;
	}
};
