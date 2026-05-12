#pragma once

#include "CoreMinimal.h"
#include "Debug/RayRopeDebugConfig.h"
#include "Types/RayRopeTypes.h"

class AActor;
class UWorld;
struct FHitResult;

/**
 * Tick-local sink for runtime rope diagnostics.
 *
 * Solvers use this instead of calling draw/log helpers directly so diagnostics stay optional and
 * scoped to the component that enabled them.
 */
struct FRayRopeDebugContext
{
	FRayRopeDebugContext(
		UWorld* InWorld,
		const AActor* InOwnerActor,
		const FRayRopeDebugSettings& InSettings,
		const TCHAR* InFrameContext);

	static bool ShouldCreateForSolvers(const FRayRopeDebugSettings& Settings);

	bool IsEnabled() const;
	bool ShouldDraw(ERayRopeDebugDrawFlags Flag) const;
	bool ShouldLog(ERayRopeDebugLogFlags Flag) const;

	void RecordTrace(
		const TCHAR* Category,
		const FVector& StartLocation,
		const FVector& EndLocation,
		bool bBlockingHit,
		const FHitResult* SurfaceHit,
		const TCHAR* Reason = nullptr);

	void RecordSolverEvent(
		const TCHAR* Category,
		const FString& Message);

	void DrawSolverLine(
		ERayRopeDebugDrawFlags Flag,
		const FVector& StartLocation,
		const FVector& EndLocation,
		const FLinearColor& Color,
		const TCHAR* Label = nullptr);

	void DrawSolverPoint(
		ERayRopeDebugDrawFlags Flag,
		const FVector& Location,
		const FLinearColor& Color,
		const TCHAR* Label = nullptr);

	void DrawSolverVector(
		ERayRopeDebugDrawFlags Flag,
		const FVector& StartLocation,
		const FVector& Direction,
		float Length,
		const FLinearColor& Color,
		const TCHAR* Label = nullptr);

	void DrawSolverRail(
		ERayRopeDebugDrawFlags Flag,
		const FVector& Origin,
		const FVector& Direction,
		float HalfLength,
		const TCHAR* Label = nullptr);

	void LogFrameSummary(const TCHAR* Context) const;

	const FRayRopeDebugSettings& GetSettings() const
	{
		return Settings;
	}

private:
	UWorld* World = nullptr;
	const AActor* OwnerActor = nullptr;
	const FRayRopeDebugSettings& Settings;
	const TCHAR* FrameContext = nullptr;

	int32 TraceQueryCount = 0;
	int32 BlockingTraceCount = 0;
	int32 ClearTraceCount = 0;
	int32 SolverEventCount = 0;
};
