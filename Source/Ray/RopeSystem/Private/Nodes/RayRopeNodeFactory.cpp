#include "RayRopeNodeFactory.h"

FRayRopeNode FRayRopeNodeFactory::CreateNodeAtLocation(
	const FRayRopeNode& SourceNode,
	const FVector& WorldLocation)
{
	FRayRopeNode Node = SourceNode;
	Node.WorldLocation = WorldLocation;
	return Node;
}
