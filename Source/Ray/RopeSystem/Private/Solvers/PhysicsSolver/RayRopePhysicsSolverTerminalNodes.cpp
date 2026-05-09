#include "RayRopePhysicsSolverInternal.h"

namespace RayRopePhysicsSolverPrivate
{
namespace
{
bool IsOwnerAnchorNode(AActor* OwnerActor, const FRayRopeNode& Node)
{
	return Node.NodeType == ERayRopeNodeType::Anchor &&
		Node.AttachedActor == OwnerActor;
}

bool TryGetOwnerEndpointInSegment(
	AActor* OwnerActor,
	const FRayRopeSegment& Segment,
	bool bCheckStart,
	FOwnerTerminalNodes& OutTerminalNodes)
{
	OutTerminalNodes = FOwnerTerminalNodes();

	if (Segment.Nodes.Num() < 2)
	{
		return false;
	}

	const int32 OwnerNodeIndex = bCheckStart ? 0 : Segment.Nodes.Num() - 1;
	const int32 AdjacentNodeIndex = bCheckStart ? 1 : OwnerNodeIndex - 1;
	const FRayRopeNode& OwnerNode = Segment.Nodes[OwnerNodeIndex];
	if (!IsOwnerAnchorNode(OwnerActor, OwnerNode))
	{
		return false;
	}

	OutTerminalNodes.OwnerNode = &OwnerNode;
	OutTerminalNodes.AdjacentNode = &Segment.Nodes[AdjacentNodeIndex];
	return true;
}
}

bool TryGetOwnerTerminalNodes(
	AActor* OwnerActor,
	const TArray<FRayRopeSegment>& Segments,
	FOwnerTerminalNodes& OutTerminalNodes)
{
	OutTerminalNodes = FOwnerTerminalNodes();

	if (!IsValid(OwnerActor) || Segments.Num() == 0)
	{
		return false;
	}

	// The length constraint moves only the component owner, so the owner anchor must stay terminal.
	const FRayRopeSegment& FirstSegment = Segments[0];
	if (TryGetOwnerEndpointInSegment(OwnerActor, FirstSegment, true, OutTerminalNodes))
	{
		return true;
	}

	const FRayRopeSegment& LastSegment = Segments.Last();
	return TryGetOwnerEndpointInSegment(OwnerActor, LastSegment, false, OutTerminalNodes);
}
}

