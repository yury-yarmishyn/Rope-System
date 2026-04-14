#include "RayRopeComponent.h"

#include <ThirdParty/ShaderConductor/ShaderConductor/External/DirectXShaderCompiler/include/dxc/DXIL/DxilConstants.h>

#include "AnalyticsEventAttribute.h"
#include "DiffResults.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "VectorUtil.h"
#include "Engine/Engine.h"
#include "RayRopeInterface.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"

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
		if (NewSegments[SegmentIndex].Nodes.Num() < 2)
		{
			continue;
		}
		
		MoveSegment(SegmentIndex, NewSegments);
		WrapSegment(SegmentIndex, NewSegments);
		UnwrapSegment(SegmentIndex, NewSegments);
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
void URayRopeComponent::MoveSegment(int32 SegmentIndex, TArray<FRayRopeSegment>& NewSegments)
{
	FRayRopeSegment& NewSegment = NewSegments[SegmentIndex];
	const int32 NodeCount = NewSegment.Nodes.Num();
	
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

	for (int32 MovePassCount = 0; MovePassCount < MaxMoveIterations; ++MovePassCount)
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

void URayRopeComponent::WrapSegment(int32 SegmentIndex, TArray<FRayRopeSegment>& NewSegments)
{
	FRayRopeSegment& NewSegment = NewSegments[SegmentIndex];
	const int32 NodeCount = NewSegment.Nodes.Num();
	
	TMap<int32, FRayRopeSegment> NodesToAdd;
	
	for (int32 WrapCount = 0; WrapCount < MaxWrapIterations; ++WrapCount)
	{
		for (int32 NodeIndex = 0; NodeIndex < NodeCount - 2; ++NodeIndex)
		{
			FRayRopeNode NodeToAdd;
			FHitResult HitResult = FHitResult();
			
			FRayRopeNode& FirstNode = NewSegment.Nodes[NodeIndex];
			FRayRopeNode& NextNode = NewSegment.Nodes[NodeIndex + 1];
			
			HitResult = TraceLine(FirstNode, NextNode);
			
			if (!HitResult.bBlockingHit)
			{
				continue;
			}
			
			if (!HitResult.GetActor()->GetClass()->ImplementsInterface(URayRopeInterface::StaticClass()))
			{
				FRayRopeNode LastValidLineStart = GetSegments()[SegmentIndex].Nodes[NodeIndex];
				FRayRopeNode LastValidLineEnd = GetSegments()[SegmentIndex].Nodes[NodeIndex + 1];
				FRayRopeNode LastInvalidLineStart = NewSegment.Nodes[NodeIndex];
				FRayRopeNode LastInvalidLineEnd = NewSegment.Nodes[NodeIndex + 1];
				
				BinarySearchCollisionBoundary(
					LastValidLineStart,
					LastValidLineEnd,
					LastInvalidLineStart,
					LastInvalidLineEnd
					);
				
				HitResult = TraceLine(LastInvalidLineStart, LastInvalidLineEnd);
				if (!HitResult.bBlockingHit)
				{
					break;
				}
				
				NodeToAdd = FindRedirectNode(LastValidLineStart, LastValidLineEnd, HitResult);
			}
			else
			{
				NodeToAdd.NodeType = ENodeType::Anchor;
				NodeToAdd.AnchorActor = HitResult.GetActor();
				NodeToAdd.WorldLocation = GetAnchorWorldLocation(NodeToAdd);
			}
			
			TArray<FRayRopeNode> NewNodes;
			NewNodes.Add(NodeToAdd);
			NodesToAdd.Add(NodeIndex, FRayRopeSegment(NewNodes));
		}
	}
	
	ApplyNewNodes(NodesToAdd, NewSegment);
}

void URayRopeComponent::UnwrapSegment(int32 SegmentIndex, TArray<FRayRopeSegment>& NewSegments)
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

FHitResult URayRopeComponent::TraceLine(const FRayRopeNode& StartNode, const FRayRopeNode& EndNode) const
{
	FHitResult HitResult;

	UWorld* World = GetWorld();
	if (!World)
	{
		return HitResult;
	}

	const FVector Start = StartNode.WorldLocation;
	const FVector End = EndNode.WorldLocation;

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnPhysicalMaterial = false;
	
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Reserve(3);

	if (AActor* Owner = GetOwner())
	{
		ActorsToIgnore.AddUnique(Owner);
	}

	if (IsValid(StartNode.AnchorActor))
	{
		ActorsToIgnore.AddUnique(StartNode.AnchorActor);
	}

	if (IsValid(EndNode.AnchorActor))
	{
		ActorsToIgnore.AddUnique(EndNode.AnchorActor);
	}

	QueryParams.AddIgnoredActors(ActorsToIgnore);

	World->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		QueryParams
	);

	return HitResult;
}

void URayRopeComponent::BinarySearchCollisionBoundary(
	FRayRopeNode& ValidLineStart, 
	FRayRopeNode& ValidLineEnd,
	FRayRopeNode& InvalidLineStart,
	FRayRopeNode& InvalidLineEnd) const
{
	FRayRopeNode MidLineStart;
	FRayRopeNode MidLineEnd;
	
	for (int32 i = 0; i < MaxBinarySearchIteration; ++i)
	{
		const bool bStartCloseEnough = 
			FVector::Dist(ValidLineStart.WorldLocation, InvalidLineStart.WorldLocation) <= WrapSolverEpsilon;
		
		const bool bEndCloseEnough =
			FVector::Dist(ValidLineEnd.WorldLocation, InvalidLineEnd.WorldLocation) <= WrapSolverEpsilon;
		
		if (bStartCloseEnough && bEndCloseEnough)
		{
			return;
		}
		
		MidLineStart.WorldLocation =
			(ValidLineStart.WorldLocation + InvalidLineStart.WorldLocation) / 2;
		
		MidLineEnd.WorldLocation =
			(ValidLineEnd.WorldLocation + InvalidLineEnd.WorldLocation) / 2;
		
		FHitResult HitResult = TraceLine(MidLineStart, MidLineEnd);
		if (HitResult.bBlockingHit)
		{
			InvalidLineStart = MidLineStart;
			InvalidLineEnd = MidLineEnd;
		}
		else
		{
			ValidLineStart = MidLineStart;
			ValidLineEnd = MidLineEnd;
		}
	}
}

FRayRopeNode URayRopeComponent::FindRedirectNode(const FRayRopeNode& LastValidLineStart,
	const FRayRopeNode& LastValidLineEnd, const FHitResult& PlaneSource) const
{
	FRayRopeNode RedirectNode;
	RedirectNode.NodeType = ENodeType::Redirect;
	RedirectNode.AnchorActor = nullptr;

	const FVector LineStart = LastValidLineStart.WorldLocation;
	const FVector LineEnd = LastValidLineEnd.WorldLocation;

	const FVector PlanePoint = PlaneSource.ImpactPoint;
	const FVector PlaneNormal = PlaneSource.ImpactNormal;

	const FVector LineDirection = LineEnd - LineStart;
	const float Denominator = FVector::DotProduct(PlaneNormal, LineDirection);

	if (FMath::IsNearlyZero(Denominator))
	{
		const float DistanceToStart = FVector::DistSquared(PlanePoint, LineStart);
		const float DistanceToEnd = FVector::DistSquared(PlanePoint, LineEnd);

		RedirectNode.WorldLocation = DistanceToStart <= DistanceToEnd
			? LineStart
			: LineEnd;

		return RedirectNode;
	}

	float T = FVector::DotProduct(PlanePoint - LineStart, PlaneNormal) / Denominator;

	T = FMath::Clamp(T, 0.f, 1.f);

	RedirectNode.WorldLocation = LineStart + LineDirection * T;
	return RedirectNode;
}

void URayRopeComponent::ApplyNewNodes(TMap<int32, FRayRopeSegment>& NodesToAdd, FRayRopeSegment& InSegment) const
{
	FRayRopeSegment NewSegment = InSegment;
	
	for (int32 NodeIndex = 0; NodeIndex < NewSegment.Nodes.Num(); ++NodeIndex)
	{
		const FRayRopeSegment* NewNodes = NodesToAdd.Find(NodeIndex);
		if (NewNodes)
		{
			continue;
		}
		
		const int32 NodesToAddCount = NewNodes->Nodes.Num();
		for (int32 NodeToAddIndex = 0; NodeToAddIndex < NodesToAddCount - 1; ++NodeToAddIndex)
		{
			FRayRopeNode NodeToAdd = NewNodes->Nodes[NodeToAddIndex];
			NewSegment.Nodes.Insert(NodeToAdd, NodeIndex);
		}
	}
	
	InSegment = NewSegment;
}

void URayRopeComponent::SplitSegmentOnAnchors(int32 NodeIndex, const FRayRopeSegment& NewSegment)
{
	 
}
