#include "Component/RayRopeComponentSettings.h"

#include "Component/RayRopeComponent.h"

FRayRopeTraceSettings FRayRopeComponentSettings::MakeTraceSettings(
	const URayRopeComponent& Component)
{
	FRayRopeTraceSettings TraceSettings;
	TraceSettings.World = Component.GetWorld();
	TraceSettings.OwnerActor = Component.GetOwner();
	TraceSettings.TraceChannel = Component.TraceChannel;
	TraceSettings.bTraceComplex = Component.bTraceComplex;
	return TraceSettings;
}

FRayRopeNodeBuildSettings FRayRopeComponentSettings::MakeNodeBuildSettings(
	const URayRopeComponent& Component)
{
	FRayRopeNodeBuildSettings Settings;
	Settings.bAllowWrapOnMovableObjects = Component.bAllowWrapOnMovableObjects;
	Settings.MaxWrapBinarySearchIterations = Component.MaxWrapBinarySearchIterations;
	Settings.WrapSolverTolerance = Component.WrapSolverTolerance;
	Settings.GeometryCollinearityTolerance = Component.GeometryCollinearityTolerance;
	Settings.WrapSurfaceOffset = Component.WrapSurfaceOffset;
	return Settings;
}

FRayRopeMoveSettings FRayRopeComponentSettings::MakeMoveSettings(
	const URayRopeComponent& Component,
	const FRayRopeNodeBuildSettings& NodeBuildSettings)
{
	FRayRopeMoveSettings MoveSettings;
	MoveSettings.NodeBuildSettings = NodeBuildSettings;
	MoveSettings.MoveSolverTolerance = Component.MoveSolverTolerance;
	MoveSettings.PlaneParallelTolerance = Component.MovePlaneParallelTolerance;
	MoveSettings.EffectivePointSearchTolerance = Component.MoveEffectivePointSearchTolerance;
	MoveSettings.MinMoveDistance = Component.MoveMinMoveDistance;
	MoveSettings.MinNodeSeparation = Component.MoveMinNodeSeparation;
	MoveSettings.MinLengthImprovement = Component.MoveMinLengthImprovement;
	MoveSettings.MaxMoveDistancePerIteration = Component.MoveMaxDistancePerIteration;
	MoveSettings.SurfaceOffset = Component.WrapSurfaceOffset;
	MoveSettings.MaxMoveIterations = Component.MaxMoveIterations;
	MoveSettings.MaxEffectivePointSearchIterations = Component.MaxEffectivePointSearchIterations;
	return MoveSettings;
}

FRayRopeRelaxSettings FRayRopeComponentSettings::MakeRelaxSettings(
	const URayRopeComponent& Component)
{
	FRayRopeRelaxSettings RelaxSettings;
	RelaxSettings.RelaxSolverTolerance = Component.RelaxSolverTolerance;
	RelaxSettings.MaxRelaxCollapseIterations = Component.MaxRelaxCollapseIterations;
	return RelaxSettings;
}

FRayRopePhysicsSettings FRayRopeComponentSettings::MakePhysicsSettings(
	const URayRopeComponent& Component)
{
	FRayRopePhysicsSettings PhysicsSettings;
	PhysicsSettings.CurrentRopeLength = Component.CurrentRopeLength;
	PhysicsSettings.MaxAllowedRopeLength = Component.MaxAllowedRopeLength;
	return PhysicsSettings;
}

FRayRopeComponentSolveSettings FRayRopeComponentSettings::MakeSolveSettings(
	const URayRopeComponent& Component)
{
	FRayRopeComponentSolveSettings SolveSettings;
	SolveSettings.TraceSettings = MakeTraceSettings(Component);
	SolveSettings.NodeBuildSettings = MakeNodeBuildSettings(Component);
	SolveSettings.MoveSettings = MakeMoveSettings(Component, SolveSettings.NodeBuildSettings);
	SolveSettings.RelaxSettings = MakeRelaxSettings(Component);
	return SolveSettings;
}
