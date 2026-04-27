#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RayRopeTypes.h"
#include "RayRopeComponent.generated.h"

class AActor;
struct FCollisionQueryParams;
struct FHitResult;

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults", meta = (ClampMin = "0", UIMin = "0"))
	int32 MaxBinarySearchIteration = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float WrapSolverEpsilon = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RopePhysicalRadius = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	bool bAllowWrapOnMovableObjects = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, Category = "Ray Rope|Solver", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RelaxSolverEpsilon = 1.f;

	UPROPERTY(EditAnywhere, Category = "Ray Rope|Solver", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float RelaxCollinearEpsilon = 0.01f;

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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Dynamic")
	bool bShouldSolveRope = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rope|Dynamic")
	float RopeLength = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Constraint", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxRopeLength = 0.f;

	URayRopeComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

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

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Dynamic")
	TArray<FRayRopeSegment> Segments;

	bool TryBuildBaseSegments(
		const TArray<AActor*>& AnchorActors,
		TArray<FRayRopeSegment>& OutSegments) const;

	bool EnforceMaxRopeLength();
	bool TryGetOwnerTerminalNodes(
		const FRayRopeNode*& OutOwnerNode,
		const FRayRopeNode*& OutAdjacentNode) const;
	bool ClampOwnerAnchorToMaxRopeLength(
		const FRayRopeNode& OwnerNode,
		const FRayRopeNode& AdjacentNode) const;
	void RemoveOwnerOutwardVelocity(const FVector& OutwardDirection) const;

	void RefreshRopeLength(bool bSyncSegmentNodes);
	float CalculateRopeLength() const;

	void SolveRope();
	AActor* ResolveRedirectAttachActor(
		const FHitResult& FrontSurfaceHit,
		const FHitResult* BackSurfaceHit) const;
	void MoveSegment(FRayRopeSegment& Segment) const;
	void WrapSegment(FRayRopeSegment& Segment, const FRayRopeSegment& ReferenceSegment) const;
	void RelaxSegment(FRayRopeSegment& Segment) const;

	bool BuildWrapNodes(
		int32 NodeIndex,
		const FRayRopeSegment& CurrentSegment,
		const FRayRopeSegment& ReferenceSegment,
		const FCollisionQueryParams& QueryParams,
		TArray<FRayRopeNode>& OutNodes) const;

	bool TryBuildWrapRedirectInputs(
		const FRayRopeSpan& CurrentSpan,
		const FRayRopeSpan& ReferenceSpan,
		const FCollisionQueryParams& QueryParams,
		const FHitResult& FrontSurfaceHit,
		FRayWrapRedirectInput& OutInput) const;

	bool TryFindBoundaryHit(
		const FRayRopeSpan& ValidSpan,
		const FRayRopeSpan& InvalidSpan,
		const FCollisionQueryParams& QueryParams,
		FHitResult& SurfaceHit) const;

	FRayRopeNode CreateRedirectNode(
		const FRayRopeSpan& ValidSpan,
		const FHitResult& FrontSurfaceHit,
		const FHitResult* BackSurfaceHit) const;

	void AppendRedirectNodes(
		const FRayRopeSpan& ValidSpan,
		const FHitResult& FrontSurfaceHit,
		const FHitResult* BackSurfaceHit,
		TArray<FRayRopeNode>& OutNodes) const;

	FVector CalculateProjectedPointOnHitPlane(
		const FRayRopeSpan& ValidSpan,
		const FHitResult& SurfaceHit) const;

	FVector CalculateRedirectLocation(
		const FRayRopeSpan& ValidSpan,
		const FHitResult& FrontSurfaceHit,
		const FHitResult* BackSurfaceHit) const;

	FVector CalculateRedirectOffset(
		const FHitResult& FrontSurfaceHit,
		const FHitResult* BackSurfaceHit = nullptr) const;

	bool AreDirectionsNearlyCollinear(
		const FVector& FirstDirection,
		const FVector& SecondDirection,
		float Epsilon) const;

	bool TryGetPlaneIntersectionLine(
		const FHitResult& FrontSurfaceHit,
		const FHitResult& BackSurfaceHit,
		FVector& OutLinePoint,
		FVector& OutLineDirection) const;

	void FindClosestPointsOnSegmentToLine(
		const FVector& SegmentStart,
		const FVector& SegmentEnd,
		const FVector& LinePoint,
		const FVector& LineDirection,
		FVector& OutPointOnSegment,
		FVector& OutPointOnLine,
		float& OutDistanceSquared) const;

	bool CanInsertWrapNode(
		int32 InsertIndex,
		const FRayRopeSegment& Segment,
		const FRayRopeNode& Candidate,
		const TArray<TPair<int32, FRayRopeNode>>& PendingInsertions) const;

	bool AreEquivalentWrapNodes(
		const FRayRopeNode& FirstNode,
		const FRayRopeNode& SecondNode) const;

	bool CanRemoveRelaxNode(
		const FRayRopeNode& PrevNode,
		const FRayRopeNode& CurrentNode,
		const FRayRopeNode& NextNode,
		const FCollisionQueryParams& QueryParams) const;
};
