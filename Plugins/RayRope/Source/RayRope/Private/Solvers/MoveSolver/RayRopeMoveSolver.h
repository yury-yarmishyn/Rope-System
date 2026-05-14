#pragma once

#include "Nodes/RayRopeNodeBuilder.h"
#include "Solvers/SolvePipeline/RayRopeSolveTypes.h"

/**
 * Settings controlling redirect movement along collision-derived rails.
 */
struct FRayRopeMoveSettings
{
	/** Geometry epsilon used for move validation and convergence. */
	float MoveSolverTolerance = KINDA_SMALL_NUMBER;

	/** Tolerance for treating two surface planes as parallel while building a rail. */
	float PlaneParallelTolerance = KINDA_SMALL_NUMBER;

	/** Search tolerance for choosing a usable point on the move rail. */
	float EffectivePointSearchTolerance = KINDA_SMALL_NUMBER;

	/** Minimum redirect displacement required for a move to be applied. */
	float MinMoveDistance = 0.05f;

	/** Minimum allowed distance between a moved redirect and its neighbors. */
	float MinNodeSeparation = 0.05f;

	/** Minimum local path-length reduction required for moving a free redirect. */
	float MinLengthImprovement = 0.01f;

	/** Per-iteration displacement cap; non-positive values allow the full target move. */
	float MaxMoveDistancePerIteration = 2.f;

	/** Distance used to keep rail targets offset from hit surfaces. */
	float SurfaceOffset = 0.f;

	/** Enables the batch rail-coordinate solver before falling back to the local move pass. */
	bool bUseGlobalMoveSolver = true;

	/** Runs the full local move pass when the global pass cannot find a valid batch step. */
	bool bFallbackToLocalMoveSolver = true;

	/** Number of alternating forward/backward sweeps over redirect nodes. */
	int32 MaxMoveIterations = 4;

	/** Base iteration budget distributed across move rail search, candidate backtracking, and validation. */
	int32 MaxEffectivePointSearchIterations = 8;

	/** Maximum number of global rail-coordinate solve iterations per move pass. */
	int32 MaxGlobalMoveIterations = 2;

	/** Maximum number of shared-alpha backtracking attempts for a global batch step. */
	int32 MaxGlobalMoveLineSearchSteps = 4;

	/** Number of intermediate alpha samples checked during global batch validation. */
	int32 MaxGlobalValidationSamples = 8;

	/** Positive diagonal regularizer used when solving the global rail-coordinate system. */
	float GlobalMoveDamping = 0.001f;
};

/**
 * Moves existing redirect nodes to reduce local rope length while preserving collision constraints.
 */
struct FRayRopeMoveSolver
{
	/**
	 * Runs the redirect move pass in place.
	 *
	 * The pass may also queue new redirects when moving a node exposes newly blocked adjacent spans.
	 */
	static FRayRopeSolveResult MoveSegment(
		const FRayRopeTraceSettings& TraceSettings,
		const FRayRopeMoveSettings& MoveSettings,
		const FRayRopeNodeBuildSettings& NodeBuildSettings,
		FRayRopeSegment& Segment);
};
