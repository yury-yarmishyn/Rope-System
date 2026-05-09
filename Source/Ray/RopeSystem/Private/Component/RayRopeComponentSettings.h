#pragma once

#include "Nodes/RayRopeNodeBuilder.h"
#include "Solvers/MoveSolver/RayRopeMoveSolver.h"
#include "Solvers/PhysicsSolver/RayRopePhysicsSolver.h"
#include "Solvers/RelaxSolver/RayRopeRelaxSolver.h"

class URayRopeComponent;

struct FRayRopeComponentSolveSettings
{
	FRayRopeTraceSettings TraceSettings;
	FRayRopeNodeBuildSettings NodeBuildSettings;
	FRayRopeMoveSettings MoveSettings;
	FRayRopeRelaxSettings RelaxSettings;
};

struct FRayRopeComponentSettings
{
	static FRayRopeTraceSettings MakeTraceSettings(const URayRopeComponent& Component);
	static FRayRopeNodeBuildSettings MakeNodeBuildSettings(const URayRopeComponent& Component);
	static FRayRopeMoveSettings MakeMoveSettings(
		const URayRopeComponent& Component,
		const FRayRopeNodeBuildSettings& NodeBuildSettings);
	static FRayRopeRelaxSettings MakeRelaxSettings(const URayRopeComponent& Component);
	static FRayRopePhysicsSettings MakePhysicsSettings(const URayRopeComponent& Component);
	static FRayRopeComponentSolveSettings MakeSolveSettings(const URayRopeComponent& Component);
};
