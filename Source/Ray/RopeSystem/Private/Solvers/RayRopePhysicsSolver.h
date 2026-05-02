#pragma once

#include "RayRopeInternalTypes.h"

class AActor;

struct FRayRopePhysicsSettings
{
	float CurrentRopeLength = 0.f;
	float MaxAllowedRopeLength = 0.f;
};

struct FRayRopePhysicsSolver
{
	static bool Solve(
		AActor* OwnerActor,
		const TArray<FRayRopeSegment>& Segments,
		const FRayRopePhysicsSettings& PhysicsSettings);

private:
	static bool TryGetOwnerTerminalNodes(
		AActor* OwnerActor,
		const TArray<FRayRopeSegment>& Segments,
		const FRayRopeNode*& OutOwnerNode,
		const FRayRopeNode*& OutAdjacentNode);

	static bool ClampOwnerAnchorToMaxRopeLength(
		AActor* OwnerActor,
		const FRayRopePhysicsSettings& PhysicsSettings,
		const FRayRopeNode& OwnerNode,
		const FRayRopeNode& AdjacentNode);

	static void RemoveOwnerOutwardVelocity(
		AActor* OwnerActor,
		const FVector& OutwardDirection);
};
