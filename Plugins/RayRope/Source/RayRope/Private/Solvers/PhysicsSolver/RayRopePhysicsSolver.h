#pragma once

#include "Types/RayRopeTypes.h"

class AActor;
struct FRayRopeDebugContext;

/**
 * Runtime length constraint state for the component owner.
 */
struct FRayRopePhysicsSettings
{
	/** Current solved rope length across all segments. */
	float CurrentRopeLength = 0.f;

	/** Maximum allowed rope length; non-positive values disable clamping. */
	float MaxAllowedRopeLength = 0.f;
};

/**
 * Applies owner-side runtime effects for rope constraints.
 */
struct FRayRopePhysicsSolver
{
	/**
	 * Clamps the owner toward its adjacent terminal node when the rope exceeds MaxAllowedRopeLength.
	 *
	 * Returns true only when the owner actor moved. May remove outward owner velocity before sweeping.
	 */
	static bool Solve(
		AActor* OwnerActor,
		const TArray<FRayRopeSegment>& Segments,
		const FRayRopePhysicsSettings& PhysicsSettings,
		FRayRopeDebugContext* DebugContext = nullptr);
};
