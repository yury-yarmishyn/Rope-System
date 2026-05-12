#pragma once

#include "Types/RayRopeTypes.h"

/**
 * Helpers for preserving node attachment metadata while changing solver locations.
 */
struct FRayRopeNodeFactory
{
	/**
	 * Copies a node and replaces only its world location.
	 *
	 * Attachment data is intentionally preserved so validation samples behave like the source node.
	 */
	static FRayRopeNode CreateNodeAtLocation(
		const FRayRopeNode& SourceNode,
		const FVector& WorldLocation);
};
