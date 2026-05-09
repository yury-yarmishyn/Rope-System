#pragma once

#include "Types/RayRopeTypes.h"

struct FRayRopeNodeFactory
{
	static FRayRopeNode CreateNodeAtLocation(
		const FRayRopeNode& SourceNode,
		const FVector& WorldLocation);
};
