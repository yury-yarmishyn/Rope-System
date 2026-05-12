#include "Component/RayRopeComponent.h"

#include "Debug/RayRopeDebugConfig.h"
#include "Debug/RayRopeDebug.h"

bool URayRopeComponent::IsDebugDrawEnabled() const
{
#if RAYROPE_WITH_DEBUG
	return RopeDebugSettings.bDebugEnabled &&
		RopeDebugSettings.bDrawDebugRope &&
		EnumHasAnyFlags(
			static_cast<ERayRopeDebugDrawFlags>(RopeDebugSettings.DebugDrawFlags),
			ERayRopeDebugDrawFlags::Topology);
#else
	return false;
#endif
}

bool URayRopeComponent::IsDebugLogEnabled() const
{
#if RAYROPE_WITH_DEBUG
	return RopeDebugSettings.bDebugEnabled && RopeDebugSettings.bLogDebugState;
#else
	return false;
#endif
}

void URayRopeComponent::TickDebug(const TCHAR* Context)
{
#if RAYROPE_WITH_DEBUG
	if (!RopeDebugSettings.bDebugEnabled)
	{
		return;
	}

	DrawDebugRope();
	LogDebugRopeState(Context);
#endif
}

void URayRopeComponent::DrawDebugRope() const
{
	if (!IsDebugDrawEnabled())
	{
		return;
	}

	FRayRopeDebug::DrawRope(
		GetWorld(),
		GetOwner(),
		RopeSegments,
		RopeDebugSettings,
		CurrentRopeLength,
		MaxAllowedRopeLength,
		bIsRopeSolving);
}

void URayRopeComponent::LogDebugRopeState(const TCHAR* Context, bool bForce)
{
#if RAYROPE_WITH_DEBUG
	if (!IsDebugLogEnabled())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float CurrentTime = World != nullptr ? World->GetTimeSeconds() : 0.f;
	const float LogInterval = FMath::Max(0.f, RopeDebugSettings.DebugLogIntervalSeconds);
	if (!bForce && LogInterval > 0.f && CurrentTime < NextDebugLogTimeSeconds)
	{
		return;
	}

	NextDebugLogTimeSeconds = CurrentTime + LogInterval;

	FRayRopeDebug::LogRopeState(
		Context,
		GetOwner(),
		RopeSegments,
		CurrentRopeLength,
		MaxAllowedRopeLength,
		bIsRopeSolving);
#endif
}
