#pragma once

#include "Trace/RayRopeTrace.h"

/**
 * Builds the initial direct topology between requested anchor actors.
 */
struct FRayRopeInitialSegmentBuilder
{
	/**
	 * Creates one direct segment for each consecutive anchor pair.
	 *
	 * Fails without at least two anchors, with invalid or duplicate neighboring anchors, or when any
	 * initial direct span intersects blocking geometry. OutSegments is reset on failure.
	 */
	static bool TryBuildSegments(
		const FRayRopeTraceSettings& TraceSettings,
		const TArray<AActor*>& AnchorActors,
		TArray<FRayRopeSegment>& OutSegments);
};

