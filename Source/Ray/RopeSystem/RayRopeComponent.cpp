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
	TArray<FRayRopeSegment> PrevSegments = GetSegments();
	
	SyncAnchors(NewSegments);
	
	for (int32 SegmentIndex = 0; SegmentIndex < NewSegments.Num(); ++SegmentIndex)
	{
		if (NewSegments[SegmentIndex].Nodes.Num() < 2)
		{
			continue;
		}
		
		MoveSegment(SegmentIndex, NewSegments);
		WrapSegment(SegmentIndex, NewSegments, PrevSegments);
		RelaxSegment(SegmentIndex, NewSegments);
		
		SplitSegmentOnAnchors(SegmentIndex, NewSegments);
		PrevSegments = NewSegments;
	}
	
	SetSegments(NewSegments);
}

void URayRopeComponent::SyncAnchors(
	TArray<FRayRopeSegment>& NewSegments) const
{
	for (FRayRopeSegment& Segment : NewSegments)
	{
		const int32 NodeCount = Segment.Nodes.Num();
		if (NodeCount < 2)
		{
			continue;
		}

		FRayRopeNode& FirstAnchor = Segment.Nodes[0];
		FRayRopeNode& LastAnchor = Segment.Nodes.Last();

		if (FirstAnchor.NodeType != ENodeType::Anchor ||
			LastAnchor.NodeType != ENodeType::Anchor)
		{
			continue;
		}

		if (!IsValid(FirstAnchor.AnchorActor) ||
			!IsValid(LastAnchor.AnchorActor))
		{
			continue;
		}

		FirstAnchor.WorldLocation = GetAnchorWorldLocation(FirstAnchor);
		LastAnchor.WorldLocation = GetAnchorWorldLocation(LastAnchor);
	}
}

/* 
 * The move function shifts the points to the desired position until they stop changing.
 * 
 */

void URayRopeComponent::MoveSegment(
	int32 SegmentIndex,
	TArray<FRayRopeSegment>& NewSegments) const
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

	for (int32 MovePassCount = 0; MovePassCount < MaxMoveIterations; ++MovePassCount)
	{
		const bool bForward = (MovePassCount % 2 == 0);
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

void URayRopeComponent::WrapSegment(
	int32 SegmentIndex,
	TArray<FRayRopeSegment>& NewSegments,
	const TArray<FRayRopeSegment>& PrevSegments) const
{
	if (!NewSegments.IsValidIndex(SegmentIndex))
	{
		return;
	}

	FRayRopeSegment& CurrentSegment = NewSegments[SegmentIndex];
	if (CurrentSegment.Nodes.Num() < 2)
	{
		return;
	}

	const FRayRopeSegment& ReferenceSegment =
		PrevSegments.IsValidIndex(SegmentIndex)
			? PrevSegments[SegmentIndex]
			: CurrentSegment;

	const int32 ComparableNodeCount =
		FMath::Min(CurrentSegment.Nodes.Num(), ReferenceSegment.Nodes.Num());

	if (ComparableNodeCount < 2)
	{
		return;
	}

	TMap<int32, FRayRopeSegment> NodesToAdd;

	for (int32 NodeIndex = 0; NodeIndex < ComparableNodeCount - 1; ++NodeIndex)
	{
		FRayRopeNode NodeToAdd;
		if (!TryCreateWrapNode(NodeIndex, CurrentSegment, ReferenceSegment, NodeToAdd))
		{
			continue;
		}

		const int32 InsertIndex = NodeIndex + 1;
		if (!CanInsertWrapNode(InsertIndex, CurrentSegment, NodeToAdd))
		{
			continue;
		}

		FRayRopeSegment SegmentToAdd;
		SegmentToAdd.Nodes.Add(MoveTemp(NodeToAdd));

		NodesToAdd.Add(InsertIndex, MoveTemp(SegmentToAdd));
	}

	if (NodesToAdd.IsEmpty())
	{
		return;
	}

	AddNodesToSegment(NodesToAdd, CurrentSegment);
}

void URayRopeComponent::RelaxSegment(int32 SegmentIndex, TArray<FRayRopeSegment>& NewSegments) const
{
	// WIP
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

FVector URayRopeComponent::GetNodeDesiredWorldLocation(
	int32 NodeIndex,
	const FRayRopeSegment& InSegment) const
{
	const FRayRopeNode& NewNode = InSegment.Nodes[NodeIndex];
	
	if (NewNode.NodeType == ENodeType::Redirect)
	{
		return FindEffectiveRedirect(NodeIndex, InSegment);
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

FVector URayRopeComponent::FindEffectiveRedirect(
	int32 NodeIndex, 
	const FRayRopeSegment& InSegment) const
{
	// WIP
	return InSegment.Nodes[NodeIndex].WorldLocation;
}

void URayRopeComponent::TraceSegmentNodes(
	const FRayRopeSegment& Segment,
	const FRayRopeNode& StartNode,
	const FRayRopeNode& EndNode,
	FHitResult& SurfaceHit) const
{
	SurfaceHit = FHitResult();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RayRopeTrace), true);
	QueryParams.bReturnPhysicalMaterial = false;

	if (AActor* Owner = GetOwner())
	{
		QueryParams.AddIgnoredActor(Owner);
	}

	for (const FRayRopeNode& Node : Segment.Nodes)
	{
		if (Node.NodeType == ENodeType::Anchor && IsValid(Node.AnchorActor))
		{
			QueryParams.AddIgnoredActor(Node.AnchorActor);
		}
	}

	World->LineTraceSingleByChannel(
		SurfaceHit,
		StartNode.WorldLocation,
		EndNode.WorldLocation,
		ECC_Visibility,
		QueryParams
	);
}

bool URayRopeComponent::CanInsertWrapNode(
	int32 InsertIndex,
	const FRayRopeSegment& Segment,
	const FRayRopeNode& Candidate) const
{
	if (!Segment.Nodes.IsValidIndex(InsertIndex - 1) ||
		!Segment.Nodes.IsValidIndex(InsertIndex))
	{
		return false;
	}

	const FVector& PrevLocation = Segment.Nodes[InsertIndex - 1].WorldLocation;
	const FVector& NextLocation = Segment.Nodes[InsertIndex].WorldLocation;

	if (Candidate.WorldLocation.Equals(PrevLocation, WrapSolverEpsilon) ||
		Candidate.WorldLocation.Equals(NextLocation, WrapSolverEpsilon))
	{
		return false;
	}

	for (const FRayRopeNode& ExistingNode : Segment.Nodes)
	{
		const bool bSameAnchor =
			Candidate.NodeType == ENodeType::Anchor &&
			ExistingNode.NodeType == ENodeType::Anchor &&
			ExistingNode.AnchorActor == Candidate.AnchorActor;

		const bool bSameRedirect =
			Candidate.NodeType == ENodeType::Redirect &&
			ExistingNode.NodeType == ENodeType::Redirect &&
			ExistingNode.WorldLocation.Equals(Candidate.WorldLocation, WrapSolverEpsilon);

		if (bSameAnchor || bSameRedirect)
		{
			return false;
		}
	}

	return true;
}

bool URayRopeComponent::TryCreateWrapNode(
	int32 NodeIndex,
	const FRayRopeSegment& CurrentSegment,
	const FRayRopeSegment& ReferenceSegment,
	FRayRopeNode& OutNode) const
{
	if (!CurrentSegment.Nodes.IsValidIndex(NodeIndex) ||
		!CurrentSegment.Nodes.IsValidIndex(NodeIndex + 1))
	{
		return false;
	}

	if (!ReferenceSegment.Nodes.IsValidIndex(NodeIndex) ||
		!ReferenceSegment.Nodes.IsValidIndex(NodeIndex + 1))
	{
		return false;
	}

	const FRayRopeNode& CurrentNode = CurrentSegment.Nodes[NodeIndex];
	const FRayRopeNode& NextNode = CurrentSegment.Nodes[NodeIndex + 1];

	FHitResult SurfaceHit; 
	TraceSegmentNodes(ReferenceSegment, CurrentNode, NextNode, SurfaceHit);
	if (!SurfaceHit.bBlockingHit)
	{
		return false;
	}

	AActor* HitActor = SurfaceHit.GetActor();
	if (!IsValid(HitActor))
	{
		return false;
	}

	if (HitActor->GetClass()->ImplementsInterface(URayRopeInterface::StaticClass()))
	{
		OutNode.NodeType = ENodeType::Anchor;
		OutNode.AnchorActor = HitActor;
		OutNode.WorldLocation = GetAnchorWorldLocation(OutNode);
		return true;
	}

	FRayRopeNode LastValidLineStart = ReferenceSegment.Nodes[NodeIndex];
	FRayRopeNode LastValidLineEnd = ReferenceSegment.Nodes[NodeIndex + 1];

	FRayRopeNode LastInvalidLineStart = CurrentSegment.Nodes[NodeIndex];
	FRayRopeNode LastInvalidLineEnd = CurrentSegment.Nodes[NodeIndex + 1];

	BinarySearchCollisionBoundary(
		ReferenceSegment,
		LastValidLineStart,
		LastValidLineEnd,
		LastInvalidLineStart,
		LastInvalidLineEnd
	);

	TraceSegmentNodes(ReferenceSegment, LastInvalidLineStart, LastInvalidLineEnd, SurfaceHit);
	if (!SurfaceHit.bBlockingHit)
	{
		return false;
	}

	CalculateRedirectNode(
		LastValidLineStart,
		LastValidLineEnd,
		SurfaceHit,
		OutNode
	);

	return true;
}


void URayRopeComponent::BinarySearchCollisionBoundary(
	const FRayRopeSegment& Segment,
	FRayRopeNode& ValidLineStart,
	FRayRopeNode& ValidLineEnd,
	FRayRopeNode& InvalidLineStart,
	FRayRopeNode& InvalidLineEnd) const
{
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

		FRayRopeNode MidLineStart = ValidLineStart;
		FRayRopeNode MidLineEnd = ValidLineEnd;

		MidLineStart.WorldLocation =
			(ValidLineStart.WorldLocation + InvalidLineStart.WorldLocation) * 0.5f;

		MidLineEnd.WorldLocation =
			(ValidLineEnd.WorldLocation + InvalidLineEnd.WorldLocation) * 0.5f;

		FHitResult SurfaceHit;
		TraceSegmentNodes(Segment, MidLineStart, MidLineEnd, SurfaceHit);

		if (SurfaceHit.bBlockingHit)
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

void URayRopeComponent::CalculateRedirectNode(
	const FRayRopeNode& LastValidLineStart,
	const FRayRopeNode& LastValidLineEnd,
	const FHitResult& SurfaceHit,
	FRayRopeNode& RedirectNode) const
{
	RedirectNode.NodeType = ENodeType::Redirect;
	RedirectNode.AnchorActor = nullptr;

	const FVector LineStart = LastValidLineStart.WorldLocation;
	const FVector LineEnd = LastValidLineEnd.WorldLocation;

	const FVector PlanePoint = SurfaceHit.ImpactPoint;
	const FVector PlaneNormal = SurfaceHit.ImpactNormal;

	const FVector LineDirection = LineEnd - LineStart;
	const float Denominator = FVector::DotProduct(PlaneNormal, LineDirection);

	if (FMath::IsNearlyZero(Denominator))
	{
		const float DistanceToStart = FVector::DistSquared(PlanePoint, LineStart);
		const float DistanceToEnd = FVector::DistSquared(PlanePoint, LineEnd);

		RedirectNode.WorldLocation = DistanceToStart <= DistanceToEnd
			? LineStart
			: LineEnd;
	}
	else
	{
		float T = FVector::DotProduct(PlanePoint - LineStart, PlaneNormal) / Denominator;
		T = FMath::Clamp(T, 0.f, 1.f);

		RedirectNode.WorldLocation = LineStart + LineDirection * T;
	}

	RedirectNode.WorldLocation += PlaneNormal * RopePhysicalRadius;
}

void URayRopeComponent::AddNodesToSegment(
	const TMap<int32, FRayRopeSegment>& NodesToAdd,
	FRayRopeSegment& NewSegment) const
{
	if (NodesToAdd.IsEmpty() || NewSegment.Nodes.Num() < 2)
	{
		return;
	}

	for (int32 NodeIndex = NewSegment.Nodes.Num() - 1; NodeIndex > 0; --NodeIndex)
	{
		const FRayRopeSegment* NodesSegment = NodesToAdd.Find(NodeIndex);
		if (!NodesSegment || NodesSegment->Nodes.IsEmpty())
		{
			continue;
		}

		NewSegment.Nodes.Insert(NodesSegment->Nodes, NodeIndex);
	}
}

void URayRopeComponent::SplitSegmentOnAnchors(
	int32 SegmentIndex,
	TArray<FRayRopeSegment>& NewSegments) const
{
	if (!NewSegments.IsValidIndex(SegmentIndex))
	{
		return;
	}
	
	// Removed &
	const TArray<FRayRopeNode> Nodes = NewSegments[SegmentIndex].Nodes;
	const int32 NodeCount = Nodes.Num();

	if (NodeCount < 2)
	{
		return;
	}

	TArray<FRayRopeSegment> SplitSegments;
	int32 StartIndex = 0;

	for (int32 i = 1; i < NodeCount; ++i)
	{
		const bool bShouldSplit =
			i == NodeCount - 1 ||
			Nodes[i].NodeType == ENodeType::Anchor;

		if (!bShouldSplit)
		{
			continue;
		}

		FRayRopeSegment NewSegment;
		NewSegment.Nodes.Append(&Nodes[StartIndex], i - StartIndex + 1);
		SplitSegments.Add(MoveTemp(NewSegment));

		StartIndex = i;
	}

	if (SplitSegments.Num() <= 1)
	{
		return;
	}

	NewSegments.RemoveAt(SegmentIndex);
	NewSegments.Insert(SplitSegments, SegmentIndex);
}