#pragma once

#include "CoreMinimal.h"
#include "RayRopeTypes.generated.h"

class AActor;
class USceneComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogRayRope, Log, All);

/**
 * Per-component controls for rope diagnostics.
 *
 * These settings only affect debug drawing and logging; they do not change solver behavior.
 */
USTRUCT(BlueprintType)
struct FRayRopeDebugSettings
{
	GENERATED_BODY()

	/** Master gate for all rope debug output on the component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|General")
	bool bDebugEnabled = false;

	/** Draws the current runtime topology in the world when debug is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (EditCondition = "bDebugEnabled"))
	bool bDrawDebugRope = true;

	/** Emits throttled rope state snapshots to LogRayRope when debug is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Logging", meta = (EditCondition = "bDebugEnabled"))
	bool bLogDebugState = false;

	/** Adds per-segment and per-node text labels to debug drawing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	bool bDrawDebugLabels = true;

	/** Draws links from rope nodes to attached actors to reveal cached attachment offsets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	bool bDrawDebugAttachmentLinks = true;

	/** Lifetime passed to debug draw calls; zero draws for the current frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bDebugEnabled && bDrawDebugRope"))
	float DebugDrawLifetime = 0.f;

	/** Radius used for node debug spheres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bDebugEnabled && bDrawDebugRope"))
	float DebugNodeRadius = 10.f;

	/** Segment count used when drawing node debug spheres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (ClampMin = "4", UIMin = "4", EditCondition = "bDebugEnabled && bDrawDebugRope"))
	int32 DebugNodeSphereSegments = 12;

	/** Thickness used for rope span debug lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bDebugEnabled && bDrawDebugRope"))
	float DebugSegmentThickness = 2.f;

	/** Thickness used for debug lines from nodes to their attached actors. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bDebugEnabled && bDrawDebugRope"))
	float DebugAttachmentLinkThickness = 1.f;

	/** Length of the owner coordinate axis drawn by rope debug visualization. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bDebugEnabled && bDrawDebugRope"))
	float DebugOwnerAxisLength = 35.f;

	/** Minimum time between automatic rope state log snapshots; zero logs every eligible tick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Logging", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bDebugEnabled && bLogDebugState"))
	float DebugLogIntervalSeconds = 1.f;

	/** Color used for owner debug markers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugOwnerColor = FLinearColor(0.65f, 0.3f, 1.f, 1.f);

	/** Color used for rope span debug lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugSegmentColor = FLinearColor(0.f, 0.8f, 1.f, 1.f);

	/** Color used for anchor node debug markers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugAnchorNodeColor = FLinearColor(0.1f, 1.f, 0.1f, 1.f);

	/** Color used for redirect node debug markers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugRedirectNodeColor = FLinearColor(1.f, 0.85f, 0.f, 1.f);

	/** Color used for debug attachment links. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugAttachmentLinkColor = FLinearColor(1.f, 1.f, 1.f, 0.6f);
};

/**
 * Runtime role of a node in a rope segment.
 */
UENUM(Blueprintable)
enum class ERayRopeNodeType : uint8
{
	/** Actor-driven point that can terminate or split rope segments. */
	Anchor,

	/** Solver-created bend point that keeps the rope clear of blocking geometry. */
	Redirect
};

/**
 * Serialized node in a rope segment.
 *
 * Anchor nodes are synchronized from their attached actors. Redirect nodes can either be free
 * world-space points or actor-relative offsets when they are attached to moving geometry.
 */
USTRUCT(BlueprintType)
struct FRayRopeNode
{
	GENERATED_BODY()
	
	/** Selects the synchronization and solver rules used for this node. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node")
	ERayRopeNodeType NodeType = ERayRopeNodeType::Redirect;
	
	/** Authoritative world-space location used by solvers after synchronization. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node")
	FVector WorldLocation = FVector::ZeroVector;
	
	/** Actor that anchors this node or owns a redirect offset on moving geometry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node|Attachment")
	AActor* AttachedActor = nullptr;

	/** When true, redirect synchronization derives WorldLocation from AttachedActorOffset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node|Attachment")
	bool bUseAttachedActorOffset = false;

	/** Actor-local redirect position captured when the node is attached to an actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Node|Attachment")
	FVector AttachedActorOffset = FVector::ZeroVector;

	/** Cached anchor component from IRayRopeInterface; refreshed during node synchronization. */
	UPROPERTY(Transient)
	USceneComponent* CachedAnchorComponent = nullptr;

	/** Cached anchor socket from IRayRopeInterface; NAME_None means use the component location. */
	UPROPERTY(Transient)
	FName CachedAnchorSocketName = NAME_None;
};

/**
 * Contiguous rope topology between two anchor endpoints.
 *
 * The solver expects active segments to contain at least two nodes.
 */
USTRUCT(BlueprintType)
struct FRayRopeSegment
{
	GENERATED_BODY()
	
	/** Ordered node chain from start anchor to end anchor, including any redirect nodes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Segment")
	TArray<FRayRopeNode> Nodes;
};
