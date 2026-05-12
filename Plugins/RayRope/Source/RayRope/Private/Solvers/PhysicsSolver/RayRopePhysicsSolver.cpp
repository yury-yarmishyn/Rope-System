#include "RayRopePhysicsSolver.h"

#include "Debug/RayRopeDebugContext.h"
#include "RayRopePhysicsSolverInternal.h"

using namespace RayRopePhysicsSolverPrivate;

bool FRayRopePhysicsSolver::Solve(
	AActor* OwnerActor,
	const TArray<FRayRopeSegment>& Segments,
	const FRayRopePhysicsSettings& PhysicsSettings,
	FRayRopeDebugContext* DebugContext)
{
	if (PhysicsSettings.MaxAllowedRopeLength <= 0.f ||
		PhysicsSettings.CurrentRopeLength <= PhysicsSettings.MaxAllowedRopeLength)
	{
		if (DebugContext != nullptr)
		{
			DebugContext->RecordSolverEvent(
				TEXT("Physics"),
				FString::Printf(
					TEXT("Clamp skipped CurrentLength=%.2f MaxLength=%.2f"),
					PhysicsSettings.CurrentRopeLength,
					PhysicsSettings.MaxAllowedRopeLength));
		}
		return false;
	}

	FOwnerTerminalNodes TerminalNodes;
	if (!TryGetOwnerTerminalNodes(OwnerActor, Segments, TerminalNodes) ||
		!TerminalNodes.IsValid())
	{
		if (DebugContext != nullptr)
		{
			DebugContext->RecordSolverEvent(
				TEXT("Physics"),
				FString::Printf(
					TEXT("Clamp rejected: owner terminal nodes not found Segments=%d"),
					Segments.Num()));
		}
		return false;
	}

	FOwnerClampDebugResult ClampDebugResult;
	const bool bMoved = ClampOwnerAnchorToMaxRopeLength(
		OwnerActor,
		PhysicsSettings,
		*TerminalNodes.OwnerNode,
		*TerminalNodes.AdjacentNode,
		&ClampDebugResult);

	if (DebugContext != nullptr)
	{
		const FLinearColor ClampColor = bMoved
			? DebugContext->GetSettings().DebugAcceptedColor
			: DebugContext->GetSettings().DebugBlockedTraceColor;
		DebugContext->DrawSolverLine(
			ERayRopeDebugDrawFlags::PhysicsClamp,
			ClampDebugResult.OwnerAnchorLocation,
			ClampDebugResult.AdjacentLocation,
			DebugContext->GetSettings().DebugSolverGuideColor,
			TEXT("ClampSpan"));
		DebugContext->DrawSolverVector(
			ERayRopeDebugDrawFlags::PhysicsClamp,
			ClampDebugResult.OwnerAnchorLocation,
			ClampDebugResult.ActorDelta,
			ClampDebugResult.ActorDelta.Size(),
			ClampColor,
			bMoved ? TEXT("ClampMove") : TEXT("ClampRejected"));
		if (ClampDebugResult.bSweepBlocked)
		{
			DebugContext->DrawSolverPoint(
				ERayRopeDebugDrawFlags::PhysicsClamp,
				ClampDebugResult.SweepHit.ImpactPoint,
				DebugContext->GetSettings().DebugBlockedTraceColor,
				TEXT("ClampSweepHit"));
		}

		DebugContext->RecordSolverEvent(
			TEXT("Physics"),
			FString::Printf(
				TEXT("Clamp %s Excess=%.2f ClampDistance=%.2f Delta=%s SweepBlocked=%s HitActor=%s"),
				bMoved ? TEXT("applied") : TEXT("rejected"),
				ClampDebugResult.ExcessLength,
				ClampDebugResult.ClampDistance,
				*ClampDebugResult.ActorDelta.ToCompactString(),
				ClampDebugResult.bSweepBlocked ? TEXT("true") : TEXT("false"),
				*GetNameSafe(ClampDebugResult.SweepHit.GetActor())));
	}

	return bMoved;
}

