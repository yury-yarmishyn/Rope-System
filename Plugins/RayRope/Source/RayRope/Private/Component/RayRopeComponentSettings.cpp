#include "Component/RayRopeComponentSettings.h"

#include "Component/RayRopeComponent.h"

FRayRopeTraceSettings FRayRopeComponentSettings::MakeTraceSettings(
	const URayRopeComponent& Component,
	FRayRopeDebugContext* DebugContext)
{
	FRayRopeTraceSettings TraceSettings;
	TraceSettings.World = Component.GetWorld();
	TraceSettings.OwnerActor = Component.GetOwner();
	TraceSettings.TraceChannel = Component.TraceChannel;
	TraceSettings.bTraceComplex = Component.bTraceComplex;
	TraceSettings.DebugContext = DebugContext;
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

FRayRopeMoveSettings FRayRopeComponentSettings::MakeMoveSettings(const URayRopeComponent& Component)
{
	FRayRopeMoveSettings MoveSettings;
	MoveSettings.MoveSolverTolerance = Component.MoveSolverTolerance;
	MoveSettings.PlaneParallelTolerance = Component.MovePlaneParallelTolerance;
	MoveSettings.EffectivePointSearchTolerance = Component.MoveEffectivePointSearchTolerance;
	MoveSettings.MinMoveDistance = Component.MoveMinMoveDistance;
	MoveSettings.MinNodeSeparation = Component.MoveMinNodeSeparation;
	MoveSettings.MinLengthImprovement = Component.MoveMinLengthImprovement;
	MoveSettings.MaxMoveDistancePerIteration = Component.MoveMaxDistancePerIteration;
	MoveSettings.SurfaceOffset = Component.WrapSurfaceOffset;
	MoveSettings.bUseGlobalMoveSolver = Component.bUseGlobalMoveSolver;
	MoveSettings.bFallbackToLocalMoveSolver = Component.bFallbackToLocalMoveSolver;
	MoveSettings.MaxMoveIterations = Component.MaxMoveIterations;
	MoveSettings.MaxEffectivePointSearchIterations = Component.MaxEffectivePointSearchIterations;
	MoveSettings.MaxGlobalMoveIterations = Component.MaxGlobalMoveIterations;
	MoveSettings.MaxGlobalMoveLineSearchSteps = Component.MaxGlobalMoveLineSearchSteps;
	MoveSettings.MaxGlobalValidationSamples = Component.MaxGlobalValidationSamples;
	MoveSettings.GlobalMoveDamping = Component.GlobalMoveDamping;
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
	const URayRopeComponent& Component,
	FRayRopeDebugContext* DebugContext)
{
	FRayRopeComponentSolveSettings SolveSettings;
	SolveSettings.TraceSettings = MakeTraceSettings(Component, DebugContext);
	SolveSettings.NodeBuildSettings = MakeNodeBuildSettings(Component);
	SolveSettings.MoveSettings = MakeMoveSettings(Component);
	SolveSettings.RelaxSettings = MakeRelaxSettings(Component);
	return SolveSettings;
}
