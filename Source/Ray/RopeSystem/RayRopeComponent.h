#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RayRopeTypes.h"
#include "RayRopeComponent.generated.h"

class AActor;
struct FCollisionQueryParams;
struct FHitResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSegmentsSet);

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RAY_API URayRopeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	int32 MaxBinarySearchIteration = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	float WrapSolverEpsilon = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	float RopePhysicalRadius = 2.f;

	UPROPERTY(EditAnywhere, Category = "Ray Rope|Solver")
	float RelaxSolverEpsilon = 1.f;

	UPROPERTY(EditAnywhere, Category = "Ray Rope|Solver")
	float RelaxCollinearEpsilon = 0.01f;

	UPROPERTY(BlueprintAssignable, Category = "Rope|Dispatchers")
	FOnSegmentsSet OnSegmentsSet;

	URayRopeComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Rope|Getters")
	const TArray<FRayRopeSegment>& GetSegments() const;

	UFUNCTION(BlueprintCallable, Category = "Rope|Setters")
	void SetSegments(TArray<FRayRopeSegment> NewSegments);

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Dynamic")
	TArray<FRayRopeSegment> Segments;

	void SolveRope();
	void SyncSegmentAnchors(FRayRopeSegment& Segment) const;
	void SyncAttachedRedirectNodes(FRayRopeSegment& Segment) const;
	void MoveSegment(FRayRopeSegment& Segment) const;
	void WrapSegment(FRayRopeSegment& Segment, const FRayRopeSegment& ReferenceSegment) const;
	void RelaxSegment(FRayRopeSegment& Segment) const;
	void SplitSegmentOnAnchors(int32 SegmentIndex);

	bool BuildWrapNodes(
		int32 NodeIndex,
		const FRayRopeSegment& CurrentSegment,
		const FRayRopeSegment& ReferenceSegment,
		const FCollisionQueryParams& QueryParams,
		TArray<FRayRopeNode>& OutNodes) const;

	bool TryFindBoundaryHit(
		const FRayRopeNode& ValidLineStart,
		const FRayRopeNode& ValidLineEnd,
		const FRayRopeNode& InvalidLineStart,
		const FRayRopeNode& InvalidLineEnd,
		const FCollisionQueryParams& QueryParams,
		FHitResult& SurfaceHit) const;

	FRayRopeNode CreateAnchorNode(AActor* AnchorActor) const;
	FRayRopeNode CreateRedirectNode(
		const FRayRopeNode& ValidLineStart,
		const FRayRopeNode& ValidLineEnd,
		const FHitResult& FrontSurfaceHit) const;

	FRayRopeNode CreateRedirectNode(
		const FRayRopeNode& ValidLineStart,
		const FRayRopeNode& ValidLineEnd,
		const FHitResult& FrontSurfaceHit,
		const FHitResult& BackSurfaceHit) const;

	void AppendRedirectNodes(
		const FRayRopeNode& ValidLineStart,
		const FRayRopeNode& ValidLineEnd,
		const FHitResult& FrontSurfaceHit,
		TArray<FRayRopeNode>& OutNodes) const;

	void AppendRedirectNodes(
		const FRayRopeNode& ValidLineStart,
		const FRayRopeNode& ValidLineEnd,
		const FHitResult& FrontSurfaceHit,
		const FHitResult& BackSurfaceHit,
		TArray<FRayRopeNode>& OutNodes) const;

	void BinarySearchCollisionBoundary(
		FRayRopeNode& ValidLineStart,
		FRayRopeNode& ValidLineEnd,
		FRayRopeNode& InvalidLineStart,
		FRayRopeNode& InvalidLineEnd,
		const FCollisionQueryParams& QueryParams) const;

	FVector CalculateRedirectLocation(
		const FRayRopeNode& ValidLineStart,
		const FRayRopeNode& ValidLineEnd,
		const FHitResult& FrontSurfaceHit) const;

	FVector CalculateRedirectLocation(
		const FRayRopeNode& ValidLineStart,
		const FRayRopeNode& ValidLineEnd,
		const FHitResult& FrontSurfaceHit,
		const FHitResult& BackSurfaceHit) const;

	FVector CalculatePlaneRedirectLocation(
		const FRayRopeNode& ValidLineStart,
		const FRayRopeNode& ValidLineEnd,
		const FHitResult& SurfaceHit) const;

	FVector CalculateRedirectOffset(const FHitResult& FrontSurfaceHit) const;

	FVector CalculateRedirectOffset(
		const FHitResult& FrontSurfaceHit,
		const FHitResult& BackSurfaceHit) const;

	bool AreHitsParallel(
		const FHitResult& FrontSurfaceHit,
		const FHitResult& BackSurfaceHit) const;

	bool TryGetPlaneIntersectionLine(
		const FHitResult& FrontSurfaceHit,
		const FHitResult& BackSurfaceHit,
		FVector& OutLinePoint,
		FVector& OutLineDirection) const;

	void FindClosestPointOnSegmentToLine(
		const FVector& SegmentStart,
		const FVector& SegmentEnd,
		const FVector& LinePoint,
		const FVector& LineDirection,
		FVector& OutPointOnSegment,
		float& OutDistanceSquared) const;

	void CacheRedirectNodeOffset(FRayRopeNode& RedirectNode) const;

	FVector GetAnchorWorldLocation(const FRayRopeNode& Node) const;

	void BuildTraceQueryParams(
		const FRayRopeSegment& Segment,
		FCollisionQueryParams& QueryParams) const;

	void TraceRopeLine(
		const FCollisionQueryParams& QueryParams,
		const FVector& StartLocation,
		const FVector& EndLocation,
		FHitResult& SurfaceHit) const;

	bool TryTraceBlockingHit(
		const FCollisionQueryParams& QueryParams,
		const FVector& StartLocation,
		const FVector& EndLocation,
		FHitResult& SurfaceHit) const;

	bool CanInsertWrapNode(
		int32 InsertIndex,
		const FRayRopeSegment& Segment,
		const FRayRopeNode& Candidate,
		const TArray<FRayRopeNode>& PendingNodes) const;

	bool AreEquivalentWrapNodes(
		const FRayRopeNode& FirstNode,
		const FRayRopeNode& SecondNode) const;

	bool CanRemoveRelaxNode(
		const FRayRopeNode& PrevNode,
		const FRayRopeNode& CurrentNode,
		const FRayRopeNode& NextNode,
		const FCollisionQueryParams& QueryParams) const;
};
