#include "Component/RayRopeComponent.h"

#include "Solvers/SolvePipeline/RayRopeSolveTypes.h"

URayRopeComponent::URayRopeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	PrimaryComponentTick.EndTickGroup = TG_PostPhysics;
}

void URayRopeComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (RopeSegments.Num() == 0)
	{
		TickDebug(TEXT("TickEmpty"));
		return;
	}

	if (!bIsRopeSolving)
	{
		SyncRopeNodes();
		RefreshRopeLength();
		if (ApplyRopeRuntimeEffects())
		{
			SyncRopeNodes();
			RefreshRopeLength();
			OnRopeSegmentsUpdated.Broadcast();
		}

		TickDebug(TEXT("TickIdle"));
		return;
	}

	const FRayRopeSolveResult SolveResult = SolveRope();

	const bool bAppliedRuntimeEffects = ApplyRopeRuntimeEffects();
	if (bAppliedRuntimeEffects)
	{
		SyncRopeNodes();
		RefreshRopeLength();
	}

	if (SolveResult.DidChangeRope() || bAppliedRuntimeEffects)
	{
		OnRopeSegmentsUpdated.Broadcast();
	}
	TickDebug(TEXT("TickSolve"));
}
