#include "RayRopeNodeSynchronizer.h"

#include "RayRopeInterface.h"

namespace
{
bool ImplementsRopeInterface(const AActor* Actor)
{
	return IsValid(Actor) &&
		Actor->GetClass()->ImplementsInterface(URayRopeInterface::StaticClass());
}

void ClearCachedAnchorTarget(FRayRopeNode& Node)
{
	Node.CachedAnchorComponent = nullptr;
	Node.CachedAnchorSocketName = NAME_None;
}

void CacheAnchorTarget(FRayRopeNode& Node)
{
	ClearCachedAnchorTarget(Node);

	USceneComponent* AnchorComponent = IRayRopeInterface::Execute_GetAnchorComponent(Node.AttachedActor);
	if (!IsValid(AnchorComponent))
	{
		return;
	}

	Node.CachedAnchorComponent = AnchorComponent;
	Node.CachedAnchorSocketName = IRayRopeInterface::Execute_GetAnchorSocketName(Node.AttachedActor);
}

FVector GetAnchorWorldLocation(const FRayRopeNode& Node)
{
	if (!IsValid(Node.AttachedActor))
	{
		return Node.WorldLocation;
	}

	if (!IsValid(Node.CachedAnchorComponent))
	{
		return Node.AttachedActor->GetActorLocation();
	}

	return Node.CachedAnchorSocketName != NAME_None &&
		Node.CachedAnchorComponent->DoesSocketExist(Node.CachedAnchorSocketName)
		? Node.CachedAnchorComponent->GetSocketLocation(Node.CachedAnchorSocketName)
		: Node.CachedAnchorComponent->GetComponentLocation();
}

void SyncAnchorNode(FRayRopeNode& Node)
{
	if (Node.NodeType != ERayRopeNodeType::Anchor)
	{
		return;
	}

	if (!IsValid(Node.AttachedActor))
	{
		ClearCachedAnchorTarget(Node);
		return;
	}

	if (!ImplementsRopeInterface(Node.AttachedActor))
	{
		ClearCachedAnchorTarget(Node);
		Node.WorldLocation = Node.AttachedActor->GetActorLocation();
		return;
	}

	CacheAnchorTarget(Node);
	Node.WorldLocation = GetAnchorWorldLocation(Node);
}

void SyncRedirectNode(FRayRopeNode& Node)
{
	if (Node.NodeType != ERayRopeNodeType::Redirect || !IsValid(Node.AttachedActor))
	{
		return;
	}

	if (!Node.bUseAttachedActorOffset)
	{
		FRayRopeNodeSynchronizer::CacheAttachedActorOffset(Node);
	}

	Node.WorldLocation =
		Node.AttachedActor->GetActorTransform().TransformPosition(Node.AttachedActorOffset);
}

void SyncNode(FRayRopeNode& Node)
{
	switch (Node.NodeType)
	{
	case ERayRopeNodeType::Anchor:
		SyncAnchorNode(Node);
		break;

	case ERayRopeNodeType::Redirect:
		SyncRedirectNode(Node);
		break;

	default:
		break;
	}
}
}

void FRayRopeNodeSynchronizer::SyncSegmentNodes(FRayRopeSegment& Segment)
{
	for (FRayRopeNode& Node : Segment.Nodes)
	{
		SyncNode(Node);
	}
}

void FRayRopeNodeSynchronizer::CacheAttachedActorOffset(FRayRopeNode& Node)
{
	if (!IsValid(Node.AttachedActor))
	{
		Node.bUseAttachedActorOffset = false;
		Node.AttachedActorOffset = FVector::ZeroVector;
		return;
	}

	Node.bUseAttachedActorOffset = true;
	Node.AttachedActorOffset =
		Node.AttachedActor->GetActorTransform().InverseTransformPosition(Node.WorldLocation);
}

FRayRopeNode FRayRopeNodeSynchronizer::CreateAnchorNode(AActor* AnchorActor)
{
	FRayRopeNode AnchorNode;
	AnchorNode.NodeType = ERayRopeNodeType::Anchor;
	AnchorNode.AttachedActor = AnchorActor;
	SyncAnchorNode(AnchorNode);
	return AnchorNode;
}
