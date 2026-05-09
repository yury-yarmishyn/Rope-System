#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/RayRopeTypes.h"
#include "RayRopeComponent.generated.h"

class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSegmentsSet);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRopeSolveStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRopeSolveEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRopeSegmentBroken, int32, SegmentIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRopeBroken);

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RAY_API URayRopeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URayRopeComponent();

	// Component lifecycle remains here because ticking and event timing are Unreal-facing concerns.
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// Public Blueprint API remains on the component.
	UFUNCTION(BlueprintCallable, Category = "Rope|Lifecycle")
	bool TryStartRopeSolve(const TArray<AActor*>& AnchorActors);

	UFUNCTION(BlueprintCallable, Category = "Rope|Lifecycle")
	void EndRopeSolve();

	UFUNCTION(BlueprintCallable, Category = "Rope|Lifecycle")
	bool BreakRopeOnSegment(int32 SegmentIndex);

	UFUNCTION(BlueprintCallable, Category = "Rope|Lifecycle")
	void BreakRope();

	UFUNCTION(BlueprintCallable, Category = "Rope|Getters")
	const TArray<FRayRopeSegment>& GetSegments() const;

	UFUNCTION(BlueprintCallable, Category = "Rope|Setters")
	void SetSegments(TArray<FRayRopeSegment> NewSegments);

	UFUNCTION(BlueprintCallable, Category = "Rope|Debug")
	void SetRopeDebugEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Rope|Debug")
	bool IsRopeDebugEnabled() const;

	// Editor-exposed solver and trace settings remain on the component for Blueprint and editor access.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Trace")
	bool bTraceComplex = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Wrap Solver")
	bool bAllowWrapOnMovableObjects = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Wrap Solver", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxWrapBinarySearchIterations = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Wrap Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WrapSolverTolerance = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Wrap Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WrapSurfaceOffset = 2.f;

	UPROPERTY(EditAnywhere, Category = "Rope|Relax Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RelaxSolverTolerance = 1.f;

	UPROPERTY(EditAnywhere, Category = "Rope|Wrap Solver", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float GeometryCollinearityTolerance = 0.01f;

	UPROPERTY(EditAnywhere, Category = "Rope|Relax Solver", meta = (ClampMin = "1", UIMin = "1"))
	int32 MaxRelaxCollapseIterations = 8;

	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveSolverTolerance = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MovePlaneParallelTolerance = 0.01f;

	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveEffectivePointSearchTolerance = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveMinMoveDistance = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveMinNodeSeparation = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveMinLengthImprovement = 0.01f;

	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MoveMaxDistancePerIteration = 2.f;

	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxMoveIterations = 4;

	UPROPERTY(EditAnywhere, Category = "Rope|Move Solver", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxEffectivePointSearchIterations = 8;

	// Blueprint dispatchers remain component-owned because they are part of the Unreal-facing API.
	UPROPERTY(BlueprintAssignable, Category = "Rope|Events")
	FOnSegmentsSet OnRopeSegmentsUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Rope|Events")
	FOnRopeSolveStarted OnRopeSolveStarted;

	UPROPERTY(BlueprintAssignable, Category = "Rope|Events")
	FOnRopeSolveEnded OnRopeSolveEnded;

	UPROPERTY(BlueprintAssignable, Category = "Rope|Events")
	FOnRopeSegmentBroken OnRopeSegmentBroken;

	UPROPERTY(BlueprintAssignable, Category = "Rope|Events")
	FOnRopeBroken OnRopeBroken;

	// Runtime rope state remains on the component for Blueprint visibility and lifecycle ownership.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Runtime")
	bool bIsRopeSolving = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rope|Runtime")
	float CurrentRopeLength = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Length Constraint", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxAllowedRopeLength = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Debug")
	FRayRopeDebugSettings RopeDebugSettings;

protected:
	// The component owns the runtime rope state that helper passes operate on.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Runtime")
	TArray<FRayRopeSegment> RopeSegments;

	UPROPERTY(Transient)
	float NextDebugLogTimeSeconds = 0.f;

	UPROPERTY(Transient)
	TArray<FRayRopeNode> ReferenceNodesScratch;

	// Component-owned orchestration stays here; the algorithmic passes live in plain C++ helpers.
	void SyncRopeNodes();
	void RefreshRopeLength();
	void SolveRope();
	void SolveSegment(FRayRopeSegment& Segment, int32 SegmentIndex) const;
	bool ApplyRopeRuntimeEffects();
	void FinalizeSolve();

	bool IsDebugDrawEnabled() const;
	bool IsDebugLogEnabled() const;
	void TickDebug(const TCHAR* Context);
	void DrawDebugRope() const;
	void LogDebugRopeState(const TCHAR* Context, bool bForce = false);
};
