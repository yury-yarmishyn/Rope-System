#include "Solvers/SolvePipeline/RayRopeSolvePipeline.h"

#include "Debug/RayRopeDebugContext.h"
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

FString DescribeSolveResult(const FRayRopeSolveResult& Result)
{
	return FString::Printf(
		TEXT("Topology=%s Locations=%s AffectedRanges=%d"),
		Result.bTopologyChanged ? TEXT("true") : TEXT("false"),
		Result.bNodeLocationsChanged ? TEXT("true") : TEXT("false"),
		Result.AffectedSpanRanges.Num());
}

void RecordPassResult(
	FRayRopeDebugContext* DebugContext,
	const TCHAR* PassName,
	int32 SegmentIndex,
	const FRayRopeSolveResult& Result,
	float LengthBefore,
	float LengthAfter,
	int32 NodeCountBefore,
	int32 NodeCountAfter)
{
	if (DebugContext == nullptr)
	{
		return;
	}

	DebugContext->RecordSolverEvent(
		PassName,
		FString::Printf(
			TEXT("Segment[%d] %s Length=%.2f->%.2f Nodes=%d->%d"),
			SegmentIndex,
			*DescribeSolveResult(Result),
			LengthBefore,
			LengthAfter,
			NodeCountBefore,
			NodeCountAfter));
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
	FRayRopeDebugContext* DebugContext = SolveSettings.TraceSettings.DebugContext;
	const int32 InitialNodeCount = Segment.Nodes.Num();
	const float InitialLength = FRayRopeSegmentTopology::CalculateSegmentLength(Segment);
	CacheReferenceNodes(Segment, ReferenceNodes);

	FRayRopeNodeSynchronizer::SyncSegmentNodes(Segment);
	const FRayRopeSolveResult SyncResult = DetectNodeLocationChanges(Segment, ReferenceNodes);
	Result.Merge(SyncResult);
	RecordPassResult(
		DebugContext,
		TEXT("Sync"),
		SegmentIndex,
		SyncResult,
		InitialLength,
		FRayRopeSegmentTopology::CalculateSegmentLength(Segment),
		InitialNodeCount,
		Segment.Nodes.Num());

	// Wrap before movement so newly blocked spans get redirects before any redirect tries to slide.
	const float PreWrapLength = FRayRopeSegmentTopology::CalculateSegmentLength(Segment);
	const int32 PreWrapNodeCount = Segment.Nodes.Num();
	const FRayRopeSolveResult WrapResult = FRayRopeWrapSolver::WrapSegment(
		SolveSettings.TraceSettings,
		SolveSettings.NodeBuildSettings,
		Segment,
		ReferenceNodes);
	Result.Merge(WrapResult);
	RecordPassResult(
		DebugContext,
		TEXT("Wrap"),
		SegmentIndex,
		WrapResult,
		PreWrapLength,
		FRayRopeSegmentTopology::CalculateSegmentLength(Segment),
		PreWrapNodeCount,
		Segment.Nodes.Num());

	if (FRayRopeSegmentTopology::HasRedirectNodes(Segment))
	{
		CacheReferenceNodes(Segment, ReferenceNodes);
		const float PreMoveLength = FRayRopeSegmentTopology::CalculateSegmentLength(Segment);
		const int32 PreMoveNodeCount = Segment.Nodes.Num();
		const FRayRopeSolveResult MoveResult = FRayRopeMoveSolver::MoveSegment(
			SolveSettings.TraceSettings,
			SolveSettings.MoveSettings,
			SolveSettings.NodeBuildSettings,
			Segment);
		Result.Merge(MoveResult);
		RecordPassResult(
			DebugContext,
			TEXT("Move"),
			SegmentIndex,
			MoveResult,
			PreMoveLength,
			FRayRopeSegmentTopology::CalculateSegmentLength(Segment),
			PreMoveNodeCount,
			Segment.Nodes.Num());

		if (MoveResult.DidChangeRope())
		{
			if (ReferenceNodes.Num() != Segment.Nodes.Num())
			{
				// Move can add redirects around its effective point; refresh the snapshot before wrapping again.
				CacheReferenceNodes(Segment, ReferenceNodes);
			}

			const float PreRewrapLength = FRayRopeSegmentTopology::CalculateSegmentLength(Segment);
			const int32 PreRewrapNodeCount = Segment.Nodes.Num();
			const FRayRopeSolveResult RewrapResult = FRayRopeWrapSolver::WrapSegmentRanges(
				SolveSettings.TraceSettings,
				SolveSettings.NodeBuildSettings,
				Segment,
				ReferenceNodes,
				MoveResult.AffectedSpanRanges);
			Result.Merge(RewrapResult);
			RecordPassResult(
				DebugContext,
				TEXT("Rewrap"),
				SegmentIndex,
				RewrapResult,
				PreRewrapLength,
				FRayRopeSegmentTopology::CalculateSegmentLength(Segment),
				PreRewrapNodeCount,
				Segment.Nodes.Num());
		}

		const float PreRelaxLength = FRayRopeSegmentTopology::CalculateSegmentLength(Segment);
		const int32 PreRelaxNodeCount = Segment.Nodes.Num();
		const FRayRopeSolveResult RelaxResult = FRayRopeRelaxSolver::RelaxSegment(
			SolveSettings.TraceSettings,
			SolveSettings.RelaxSettings,
			Segment);
		Result.Merge(RelaxResult);
		RecordPassResult(
			DebugContext,
			TEXT("Relax"),
			SegmentIndex,
			RelaxResult,
			PreRelaxLength,
			FRayRopeSegmentTopology::CalculateSegmentLength(Segment),
			PreRelaxNodeCount,
			Segment.Nodes.Num());
	}
	else if (DebugContext != nullptr)
	{
		DebugContext->RecordSolverEvent(
			TEXT("MoveRelax"),
			FString::Printf(
				TEXT("Segment[%d] skipped: no redirect nodes"),
				SegmentIndex));
	}

	if (bLogNodeCountChanges && Segment.Nodes.Num() != InitialNodeCount)
	{
#if RAYROPE_WITH_DEBUG
		UE_LOG(
			LogRayRope,
			Log,
			TEXT("[Debug] SolveSegment[%d] node count changed: %d -> %d"),
			SegmentIndex,
			InitialNodeCount,
			Segment.Nodes.Num());
#endif
	}

	if (DebugContext != nullptr && Segment.Nodes.Num() != InitialNodeCount)
	{
		DebugContext->RecordSolverEvent(
			TEXT("Pipeline"),
			FString::Printf(
				TEXT("Segment[%d] node count changed: %d -> %d"),
				SegmentIndex,
				InitialNodeCount,
				Segment.Nodes.Num()));
	}

	return Result;
}

