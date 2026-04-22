#include "RayRopeComponent.h"

#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
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
	Segments = MoveTemp(NewSegments);
	OnSegmentsSet.Broadcast();
}

void URayRopeComponent::SolveRope()
{
	for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		if (!Segments.IsValidIndex(SegmentIndex))
		{
			continue;
		}

		FRayRopeSegment& Segment = Segments[SegmentIndex];
		if (Segment.Nodes.Num() < 2)
		{
			continue;
		}

		const FRayRopeSegment ReferenceSegment = Segment;
		SyncSegmentAnchors(Segment);
		MoveSegment(Segment);
		WrapSegment(Segment, ReferenceSegment);
		RelaxSegment(Segment);
		SplitSegmentOnAnchors(SegmentIndex);
	}

	OnSegmentsSet.Broadcast();
}

void URayRopeComponent::SyncSegmentAnchors(FRayRopeSegment& Segment) const
{
	const int32 NodeCount = Segment.Nodes.Num();
	if (NodeCount < 2)
	{
		return;
	}

	FRayRopeNode& FirstAnchor = Segment.Nodes[0];
	FRayRopeNode& LastAnchor = Segment.Nodes.Last();

	if (FirstAnchor.NodeType != ENodeType::Anchor ||
		LastAnchor.NodeType != ENodeType::Anchor)
	{
		return;
	}

	if (!IsValid(FirstAnchor.AnchorActor) ||
		!IsValid(LastAnchor.AnchorActor))
	{
		return;
	}

	FirstAnchor.WorldLocation = GetAnchorWorldLocation(FirstAnchor);
	LastAnchor.WorldLocation = GetAnchorWorldLocation(LastAnchor);
}

void URayRopeComponent::MoveSegment(FRayRopeSegment& Segment) const
{
	// Reserved for the next solver step.
}

void URayRopeComponent::WrapSegment(
	FRayRopeSegment& Segment,
	const FRayRopeSegment& ReferenceSegment) const
{
	const int32 ComparableNodeCount =
		FMath::Min(Segment.Nodes.Num(), ReferenceSegment.Nodes.Num());

	if (ComparableNodeCount < 2)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RayRopeTrace), true);
	BuildTraceQueryParams(Segment, QueryParams);

	TArray<int32> InsertIndices;
	TArray<FRayRopeNode> PendingNodes;
	InsertIndices.Reserve(ComparableNodeCount - 1);
	PendingNodes.Reserve(ComparableNodeCount - 1);

	for (int32 NodeIndex = 0; NodeIndex < ComparableNodeCount - 1; ++NodeIndex)
	{
		FRayRopeNode NodeToAdd;
		if (!TryCreateWrapNode(NodeIndex, Segment, ReferenceSegment, QueryParams, NodeToAdd))
		{
			continue;
		}

		const int32 InsertIndex = NodeIndex + 1;
		if (!CanInsertWrapNode(InsertIndex, Segment, NodeToAdd, PendingNodes))
		{
			continue;
		}

		InsertIndices.Add(InsertIndex);
		PendingNodes.Add(MoveTemp(NodeToAdd));
	}

	for (int32 PendingIndex = PendingNodes.Num() - 1; PendingIndex >= 0; --PendingIndex)
	{
		Segment.Nodes.Insert(MoveTemp(PendingNodes[PendingIndex]), InsertIndices[PendingIndex]);
	}
}

void URayRopeComponent::RelaxSegment(FRayRopeSegment& Segment) const
{
	if (Segment.Nodes.Num() < 3)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(RayRopeRelaxTrace), true);
	BuildTraceQueryParams(Segment, QueryParams);

	int32 NodeIndex = 1;
	while (NodeIndex < Segment.Nodes.Num() - 1)
	{
		const FRayRopeNode& CurrentNode = Segment.Nodes[NodeIndex];
		if (CurrentNode.NodeType == ENodeType::Anchor)
		{
			++NodeIndex;
			continue;
		}

		const FRayRopeNode& PrevNode = Segment.Nodes[NodeIndex - 1];
		const FRayRopeNode& NextNode = Segment.Nodes[NodeIndex + 1];
		if (CanRemoveRelaxNode(PrevNode, CurrentNode, NextNode, QueryParams))
		{
			Segment.Nodes.RemoveAt(NodeIndex, 1, EAllowShrinking::No);
			continue;
		}

		++NodeIndex;
	}
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

void URayRopeComponent::BuildTraceQueryParams(
	const FRayRopeSegment& Segment,
	FCollisionQueryParams& QueryParams) const
{
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
}

void URayRopeComponent::TraceRopeLine(
	const FCollisionQueryParams& QueryParams,
	const FVector& StartLocation,
	const FVector& EndLocation,
	FHitResult& SurfaceHit) const
{
	SurfaceHit = FHitResult();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	World->LineTraceSingleByChannel(
		SurfaceHit,
		StartLocation,
		EndLocation,
		ECC_Visibility,
		QueryParams
	);
}

bool URayRopeComponent::CanInsertWrapNode(
	int32 InsertIndex,
	const FRayRopeSegment& Segment,
	const FRayRopeNode& Candidate,
	const TArray<FRayRopeNode>& PendingNodes) const
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
		if (AreEquivalentWrapNodes(ExistingNode, Candidate))
		{
			return false;
		}
	}

	for (const FRayRopeNode& PendingNode : PendingNodes)
	{
		if (AreEquivalentWrapNodes(PendingNode, Candidate))
		{
			return false;
		}
	}

	return true;
}

bool URayRopeComponent::AreEquivalentWrapNodes(
	const FRayRopeNode& FirstNode,
	const FRayRopeNode& SecondNode) const
{
	const bool bSameAnchor =
		FirstNode.NodeType == ENodeType::Anchor &&
		SecondNode.NodeType == ENodeType::Anchor &&
		FirstNode.AnchorActor == SecondNode.AnchorActor;

	const bool bSameRedirect =
		FirstNode.NodeType == ENodeType::Redirect &&
		SecondNode.NodeType == ENodeType::Redirect &&
		FirstNode.WorldLocation.Equals(SecondNode.WorldLocation, WrapSolverEpsilon);

	return bSameAnchor || bSameRedirect;
}

bool URayRopeComponent::TryCreateWrapNode(
	int32 NodeIndex,
	const FRayRopeSegment& CurrentSegment,
	const FRayRopeSegment& ReferenceSegment,
	const FCollisionQueryParams& QueryParams,
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
	TraceRopeLine(
		QueryParams,
		CurrentNode.WorldLocation,
		NextNode.WorldLocation,
		SurfaceHit);
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
		LastValidLineStart,
		LastValidLineEnd,
		LastInvalidLineStart,
		LastInvalidLineEnd,
		QueryParams
	);

	TraceRopeLine(
		QueryParams,
		LastInvalidLineStart.WorldLocation,
		LastInvalidLineEnd.WorldLocation,
		SurfaceHit);
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

bool URayRopeComponent::CanRemoveRelaxNode(
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FCollisionQueryParams& QueryParams) const
{
	if (!GetWorld())
	{
		return false;
	}

	FHitResult SurfaceHit;
	TraceRopeLine(QueryParams, PrevNode.WorldLocation, NextNode.WorldLocation, SurfaceHit);
	if (SurfaceHit.bBlockingHit)
	{
		return false;
	}

	const FVector FirstDirection = CurrentNode.WorldLocation - PrevNode.WorldLocation;
	const FVector SecondDirection = NextNode.WorldLocation - CurrentNode.WorldLocation;

	if (FirstDirection.IsNearlyZero(RelaxSolverEpsilon) ||
		SecondDirection.IsNearlyZero(RelaxSolverEpsilon))
	{
		return true;
	}

	const FVector FirstNormal = FirstDirection.GetSafeNormal();
	const FVector SecondNormal = SecondDirection.GetSafeNormal();

	if (FVector::CrossProduct(FirstNormal, SecondNormal).SizeSquared() <=
		FMath::Square(RelaxCollinearEpsilon))
	{
		return true;
	}

	const FVector LineDirection = NextNode.WorldLocation - PrevNode.WorldLocation;
	const float T = FVector::DotProduct(
		CurrentNode.WorldLocation - PrevNode.WorldLocation,
		LineDirection) / LineDirection.SizeSquared();
	const FVector ClosestPoint = PrevNode.WorldLocation + LineDirection * T;

	if (CurrentNode.WorldLocation.Equals(ClosestPoint, RelaxSolverEpsilon))
	{
		return true;
	}

	TraceRopeLine(QueryParams, CurrentNode.WorldLocation, ClosestPoint, SurfaceHit);

	return !SurfaceHit.bBlockingHit;
}

void URayRopeComponent::BinarySearchCollisionBoundary(
	FRayRopeNode& ValidLineStart,
	FRayRopeNode& ValidLineEnd,
	FRayRopeNode& InvalidLineStart,
	FRayRopeNode& InvalidLineEnd,
	const FCollisionQueryParams& QueryParams) const
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
		TraceRopeLine(
			QueryParams,
			MidLineStart.WorldLocation,
			MidLineEnd.WorldLocation,
			SurfaceHit);

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

void URayRopeComponent::SplitSegmentOnAnchors(int32 SegmentIndex)
{
	if (!Segments.IsValidIndex(SegmentIndex))
	{
		return;
	}

	const TArray<FRayRopeNode>& Nodes = Segments[SegmentIndex].Nodes;
	const int32 NodeCount = Nodes.Num();
	if (NodeCount < 3)
	{
		return;
	}

	TArray<FRayRopeSegment> SplitSegments;
	int32 StartIndex = 0;
	for (int32 NodeIndex = 1; NodeIndex < NodeCount; ++NodeIndex)
	{
		const bool bShouldSplit =
			NodeIndex == NodeCount - 1 ||
			Nodes[NodeIndex].NodeType == ENodeType::Anchor;

		if (!bShouldSplit)
		{
			continue;
		}

		const int32 NewNodeCount = NodeIndex - StartIndex + 1;
		FRayRopeSegment NewSegment;
		NewSegment.Nodes.Reserve(NewNodeCount);
		NewSegment.Nodes.Append(&Nodes[StartIndex], NewNodeCount);
		SplitSegments.Add(MoveTemp(NewSegment));

		StartIndex = NodeIndex;
	}

	if (SplitSegments.Num() <= 1)
	{
		return;
	}

	const int32 SegmentCountAfterSplit = Segments.Num() + SplitSegments.Num() - 1;
	Segments.RemoveAt(SegmentIndex, 1, EAllowShrinking::No);
	Segments.Reserve(SegmentCountAfterSplit);

	for (int32 SplitIndex = 0; SplitIndex < SplitSegments.Num(); ++SplitIndex)
	{
		Segments.Insert(MoveTemp(SplitSegments[SplitIndex]), SegmentIndex + SplitIndex);
	}
}
