#include "Solvers/SolvePipeline/RayRopeSolvePipeline.h"

#include "Nodes/RayRopeNodeSynchronizer.h"
#include "Solvers/MoveSolver/RayRopeMoveSolver.h"
#include "Solvers/RelaxSolver/RayRopeRelaxSolver.h"
#include "Solvers/WrapSolver/RayRopeWrapSolver.h"
#include "Topology/RayRopeSegmentTopology.h"

namespace
{
void CacheReferenceNodes(
	const FRayRopeSegment& Segment,
	TArray<FRayRopeNode>& ReferenceNodes)
{
	ReferenceNodes.Reset(Segment.Nodes.Num());
	ReferenceNodes.Append(Segment.Nodes);
}

FRayRopeSolveResult DetectNodeLocationChanges(
	const FRayRopeSegment& Segment,
	TConstArrayView<FRayRopeNode> ReferenceNodes)
{
	FRayRopeSolveResult Result;
	if (Segment.Nodes.Num() != ReferenceNodes.Num())
	{
		Result.MarkTopologyChanged();
		Result.AddAffectedSpanRange(0, Segment.Nodes.Num() - 2);
		return Result;
	}

	for (int32 NodeIndex = 0; NodeIndex < Segment.Nodes.Num(); ++NodeIndex)
	{
		if (!Segment.Nodes[NodeIndex].WorldLocation.Equals(
			ReferenceNodes[NodeIndex].WorldLocation,
			KINDA_SMALL_NUMBER))
		{
			Result.MarkNodeLocationsChanged();
			Result.AddAffectedSpanRange(NodeIndex - 1, NodeIndex);
		}
	}

	return Result;
}

}

FRayRopeSolveResult FRayRopeSolvePipeline::SolveSegment(
	const FRayRopeComponentSolveSettings& SolveSettings,
	FRayRopeSegment& Segment,
	int32 SegmentIndex,
	TArray<FRayRopeNode>& ReferenceNodes,
	bool bLogNodeCountChanges)
{
	FRayRopeSolveResult Result;
	const int32 InitialNodeCount = Segment.Nodes.Num();
	CacheReferenceNodes(Segment, ReferenceNodes);

	FRayRopeNodeSynchronizer::SyncSegmentNodes(Segment);
	Result.Merge(DetectNodeLocationChanges(Segment, ReferenceNodes));
	// Wrap before movement so newly blocked spans get redirects before any redirect tries to slide.
	Result.Merge(FRayRopeWrapSolver::WrapSegment(
		SolveSettings.TraceSettings,
		SolveSettings.NodeBuildSettings,
		Segment,
		ReferenceNodes));

	if (FRayRopeSegmentTopology::HasRedirectNodes(Segment))
	{
		CacheReferenceNodes(Segment, ReferenceNodes);
		const FRayRopeSolveResult MoveResult = FRayRopeMoveSolver::MoveSegment(
			SolveSettings.TraceSettings,
			SolveSettings.MoveSettings,
			SolveSettings.NodeBuildSettings,
			Segment);
		Result.Merge(MoveResult);
		if (MoveResult.DidChangeRope())
		{
			if (ReferenceNodes.Num() != Segment.Nodes.Num())
			{
				// Move can add redirects around its effective point; refresh the snapshot before wrapping again.
				CacheReferenceNodes(Segment, ReferenceNodes);
			}

			Result.Merge(FRayRopeWrapSolver::WrapSegmentRanges(
				SolveSettings.TraceSettings,
				SolveSettings.NodeBuildSettings,
				Segment,
				ReferenceNodes,
				MoveResult.AffectedSpanRanges));
		}

		Result.Merge(FRayRopeRelaxSolver::RelaxSegment(
			SolveSettings.TraceSettings,
			SolveSettings.RelaxSettings,
			Segment));
	}

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

	return Result;
}

