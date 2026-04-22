#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RayRopeTypes.h"
#include "RayRopeComponent.generated.h"

struct FCollisionQueryParams;
struct FHitResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSegmentsSet);

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RAY_API URayRopeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	
	// Defaults
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	int32 MaxBinarySearchIteration = 4;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	float WrapSolverEpsilon = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	float RopePhysicalRadius = 2.f;
	
	UPROPERTY(EditAnywhere, Category = "Ray Rope|Solver")
	float RelaxSolverEpsilon = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Ray Rope|Solver")
	float RelaxCollinearEpsilon = 0.01f;
	
	// Dispatchers
	
	UPROPERTY(BlueprintAssignable, Category = "Rope|Dispatchers")
	FOnSegmentsSet OnSegmentsSet;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Dynamic")
	TArray<FRayRopeSegment> Segments;
	
public:
	URayRopeComponent();
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	
	// Getters
	
	UFUNCTION(BlueprintCallable, Category = "Rope|Getters")
	const TArray<FRayRopeSegment>& GetSegments() const;
	
	// Setters
	
	UFUNCTION(BlueprintCallable, Category = "Rope|Setters")
	void SetSegments(TArray<FRayRopeSegment> NewSegments);

protected:
	virtual void BeginPlay() override;
	
	// Solvers
	
	void SolveRope();
	
	void SyncSegmentAnchors(FRayRopeSegment& Segment) const;

	void MoveSegment(FRayRopeSegment& Segment) const;

	void WrapSegment(
		FRayRopeSegment& Segment,
		const FRayRopeSegment& ReferenceSegment) const;
	
	void RelaxSegment(FRayRopeSegment& Segment) const;
	
	void SplitSegmentOnAnchors(int32 SegmentIndex);
	
	void BinarySearchCollisionBoundary(
		FRayRopeNode& ValidLineStart,
		FRayRopeNode& ValidLineEnd,
		FRayRopeNode& InvalidLineStart,
		FRayRopeNode& InvalidLineEnd,
		const FCollisionQueryParams& QueryParams) const;
	
	void CalculateRedirectNode(
		const FRayRopeNode& LastValidLineStart,
		const FRayRopeNode& LastValidLineEnd,
		const FHitResult& SurfaceHit,
		FRayRopeNode& RedirectNode) const;
	
	// Helpers - must not return invalid values, only fallbacks.
	
	FVector GetAnchorWorldLocation(const FRayRopeNode& Node) const;
	void BuildTraceQueryParams(
		const FRayRopeSegment& Segment,
		FCollisionQueryParams& QueryParams) const;
	void TraceRopeLine(
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
	bool TryCreateWrapNode(
		int32 NodeIndex,
		const FRayRopeSegment& CurrentSegment,
		const FRayRopeSegment& ReferenceSegment,
		const FCollisionQueryParams& QueryParams,
		FRayRopeNode& OutNode) const;
	bool CanRemoveRelaxNode(
		const FRayRopeNode& PrevNode,
		const FRayRopeNode& CurrentNode,
		const FRayRopeNode& NextNode,
		const FCollisionQueryParams& QueryParams) const;
};
