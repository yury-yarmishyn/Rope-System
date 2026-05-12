#include "Component/RayRopeComponent.h"

#include "Component/RayRopeComponentSettings.h"
#include "Debug/RayRopeDebugContext.h"
#include "Topology/RayRopeInitialSegmentBuilder.h"

bool URayRopeComponent::TryStartRopeSolve(const TArray<AActor*>& AnchorActors)
{
	TArray<FRayRopeSegment> BaseSegments;
#if RAYROPE_WITH_DEBUG
	FRayRopeDebugContext DebugContext(
		GetWorld(),
		GetOwner(),
		RopeDebugSettings,
		TEXT("TryStartRopeSolve"));
	FRayRopeDebugContext* DebugContextPtr = FRayRopeDebugContext::ShouldCreateForSolvers(RopeDebugSettings)
		? &DebugContext
		: nullptr;
#else
	FRayRopeDebugContext* DebugContextPtr = nullptr;
#endif

	if (!FRayRopeInitialSegmentBuilder::TryBuildSegments(
		FRayRopeComponentSettings::MakeTraceSettings(*this, DebugContextPtr),
		AnchorActors,
		BaseSegments))
	{
#if RAYROPE_WITH_DEBUG
		if (IsDebugLogEnabled())
		{
			UE_LOG(
				LogRayRope,
				Log,
				TEXT("[Debug] TryStartRopeSolve failed. Owner=%s AnchorActors=%d"),
				*GetNameSafe(GetOwner()),
				AnchorActors.Num());
		}
#endif

		LogDebugRopeState(TEXT("TryStartRopeSolveFailed"), true);
#if RAYROPE_WITH_DEBUG
		DebugContext.LogFrameSummary(TEXT("TryStartRopeSolveFailed"));
#endif
		return false;
	}

	const bool bWasSolvingRope = bIsRopeSolving;
	bIsRopeSolving = true;
	SetSegments(MoveTemp(BaseSegments));

	if (!bWasSolvingRope)
	{
		OnRopeSolveStarted.Broadcast();
	}

	LogDebugRopeState(TEXT("TryStartRopeSolve"), true);
#if RAYROPE_WITH_DEBUG
	DebugContext.LogFrameSummary(TEXT("TryStartRopeSolve"));
#endif
	return true;
}

void URayRopeComponent::EndRopeSolve()
{
	if (!bIsRopeSolving)
	{
		return;
	}

	SyncRopeNodes();
	RefreshRopeLength();
	bIsRopeSolving = false;
	OnRopeSolveEnded.Broadcast();
	LogDebugRopeState(TEXT("EndRopeSolve"), true);
}

bool URayRopeComponent::BreakRopeOnSegment(int32 SegmentIndex)
{
	if (!RopeSegments.IsValidIndex(SegmentIndex))
	{
#if RAYROPE_WITH_DEBUG
		if (IsDebugLogEnabled())
		{
			UE_LOG(
				LogRayRope,
				Log,
				TEXT("[Debug] BreakRopeOnSegment ignored invalid segment. Owner=%s SegmentIndex=%d RopeSegments=%d"),
				*GetNameSafe(GetOwner()),
				SegmentIndex,
				RopeSegments.Num());
		}
#endif

		return false;
	}

	RopeSegments.RemoveAt(SegmentIndex, 1, EAllowShrinking::No);

	const bool bHasSegments = RopeSegments.Num() > 0;
	const bool bWasSolvingRope = bIsRopeSolving;
	if (!bHasSegments)
	{
		bIsRopeSolving = false;
	}

	RefreshRopeLength();
	OnRopeSegmentsUpdated.Broadcast();
	OnRopeSegmentBroken.Broadcast(SegmentIndex);

	if (!bHasSegments)
	{
		if (bWasSolvingRope)
		{
			OnRopeSolveEnded.Broadcast();
		}

		OnRopeBroken.Broadcast();
	}

	LogDebugRopeState(TEXT("BreakRopeOnSegment"), true);
	return true;
}

void URayRopeComponent::BreakRope()
{
	const bool bHadSegments = RopeSegments.Num() > 0;
	const bool bWasSolvingRope = bIsRopeSolving;
	if (!bHadSegments && !bWasSolvingRope)
	{
		return;
	}

	RopeSegments.Reset();
	bIsRopeSolving = false;
	RefreshRopeLength();

	if (bHadSegments)
	{
		OnRopeSegmentsUpdated.Broadcast();
	}

	if (bWasSolvingRope)
	{
		OnRopeSolveEnded.Broadcast();
	}

	OnRopeBroken.Broadcast();
	LogDebugRopeState(TEXT("BreakRope"), true);
}

const TArray<FRayRopeSegment>& URayRopeComponent::GetSegments() const
{
	return RopeSegments;
}

void URayRopeComponent::SetSegments(TArray<FRayRopeSegment> NewSegments)
{
	RopeSegments = MoveTemp(NewSegments);
	SyncRopeNodes();
	RefreshRopeLength();
	OnRopeSegmentsUpdated.Broadcast();
	LogDebugRopeState(TEXT("SetSegments"), true);
}

void URayRopeComponent::SetRopeDebugEnabled(bool bEnabled)
{
#if RAYROPE_WITH_DEBUG
	RopeDebugSettings.bDebugEnabled = bEnabled;
	NextDebugLogTimeSeconds = 0.f;

	TickDebug(bEnabled ? TEXT("SetRopeDebugEnabled") : TEXT("SetRopeDebugDisabled"));
#else
	RopeDebugSettings.bDebugEnabled = false;
#endif
}

bool URayRopeComponent::IsRopeDebugEnabled() const
{
#if RAYROPE_WITH_DEBUG
	return RopeDebugSettings.bDebugEnabled;
#else
	return false;
#endif
}
