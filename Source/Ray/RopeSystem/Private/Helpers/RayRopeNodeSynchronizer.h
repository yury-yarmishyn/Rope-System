#pragma once

#include "CoreMinimal.h"
#include "RayRopeTypes.h"

class AActor;

struct FRayRopeNodeSynchronizer
{
	static void SyncSegmentNodes(FRayRopeSegment& Segment);
	static void CacheAttachedActorOffset(FRayRopeNode& Node);
	static FRayRopeNode CreateAnchorNode(AActor* AnchorActor);
};
