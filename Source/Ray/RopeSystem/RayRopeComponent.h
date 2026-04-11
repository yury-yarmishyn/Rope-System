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
	int32 WrapSolverIterations = 2;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	float WrapSolverEpsilon = 1.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	int32 MoveSolverIterations = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults", meta = (ClampMin = "1"))
	int32 MovePassMaxIterations = 8;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	float MoveSolverEpsilon = 1.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rope|Defaults")
	float RopePhysicalRadius = 5.f;
	
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
	
	void MovePass(int32 SegmentIndex, TArray<FRayRopeSegment>& NewSegments);
	void WrapPass(int32 SegmentIndex, TArray<FRayRopeSegment>& NewSegments);
	void UnwrapPass(int32 SegmentIndex, TArray<FRayRopeSegment>& NewSegments);
	
	void MoveNode(int32 NodeIndex, FRayRopeSegment& NewSegment, bool& bAnyNodeChanged) const;

	// Helpers - must not return invalid values, only fallbacks.
	
	FVector GetNodeDesiredWorldLocation(int32 NodeIndex, const FRayRopeSegment& NewSegment) const;
	FVector GetAnchorWorldLocation(const FRayRopeNode& Node) const;
	FVector FindEffectiveRedirection(int32 NodeIndex, const FRayRopeSegment& NewSegment) const;
};
