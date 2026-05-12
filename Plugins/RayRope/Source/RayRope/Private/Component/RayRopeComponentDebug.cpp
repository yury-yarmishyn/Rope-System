#include "Component/RayRopeComponent.h"

#include "Debug/RayRopeDebug.h"

bool URayRopeComponent::IsDebugDrawEnabled() const
{
	return RopeDebugSettings.bDebugEnabled && RopeDebugSettings.bDrawDebugRope;
}

bool URayRopeComponent::IsDebugLogEnabled() const
{
	return RopeDebugSettings.bDebugEnabled && RopeDebugSettings.bLogDebugState;
}

void URayRopeComponent::TickDebug(const TCHAR* Context)
{
	if (!RopeDebugSettings.bDebugEnabled)
	{
		return;
	}

	DrawDebugRope();
	LogDebugRopeState(Context);
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
}
