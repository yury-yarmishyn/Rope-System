#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RayRopeTypes.h"
#include "RayRopeComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSegmentsSet);

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RAY_API URayRopeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	
	// Defaults
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	int32 MaxWrapIterations = 2;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	int32 MaxBinarySearchIteration = 4;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	float WrapSolverEpsilon = 1.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults", meta = (ClampMin = "1"))
	int32 MoveSolverIterations = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults", meta = (ClampMin = "1"))
	int32 MaxMoveIterations = 8;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	float MoveSolverEpsilon = 1.f;
	
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
	
	void SyncAnchors(TArray<FRayRopeSegment>& NewSegments) const;

	void MoveSegment(
		int32 SegmentIndex, 
		TArray<FRayRopeSegment>& NewSegments) const;
	
	void WrapSegment(
		int32 SegmentIndex, 
		TArray<FRayRopeSegment>& NewSegments,
		const TArray<FRayRopeSegment>& PrevSegments) const;
	
	void RelaxSegment(
		int32 SegmentIndex, 
		TArray<FRayRopeSegment>& NewSegments) const;
	
	void SplitSegmentOnAnchors(
		int32 SegmentIndex,
		TArray<FRayRopeSegment>& NewSegments) const;
	
	void MoveNode(
		int32 NodeIndex, 
		FRayRopeSegment& NewSegment, 
		bool& bAnyNodeChanged) const;
	
	void BinarySearchCollisionBoundary(
		const FRayRopeSegment& Segment,
		FRayRopeNode& ValidLineStart,
		FRayRopeNode& ValidLineEnd,
		FRayRopeNode& InvalidLineStart,
		FRayRopeNode& InvalidLineEnd) const;
	
	void CalculateRedirectNode(
		const FRayRopeNode& LastValidLineStart,
		const FRayRopeNode& LastValidLineEnd,
		const FHitResult& SurfaceHit,
		FRayRopeNode& RedirectNode) const;
	
	void AddNodesToSegment(
		const TMap<int32, FRayRopeSegment>& NodesToAdd,
		FRayRopeSegment& NewSegment) const;
	
	// Helpers - must not return invalid values, only fallbacks.
	
	FVector GetNodeDesiredWorldLocation(int32 NodeIndex, const FRayRopeSegment& InSegment) const;
	FVector GetAnchorWorldLocation(const FRayRopeNode& Node) const;
	FVector FindEffectiveRedirect(int32 NodeIndex, const FRayRopeSegment& InSegment) const;
	void TraceSegmentNodes(
		const FRayRopeSegment& Segment,
		const FRayRopeNode& StartNode,
		const FRayRopeNode& EndNode,
		FHitResult& SurfaceHit) const;
	bool CanInsertWrapNode(
		int32 InsertIndex, 
		const FRayRopeSegment& Segment, 
		const FRayRopeNode& Candidate) const;
	bool TryCreateWrapNode(
		int32 NodeIndex,
		const FRayRopeSegment& CurrentSegment,
		const FRayRopeSegment& ReferenceSegment,
		FRayRopeNode& OutNode) const;
	bool CanRelaxNodeByTrace(
	const FRayRopeSegment& Segment,
	int32 NodeIndex) const;
	bool AreSegmentsCollinear(
		const FRayRopeNode& PrevNode,
		const FRayRopeNode& CurrentNode,
		const FRayRopeNode& NextNode) const;
	FVector GetClosestPointOnLine(
		const FVector& Point,
		const FVector& LineStart,
		const FVector& LineEnd) const;
};
