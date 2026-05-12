#include "Component/RayRopeComponent.h"

#include "Debug/RayRopeDebugContext.h"
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

#if RAYROPE_WITH_DEBUG
	FRayRopeDebugContext DebugContext(
		GetWorld(),
		GetOwner(),
		RopeDebugSettings,
		TEXT("Tick"));
	FRayRopeDebugContext* DebugContextPtr = FRayRopeDebugContext::ShouldCreateForSolvers(RopeDebugSettings)
		? &DebugContext
		: nullptr;
#else
	FRayRopeDebugContext* DebugContextPtr = nullptr;
#endif

	if (RopeSegments.Num() == 0)
	{
		TickDebug(TEXT("TickEmpty"));
#if RAYROPE_WITH_DEBUG
		DebugContext.LogFrameSummary(TEXT("TickEmpty"));
#endif
		return;
	}

	if (!bIsRopeSolving)
	{
		SyncRopeNodes();
		RefreshRopeLength();
		if (ApplyRopeRuntimeEffects(DebugContextPtr))
		{
			SyncRopeNodes();
			RefreshRopeLength();
			OnRopeSegmentsUpdated.Broadcast();
		}

		TickDebug(TEXT("TickIdle"));
#if RAYROPE_WITH_DEBUG
		DebugContext.LogFrameSummary(TEXT("TickIdle"));
#endif
		return;
	}

	const FRayRopeSolveResult SolveResult = SolveRope(DebugContextPtr);

	const bool bAppliedRuntimeEffects = ApplyRopeRuntimeEffects(DebugContextPtr);
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
#if RAYROPE_WITH_DEBUG
	DebugContext.LogFrameSummary(TEXT("TickSolve"));
#endif
}
