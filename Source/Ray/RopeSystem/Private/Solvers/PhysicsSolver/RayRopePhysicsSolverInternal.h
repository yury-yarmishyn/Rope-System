#pragma once

#include "RayRopePhysicsSolver.h"

namespace RayRopePhysicsSolverPrivate
{
struct FOwnerTerminalNodes
{
	const FRayRopeNode* OwnerNode = nullptr;
	const FRayRopeNode* AdjacentNode = nullptr;

	bool IsValid() const
	{
		return OwnerNode != nullptr && AdjacentNode != nullptr;
	}
};

bool TryGetOwnerTerminalNodes(
	AActor* OwnerActor,
	const TArray<FRayRopeSegment>& Segments,
	FOwnerTerminalNodes& OutTerminalNodes);

void RemoveOwnerOutwardVelocity(AActor* OwnerActor, const FVector& OutwardDirection);

bool ClampOwnerAnchorToMaxRopeLength(
	AActor* OwnerActor,
	const FRayRopePhysicsSettings& PhysicsSettings,
	const FRayRopeNode& OwnerNode,
	const FRayRopeNode& AdjacentNode);
}

