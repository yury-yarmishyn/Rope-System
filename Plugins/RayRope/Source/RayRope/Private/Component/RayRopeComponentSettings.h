#pragma once

#include "Nodes/RayRopeNodeBuilder.h"
#include "Solvers/MoveSolver/RayRopeMoveSolver.h"
#include "Solvers/PhysicsSolver/RayRopePhysicsSolver.h"
#include "Solvers/RelaxSolver/RayRopeRelaxSolver.h"

class URayRopeComponent;
struct FRayRopeDebugContext;

/**
 * Aggregated settings consumed by one segment solve.
 */
struct FRayRopeComponentSolveSettings
{
	/** Trace policy shared by wrap, move, and relax passes. */
	FRayRopeTraceSettings TraceSettings;

	/** Redirect creation settings used by wrap and move insertion paths. */
	FRayRopeNodeBuildSettings NodeBuildSettings;

	/** Move solver settings derived from component properties. */
	FRayRopeMoveSettings MoveSettings;

	/** Relax solver settings derived from component properties. */
	FRayRopeRelaxSettings RelaxSettings;
};

/**
 * Converts editor-facing component properties into plain solver settings.
 */
struct FRayRopeComponentSettings
{
	static FRayRopeTraceSettings MakeTraceSettings(
		const URayRopeComponent& Component,
		FRayRopeDebugContext* DebugContext = nullptr);

	static FRayRopeNodeBuildSettings MakeNodeBuildSettings(const URayRopeComponent& Component);

	static FRayRopeMoveSettings MakeMoveSettings(const URayRopeComponent& Component);

	static FRayRopeRelaxSettings MakeRelaxSettings(const URayRopeComponent& Component);

	/** Builds runtime length constraint settings from current component state. */
	static FRayRopePhysicsSettings MakePhysicsSettings(const URayRopeComponent& Component);

	static FRayRopeComponentSolveSettings MakeSolveSettings(
		const URayRopeComponent& Component,
		FRayRopeDebugContext* DebugContext = nullptr);
};
