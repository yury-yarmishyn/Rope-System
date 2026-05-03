#include "RayRopeComponent.h"

#include "Helpers/RayRopeDebug.h"
#include "Helpers/RayRopeNodeSynchronizer.h"
#include "Helpers/RayRopeSegmentTopology.h"
#include "Solvers/RayRopeMoveSolver.h"
#include "Solvers/RayRopePhysicsSolver.h"
#include "Solvers/RayRopeRelaxSolver.h"
#include "Solvers/RayRopeWrapSolver.h"

namespace
{
bool TryBuild(
	const FRayRopeTraceSettings& TraceSettings,
	const TArray<AActor*>& AnchorActors,
	TArray<FRayRopeSegment>& OutSegments)
{
	OutSegments.Reset();

	if (!IsValid(TraceSettings.World) || AnchorActors.Num() < 2)
	{
		return false;
	}

	OutSegments.Reserve(AnchorActors.Num() - 1);
	const FRayRopeTraceContext TraceContext = FRayRopeTrace::MakeTraceContext(
		TraceSettings,
		FCollisionQueryParams(SCENE_QUERY_STAT(RayRopeStartTrace)));

	for (int32 AnchorIndex = 0; AnchorIndex < AnchorActors.Num() - 1; ++AnchorIndex)
	{
		AActor* StartAnchorActor = AnchorActors[AnchorIndex];
		AActor* EndAnchorActor = AnchorActors[AnchorIndex + 1];
		if (!IsValid(StartAnchorActor) ||
			!IsValid(EndAnchorActor) ||
			StartAnchorActor == EndAnchorActor)
		{
			OutSegments.Reset();
			return false;
		}

		FRayRopeSegment BaseSegment;
		BaseSegment.Nodes.Reserve(2);
		BaseSegment.Nodes.Add(FRayRopeNodeSynchronizer::CreateAnchorNode(StartAnchorActor));
		BaseSegment.Nodes.Add(FRayRopeNodeSynchronizer::CreateAnchorNode(EndAnchorActor));

		FRayRopeSpan BaseSpan;
		if (!FRayRopeSegmentTopology::TryGetSegmentSpan(BaseSegment, 0, BaseSpan) ||
			BaseSpan.IsDegenerate(KINDA_SMALL_NUMBER) ||
			FRayRopeTrace::HasBlockingSpanHit(TraceContext, BaseSpan))
		{
			OutSegments.Reset();
			return false;
		}

		OutSegments.Add(MoveTemp(BaseSegment));
	}

	return OutSegments.Num() > 0;
}

FRayRopeTraceSettings MakeTraceSettings(const URayRopeComponent& Component)
{
	FRayRopeTraceSettings TraceSettings;
	TraceSettings.World = Component.GetWorld();
	TraceSettings.OwnerActor = Component.GetOwner();
	TraceSettings.TraceChannel = Component.TraceChannel;
	TraceSettings.bTraceComplex = Component.bTraceComplex;
	return TraceSettings;
}

FRayRopeNodeBuildSettings MakeNodeBuildSettings(const URayRopeComponent& Component)
{
	FRayRopeNodeBuildSettings Settings;
	Settings.bAllowWrapOnMovableObjects = Component.bAllowWrapOnMovableObjects;
	Settings.MaxWrapBinarySearchIterations = Component.MaxWrapBinarySearchIterations;
	Settings.WrapSolverTolerance = Component.WrapSolverTolerance;
	Settings.GeometryCollinearityTolerance = Component.GeometryCollinearityTolerance;
	Settings.WrapSurfaceOffset = Component.WrapSurfaceOffset;
	return Settings;
}

FRayRopeWrapSettings MakeWrapSettings(const URayRopeComponent& Component)
{
	FRayRopeWrapSettings WrapSettings;
	static_cast<FRayRopeNodeBuildSettings&>(WrapSettings) = MakeNodeBuildSettings(Component);
	return WrapSettings;
}

FRayRopeMoveSettings MakeMoveSettings(const URayRopeComponent& Component)
{
	FRayRopeMoveSettings MoveSettings;
	MoveSettings.NodeBuildSettings = MakeNodeBuildSettings(Component);
	MoveSettings.MoveSolverTolerance = Component.MoveSolverTolerance;
	MoveSettings.PlaneParallelTolerance = Component.MovePlaneParallelTolerance;
	MoveSettings.EffectivePointSearchTolerance = Component.MoveEffectivePointSearchTolerance;
	MoveSettings.SurfaceOffset = Component.WrapSurfaceOffset;
	MoveSettings.MaxMoveIterations = Component.MaxMoveIterations;
	MoveSettings.MaxEffectivePointSearchIterations = Component.MaxEffectivePointSearchIterations;
	return MoveSettings;
}

FRayRopeRelaxSettings MakeRelaxSettings(const URayRopeComponent& Component)
{
	FRayRopeRelaxSettings RelaxSettings;
	RelaxSettings.RelaxSolverTolerance = Component.RelaxSolverTolerance;
	RelaxSettings.MaxRelaxCollapseIterations = Component.MaxRelaxCollapseIterations;
	return RelaxSettings;
}

FRayRopePhysicsSettings MakePhysicsSettings(const URayRopeComponent& Component)
{
	FRayRopePhysicsSettings PhysicsSettings;
	PhysicsSettings.CurrentRopeLength = Component.CurrentRopeLength;
	PhysicsSettings.MaxAllowedRopeLength = Component.MaxAllowedRopeLength;
	return PhysicsSettings;
}

void SolveSegmentWithSettings(
	FRayRopeSegment& Segment,
	int32 SegmentIndex,
	const FRayRopeTraceSettings& TraceSettings,
	const FRayRopeWrapSettings& WrapSettings,
	const FRayRopeMoveSettings& MoveSettings,
	const FRayRopeRelaxSettings& RelaxSettings,
	TArray<FRayRopeNode>& ReferenceNodes,
	bool bLogNodeCountChanges)
{
	const int32 InitialNodeCount = Segment.Nodes.Num();
	ReferenceNodes.Reset(InitialNodeCount);
	ReferenceNodes.Append(Segment.Nodes);

	FRayRopeNodeSynchronizer::SyncSegmentNodes(Segment);
	FRayRopeWrapSolver::WrapSegment(TraceSettings, WrapSettings, Segment, ReferenceNodes);

	ReferenceNodes.Reset(Segment.Nodes.Num());
	ReferenceNodes.Append(Segment.Nodes);
	FRayRopeMoveSolver::MoveSegment(TraceSettings, MoveSettings, Segment);
	if (ReferenceNodes.Num() != Segment.Nodes.Num())
	{
		ReferenceNodes.Reset(Segment.Nodes.Num());
		ReferenceNodes.Append(Segment.Nodes);
	}
	FRayRopeWrapSolver::WrapSegment(TraceSettings, WrapSettings, Segment, ReferenceNodes);

	FRayRopeRelaxSolver::RelaxSegment(TraceSettings, RelaxSettings, Segment);

	if (bLogNodeCountChanges && Segment.Nodes.Num() != InitialNodeCount)
	{
		UE_LOG(
			LogRayRope,
			Log,
			TEXT("[Debug] SolveSegment[%d] node count changed: %d -> %d"),
			SegmentIndex,
			InitialNodeCount,
			Segment.Nodes.Num());
	}
}

}

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

	SolveRope();

	const bool bAppliedRuntimeEffects = ApplyRopeRuntimeEffects();
	if (bAppliedRuntimeEffects)
	{
		SyncRopeNodes();
		RefreshRopeLength();
	}

	OnRopeSegmentsUpdated.Broadcast();
	TickDebug(TEXT("TickSolve"));
}

bool URayRopeComponent::TryStartRopeSolve(const TArray<AActor*>& AnchorActors)
{
	TArray<FRayRopeSegment> BaseSegments;
	if (!TryBuild(
		MakeTraceSettings(*this),
		AnchorActors,
		BaseSegments))
	{
		if (IsDebugLogEnabled())
		{
			UE_LOG(
				LogRayRope,
				Log,
				TEXT("[Debug] TryStartRopeSolve failed. Owner=%s AnchorActors=%d"),
				*GetNameSafe(GetOwner()),
				AnchorActors.Num());
		}

		LogDebugRopeState(TEXT("TryStartRopeSolveFailed"), true);
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
	RopeDebugSettings.bDebugEnabled = bEnabled;
	NextDebugLogTimeSeconds = 0.f;

	TickDebug(bEnabled ? TEXT("SetRopeDebugEnabled") : TEXT("SetRopeDebugDisabled"));
}

bool URayRopeComponent::IsRopeDebugEnabled() const
{
	return RopeDebugSettings.bDebugEnabled;
}

void URayRopeComponent::SyncRopeNodes()
{
	for (FRayRopeSegment& Segment : RopeSegments)
	{
		FRayRopeNodeSynchronizer::SyncSegmentNodes(Segment);
	}
}

void URayRopeComponent::RefreshRopeLength()
{
	CurrentRopeLength = FRayRopeSegmentTopology::CalculateRopeLength(RopeSegments);
}

bool URayRopeComponent::ApplyRopeRuntimeEffects()
{
	return FRayRopePhysicsSolver::Solve(
		GetOwner(),
		RopeSegments,
		MakePhysicsSettings(*this));
}

void URayRopeComponent::SolveRope()
{
	if (!bIsRopeSolving || RopeSegments.Num() == 0)
	{
		return;
	}

	const FRayRopeTraceSettings TraceSettings = MakeTraceSettings(*this);
	const FRayRopeWrapSettings WrapSettings = MakeWrapSettings(*this);
	const FRayRopeMoveSettings MoveSettings = MakeMoveSettings(*this);
	const FRayRopeRelaxSettings RelaxSettings = MakeRelaxSettings(*this);
	const bool bLogNodeCountChanges = IsDebugLogEnabled();

	for (int32 SegmentIndex = 0; SegmentIndex < RopeSegments.Num(); ++SegmentIndex)
	{
		FRayRopeSegment& Segment = RopeSegments[SegmentIndex];
		if (Segment.Nodes.Num() < 2)
		{
			continue;
		}

		SolveSegmentWithSettings(
			Segment,
			SegmentIndex,
			TraceSettings,
			WrapSettings,
			MoveSettings,
			RelaxSettings,
			ReferenceNodesScratch,
			bLogNodeCountChanges);
	}

	ReferenceNodesScratch.Reset();
	FinalizeSolve();
}

void URayRopeComponent::SolveSegment(FRayRopeSegment& Segment, int32 SegmentIndex) const
{
	TArray<FRayRopeNode> ReferenceNodes;
	SolveSegmentWithSettings(
		Segment,
		SegmentIndex,
		MakeTraceSettings(*this),
		MakeWrapSettings(*this),
		MakeMoveSettings(*this),
		MakeRelaxSettings(*this),
		ReferenceNodes,
		IsDebugLogEnabled());
}

void URayRopeComponent::FinalizeSolve()
{
	FRayRopeSegmentTopology::SplitSegmentsOnAnchors(RopeSegments);
	RefreshRopeLength();
}

bool URayRopeComponent::IsDebugDrawEnabled() const
{
	return RopeDebugSettings.bDebugEnabled && RopeDebugSettings.bDrawDebugRope;
}

bool URayRopeComponent::IsDebugLogEnabled() const
{
	return RopeDebugSettings.bDebugEnabled && RopeDebugSettings.bLogDebugState;
}

void URayRopeComponent::TickDebug(const TCHAR* Context)
{
	DrawDebugRope();
	LogDebugRopeState(Context);
}

void URayRopeComponent::DrawDebugRope() const
{
	if (!IsDebugDrawEnabled())
	{
		return;
	}

	FRayRopeDebug::DrawRope(
		GetWorld(),
		GetOwner(),
		RopeSegments,
		RopeDebugSettings,
		CurrentRopeLength,
		MaxAllowedRopeLength,
		bIsRopeSolving);
}

void URayRopeComponent::LogDebugRopeState(const TCHAR* Context, bool bForce)
{
	if (!IsDebugLogEnabled())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float CurrentTime = World != nullptr ? World->GetTimeSeconds() : 0.f;
	const float LogInterval = FMath::Max(0.f, RopeDebugSettings.DebugLogIntervalSeconds);
	if (!bForce && LogInterval > 0.f && CurrentTime < NextDebugLogTimeSeconds)
	{
		return;
	}

	NextDebugLogTimeSeconds = CurrentTime + LogInterval;

	FRayRopeDebug::LogRopeState(
		Context,
		GetOwner(),
		RopeSegments,
		CurrentRopeLength,
		MaxAllowedRopeLength,
		bIsRopeSolving);
}
