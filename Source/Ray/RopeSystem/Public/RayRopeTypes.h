#pragma once

#include "CoreMinimal.h"
#include "RayRopeTypes.generated.h"

class AActor;
class USceneComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogRayRope, Log, All);

USTRUCT(BlueprintType)
struct FRayRopeDebugSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|General")
	bool bDebugEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (EditCondition = "bDebugEnabled"))
	bool bDrawDebugRope = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Logging", meta = (EditCondition = "bDebugEnabled"))
	bool bLogDebugState = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	bool bDrawDebugLabels = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	bool bDrawDebugAttachmentLinks = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bDebugEnabled && bDrawDebugRope"))
	float DebugDrawLifetime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bDebugEnabled && bDrawDebugRope"))
	float DebugNodeRadius = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (ClampMin = "4", UIMin = "4", EditCondition = "bDebugEnabled && bDrawDebugRope"))
	int32 DebugNodeSphereSegments = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bDebugEnabled && bDrawDebugRope"))
	float DebugSegmentThickness = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bDebugEnabled && bDrawDebugRope"))
	float DebugAttachmentLinkThickness = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bDebugEnabled && bDrawDebugRope"))
	float DebugOwnerAxisLength = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Logging", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bDebugEnabled && bLogDebugState"))
	float DebugLogIntervalSeconds = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugOwnerColor = FLinearColor(0.65f, 0.3f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugSegmentColor = FLinearColor(0.f, 0.8f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugAnchorNodeColor = FLinearColor(0.1f, 1.f, 0.1f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugRedirectNodeColor = FLinearColor(1.f, 0.85f, 0.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugAttachmentLinkColor = FLinearColor(1.f, 1.f, 1.f, 0.6f);
};

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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node|Attachment")
	AActor* AttachedActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node|Attachment")
	bool bUseAttachedActorOffset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node|Attachment")
	FVector AttachedActorOffset = FVector::ZeroVector;

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
