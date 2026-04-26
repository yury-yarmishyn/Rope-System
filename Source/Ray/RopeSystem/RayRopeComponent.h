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

	struct FRayNodeSpan
	{
		const FRayRopeNode* Start = nullptr;
		const FRayRopeNode* End = nullptr;
	};

	struct FRayWrapRedirectInputs
	{
		FRayNodeSpan ValidSpan;
		const FHitResult* FrontSurfaceHit = nullptr;
		const FHitResult* BackSurfaceHit = nullptr;
	};

	struct FPendingWrapInsertion
	{
		int32 InsertIndex = INDEX_NONE;
		FRayRopeNode Node;
	};

	void SolveRope();
	void SyncSegmentNodes(FRayRopeSegment& Segment) const;
	void SyncNode(FRayRopeNode& Node) const;
	void SyncRedirectNode(FRayRopeNode& Node) const;
	void CacheAttachActorOffset(FRayRopeNode& Node) const;
	AActor* ResolveRedirectAttachActor(
		const FHitResult& FrontSurfaceHit,
		const FHitResult* BackSurfaceHit) const;
	void MoveSegment(FRayRopeSegment& Segment) const;
	void WrapSegment(FRayRopeSegment& Segment, const FRayRopeSegment& ReferenceSegment) const;
	void RelaxSegment(FRayRopeSegment& Segment) const;
	void SplitSegmentOnAnchors(int32 SegmentIndex);

	FRayNodeSpan MakeSegmentSpan(const FRayRopeSegment& Segment, int32 NodeIndex) const;
	FRayNodeSpan ReverseSpan(const FRayNodeSpan& Span) const;
	bool IsValidSpan(const FRayNodeSpan& Span) const;

	bool BuildWrapNodes(
		int32 NodeIndex,
		const FRayRopeSegment& CurrentSegment,
		const FRayRopeSegment& ReferenceSegment,
		const FCollisionQueryParams& QueryParams,
		TArray<FRayRopeNode>& OutNodes) const;

	bool TryBuildWrapRedirectInputs(
		const FRayNodeSpan& CurrentSpan,
		const FRayNodeSpan& ReferenceSpan,
		const FCollisionQueryParams& QueryParams,
		const FHitResult& FrontSurfaceHit,
		FHitResult& OutResolvedFrontSurfaceHit,
		FHitResult& OutResolvedBackSurfaceHit,
		FRayWrapRedirectInputs& OutRedirectInputs) const;

	bool TryFindBoundaryHit(
		const FRayNodeSpan& ValidSpan,
		const FRayNodeSpan& InvalidSpan,
		const FCollisionQueryParams& QueryParams,
		FHitResult& SurfaceHit) const;

	FRayRopeNode CreateAnchorNode(AActor* AnchorActor) const;

	FRayRopeNode CreateRedirectNode(const FRayWrapRedirectInputs& RedirectInputs) const;

	void AppendRedirectNodes(const FRayWrapRedirectInputs& RedirectInputs, TArray<FRayRopeNode>& OutNodes) const;

	FVector CalculateProjectedPointOnHitPlane(
		const FRayNodeSpan& ValidSpan,
		const FHitResult& SurfaceHit) const;

	FVector CalculateRedirectLocation(const FRayWrapRedirectInputs& RedirectInputs) const;

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

	FVector GetAnchorWorldLocation(const FRayRopeNode& Node) const;

	void BuildTraceQueryParams(
		const FRayRopeSegment& Segment,
		FCollisionQueryParams& QueryParams) const;

	bool TryTraceSpan(
		const FRayNodeSpan& Span,
		const FCollisionQueryParams& QueryParams,
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
		const TArray<FPendingWrapInsertion>& PendingInsertions) const;

	bool AreEquivalentWrapNodes(
		const FRayRopeNode& FirstNode,
		const FRayRopeNode& SecondNode) const;

	bool IsTraceEnteringHitSurface(
		const FVector& StartLocation,
		const FVector& EndLocation,
		const FHitResult& SurfaceHit) const;

	bool CanRemoveRelaxNode(
		const FRayRopeNode& PrevNode,
		const FRayRopeNode& CurrentNode,
		const FRayRopeNode& NextNode,
		const FCollisionQueryParams& QueryParams) const;
};
