#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "RayRopeTypes.h"
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

	// Editor-exposed solver and trace settings remain on the component for Blueprint and editor access.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxBinarySearchIteration = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WrapSolverEpsilon = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WrapOffset = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	bool bAllowWrapOnMovableObjects = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	bool bTraceComplex = false;

	UPROPERTY(EditAnywhere, Category = "Ray Rope|Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RelaxSolverEpsilon = 1.f;

	UPROPERTY(EditAnywhere, Category = "Ray Rope|Solver", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float RelaxCollinearEpsilon = 0.01f;

	// Blueprint dispatchers remain component-owned because they are part of the Unreal-facing API.
	UPROPERTY(BlueprintAssignable, Category = "Rope|Dispatchers")
	FOnSegmentsSet OnSegmentsSet;

	UPROPERTY(BlueprintAssignable, Category = "Rope|Dispatchers")
	FOnRopeSolveStarted OnRopeSolveStarted;

	UPROPERTY(BlueprintAssignable, Category = "Rope|Dispatchers")
	FOnRopeSolveEnded OnRopeSolveEnded;

	UPROPERTY(BlueprintAssignable, Category = "Rope|Dispatchers")
	FOnRopeSegmentBroken OnRopeSegmentBroken;

	UPROPERTY(BlueprintAssignable, Category = "Rope|Dispatchers")
	FOnRopeBroken OnRopeBroken;

	// Runtime rope state remains on the component for Blueprint visibility and lifecycle ownership.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Dynamic")
	bool bShouldSolveRope = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rope|Dynamic")
	float RopeLength = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Constraint", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxRopeLength = 0.f;

protected:
	// The component owns the runtime rope state that helper passes operate on.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Dynamic")
	TArray<FRayRopeSegment> Segments;

	// Component-owned orchestration stays here; the algorithmic passes live in plain C++ helpers.
	void SyncRopeNodes();
	void RefreshRopeLength();
	void SolveRope();
	void SolveSegment(FRayRopeSegment& Segment) const;
	bool SolveRopePhysics() const;
	void FinalizeSolve();
};
