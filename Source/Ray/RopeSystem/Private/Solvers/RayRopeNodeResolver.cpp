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

	if (!IsValid(Node.AttachedActor))
	{
		Node.CachedAnchorComponent = nullptr;
		Node.CachedAnchorSocketName = NAME_None;
		return;
	}

	if (!Node.AttachedActor->GetClass()->ImplementsInterface(URayRopeInterface::StaticClass()))
	{
		Node.CachedAnchorComponent = nullptr;
		Node.CachedAnchorSocketName = NAME_None;
		Node.WorldLocation = Node.AttachedActor->GetActorLocation();
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
	if (Node.NodeType != ERayRopeNodeType::Redirect || !IsValid(Node.AttachedActor))
	{
		return;
	}

	if (!Node.bUseAttachedActorOffset)
	{
		CacheAttachedActorOffset(Node);
	}

	Node.WorldLocation =
		Node.AttachedActor->GetActorTransform().TransformPosition(Node.AttachedActorOffset);
}

void FRayRopeNodeResolver::CacheAttachedActorOffset(FRayRopeNode& Node)
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

void FRayRopeNodeResolver::CacheAnchorTarget(FRayRopeNode& Node)
{
	Node.CachedAnchorComponent = nullptr;
	Node.CachedAnchorSocketName = NAME_None;

	if (!IsValid(Node.AttachedActor) ||
		!Node.AttachedActor->GetClass()->ImplementsInterface(URayRopeInterface::StaticClass()))
	{
		return;
	}

	USceneComponent* AnchorComponent = IRayRopeInterface::Execute_GetAnchorComponent(Node.AttachedActor);
	if (!IsValid(AnchorComponent))
	{
		return;
	}

	Node.CachedAnchorComponent = AnchorComponent;
	Node.CachedAnchorSocketName = IRayRopeInterface::Execute_GetAnchorSocketName(Node.AttachedActor);
}

FVector FRayRopeNodeResolver::GetAnchorWorldLocation(const FRayRopeNode& Node)
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

FRayRopeNode FRayRopeNodeResolver::CreateAnchorNode(AActor* AnchorActor)
{
	FRayRopeNode AnchorNode;
	AnchorNode.NodeType = ERayRopeNodeType::Anchor;
	AnchorNode.AttachedActor = AnchorActor;
	SyncAnchorNode(AnchorNode);
	return AnchorNode;
}
