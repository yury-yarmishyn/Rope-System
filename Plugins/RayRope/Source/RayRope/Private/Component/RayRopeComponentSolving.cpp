#include "Component/RayRopeComponent.h"

#include "Nodes/RayRopeNodeSynchronizer.h"
#include "Component/RayRopeComponentSettings.h"
#include "Solvers/PhysicsSolver/RayRopePhysicsSolver.h"
#include "Solvers/SolvePipeline/RayRopeSolvePipeline.h"
#include "Solvers/SolvePipeline/RayRopeSolveTypes.h"
#include "Topology/RayRopeSegmentTopology.h"

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
		FRayRopeComponentSettings::MakePhysicsSettings(*this));
}

FRayRopeSolveResult URayRopeComponent::SolveRope()
{
	FRayRopeSolveResult Result;
	if (!bIsRopeSolving || RopeSegments.Num() == 0)
	{
		return Result;
	}

	const FRayRopeComponentSolveSettings SolveSettings =
		FRayRopeComponentSettings::MakeSolveSettings(*this);
	const bool bLogNodeCountChanges = IsDebugLogEnabled();

	for (int32 SegmentIndex = 0; SegmentIndex < RopeSegments.Num(); ++SegmentIndex)
	{
		FRayRopeSegment& Segment = RopeSegments[SegmentIndex];
		if (Segment.Nodes.Num() < 2)
		{
			continue;
		}

		Result.Merge(FRayRopeSolvePipeline::SolveSegment(
			SolveSettings,
			Segment,
			SegmentIndex,
			ReferenceNodesScratch,
			bLogNodeCountChanges));
	}

	ReferenceNodesScratch.Reset();
	if (Result.bTopologyChanged)
	{
		FinalizeSolve();
	}

	if (Result.DidChangeRope())
	{
		RefreshRopeLength();
	}

	return Result;
}

FRayRopeSolveResult URayRopeComponent::SolveSegment(FRayRopeSegment& Segment, int32 SegmentIndex) const
{
	TArray<FRayRopeNode> ReferenceNodes;
	return FRayRopeSolvePipeline::SolveSegment(
		FRayRopeComponentSettings::MakeSolveSettings(*this),
		Segment,
		SegmentIndex,
		ReferenceNodes,
		IsDebugLogEnabled());
}

void URayRopeComponent::FinalizeSolve()
{
	FRayRopeSegmentTopology::SplitSegmentsOnAnchors(RopeSegments);
}
