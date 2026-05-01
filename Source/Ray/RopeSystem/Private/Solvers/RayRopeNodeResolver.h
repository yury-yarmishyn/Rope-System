#pragma once

#include "CoreMinimal.h"
#include "RayRopeTypes.h"

class AActor;

struct FRayRopeNodeResolver
{
	static void SyncSegmentNodes(FRayRopeSegment& Segment);
	static void SyncNode(FRayRopeNode& Node);
	static void SyncAnchorNode(FRayRopeNode& Node);
	static void SyncRedirectNode(FRayRopeNode& Node);
	static void CacheAttachActorOffset(FRayRopeNode& Node);
	static void CacheAnchorTarget(FRayRopeNode& Node);
	static FVector GetAnchorWorldLocation(const FRayRopeNode& Node);
	static FRayRopeNode CreateAnchorNode(AActor* AnchorActor);
};
