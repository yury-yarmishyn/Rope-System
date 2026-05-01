#include "RayRopeNodeResolver.h"

#include "RayRopeInterface.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

void FRayRopeNodeResolver::SyncSegmentNodes(FRayRopeSegment& Segment)
{
	for (FRayRopeNode& Node : Segment.Nodes)
	{
		SyncNode(Node);
	}
}

void FRayRopeNodeResolver::SyncNode(FRayRopeNode& Node)
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

void FRayRopeNodeResolver::SyncAnchorNode(FRayRopeNode& Node)
{
	if (Node.NodeType != ERayRopeNodeType::Anchor)
	{
		return;
	}

	if (!IsValid(Node.AttachActor))
	{
		Node.CachedAnchorComponent = nullptr;
		Node.CachedAnchorSocketName = NAME_None;
		return;
	}

	if (!Node.AttachActor->GetClass()->ImplementsInterface(URayRopeInterface::StaticClass()))
	{
		Node.CachedAnchorComponent = nullptr;
		Node.CachedAnchorSocketName = NAME_None;
		Node.WorldLocation = Node.AttachActor->GetActorLocation();
		return;
	}

	if (!IsValid(Node.CachedAnchorComponent))
	{
		CacheAnchorTarget(Node);
	}

	Node.WorldLocation = GetAnchorWorldLocation(Node);
}

void FRayRopeNodeResolver::SyncRedirectNode(FRayRopeNode& Node)
{
	if (Node.NodeType != ERayRopeNodeType::Redirect || !IsValid(Node.AttachActor))
	{
		return;
	}

	if (!Node.bUseAttachActorOffset)
	{
		CacheAttachActorOffset(Node);
	}

	Node.WorldLocation =
		Node.AttachActor->GetActorTransform().TransformPosition(Node.AttachActorOffset);
}

void FRayRopeNodeResolver::CacheAttachActorOffset(FRayRopeNode& Node)
{
	if (!IsValid(Node.AttachActor))
	{
		Node.bUseAttachActorOffset = false;
		Node.AttachActorOffset = FVector::ZeroVector;
		return;
	}

	Node.bUseAttachActorOffset = true;
	Node.AttachActorOffset =
		Node.AttachActor->GetActorTransform().InverseTransformPosition(Node.WorldLocation);
}

void FRayRopeNodeResolver::CacheAnchorTarget(FRayRopeNode& Node)
{
	Node.CachedAnchorComponent = nullptr;
	Node.CachedAnchorSocketName = NAME_None;

	if (!IsValid(Node.AttachActor) ||
		!Node.AttachActor->GetClass()->ImplementsInterface(URayRopeInterface::StaticClass()))
	{
		return;
	}

	USceneComponent* AnchorComponent = IRayRopeInterface::Execute_GetAnchorComponent(Node.AttachActor);
	if (!IsValid(AnchorComponent))
	{
		return;
	}

	Node.CachedAnchorComponent = AnchorComponent;
	Node.CachedAnchorSocketName = IRayRopeInterface::Execute_GetAnchorSocketName(Node.AttachActor);
}

FVector FRayRopeNodeResolver::GetAnchorWorldLocation(const FRayRopeNode& Node)
{
	if (!IsValid(Node.AttachActor))
	{
		return Node.WorldLocation;
	}

	if (!IsValid(Node.CachedAnchorComponent))
	{
		return Node.AttachActor->GetActorLocation();
	}

	return Node.CachedAnchorSocketName != NAME_None &&
		Node.CachedAnchorComponent->DoesSocketExist(Node.CachedAnchorSocketName)
		? Node.CachedAnchorComponent->GetSocketLocation(Node.CachedAnchorSocketName)
		: Node.CachedAnchorComponent->GetComponentLocation();
}

FRayRopeNode FRayRopeNodeResolver::CreateAnchorNode(AActor* AnchorActor)
{
	FRayRopeNode AnchorNode;
	AnchorNode.NodeType = ERayRopeNodeType::Anchor;
	AnchorNode.AttachActor = AnchorActor;
	SyncAnchorNode(AnchorNode);
	return AnchorNode;
}
