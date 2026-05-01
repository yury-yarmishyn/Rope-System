#include "RayRopeComponent.h"

#include "Solvers/RayRopeMoveSolver.h"
#include "Solvers/RayRopeNodeResolver.h"
#include "Solvers/RayRopePhysicsSolver.h"
#include "Solvers/RayRopeTopology.h"
#include "Solvers/RayRopeWrapSolver.h"

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

	if (Segments.Num() == 0)
	{
		return;
	}

	if (!bShouldSolveRope)
	{
		SyncRopeNodes();
		RefreshRopeLength();
		if (ApplyRopeRuntimeEffects())
		{
			SyncRopeNodes();
			RefreshRopeLength();
			OnSegmentsSet.Broadcast();
		}

		return;
	}

	SolveRope();

	const bool bAppliedRuntimeEffects = ApplyRopeRuntimeEffects();
	if (bAppliedRuntimeEffects)
	{
		SyncRopeNodes();
		RefreshRopeLength();
	}

	OnSegmentsSet.Broadcast();
}

bool URayRopeComponent::TryStartRopeSolve(const TArray<AActor*>& AnchorActors)
{
	FRayRopeTraceSettings TraceSettings;
	TraceSettings.World = GetWorld();
	TraceSettings.OwnerActor = GetOwner();
	TraceSettings.TraceChannel = TraceChannel;
	TraceSettings.bTraceComplex = bTraceComplex;

	TArray<FRayRopeSegment> BaseSegments;
	if (!FRayRopeTopology::TryBuildBaseSegments(
		TraceSettings,
		AnchorActors,
		BaseSegments))
	{
		return false;
	}

	const bool bWasSolvingRope = bShouldSolveRope;
	bShouldSolveRope = true;
	SetSegments(MoveTemp(BaseSegments));

	if (!bWasSolvingRope)
	{
		OnRopeSolveStarted.Broadcast();
	}

	return true;
}

void URayRopeComponent::EndRopeSolve()
{
	if (!bShouldSolveRope)
	{
		return;
	}

	SyncRopeNodes();
	RefreshRopeLength();
	bShouldSolveRope = false;
	OnRopeSolveEnded.Broadcast();
}

bool URayRopeComponent::BreakRopeOnSegment(int32 SegmentIndex)
{
	if (!Segments.IsValidIndex(SegmentIndex))
	{
		return false;
	}

	Segments.RemoveAt(SegmentIndex, 1, EAllowShrinking::No);

	const bool bHasSegments = Segments.Num() > 0;
	const bool bWasSolvingRope = bShouldSolveRope;
	if (!bHasSegments)
	{
		bShouldSolveRope = false;
	}

	RefreshRopeLength();
	OnSegmentsSet.Broadcast();
	OnRopeSegmentBroken.Broadcast(SegmentIndex);

	if (!bHasSegments)
	{
		if (bWasSolvingRope)
		{
			OnRopeSolveEnded.Broadcast();
		}

		OnRopeBroken.Broadcast();
	}

	return true;
}

void URayRopeComponent::BreakRope()
{
	const bool bHadSegments = Segments.Num() > 0;
	const bool bWasSolvingRope = bShouldSolveRope;
	if (!bHadSegments && !bWasSolvingRope)
	{
		return;
	}

	Segments.Reset();
	bShouldSolveRope = false;
	RefreshRopeLength();

	if (bHadSegments)
	{
		OnSegmentsSet.Broadcast();
	}

	if (bWasSolvingRope)
	{
		OnRopeSolveEnded.Broadcast();
	}

	OnRopeBroken.Broadcast();
}

const TArray<FRayRopeSegment>& URayRopeComponent::GetSegments() const
{
	return Segments;
}

void URayRopeComponent::SetSegments(TArray<FRayRopeSegment> NewSegments)
{
	Segments = MoveTemp(NewSegments);
	SyncRopeNodes();
	RefreshRopeLength();
	OnSegmentsSet.Broadcast();
}

void URayRopeComponent::SyncRopeNodes()
{
	for (FRayRopeSegment& Segment : Segments)
	{
		FRayRopeNodeResolver::SyncSegmentNodes(Segment);
	}
}

void URayRopeComponent::RefreshRopeLength()
{
	RopeLength = FRayRopeTopology::CalculateRopeLength(Segments);
}

bool URayRopeComponent::ApplyRopeRuntimeEffects()
{
	FRayRopePhysicsSettings PhysicsSettings;
	PhysicsSettings.RopeLength = RopeLength;
	PhysicsSettings.MaxRopeLength = MaxRopeLength;

	return FRayRopePhysicsSolver::Solve(
		GetOwner(),
		Segments,
		PhysicsSettings);
}

void URayRopeComponent::SolveRope()
{
	if (!bShouldSolveRope || Segments.Num() == 0)
	{
		return;
	}

	for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		FRayRopeSegment& Segment = Segments[SegmentIndex];
		if (Segment.Nodes.Num() < 2)
		{
			continue;
		}

		SolveSegment(Segment);
	}

	FinalizeSolve();
}

void URayRopeComponent::SolveSegment(FRayRopeSegment& Segment) const
{
	FRayRopeTraceSettings TraceSettings;
	TraceSettings.World = GetWorld();
	TraceSettings.OwnerActor = GetOwner();
	TraceSettings.TraceChannel = TraceChannel;
	TraceSettings.bTraceComplex = bTraceComplex;

	FRayRopeWrapSettings WrapSettings;
	WrapSettings.bAllowWrapOnMovableObjects = bAllowWrapOnMovableObjects;
	WrapSettings.MaxBinarySearchIteration = MaxBinarySearchIteration;
	WrapSettings.WrapSolverEpsilon = WrapSolverEpsilon;
	WrapSettings.GeometryCollinearEpsilon = RelaxCollinearEpsilon;
	WrapSettings.WrapOffset = WrapOffset;

	FRayRopeMoveSettings MoveSettings;
	MoveSettings.MoveSolverEpsilon = MoveSolverEpsilon;
	MoveSettings.PlaneParallelEpsilon = MovePlaneParallelEpsilon;
	MoveSettings.EffectivePointSearchEpsilon = MoveEffectivePointSearchEpsilon;
	MoveSettings.MaxMoveIterations = MaxMoveIterations;
	MoveSettings.MaxEffectivePointSearchIterations = MaxMoveEffectivePointSearchIterations;

	FRayRopeRelaxSettings RelaxSettings;
	RelaxSettings.RelaxSolverEpsilon = RelaxSolverEpsilon;
	RelaxSettings.RelaxCollinearEpsilon = RelaxCollinearEpsilon;

	const FRayRopeSegment ReferenceSegment = Segment;
	FRayRopeNodeResolver::SyncSegmentNodes(Segment);
	FRayRopeMoveSolver::MoveSegment(TraceSettings, MoveSettings, Segment);
	FRayRopeWrapSolver::WrapSegment(TraceSettings, WrapSettings, Segment, ReferenceSegment);
	FRayRopeTopology::RelaxSegment(TraceSettings, RelaxSettings, Segment);
}

void URayRopeComponent::FinalizeSolve()
{
	FRayRopeTopology::SplitSegmentsOnAnchors(Segments);
	RefreshRopeLength();
}
