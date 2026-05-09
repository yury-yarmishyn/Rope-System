#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/RayRopeTypes.h"
#include "RayRopeComponent.generated.h"

class AActor;

/** Event signature for rope segment update notifications. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSegmentsSet);

/** Event signature for transitions into active rope solving. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRopeSolveStarted);

/** Event signature for transitions out of active rope solving. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRopeSolveEnded);

/** Event signature for removal of a single rope segment. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRopeSegmentBroken, int32, SegmentIndex);

/** Event signature for complete rope teardown. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRopeBroken);

/**
 * Actor component that owns and solves segmented rope topology between anchor actors.
 *
 * During active solving it synchronizes attached nodes, wraps blocked spans, moves redirects along
 * contact rails, relaxes removable redirects, and optionally clamps the owner to a maximum length.
 */
UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RAY_API URayRopeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Creates a post-physics ticking rope component. */
	URayRopeComponent();

	/**
	 * Advances synchronization, solving, runtime length effects, debug output, and update events.
	 */
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Builds direct base segments between consecutive anchors and starts per-tick solving.
	 *
	 * Fails if anchors are invalid, duplicated within a span, or the initial direct spans are blocked.
	 * Broadcasts OnRopeSolveStarted only when transitioning from idle.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rope|Lifecycle")
	bool TryStartRopeSolve(const TArray<AActor*>& AnchorActors);

	/**
	 * Stops active solving after synchronizing the current topology.
	 *
	 * Keeps existing segments available and broadcasts OnRopeSolveEnded only when solving was active.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rope|Lifecycle")
	void EndRopeSolve();

	/**
	 * Removes one segment from the runtime topology.
	 *
	 * Ends solving and broadcasts rope-broken events if no segments remain.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rope|Lifecycle")
	bool BreakRopeOnSegment(int32 SegmentIndex);

	/**
	 * Clears all runtime rope topology.
	 *
	 * Broadcasts update/end/broken events according to the component's prior state.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rope|Lifecycle")
	void BreakRope();

	/**
	 * Returns the component-owned runtime topology.
	 *
	 * The returned reference is invalidated by later solve, set, or break operations.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rope|Getters")
	const TArray<FRayRopeSegment>& GetSegments() const;

	/**
	 * Replaces the runtime topology, synchronizes attached nodes, refreshes length, and broadcasts an update.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rope|Setters")
	void SetSegments(TArray<FRayRopeSegment> NewSegments);

	/**
	 * Toggles the debug master gate and resets throttled logging.
	 *
	 * May draw or log immediately when enabling debug output.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rope|Debug")
	void SetRopeDebugEnabled(bool bEnabled);

	/**
	 * Returns the debug master gate; draw and log sub-settings can still disable individual outputs.
	 */
	UFUNCTION(BlueprintPure, Category = "Rope|Debug")
	bool IsRopeDebugEnabled() const;

	/** Collision channel used for all rope line traces and overlap probes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Passed through to FCollisionQueryParams for rope traces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Trace")
	bool bTraceComplex = false;

	/** Allows redirect nodes to attach to movable or simulating hit objects. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Wrap Solver")
	bool bAllowWrapOnMovableObjects = true;

	/** Iteration budget for locating the clear-to-blocked span boundary during wrapping. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Wrap Solver", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxWrapBinarySearchIterations = 4;

	/** World-space tolerance for wrap boundary convergence and duplicate redirect filtering. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Wrap Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WrapSolverTolerance = 1.f;

	/** Distance applied along hit normals when placing redirect nodes off blocking surfaces. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Wrap Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WrapSurfaceOffset = 2.f;

	/** World-space tolerance for deciding whether a redirect has collapsed onto its shortcut. */
	UPROPERTY(EditAnywhere, Category = "Rope|Relax Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RelaxSolverTolerance = 1.f;

	/** Cross-product tolerance used to treat two surface normals as collinear. */
	UPROPERTY(EditAnywhere, Category = "Rope|Wrap Solver", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float GeometryCollinearityTolerance = 0.01f;

	/** Iteration budget for validating redirect collapse and removal transitions. */
	UPROPERTY(EditAnywhere, Category = "Rope|Relax Solver", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxRelaxCollapseIterations = 8;

	/** Geometry epsilon used when validating redirect movement. */
	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveSolverTolerance = 0.05f;

	/** Tolerance for treating two contact planes as parallel while constructing a move rail. */
	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MovePlaneParallelTolerance = 0.01f;

	/** Search tolerance used when selecting a reachable effective point on a move rail. */
	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveEffectivePointSearchTolerance = 0.05f;

	/** Minimum displacement required before a redirect move is considered useful. */
	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveMinMoveDistance = 0.05f;

	/** Minimum separation a moved redirect must keep from its neighboring nodes. */
	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveMinNodeSeparation = 0.05f;

	/** Required local path-length reduction for moving a free redirect. */
	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveMinLengthImprovement = 0.01f;

	/** Maximum distance a redirect may move in one iteration; zero disables the cap. */
	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveMaxDistancePerIteration = 2.f;

	/** Maximum number of alternating forward/backward move sweeps per solve. */
	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxMoveIterations = 4;

	/** Shared iteration budget for move rail hit search, rail optimization, and transition validation. */
	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxEffectivePointSearchIterations = 8;

	/** Broadcast after segment replacement, break operations, or solve ticks. */
	UPROPERTY(BlueprintAssignable, Category = "Rope|Events")
	FOnSegmentsSet OnRopeSegmentsUpdated;

	/** Broadcast when the component transitions into active solving. */
	UPROPERTY(BlueprintAssignable, Category = "Rope|Events")
	FOnRopeSolveStarted OnRopeSolveStarted;

	/** Broadcast when the component transitions out of active solving. */
	UPROPERTY(BlueprintAssignable, Category = "Rope|Events")
	FOnRopeSolveEnded OnRopeSolveEnded;

	/** Broadcast after BreakRopeOnSegment removes a segment. */
	UPROPERTY(BlueprintAssignable, Category = "Rope|Events")
	FOnRopeSegmentBroken OnRopeSegmentBroken;

	/** Broadcast after all rope topology has been cleared. */
	UPROPERTY(BlueprintAssignable, Category = "Rope|Events")
	FOnRopeBroken OnRopeBroken;

	/** True while the component runs wrap, move, and relax solver passes each tick. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Runtime")
	bool bIsRopeSolving = false;

	/** Total length of all current segments after the last synchronization or solve. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rope|Runtime")
	float CurrentRopeLength = 0.f;

	/** Optional length limit; values less than or equal to zero disable owner clamping. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Length Constraint", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxAllowedRopeLength = 0.f;

	/** Per-component debug drawing and logging settings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug")
	FRayRopeDebugSettings RopeDebugSettings;

protected:
	/** Component-owned mutable topology operated on by helper passes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Runtime")
	TArray<FRayRopeSegment> RopeSegments;

	/** Next world time at which non-forced debug logging may emit. */
	UPROPERTY(Transient)
	float NextDebugLogTimeSeconds = 0.f;

	/** Reused reference topology buffer to avoid per-segment allocation churn during solving. */
	UPROPERTY(Transient)
	TArray<FRayRopeNode> ReferenceNodesScratch;

	void SyncRopeNodes();
	void RefreshRopeLength();
	void SolveRope();
	void SolveSegment(FRayRopeSegment& Segment, int32 SegmentIndex) const;
	/** Applies runtime effects such as owner length clamping; returns true when owner movement occurred. */
	bool ApplyRopeRuntimeEffects();

	/** Splits internal anchors into terminal segment anchors and refreshes length after solve passes. */
	void FinalizeSolve();

	bool IsDebugDrawEnabled() const;
	bool IsDebugLogEnabled() const;
	void TickDebug(const TCHAR* Context);
	void DrawDebugRope() const;
	void LogDebugRopeState(const TCHAR* Context, bool bForce = false);
};
