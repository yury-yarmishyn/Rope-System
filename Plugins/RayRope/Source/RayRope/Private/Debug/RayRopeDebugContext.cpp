#include "Debug/RayRopeDebugContext.h"

#if RAYROPE_WITH_DEBUG
#include "DrawDebugHelpers.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#endif

namespace
{
#if RAYROPE_WITH_DEBUG
FColor ToDebugContextColor(const FLinearColor& Color)
{
	return Color.ToFColor(true);
}
#endif

const TCHAR* SafeText(const TCHAR* Text, const TCHAR* Fallback)
{
	return Text != nullptr ? Text : Fallback;
}

ERayRopeDebugLogFlags GetLogFlagForCategory(const TCHAR* Category)
{
	const FString CategoryString = SafeText(Category, TEXT("Solver"));
	if (CategoryString == TEXT("Trace"))
	{
		return ERayRopeDebugLogFlags::TraceQueries;
	}

	if (CategoryString == TEXT("Start"))
	{
		return ERayRopeDebugLogFlags::Start;
	}

	if (CategoryString == TEXT("Wrap") ||
		CategoryString == TEXT("NodeBuilder"))
	{
		return ERayRopeDebugLogFlags::Wrap;
	}

	if (CategoryString == TEXT("MoveGlobal"))
	{
		return ERayRopeDebugLogFlags::GlobalMove;
	}

	if (CategoryString == TEXT("Move") ||
		CategoryString == TEXT("MoveLocal") ||
		CategoryString == TEXT("MoveRail") ||
		CategoryString == TEXT("MovePointSearch") ||
		CategoryString == TEXT("MoveValidation"))
	{
		return ERayRopeDebugLogFlags::Move;
	}

	if (CategoryString == TEXT("Transition"))
	{
		return ERayRopeDebugLogFlags::TransitionValidation;
	}

	if (CategoryString == TEXT("Relax"))
	{
		return ERayRopeDebugLogFlags::Relax;
	}

	if (CategoryString == TEXT("Physics"))
	{
		return ERayRopeDebugLogFlags::PhysicsClamp;
	}

	return ERayRopeDebugLogFlags::Pipeline;
}
}

FRayRopeDebugContext::FRayRopeDebugContext(
	UWorld* InWorld,
	const AActor* InOwnerActor,
	const FRayRopeDebugSettings& InSettings,
	const TCHAR* InFrameContext)
	: World(InWorld)
	, OwnerActor(InOwnerActor)
	, Settings(InSettings)
	, FrameContext(InFrameContext)
{
}

bool FRayRopeDebugContext::ShouldCreateForSolvers(const FRayRopeDebugSettings& Settings)
{
#if RAYROPE_WITH_DEBUG
	const int32 SolverDrawMask =
		static_cast<int32>(ERayRopeDebugDrawFlags::TraceQueries) |
		static_cast<int32>(ERayRopeDebugDrawFlags::Wrap) |
		static_cast<int32>(ERayRopeDebugDrawFlags::MoveRails) |
		static_cast<int32>(ERayRopeDebugDrawFlags::MoveCandidates) |
		static_cast<int32>(ERayRopeDebugDrawFlags::GlobalMove) |
		static_cast<int32>(ERayRopeDebugDrawFlags::TransitionValidation) |
		static_cast<int32>(ERayRopeDebugDrawFlags::Relax) |
		static_cast<int32>(ERayRopeDebugDrawFlags::PhysicsClamp);
	const bool bHasSolverDraw =
		Settings.bDrawDebugRope &&
		(Settings.DebugDrawFlags & SolverDrawMask) != 0;
	const bool bHasSolverLogs =
		Settings.DebugLogFlags != static_cast<int32>(ERayRopeDebugLogFlags::None);
	return Settings.bDebugEnabled && (bHasSolverDraw || bHasSolverLogs);
#else
	return false;
#endif
}

bool FRayRopeDebugContext::IsEnabled() const
{
#if RAYROPE_WITH_DEBUG
	return Settings.bDebugEnabled;
#else
	return false;
#endif
}

bool FRayRopeDebugContext::ShouldDraw(ERayRopeDebugDrawFlags Flag) const
{
#if RAYROPE_WITH_DEBUG
	return IsEnabled() &&
		Settings.bDrawDebugRope &&
		World != nullptr &&
		EnumHasAnyFlags(
			static_cast<ERayRopeDebugDrawFlags>(Settings.DebugDrawFlags),
			Flag);
#else
	return false;
#endif
}

bool FRayRopeDebugContext::ShouldLog(ERayRopeDebugLogFlags Flag) const
{
#if RAYROPE_WITH_DEBUG
	return IsEnabled() &&
		EnumHasAnyFlags(
			static_cast<ERayRopeDebugLogFlags>(Settings.DebugLogFlags),
			Flag);
#else
	return false;
#endif
}

void FRayRopeDebugContext::RecordTrace(
	const TCHAR* Category,
	const FVector& StartLocation,
	const FVector& EndLocation,
	bool bBlockingHit,
	const FHitResult* SurfaceHit,
	const TCHAR* Reason)
{
#if RAYROPE_WITH_DEBUG
	++TraceQueryCount;
	if (bBlockingHit)
	{
		++BlockingTraceCount;
	}
	else
	{
		++ClearTraceCount;
	}

	const FLinearColor TraceColor = bBlockingHit
		? Settings.DebugBlockedTraceColor
		: Settings.DebugClearTraceColor;

	if (ShouldDraw(ERayRopeDebugDrawFlags::TraceQueries))
	{
		DrawSolverLine(
			ERayRopeDebugDrawFlags::TraceQueries,
			StartLocation,
			EndLocation,
			TraceColor,
			Reason);

		if (SurfaceHit != nullptr && SurfaceHit->bBlockingHit)
		{
			DrawSolverPoint(
				ERayRopeDebugDrawFlags::TraceQueries,
				SurfaceHit->ImpactPoint,
				TraceColor,
				SafeText(Category, TEXT("TraceHit")));
			DrawSolverVector(
				ERayRopeDebugDrawFlags::TraceQueries,
				SurfaceHit->ImpactPoint,
				SurfaceHit->ImpactNormal,
				FMath::Max(25.f, Settings.DebugNodeRadius * 2.f),
				Settings.DebugSolverGuideColor,
				TEXT("Normal"));
		}
	}

	if (ShouldLog(ERayRopeDebugLogFlags::TraceQueries) && bBlockingHit)
	{
		RecordSolverEvent(
			SafeText(Category, TEXT("Trace")),
			FString::Printf(
				TEXT("%s Start=%s End=%s HitActor=%s HitComponent=%s Impact=%s Normal=%s Reason=%s"),
				bBlockingHit ? TEXT("Blocked") : TEXT("Clear"),
				*StartLocation.ToCompactString(),
				*EndLocation.ToCompactString(),
				SurfaceHit != nullptr ? *GetNameSafe(SurfaceHit->GetActor()) : TEXT("None"),
				SurfaceHit != nullptr ? *GetNameSafe(SurfaceHit->GetComponent()) : TEXT("None"),
				SurfaceHit != nullptr ? *SurfaceHit->ImpactPoint.ToCompactString() : TEXT("None"),
				SurfaceHit != nullptr ? *SurfaceHit->ImpactNormal.ToCompactString() : TEXT("None"),
				SafeText(Reason, TEXT("None"))));
	}
#endif
}

void FRayRopeDebugContext::RecordSolverEvent(
	const TCHAR* Category,
	const FString& Message)
{
#if RAYROPE_WITH_DEBUG
	++SolverEventCount;

	const ERayRopeDebugLogFlags LogFlag = GetLogFlagForCategory(Category);
	if (!ShouldLog(LogFlag))
	{
		return;
	}

	UE_LOG(
		LogRayRope,
		Log,
		TEXT("[Debug][%s][%s] Owner=%s %s"),
		SafeText(FrameContext, TEXT("Unknown")),
		SafeText(Category, TEXT("Solver")),
		*GetNameSafe(OwnerActor),
		*Message);
#endif
}

void FRayRopeDebugContext::DrawSolverLine(
	ERayRopeDebugDrawFlags Flag,
	const FVector& StartLocation,
	const FVector& EndLocation,
	const FLinearColor& Color,
	const TCHAR* Label)
{
#if RAYROPE_WITH_DEBUG
	if (!ShouldDraw(Flag))
	{
		return;
	}

	const float LifeTime = FMath::Max(0.f, Settings.DebugDrawLifetime);
	const float Thickness = FMath::Max(0.f, Settings.DebugSolverLineThickness);
	DrawDebugLine(
		World,
		StartLocation,
		EndLocation,
		ToDebugContextColor(Color),
		false,
		LifeTime,
		0,
		Thickness);

	if (Settings.bDrawDebugLabels && Label != nullptr)
	{
		DrawDebugString(
			World,
			(StartLocation + EndLocation) * 0.5f,
			Label,
			nullptr,
			ToDebugContextColor(Color),
			LifeTime,
			true);
	}
#endif
}

void FRayRopeDebugContext::DrawSolverPoint(
	ERayRopeDebugDrawFlags Flag,
	const FVector& Location,
	const FLinearColor& Color,
	const TCHAR* Label)
{
#if RAYROPE_WITH_DEBUG
	if (!ShouldDraw(Flag))
	{
		return;
	}

	const float LifeTime = FMath::Max(0.f, Settings.DebugDrawLifetime);
	const float Radius = FMath::Max(2.f, Settings.DebugNodeRadius * 0.6f);
	const int32 Segments = FMath::Max(4, Settings.DebugNodeSphereSegments);
	DrawDebugSphere(
		World,
		Location,
		Radius,
		Segments,
		ToDebugContextColor(Color),
		false,
		LifeTime,
		0,
		FMath::Max(0.f, Settings.DebugSolverLineThickness));

	if (Settings.bDrawDebugLabels && Label != nullptr)
	{
		DrawDebugString(
			World,
			Location + FVector(0.f, 0.f, Radius),
			Label,
			nullptr,
			ToDebugContextColor(Color),
			LifeTime,
			true);
	}
#endif
}

void FRayRopeDebugContext::DrawSolverVector(
	ERayRopeDebugDrawFlags Flag,
	const FVector& StartLocation,
	const FVector& Direction,
	float Length,
	const FLinearColor& Color,
	const TCHAR* Label)
{
#if RAYROPE_WITH_DEBUG
	if (!ShouldDraw(Flag))
	{
		return;
	}

	const FVector SafeDirection = Direction.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		return;
	}

	const FVector EndLocation = StartLocation + SafeDirection * FMath::Max(0.f, Length);
	DrawDebugDirectionalArrow(
		World,
		StartLocation,
		EndLocation,
		FMath::Max(8.f, Settings.DebugNodeRadius),
		ToDebugContextColor(Color),
		false,
		FMath::Max(0.f, Settings.DebugDrawLifetime),
		0,
		FMath::Max(0.f, Settings.DebugSolverLineThickness));

	if (Settings.bDrawDebugLabels && Label != nullptr)
	{
		DrawDebugString(
			World,
			EndLocation,
			Label,
			nullptr,
			ToDebugContextColor(Color),
			FMath::Max(0.f, Settings.DebugDrawLifetime),
			true);
	}
#endif
}

void FRayRopeDebugContext::DrawSolverRail(
	ERayRopeDebugDrawFlags Flag,
	const FVector& Origin,
	const FVector& Direction,
	float HalfLength,
	const TCHAR* Label)
{
#if RAYROPE_WITH_DEBUG
	const FVector SafeDirection = Direction.GetSafeNormal();
	if (SafeDirection.IsNearlyZero())
	{
		return;
	}

	const float DrawHalfLength = FMath::Max(10.f, HalfLength);
	DrawSolverLine(
		Flag,
		Origin - SafeDirection * DrawHalfLength,
		Origin + SafeDirection * DrawHalfLength,
		Settings.DebugSolverGuideColor,
		Label);
	DrawSolverPoint(
		Flag,
		Origin,
		Settings.DebugSolverGuideColor,
		Label != nullptr ? TEXT("RailOrigin") : nullptr);
#endif
}

void FRayRopeDebugContext::LogFrameSummary(const TCHAR* Context) const
{
#if RAYROPE_WITH_DEBUG
	if (!ShouldLog(ERayRopeDebugLogFlags::FrameSummary))
	{
		return;
	}

	UE_LOG(
		LogRayRope,
		Log,
		TEXT("[Debug][%s] Owner=%s SolverEvents=%d TraceQueries=%d Blocking=%d Clear=%d"),
		SafeText(Context, SafeText(FrameContext, TEXT("Unknown"))),
		*GetNameSafe(OwnerActor),
		SolverEventCount,
		TraceQueryCount,
		BlockingTraceCount,
		ClearTraceCount);
#endif
}
