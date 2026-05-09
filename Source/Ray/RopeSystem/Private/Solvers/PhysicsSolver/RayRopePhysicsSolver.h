#pragma once

#include "Types/RayRopeTypes.h"

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
};
