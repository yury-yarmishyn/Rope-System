#pragma once

#include "RayRopePhysicsSolver.h"

namespace RayRopePhysicsSolverPrivate
{
/** Owner anchor and its in-segment neighbor used by the length clamp. */
struct FOwnerTerminalNodes
{
	/** Anchor node attached to the component owner. */
	const FRayRopeNode* OwnerNode = nullptr;

	/** Neighbor node that defines the inward clamp direction. */
	const FRayRopeNode* AdjacentNode = nullptr;

	bool IsValid() const
	{
		return OwnerNode != nullptr && AdjacentNode != nullptr;
	}
};

/**
 * Finds the owner anchor only when it is at the start or end of the full rope.
 */
bool TryGetOwnerTerminalNodes(
	AActor* OwnerActor,
	const TArray<FRayRopeSegment>& Segments,
	FOwnerTerminalNodes& OutTerminalNodes);

/**
 * Removes velocity that would keep pulling the owner farther away from the adjacent rope node.
 */
void RemoveOwnerOutwardVelocity(AActor* OwnerActor, const FVector& OutwardDirection);

/**
 * Sweeps the owner actor inward enough to satisfy the configured max rope length when possible.
 */
bool ClampOwnerAnchorToMaxRopeLength(
	AActor* OwnerActor,
	const FRayRopePhysicsSettings& PhysicsSettings,
	const FRayRopeNode& OwnerNode,
	const FRayRopeNode& AdjacentNode);
}

