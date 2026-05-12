#pragma once

#include "CoreMinimal.h"
#include "RayRopeTypes.generated.h"

class AActor;
class USceneComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogRayRope, Log, All);

/**
 * Debug drawing layers for runtime rope diagnostics.
 */
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ERayRopeDebugDrawFlags : uint32
{
	None = 0 UMETA(Hidden),

	/** Draws owner, segment, and node topology. */
	Topology = 1 << 0,

	/** Draws collision queries issued by the rope solvers. */
	TraceQueries = 1 << 1,

	/** Draws wrap candidates and inserted redirect/anchor decisions. */
	Wrap = 1 << 2,

	/** Draws move rails derived from collision surfaces. */
	MoveRails = 1 << 3,

	/** Draws local move candidates, accepted targets, and rejected targets. */
	MoveCandidates = 1 << 4,

	/** Draws batch/global move accepted targets. */
	GlobalMove = 1 << 5,

	/** Draws transition validation paths and sampled fan failures. */
	TransitionValidation = 1 << 6,

	/** Draws relax shortcuts and collapse targets. */
	Relax = 1 << 7,

	/** Draws owner-side length clamp spans, deltas, and sweep hits. */
	PhysicsClamp = 1 << 8
};

ENUM_CLASS_FLAGS(ERayRopeDebugDrawFlags);

/**
 * Debug logging layers for runtime rope diagnostics.
 */
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ERayRopeDebugLogFlags : uint32
{
	None = 0 UMETA(Hidden),

	/** Logs per-frame solver event and trace counts. */
	FrameSummary = 1 << 0,

	/** Logs startup/base-span validation. */
	Start = 1 << 1,

	/** Logs pipeline pass summaries such as sync/wrap/move/relax length and node deltas. */
	Pipeline = 1 << 2,

	/** Logs trace query hits and rejection reasons. */
	TraceQueries = 1 << 3,

	/** Logs wrap and node-builder decisions. */
	Wrap = 1 << 4,

	/** Logs local move, rail, point-search, and move-validation decisions. */
	Move = 1 << 5,

	/** Logs batch/global move solver decisions. */
	GlobalMove = 1 << 6,

	/** Logs node transition validation failures. */
	TransitionValidation = 1 << 7,

	/** Logs relax collapse/remove decisions. */
	Relax = 1 << 8,

	/** Logs owner-side length clamp decisions. */
	PhysicsClamp = 1 << 9
};

ENUM_CLASS_FLAGS(ERayRopeDebugLogFlags);

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

	/** Selects which runtime diagnostic drawing layers are emitted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (Bitmask, BitmaskEnum = "/Script/RayRope.ERayRopeDebugDrawFlags", EditCondition = "bDebugEnabled && bDrawDebugRope"))
	int32 DebugDrawFlags =
		static_cast<int32>(ERayRopeDebugDrawFlags::Topology) |
		static_cast<int32>(ERayRopeDebugDrawFlags::Wrap) |
		static_cast<int32>(ERayRopeDebugDrawFlags::MoveRails) |
		static_cast<int32>(ERayRopeDebugDrawFlags::MoveCandidates) |
		static_cast<int32>(ERayRopeDebugDrawFlags::GlobalMove) |
		static_cast<int32>(ERayRopeDebugDrawFlags::TransitionValidation) |
		static_cast<int32>(ERayRopeDebugDrawFlags::Relax) |
		static_cast<int32>(ERayRopeDebugDrawFlags::PhysicsClamp);

	/** Selects which runtime diagnostic log layers are emitted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Logging", meta = (Bitmask, BitmaskEnum = "/Script/RayRope.ERayRopeDebugLogFlags", EditCondition = "bDebugEnabled"))
	int32 DebugLogFlags = static_cast<int32>(ERayRopeDebugLogFlags::None);

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

	/** Thickness used for solver trace, rail, and candidate debug lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Drawing", meta = (ClampMin = "0.0", UIMin = "0.0", EditCondition = "bDebugEnabled && bDrawDebugRope"))
	float DebugSolverLineThickness = 1.5f;

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

	/** Color used for blocking trace queries and rejected solver candidates. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugBlockedTraceColor = FLinearColor(1.f, 0.08f, 0.04f, 1.f);

	/** Color used for clear trace queries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugClearTraceColor = FLinearColor(0.1f, 0.9f, 0.25f, 1.f);

	/** Color used for candidate solver points before validation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugCandidateColor = FLinearColor(1.f, 0.55f, 0.f, 1.f);

	/** Color used for accepted solver points and movement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugAcceptedColor = FLinearColor(0.15f, 1.f, 0.55f, 1.f);

	/** Color used for move rails and transition guides. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug|Colors", meta = (EditCondition = "bDebugEnabled && bDrawDebugRope"))
	FLinearColor DebugSolverGuideColor = FLinearColor(1.f, 0.1f, 0.9f, 1.f);
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
