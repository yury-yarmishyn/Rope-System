#include "Solvers/SolvePipeline/RayRopeSolvePipeline.h"

#include "Nodes/RayRopeNodeSynchronizer.h"
#include "Solvers/MoveSolver/RayRopeMoveSolver.h"
#include "Solvers/RelaxSolver/RayRopeRelaxSolver.h"
#include "Solvers/WrapSolver/RayRopeWrapSolver.h"

namespace
{
void CacheReferenceNodes(
	const FRayRopeSegment& Segment,
	TArray<FRayRopeNode>& ReferenceNodes)
{
	ReferenceNodes.Reset(Segment.Nodes.Num());
	ReferenceNodes.Append(Segment.Nodes);
}
}

void FRayRopeSolvePipeline::SolveSegment(
	const FRayRopeComponentSolveSettings& SolveSettings,
	FRayRopeSegment& Segment,
	int32 SegmentIndex,
	TArray<FRayRopeNode>& ReferenceNodes,
	bool bLogNodeCountChanges)
{
	const int32 InitialNodeCount = Segment.Nodes.Num();
	CacheReferenceNodes(Segment, ReferenceNodes);

	FRayRopeNodeSynchronizer::SyncSegmentNodes(Segment);
	// Wrap before movement so newly blocked spans get redirects before any redirect tries to slide.
	FRayRopeWrapSolver::WrapSegment(
		SolveSettings.TraceSettings,
		SolveSettings.NodeBuildSettings,
		Segment,
		ReferenceNodes);

	CacheReferenceNodes(Segment, ReferenceNodes);
	FRayRopeMoveSolver::MoveSegment(
		SolveSettings.TraceSettings,
		SolveSettings.MoveSettings,
		Segment);
	if (ReferenceNodes.Num() != Segment.Nodes.Num())
	{
		// Move can add redirects around its effective point; refresh the snapshot before wrapping again.
		CacheReferenceNodes(Segment, ReferenceNodes);
	}
	FRayRopeWrapSolver::WrapSegment(
		SolveSettings.TraceSettings,
		SolveSettings.NodeBuildSettings,
		Segment,
		ReferenceNodes);

	FRayRopeRelaxSolver::RelaxSegment(
		SolveSettings.TraceSettings,
		SolveSettings.RelaxSettings,
		Segment);

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

