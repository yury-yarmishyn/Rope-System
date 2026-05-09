#include "Component/RayRopeComponent.h"

#include "Nodes/RayRopeNodeSynchronizer.h"
#include "Component/RayRopeComponentSettings.h"
#include "Solvers/PhysicsSolver/RayRopePhysicsSolver.h"
#include "Solvers/SolvePipeline/RayRopeSolvePipeline.h"
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

void URayRopeComponent::SolveRope()
{
	if (!bIsRopeSolving || RopeSegments.Num() == 0)
	{
		return;
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

		FRayRopeSolvePipeline::SolveSegment(
			SolveSettings,
			Segment,
			SegmentIndex,
			ReferenceNodesScratch,
			bLogNodeCountChanges);
	}

	ReferenceNodesScratch.Reset();
	FinalizeSolve();
}

void URayRopeComponent::SolveSegment(FRayRopeSegment& Segment, int32 SegmentIndex) const
{
	TArray<FRayRopeNode> ReferenceNodes;
	FRayRopeSolvePipeline::SolveSegment(
		FRayRopeComponentSettings::MakeSolveSettings(*this),
		Segment,
		SegmentIndex,
		ReferenceNodes,
		IsDebugLogEnabled());
}

void URayRopeComponent::FinalizeSolve()
{
	FRayRopeSegmentTopology::SplitSegmentsOnAnchors(RopeSegments);
	RefreshRopeLength();
}
