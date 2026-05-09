#pragma once

#include "Types/RayRopeTypes.h"

class AActor;
class UWorld;

struct FRayRopeDebug
{
	static void DrawRope(
		UWorld* World,
		const AActor* OwnerActor,
		const TArray<FRayRopeSegment>& Segments,
		const FRayRopeDebugSettings& DebugSettings,
		float CurrentRopeLength,
		float MaxAllowedRopeLength,
		bool bIsRopeSolving);

	static void LogRopeState(
		const TCHAR* Context,
		const AActor* OwnerActor,
		const TArray<FRayRopeSegment>& Segments,
		float CurrentRopeLength,
		float MaxAllowedRopeLength,
		bool bIsRopeSolving);
};
