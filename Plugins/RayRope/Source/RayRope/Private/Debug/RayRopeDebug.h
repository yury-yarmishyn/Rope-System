#pragma once

#include "Types/RayRopeTypes.h"

class AActor;
class UWorld;

/**
 * Stateless debug rendering and logging utilities for rope topology.
 */
struct FRayRopeDebug
{
	/**
	 * Draws the current rope state using transient debug primitives.
	 *
	 * All sizing, color, label, and attachment-link policy comes from DebugSettings.
	 */
	static void DrawRope(
		UWorld* World,
		const AActor* OwnerActor,
		const TArray<FRayRopeSegment>& Segments,
		const FRayRopeDebugSettings& DebugSettings,
		float CurrentRopeLength,
		float MaxAllowedRopeLength,
		bool bIsRopeSolving);

	/**
	 * Logs a full rope state snapshot, including per-segment length and per-node attachment data.
	 */
	static void LogRopeState(
		const TCHAR* Context,
		const AActor* OwnerActor,
		const TArray<FRayRopeSegment>& Segments,
		float CurrentRopeLength,
		float MaxAllowedRopeLength,
		bool bIsRopeSolving);
};
