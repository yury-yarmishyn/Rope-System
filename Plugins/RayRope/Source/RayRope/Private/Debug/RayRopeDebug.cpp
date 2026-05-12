#include "RayRopeDebug.h"

#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "Topology/RayRopeSegmentTopology.h"

namespace
{
FColor ToDebugColor(const FLinearColor& Color)
{
	return Color.ToFColor(true);
}

const TCHAR* GetRopeNodeTypeName(ERayRopeNodeType NodeType)
{
	switch (NodeType)
	{
	case ERayRopeNodeType::Anchor:
		return TEXT("Anchor");

	case ERayRopeNodeType::Redirect:
		return TEXT("Redirect");

	default:
		return TEXT("Unknown");
	}
}

FVector CalculateSegmentCenter(const FRayRopeSegment& Segment)
{
	if (Segment.Nodes.Num() == 0)
	{
		return FVector::ZeroVector;
	}

	FVector Center = FVector::ZeroVector;
	for (const FRayRopeNode& Node : Segment.Nodes)
	{
		Center += Node.WorldLocation;
	}

	return Center / static_cast<float>(Segment.Nodes.Num());
}
}

void FRayRopeDebug::DrawRope(
	UWorld* World,
	const AActor* OwnerActor,
	const TArray<FRayRopeSegment>& Segments,
	const FRayRopeDebugSettings& DebugSettings,
	float CurrentRopeLength,
	float MaxAllowedRopeLength,
	bool bIsRopeSolving)
{
	if (World == nullptr)
	{
		return;
	}

	const float LifeTime = FMath::Max(0.f, DebugSettings.DebugDrawLifetime);
	const int32 NodeSegments = FMath::Max(4, DebugSettings.DebugNodeSphereSegments);
	const float NodeRadius = FMath::Max(0.f, DebugSettings.DebugNodeRadius);
	const float SegmentThickness = FMath::Max(0.f, DebugSettings.DebugSegmentThickness);
	const float AttachmentLinkThickness = FMath::Max(0.f, DebugSettings.DebugAttachmentLinkThickness);
	const FColor OwnerColor = ToDebugColor(DebugSettings.DebugOwnerColor);
	const FColor SegmentColor = ToDebugColor(DebugSettings.DebugSegmentColor);
	const FColor AnchorNodeColor = ToDebugColor(DebugSettings.DebugAnchorNodeColor);
	const FColor RedirectNodeColor = ToDebugColor(DebugSettings.DebugRedirectNodeColor);
	const FColor AttachmentLinkColor = ToDebugColor(DebugSettings.DebugAttachmentLinkColor);

	if (OwnerActor != nullptr)
	{
		const FVector OwnerLocation = OwnerActor->GetActorLocation();
		DrawDebugCoordinateSystem(
			World,
			OwnerLocation,
			OwnerActor->GetActorRotation(),
			FMath::Max(0.f, DebugSettings.DebugOwnerAxisLength),
			false,
			LifeTime,
			0,
			SegmentThickness);

		DrawDebugSphere(
			World,
			OwnerLocation,
			FMath::Max(1.f, NodeRadius * 0.5f),
			NodeSegments,
			OwnerColor,
			false,
			LifeTime,
			0,
			SegmentThickness);

		if (DebugSettings.bDrawDebugLabels)
		{
			DrawDebugString(
				World,
				OwnerLocation + FVector(0.f, 0.f, NodeRadius * 2.f),
				FString::Printf(
					TEXT("RopeComponent RopeSegments:%d Length:%.1f/%.1f %s"),
					Segments.Num(),
					CurrentRopeLength,
					MaxAllowedRopeLength,
					bIsRopeSolving ? TEXT("Solving") : TEXT("Idle")),
				nullptr,
				OwnerColor,
				LifeTime,
				true);
		}
	}

	for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		const FRayRopeSegment& Segment = Segments[SegmentIndex];
		for (int32 NodeIndex = 1; NodeIndex < Segment.Nodes.Num(); ++NodeIndex)
		{
			DrawDebugLine(
				World,
				Segment.Nodes[NodeIndex - 1].WorldLocation,
				Segment.Nodes[NodeIndex].WorldLocation,
				SegmentColor,
				false,
				LifeTime,
				0,
				SegmentThickness);
		}

		if (DebugSettings.bDrawDebugLabels && Segment.Nodes.Num() > 0)
		{
			DrawDebugString(
				World,
				CalculateSegmentCenter(Segment),
				FString::Printf(
					TEXT("Segment[%d] Nodes:%d Length:%.1f"),
					SegmentIndex,
					Segment.Nodes.Num(),
					FRayRopeSegmentTopology::CalculateSegmentLength(Segment)),
				nullptr,
				SegmentColor,
				LifeTime,
				true);
		}

		for (int32 NodeIndex = 0; NodeIndex < Segment.Nodes.Num(); ++NodeIndex)
		{
			const FRayRopeNode& Node = Segment.Nodes[NodeIndex];
			const FColor NodeColor = Node.NodeType == ERayRopeNodeType::Anchor
				? AnchorNodeColor
				: RedirectNodeColor;

			DrawDebugSphere(
				World,
				Node.WorldLocation,
				NodeRadius,
				NodeSegments,
				NodeColor,
				false,
				LifeTime,
				0,
				SegmentThickness);

			if (DebugSettings.bDrawDebugAttachmentLinks && IsValid(Node.AttachedActor))
			{
				DrawDebugLine(
					World,
					Node.WorldLocation,
					Node.AttachedActor->GetActorLocation(),
					AttachmentLinkColor,
					false,
					LifeTime,
					0,
					AttachmentLinkThickness);
			}

			if (DebugSettings.bDrawDebugLabels)
			{
				DrawDebugString(
					World,
					Node.WorldLocation + FVector(0.f, 0.f, NodeRadius),
					FString::Printf(
						TEXT("S%d N%d %s"),
						SegmentIndex,
						NodeIndex,
						GetRopeNodeTypeName(Node.NodeType)),
					nullptr,
					NodeColor,
					LifeTime,
					true);
			}
		}
	}
}

void FRayRopeDebug::LogRopeState(
	const TCHAR* Context,
	const AActor* OwnerActor,
	const TArray<FRayRopeSegment>& Segments,
	float CurrentRopeLength,
	float MaxAllowedRopeLength,
	bool bIsRopeSolving)
{
	const TCHAR* SafeContext = Context != nullptr ? Context : TEXT("Unknown");
	UE_LOG(
		LogRayRope,
		Log,
		TEXT("[Debug] %s Owner=%s Solving=%s RopeSegments=%d CurrentRopeLength=%.2f MaxAllowedRopeLength=%.2f"),
		SafeContext,
		*GetNameSafe(OwnerActor),
		bIsRopeSolving ? TEXT("true") : TEXT("false"),
		Segments.Num(),
		CurrentRopeLength,
		MaxAllowedRopeLength);

	for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		const FRayRopeSegment& Segment = Segments[SegmentIndex];
		UE_LOG(
			LogRayRope,
			Log,
			TEXT("[Debug] %s Segment[%d] Nodes=%d Length=%.2f"),
			SafeContext,
			SegmentIndex,
			Segment.Nodes.Num(),
			FRayRopeSegmentTopology::CalculateSegmentLength(Segment));

		for (int32 NodeIndex = 0; NodeIndex < Segment.Nodes.Num(); ++NodeIndex)
		{
			const FRayRopeNode& Node = Segment.Nodes[NodeIndex];
			UE_LOG(
				LogRayRope,
				Log,
				TEXT("[Debug] %s Segment[%d].Node[%d] Type=%s Location=%s AttachedActor=%s UseOffset=%s Offset=%s"),
				SafeContext,
				SegmentIndex,
				NodeIndex,
				GetRopeNodeTypeName(Node.NodeType),
				*Node.WorldLocation.ToCompactString(),
				*GetNameSafe(Node.AttachedActor),
				Node.bUseAttachedActorOffset ? TEXT("true") : TEXT("false"),
				*Node.AttachedActorOffset.ToCompactString());
		}
	}
}
