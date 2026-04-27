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
	if (!IsValid(Node.AttachActor))
	{
		return;
	}

	switch (Node.NodeType)
	{
	case ENodeType::Anchor:
		Node.WorldLocation = GetAnchorWorldLocation(Node);
		break;

	case ENodeType::Redirect:
		SyncRedirectNode(Node);
		break;

	default:
		break;
	}
}

void FRayRopeNodeResolver::SyncRedirectNode(FRayRopeNode& Node)
{
	if (Node.NodeType != ENodeType::Redirect || !IsValid(Node.AttachActor))
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

FVector FRayRopeNodeResolver::GetAnchorWorldLocation(const FRayRopeNode& Node)
{
	if (!IsValid(Node.AttachActor))
	{
		return Node.WorldLocation;
	}

	if (!Node.AttachActor->GetClass()->ImplementsInterface(URayRopeInterface::StaticClass()))
	{
		return Node.AttachActor->GetActorLocation();
	}

	USceneComponent* AnchorComponent = IRayRopeInterface::Execute_GetAnchorComponent(Node.AttachActor);
	if (!IsValid(AnchorComponent))
	{
		return Node.AttachActor->GetActorLocation();
	}

	const FName SocketName = IRayRopeInterface::Execute_GetAnchorSocketName(Node.AttachActor);
	return SocketName != NAME_None && AnchorComponent->DoesSocketExist(SocketName)
		? AnchorComponent->GetSocketLocation(SocketName)
		: AnchorComponent->GetComponentLocation();
}

FRayRopeNode FRayRopeNodeResolver::CreateAnchorNode(AActor* AnchorActor)
{
	FRayRopeNode AnchorNode;
	AnchorNode.NodeType = ENodeType::Anchor;
	AnchorNode.AttachActor = AnchorActor;
	AnchorNode.WorldLocation = GetAnchorWorldLocation(AnchorNode);
	return AnchorNode;
}
