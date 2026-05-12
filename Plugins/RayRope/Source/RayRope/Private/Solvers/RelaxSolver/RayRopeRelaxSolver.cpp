#include "RayRopeRelaxSolver.h"

#include "Debug/RayRopeDebugContext.h"
#include "RayRopeRelaxSolverInternal.h"

using namespace RayRopeRelaxSolverPrivate;

FRayRopeSolveResult FRayRopeRelaxSolver::RelaxSegment(
	const FRayRopeTraceSettings& TraceSettings,
	const FRayRopeRelaxSettings& RelaxSettings,
	FRayRopeSegment& Segment)
{
	FRayRopeSolveResult Result;
	if (Segment.Nodes.Num() < 3)
	{
		return Result;
	}

	const FRayRopeTraceContext TraceContext = FRayRopeTrace::MakeTraceContext(
		TraceSettings,
		FCollisionQueryParams(SCENE_QUERY_STAT(RayRopeRelaxTrace)));
	const FRelaxSolveContext SolveContext(TraceContext, RelaxSettings);

	for (int32 NodeIndex = 1; NodeIndex < Segment.Nodes.Num() - 1;)
	{
		if (Segment.Nodes[NodeIndex].NodeType != ERayRopeNodeType::Redirect)
		{
			++NodeIndex;
			continue;
		}

		const ERelaxNodeResult RelaxResult = RelaxNode(SolveContext, Segment, NodeIndex);
		if (RelaxResult == ERelaxNodeResult::Removed)
		{
			if (TraceContext.DebugContext != nullptr)
			{
				TraceContext.DebugContext->RecordSolverEvent(
					TEXT("Relax"),
					FString::Printf(
						TEXT("Node[%d] removed"),
						NodeIndex));
			}
			Result.MarkTopologyChanged();
			Result.AddAffectedSpanRange(NodeIndex - 1, NodeIndex);
			continue;
		}

		if (RelaxResult == ERelaxNodeResult::Collapsed)
		{
			if (TraceContext.DebugContext != nullptr)
			{
				TraceContext.DebugContext->RecordSolverEvent(
					TEXT("Relax"),
					FString::Printf(
						TEXT("Node[%d] collapsed"),
						NodeIndex));
			}
			Result.MarkNodeLocationsChanged();
			Result.AddAffectedSpanRange(NodeIndex - 1, NodeIndex);
		}

		++NodeIndex;
	}

	return Result;
}
