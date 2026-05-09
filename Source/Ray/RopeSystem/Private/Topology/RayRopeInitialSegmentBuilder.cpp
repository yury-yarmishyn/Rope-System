#include "Topology/RayRopeInitialSegmentBuilder.h"

#include "Nodes/RayRopeNodeSynchronizer.h"
#include "Topology/RayRopeSegmentTopology.h"

bool FRayRopeInitialSegmentBuilder::TryBuildSegments(
	const FRayRopeTraceSettings& TraceSettings,
	const TArray<AActor*>& AnchorActors,
	TArray<FRayRopeSegment>& OutSegments)
{
	OutSegments.Reset();

	if (!IsValid(TraceSettings.World) || AnchorActors.Num() < 2)
	{
		return false;
	}

	OutSegments.Reserve(AnchorActors.Num() - 1);
	const FRayRopeTraceContext TraceContext = FRayRopeTrace::MakeTraceContext(
		TraceSettings,
		FCollisionQueryParams(SCENE_QUERY_STAT(RayRopeStartTrace)));

	for (int32 AnchorIndex = 0; AnchorIndex < AnchorActors.Num() - 1; ++AnchorIndex)
	{
		AActor* StartAnchorActor = AnchorActors[AnchorIndex];
		AActor* EndAnchorActor = AnchorActors[AnchorIndex + 1];
		if (!IsValid(StartAnchorActor) ||
			!IsValid(EndAnchorActor) ||
			StartAnchorActor == EndAnchorActor)
		{
			OutSegments.Reset();
			return false;
		}

		FRayRopeSegment BaseSegment;
		BaseSegment.Nodes.Reserve(2);
		BaseSegment.Nodes.Add(FRayRopeNodeSynchronizer::CreateAnchorNode(StartAnchorActor));
		BaseSegment.Nodes.Add(FRayRopeNodeSynchronizer::CreateAnchorNode(EndAnchorActor));

		FRayRopeSpan BaseSpan;
		if (!FRayRopeSegmentTopology::TryGetSegmentSpan(BaseSegment, 0, BaseSpan) ||
			BaseSpan.IsDegenerate(KINDA_SMALL_NUMBER) ||
			FRayRopeTrace::HasBlockingSpanHit(TraceContext, BaseSpan))
		{
			OutSegments.Reset();
			return false;
		}

		OutSegments.Add(MoveTemp(BaseSegment));
	}

	return OutSegments.Num() > 0;
}
