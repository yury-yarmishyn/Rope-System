#include "RayRopePhysicsSolver.h"

#include "RayRopePhysicsSolverInternal.h"

using namespace RayRopePhysicsSolverPrivate;

bool FRayRopePhysicsSolver::Solve(
	AActor* OwnerActor,
	const TArray<FRayRopeSegment>& Segments,
	const FRayRopePhysicsSettings& PhysicsSettings)
{
	if (PhysicsSettings.MaxAllowedRopeLength <= 0.f ||
		PhysicsSettings.CurrentRopeLength <= PhysicsSettings.MaxAllowedRopeLength)
	{
		return false;
	}

	FOwnerTerminalNodes TerminalNodes;
	if (!TryGetOwnerTerminalNodes(OwnerActor, Segments, TerminalNodes) ||
		!TerminalNodes.IsValid())
	{
		return false;
	}

	return ClampOwnerAnchorToMaxRopeLength(
		OwnerActor,
		PhysicsSettings,
		*TerminalNodes.OwnerNode,
		*TerminalNodes.AdjacentNode);
}

