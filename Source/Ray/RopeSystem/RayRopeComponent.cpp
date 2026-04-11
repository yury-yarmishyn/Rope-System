#include "RayRopeComponent.h"

#include "DiffResults.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "VectorUtil.h"
#include "Engine/Engine.h"
#include "RayRopeInterface.h"

URayRopeComponent::URayRopeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URayRopeComponent::BeginPlay()
{
	Super::BeginPlay();
}

void URayRopeComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	SolveRope();
}

const TArray<FRayRopeSegment>& URayRopeComponent::GetSegments() const
{
	return Segments;
}

void URayRopeComponent::SetSegments(TArray<FRayRopeSegment> NewSegments)
{
	Segments = NewSegments;
	
	// Example: You can bind your rope drawing component to OnSegmentSet.
	OnSegmentsSet.Broadcast();
}

void URayRopeComponent::SolveRope()
{
	/*
	 * Segments is the source of visual data, to avoid rope jittering better change them once a tick.
	 * Because of this we're copying Segments and constructing new array inside the function.
	 * 
	 * Q: Why copy instead of creating a segment from scratch?
	 * A: Because we want to follow the logic of the rope, not the shortest path. 
	 * Therefore, for the rope to function properly, we need to know its previous position.
	 * No, permanent accessing Segments during calculation is not safe, as they may be modified.
	 */
	TArray<FRayRopeSegment> NewSegments = GetSegments();
	
	for (int SegmentIndex = 0; SegmentIndex < NewSegments.Num(); ++SegmentIndex)
	{
		MovePass(SegmentIndex, NewSegments);
		WrapPass(SegmentIndex, NewSegments);
		UnwrapPass(SegmentIndex, NewSegments);
	}
	
	SetSegments(NewSegments);
}

void URayRopeComponent::MoveNode(int32 NodeIndex, FRayRopeSegment& NewSegment, bool& bAnyNodeChanged) const
{
	const FRayRopeNode OldNode = NewSegment.Nodes[NodeIndex];
	FRayRopeNode& NewNode = NewSegment.Nodes[NodeIndex];
			
	// Getting new WorldLocation.
	NewNode.WorldLocation = GetNodeDesiredWorldLocation(NodeIndex, NewSegment);

	if (!NewNode.WorldLocation.Equals(OldNode.WorldLocation, MoveSolverEpsilon))
	{
		bAnyNodeChanged = true;
	}
}

/* 
 * The move function shifts the points to the desired position until they stop changing.
 * 
 */
void URayRopeComponent::MovePass(int32 SegmentIndex, TArray<FRayRopeSegment>& NewSegments)
{
	if (!NewSegments.IsValidIndex(SegmentIndex))
	{
		return;
	}

	FRayRopeSegment& NewSegment = NewSegments[SegmentIndex];
	const int32 NodeCount = NewSegment.Nodes.Num();
	if (NodeCount < 2)
	{
		return;
	}
	
	// Updating Anchors first.
	FRayRopeNode& FirstAnchor = NewSegment.Nodes[0];
	FRayRopeNode& LastAnchor = NewSegment.Nodes.Last();
	
	if (FirstAnchor.NodeType != ENodeType::Anchor || LastAnchor.NodeType != ENodeType::Anchor)
	{
		return;
	}
	
	if (!IsValid(FirstAnchor.AnchorActor) || !IsValid(LastAnchor.AnchorActor))
	{
		return;
	}
	
	FirstAnchor.WorldLocation = GetAnchorWorldLocation(FirstAnchor);
	LastAnchor.WorldLocation = GetAnchorWorldLocation(LastAnchor);
	
	// Updating all nodes between anchors.

	for (int32 MovePassCount = 0; MovePassCount < MovePassMaxIterations; ++MovePassCount)
	{
		// Changing the direction of the iterations speeds up convergence on a large number of nodes.
		const bool bForward = (MovePassCount % 2 == 0);
		// Continue iterating while at least one internal node is still changing.
		bool bAnyNodeChanged = false;

		if (bForward)
		{
			for (int32 NodeIndex = 1; NodeIndex < NodeCount - 1; ++NodeIndex)
			{
				MoveNode(NodeIndex, NewSegment, bAnyNodeChanged);
			}
		}
		else
		{
			for (int32 NodeIndex = NodeCount - 2; NodeIndex > 0; --NodeIndex)
			{
				MoveNode(NodeIndex, NewSegment, bAnyNodeChanged);
			}
		}
		
		if (!bAnyNodeChanged)
		{
			break;
		}
	}
}

void URayRopeComponent::WrapPass(int32 SegmentIndex, TArray<FRayRopeSegment>& NewSegments)
{
	// WIP
}

void URayRopeComponent::UnwrapPass(int32 SegmentIndex, TArray<FRayRopeSegment>& NewSegments)
{
	// WIP
}


FVector URayRopeComponent::GetNodeDesiredWorldLocation(
	int32 NodeIndex,
	const FRayRopeSegment& NewSegment) const
{
	const FRayRopeNode& NewNode = NewSegment.Nodes[NodeIndex];
	
	if (NewNode.NodeType == ENodeType::Redirect)
	{
		return FindEffectiveRedirection(NodeIndex, NewSegment);
	}
	
	return NewNode.WorldLocation;
}


FVector URayRopeComponent::GetAnchorWorldLocation(const FRayRopeNode& Node) const
{
	if (!IsValid(Node.AnchorActor))
	{
		return Node.WorldLocation;
	}

	if (!Node.AnchorActor->GetClass()->ImplementsInterface(URayRopeInterface::StaticClass()))
	{
		return Node.AnchorActor->GetActorLocation();
	}

	USceneComponent* AnchorComponent = IRayRopeInterface::Execute_GetAnchorComponent(Node.AnchorActor);
	if (!IsValid(AnchorComponent))
	{
		return Node.AnchorActor->GetActorLocation();
	}

	const FName SocketName = IRayRopeInterface::Execute_GetAnchorSocketName(Node.AnchorActor);

	if (SocketName == NAME_None || !AnchorComponent->DoesSocketExist(SocketName))
	{
		return AnchorComponent->GetComponentLocation();
	}

	return AnchorComponent->GetSocketLocation(SocketName);
}

FVector URayRopeComponent::FindEffectiveRedirection(
	int32 NodeIndex, 
	const FRayRopeSegment& NewSegment) const
{
	// WIP
	return NewSegment.Nodes[NodeIndex].WorldLocation;
}
