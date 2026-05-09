#pragma once

#include "Nodes/RayRopeNodeBuilder.h"

/**
 * Settings controlling redirect movement along collision-derived rails.
 */
struct FRayRopeMoveSettings
{
	/** Node creation policy used if a move target requires extra redirects. */
	FRayRopeNodeBuildSettings NodeBuildSettings;

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

	/** Number of alternating forward/backward sweeps over redirect nodes. */
	int32 MaxMoveIterations = 4;

	/** Iteration budget shared by rail hit search, rail optimization, and transition validation. */
	int32 MaxEffectivePointSearchIterations = 8;
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
	static void MoveSegment(
		const FRayRopeTraceSettings& TraceSettings,
		const FRayRopeMoveSettings& MoveSettings,
		FRayRopeSegment& Segment);
};
