#pragma once

#include "Trace/RayRopeTrace.h"

struct FRayRopeInitialSegmentBuilder
{
	static bool TryBuildSegments(
		const FRayRopeTraceSettings& TraceSettings,
		const TArray<AActor*>& AnchorActors,
		TArray<FRayRopeSegment>& OutSegments);
};

