#include "RayRopeRelaxSolver.h"

#include "RayRopeRelaxSolverInternal.h"

using namespace RayRopeRelaxSolverPrivate;

void FRayRopeRelaxSolver::RelaxSegment(
	const FRayRopeTraceSettings& TraceSettings,
	const FRayRopeRelaxSettings& RelaxSettings,
	FRayRopeSegment& Segment)
{
	if (Segment.Nodes.Num() < 3)
	{
		return;
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

		if (RelaxNode(SolveContext, Segment, NodeIndex) == ERelaxNodeResult::Removed)
		{
			continue;
		}

		++NodeIndex;
	}
}

