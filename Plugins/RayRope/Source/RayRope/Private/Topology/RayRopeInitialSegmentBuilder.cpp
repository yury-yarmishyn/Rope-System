#include "Topology/RayRopeInitialSegmentBuilder.h"

#include "Debug/RayRopeDebugContext.h"
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
		if (TraceSettings.DebugContext != nullptr)
		{
			TraceSettings.DebugContext->RecordSolverEvent(
				TEXT("Start"),
				FString::Printf(
					TEXT("Rejected: World=%s AnchorActors=%d"),
					IsValid(TraceSettings.World) ? TEXT("Valid") : TEXT("Invalid"),
					AnchorActors.Num()));
		}
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
			if (TraceContext.DebugContext != nullptr)
			{
				TraceContext.DebugContext->RecordSolverEvent(
					TEXT("Start"),
					FString::Printf(
						TEXT("Rejected anchor pair [%d,%d] Start=%s End=%s"),
						AnchorIndex,
						AnchorIndex + 1,
						*GetNameSafe(StartAnchorActor),
						*GetNameSafe(EndAnchorActor)));
			}
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
			if (TraceContext.DebugContext != nullptr)
			{
				TraceContext.DebugContext->RecordSolverEvent(
					TEXT("Start"),
					FString::Printf(
						TEXT("Rejected base span [%d,%d] Start=%s End=%s Degenerate=%s"),
						AnchorIndex,
						AnchorIndex + 1,
						*GetNameSafe(StartAnchorActor),
						*GetNameSafe(EndAnchorActor),
						BaseSpan.IsDegenerate(KINDA_SMALL_NUMBER) ? TEXT("true") : TEXT("false")));
			}
			OutSegments.Reset();
			return false;
		}

		if (TraceContext.DebugContext != nullptr)
		{
			TraceContext.DebugContext->RecordSolverEvent(
				TEXT("Start"),
				FString::Printf(
					TEXT("Accepted base span [%d,%d] Start=%s End=%s"),
					AnchorIndex,
					AnchorIndex + 1,
					*GetNameSafe(StartAnchorActor),
					*GetNameSafe(EndAnchorActor)));
		}

		OutSegments.Add(MoveTemp(BaseSegment));
	}

	return OutSegments.Num() > 0;
}
