#include "RayRopeComponent.h"

#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RayRopeInterface.h"

URayRopeComponent::URayRopeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	PrimaryComponentTick.EndTickGroup = TG_PostPhysics;
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
		FRayRopeSegment& Segment = Segments[SegmentIndex];
		if (Segment.Nodes.Num() < 2)
		{
			continue;
		}

		const FRayRopeSegment ReferenceSegment = Segment;
		SyncSegmentAnchors(Segment);
		SyncAttachedRedirectNodes(Segment);
		MoveSegment(Segment);
		WrapSegment(Segment, ReferenceSegment);
		RelaxSegment(Segment);
		SplitSegmentOnAnchors(SegmentIndex);
	}

	OnSegmentsSet.Broadcast();
}

void URayRopeComponent::SyncSegmentAnchors(FRayRopeSegment& Segment) const
{
	if (Segment.Nodes.Num() < 2)
	{
		return;
	}

	FRayRopeNode& FirstAnchor = Segment.Nodes[0];
	FRayRopeNode& LastAnchor = Segment.Nodes.Last();

	if (FirstAnchor.NodeType != ENodeType::Anchor ||
		LastAnchor.NodeType != ENodeType::Anchor ||
		!IsValid(FirstAnchor.AttachActor) ||
		!IsValid(LastAnchor.AttachActor))
	{
		return;
	}

	FirstAnchor.WorldLocation = GetAnchorWorldLocation(FirstAnchor);
	LastAnchor.WorldLocation = GetAnchorWorldLocation(LastAnchor);
}

void URayRopeComponent::SyncAttachedRedirectNodes(FRayRopeSegment& Segment) const
{
	for (FRayRopeNode& Node : Segment.Nodes)
	{
		if (Node.NodeType != ENodeType::Redirect || !IsValid(Node.AttachActor))
		{
			continue;
		}

		if (!Node.bUseAttachActorOffset)
		{
			Node.bUseAttachActorOffset = true;
			Node.AttachActorOffset =
				Node.AttachActor->GetActorTransform().InverseTransformPosition(Node.WorldLocation);
		}

		Node.WorldLocation =
			Node.AttachActor->GetActorTransform().TransformPosition(Node.AttachActorOffset);
	}
}

void URayRopeComponent::MoveSegment(FRayRopeSegment& Segment) const
{
	static_cast<void>(Segment);
	// Placeholder for future segment movement or constraint solving logic.
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
		TArray<FRayRopeNode> NewNodes;
		if (!BuildWrapNodes(NodeIndex, Segment, ReferenceSegment, QueryParams, NewNodes))
		{
			continue;
		}

		const int32 InsertIndex = NodeIndex + 1;
		for (FRayRopeNode& Node : NewNodes)
		{
			if (!CanInsertWrapNode(InsertIndex, Segment, Node, PendingNodes))
			{
				continue;
			}

			InsertIndices.Add(InsertIndex);
			PendingNodes.Add(MoveTemp(Node));
		}
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

	for (int32 NodeIndex = 1; NodeIndex < Segment.Nodes.Num() - 1;)
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

bool URayRopeComponent::BuildWrapNodes(
	int32 NodeIndex,
	const FRayRopeSegment& CurrentSegment,
	const FRayRopeSegment& ReferenceSegment,
	const FCollisionQueryParams& QueryParams,
	TArray<FRayRopeNode>& OutNodes) const
{
	OutNodes.Reset();

	if (!CurrentSegment.Nodes.IsValidIndex(NodeIndex) ||
		!CurrentSegment.Nodes.IsValidIndex(NodeIndex + 1) ||
		!ReferenceSegment.Nodes.IsValidIndex(NodeIndex) ||
		!ReferenceSegment.Nodes.IsValidIndex(NodeIndex + 1))
	{
		return false;
	}

	const FRayRopeNode& CurrentNode = CurrentSegment.Nodes[NodeIndex];
	const FRayRopeNode& NextNode = CurrentSegment.Nodes[NodeIndex + 1];

	FHitResult FrontSurfaceHit;
	if (!TryTraceBlockingHit(
		QueryParams,
		CurrentNode.WorldLocation,
		NextNode.WorldLocation,
		FrontSurfaceHit))
	{
		return false;
	}

	AActor* HitActor = FrontSurfaceHit.GetActor();
	if (!IsValid(HitActor))
	{
		return false;
	}

	if (HitActor->GetClass()->ImplementsInterface(URayRopeInterface::StaticClass()))
	{
		OutNodes.Add(CreateAnchorNode(HitActor));
		return true;
	}

	const FRayRopeNode& ReferenceStart = ReferenceSegment.Nodes[NodeIndex];
	const FRayRopeNode& ReferenceEnd = ReferenceSegment.Nodes[NodeIndex + 1];

	FHitResult ReferenceHit;
	if (TryTraceBlockingHit(
		QueryParams,
		ReferenceStart.WorldLocation,
		ReferenceEnd.WorldLocation,
		ReferenceHit))
	{
		FHitResult BackSurfaceHit;
		const bool bHasBackHit = TryTraceBlockingHit(
			QueryParams,
			NextNode.WorldLocation,
			CurrentNode.WorldLocation,
			BackSurfaceHit);

		if (bHasBackHit)
		{
			AppendRedirectNodes(
				CurrentNode,
				NextNode,
				FrontSurfaceHit,
				BackSurfaceHit,
				OutNodes);
		}
		else
		{
			AppendRedirectNodes(
				CurrentNode,
				NextNode,
				FrontSurfaceHit,
				OutNodes);
		}

		return OutNodes.Num() > 0;
	}

	FHitResult BoundaryFrontHit;
	if (!TryFindBoundaryHit(
		ReferenceStart,
		ReferenceEnd,
		CurrentNode,
		NextNode,
		QueryParams,
		BoundaryFrontHit))
	{
		return false;
	}

	FHitResult BoundaryBackHit;
	const bool bHasBoundaryBackHit = TryFindBoundaryHit(
		ReferenceEnd,
		ReferenceStart,
		NextNode,
		CurrentNode,
		QueryParams,
		BoundaryBackHit);

	if (bHasBoundaryBackHit)
	{
		AppendRedirectNodes(
			ReferenceStart,
			ReferenceEnd,
			BoundaryFrontHit,
			BoundaryBackHit,
			OutNodes);
	}
	else
	{
		AppendRedirectNodes(
			ReferenceStart,
			ReferenceEnd,
			BoundaryFrontHit,
			OutNodes);
	}

	return OutNodes.Num() > 0;
}

bool URayRopeComponent::TryFindBoundaryHit(
	const FRayRopeNode& ValidLineStart,
	const FRayRopeNode& ValidLineEnd,
	const FRayRopeNode& InvalidLineStart,
	const FRayRopeNode& InvalidLineEnd,
	const FCollisionQueryParams& QueryParams,
	FHitResult& SurfaceHit) const
{
	FRayRopeNode LastValidLineStart = ValidLineStart;
	FRayRopeNode LastValidLineEnd = ValidLineEnd;
	FRayRopeNode LastInvalidLineStart = InvalidLineStart;
	FRayRopeNode LastInvalidLineEnd = InvalidLineEnd;

	BinarySearchCollisionBoundary(
		LastValidLineStart,
		LastValidLineEnd,
		LastInvalidLineStart,
		LastInvalidLineEnd,
		QueryParams);

	return TryTraceBlockingHit(
		QueryParams,
		LastInvalidLineStart.WorldLocation,
		LastInvalidLineEnd.WorldLocation,
		SurfaceHit);
}

FRayRopeNode URayRopeComponent::CreateAnchorNode(AActor* AnchorActor) const
{
	FRayRopeNode AnchorNode;
	AnchorNode.NodeType = ENodeType::Anchor;
	AnchorNode.AttachActor = AnchorActor;
	AnchorNode.WorldLocation = GetAnchorWorldLocation(AnchorNode);
	return AnchorNode;
}

FRayRopeNode URayRopeComponent::CreateRedirectNode(
	const FRayRopeNode& ValidLineStart,
	const FRayRopeNode& ValidLineEnd,
	const FHitResult& FrontSurfaceHit) const
{
	FRayRopeNode RedirectNode;
	RedirectNode.NodeType = ENodeType::Redirect;
	RedirectNode.AttachActor = FrontSurfaceHit.GetActor();
	RedirectNode.WorldLocation =
		CalculateRedirectLocation(ValidLineStart, ValidLineEnd, FrontSurfaceHit);

	RedirectNode.WorldLocation += CalculateRedirectOffset(FrontSurfaceHit);
	CacheRedirectNodeOffset(RedirectNode);
	return RedirectNode;
}

FRayRopeNode URayRopeComponent::CreateRedirectNode(
	const FRayRopeNode& ValidLineStart,
	const FRayRopeNode& ValidLineEnd,
	const FHitResult& FrontSurfaceHit,
	const FHitResult& BackSurfaceHit) const
{
	FRayRopeNode RedirectNode;
	RedirectNode.NodeType = ENodeType::Redirect;
	RedirectNode.AttachActor = FrontSurfaceHit.GetActor();
	RedirectNode.WorldLocation =
		CalculateRedirectLocation(
			ValidLineStart,
			ValidLineEnd,
			FrontSurfaceHit,
			BackSurfaceHit);

	RedirectNode.WorldLocation += CalculateRedirectOffset(FrontSurfaceHit, BackSurfaceHit);
	CacheRedirectNodeOffset(RedirectNode);
	return RedirectNode;
}

void URayRopeComponent::AppendRedirectNodes(
	const FRayRopeNode& ValidLineStart,
	const FRayRopeNode& ValidLineEnd,
	const FHitResult& FrontSurfaceHit,
	TArray<FRayRopeNode>& OutNodes) const
{
	OutNodes.Add(CreateRedirectNode(
		ValidLineStart,
		ValidLineEnd,
		FrontSurfaceHit));
}

void URayRopeComponent::AppendRedirectNodes(
	const FRayRopeNode& ValidLineStart,
	const FRayRopeNode& ValidLineEnd,
	const FHitResult& FrontSurfaceHit,
	const FHitResult& BackSurfaceHit,
	TArray<FRayRopeNode>& OutNodes) const
{
	if (!AreHitsParallel(FrontSurfaceHit, BackSurfaceHit))
	{
		OutNodes.Add(CreateRedirectNode(
			ValidLineStart,
			ValidLineEnd,
			FrontSurfaceHit,
			BackSurfaceHit));
		return;
	}

	FRayRopeNode FirstNode = CreateRedirectNode(
		ValidLineStart,
		ValidLineEnd,
		FrontSurfaceHit);

	FRayRopeNode SecondNode = CreateRedirectNode(
		ValidLineStart,
		ValidLineEnd,
		BackSurfaceHit);

	if (FVector::DistSquared(ValidLineStart.WorldLocation, FirstNode.WorldLocation) >
		FVector::DistSquared(ValidLineStart.WorldLocation, SecondNode.WorldLocation))
	{
		Swap(FirstNode, SecondNode);
	}

	OutNodes.Add(MoveTemp(FirstNode));
	OutNodes.Add(MoveTemp(SecondNode));
}

void URayRopeComponent::BinarySearchCollisionBoundary(
	FRayRopeNode& ValidLineStart,
	FRayRopeNode& ValidLineEnd,
	FRayRopeNode& InvalidLineStart,
	FRayRopeNode& InvalidLineEnd,
	const FCollisionQueryParams& QueryParams) const
{
	for (int32 Iteration = 0; Iteration < MaxBinarySearchIteration; ++Iteration)
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
			continue;
		}

		ValidLineStart = MidLineStart;
		ValidLineEnd = MidLineEnd;
	}
}

FVector URayRopeComponent::CalculateRedirectLocation(
	const FRayRopeNode& ValidLineStart,
	const FRayRopeNode& ValidLineEnd,
	const FHitResult& FrontSurfaceHit) const
{
	return CalculatePlaneRedirectLocation(ValidLineStart, ValidLineEnd, FrontSurfaceHit);
}

FVector URayRopeComponent::CalculateRedirectLocation(
	const FRayRopeNode& ValidLineStart,
	const FRayRopeNode& ValidLineEnd,
	const FHitResult& FrontSurfaceHit,
	const FHitResult& BackSurfaceHit) const
{
	const FVector FallbackLocation =
		CalculatePlaneRedirectLocation(ValidLineStart, ValidLineEnd, FrontSurfaceHit);

	FVector PlaneIntersectionPoint = FVector::ZeroVector;
	FVector PlaneIntersectionDirection = FVector::ZeroVector;
	if (!TryGetPlaneIntersectionLine(
		FrontSurfaceHit,
		BackSurfaceHit,
		PlaneIntersectionPoint,
		PlaneIntersectionDirection))
	{
		return FallbackLocation;
	}

	FVector ClosestPointOnValidLine = FVector::ZeroVector;
	float DistanceToIntersectionLineSquared = 0.f;
	FindClosestPointOnSegmentToLine(
		ValidLineStart.WorldLocation,
		ValidLineEnd.WorldLocation,
		PlaneIntersectionPoint,
		PlaneIntersectionDirection,
		ClosestPointOnValidLine,
		DistanceToIntersectionLineSquared);

	return DistanceToIntersectionLineSquared > FMath::Square(WrapSolverEpsilon)
		? FallbackLocation
		: ClosestPointOnValidLine;
}

FVector URayRopeComponent::CalculatePlaneRedirectLocation(
	const FRayRopeNode& ValidLineStart,
	const FRayRopeNode& ValidLineEnd,
	const FHitResult& SurfaceHit) const
{
	const FVector LineStart = ValidLineStart.WorldLocation;
	const FVector LineEnd = ValidLineEnd.WorldLocation;
	const FVector LineDirection = LineEnd - LineStart;
	const float Denominator = FVector::DotProduct(SurfaceHit.ImpactNormal, LineDirection);

	if (FMath::IsNearlyZero(Denominator))
	{
		return FVector::DistSquared(SurfaceHit.ImpactPoint, LineStart) <=
			FVector::DistSquared(SurfaceHit.ImpactPoint, LineEnd)
			? LineStart
			: LineEnd;
	}

	float T = FVector::DotProduct(
		SurfaceHit.ImpactPoint - LineStart,
		SurfaceHit.ImpactNormal) / Denominator;

	T = FMath::Clamp(T, 0.f, 1.f);
	return LineStart + LineDirection * T;
}

FVector URayRopeComponent::CalculateRedirectOffset(const FHitResult& FrontSurfaceHit) const
{
	return FrontSurfaceHit.ImpactNormal.GetSafeNormal() * RopePhysicalRadius;
}

FVector URayRopeComponent::CalculateRedirectOffset(
	const FHitResult& FrontSurfaceHit,
	const FHitResult& BackSurfaceHit) const
{
	FVector OffsetDirection = FrontSurfaceHit.ImpactNormal + BackSurfaceHit.ImpactNormal;
	if (OffsetDirection.IsNearlyZero())
	{
		OffsetDirection = FrontSurfaceHit.ImpactNormal;
	}

	return OffsetDirection.GetSafeNormal() * RopePhysicalRadius;
}

bool URayRopeComponent::AreHitsParallel(
	const FHitResult& FrontSurfaceHit,
	const FHitResult& BackSurfaceHit) const
{
	return FVector::CrossProduct(
		FrontSurfaceHit.ImpactNormal.GetSafeNormal(),
		BackSurfaceHit.ImpactNormal.GetSafeNormal()).SizeSquared() <=
		FMath::Square(RelaxCollinearEpsilon);
}

bool URayRopeComponent::TryGetPlaneIntersectionLine(
	const FHitResult& FrontSurfaceHit,
	const FHitResult& BackSurfaceHit,
	FVector& OutLinePoint,
	FVector& OutLineDirection) const
{
	const FVector FrontNormal = FrontSurfaceHit.ImpactNormal.GetSafeNormal();
	const FVector BackNormal = BackSurfaceHit.ImpactNormal.GetSafeNormal();

	if (FrontNormal.IsNearlyZero() || BackNormal.IsNearlyZero())
	{
		return false;
	}

	const FVector RawLineDirection = FVector::CrossProduct(FrontNormal, BackNormal);
	const float RawLineDirectionSizeSquared = RawLineDirection.SizeSquared();
	if (RawLineDirectionSizeSquared <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float FrontPlaneDistance =
		FVector::DotProduct(FrontNormal, FrontSurfaceHit.ImpactPoint);

	const float BackPlaneDistance =
		FVector::DotProduct(BackNormal, BackSurfaceHit.ImpactPoint);

	OutLinePoint = FVector::CrossProduct(
		FrontPlaneDistance * BackNormal - BackPlaneDistance * FrontNormal,
		RawLineDirection) / RawLineDirectionSizeSquared;

	OutLineDirection = RawLineDirection.GetSafeNormal();
	return true;
}

void URayRopeComponent::FindClosestPointOnSegmentToLine(
	const FVector& SegmentStart,
	const FVector& SegmentEnd,
	const FVector& LinePoint,
	const FVector& LineDirection,
	FVector& OutPointOnSegment,
	float& OutDistanceSquared) const
{
	const FVector SegmentDirection = SegmentEnd - SegmentStart;
	const float SegmentLengthSquared = SegmentDirection.SizeSquared();
	const float LineLengthSquared = LineDirection.SizeSquared();

	if (SegmentLengthSquared <= KINDA_SMALL_NUMBER ||
		LineLengthSquared <= KINDA_SMALL_NUMBER)
	{
		OutPointOnSegment = SegmentStart;

		const float T = LineLengthSquared <= KINDA_SMALL_NUMBER
			? 0.f
			: FVector::DotProduct(SegmentStart - LinePoint, LineDirection) / LineLengthSquared;

		OutDistanceSquared = FVector::DistSquared(
			OutPointOnSegment,
			LinePoint + LineDirection * T);
		return;
	}

	const FVector Offset = SegmentStart - LinePoint;
	const float A = SegmentLengthSquared;
	const float B = FVector::DotProduct(SegmentDirection, LineDirection);
	const float C = LineLengthSquared;
	const float D = FVector::DotProduct(SegmentDirection, Offset);
	const float E = FVector::DotProduct(LineDirection, Offset);
	const float Denominator = A * C - B * B;

	float SegmentT = FMath::IsNearlyZero(Denominator)
		? FVector::DotProduct(LinePoint - SegmentStart, SegmentDirection) / A
		: (B * E - C * D) / Denominator;

	SegmentT = FMath::Clamp(SegmentT, 0.f, 1.f);

	const float LineT = (E + B * SegmentT) / C;
	const FVector ClosestPointOnLine = LinePoint + LineDirection * LineT;

	OutPointOnSegment = SegmentStart + SegmentDirection * SegmentT;
	OutDistanceSquared = FVector::DistSquared(OutPointOnSegment, ClosestPointOnLine);
}

void URayRopeComponent::CacheRedirectNodeOffset(FRayRopeNode& RedirectNode) const
{
	if (RedirectNode.NodeType != ENodeType::Redirect || !IsValid(RedirectNode.AttachActor))
	{
		RedirectNode.bUseAttachActorOffset = false;
		RedirectNode.AttachActorOffset = FVector::ZeroVector;
		return;
	}

	RedirectNode.bUseAttachActorOffset = true;
	RedirectNode.AttachActorOffset =
		RedirectNode.AttachActor->GetActorTransform().InverseTransformPosition(RedirectNode.WorldLocation);
}

FVector URayRopeComponent::GetAnchorWorldLocation(const FRayRopeNode& Node) const
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
		if (Node.NodeType == ENodeType::Anchor && IsValid(Node.AttachActor))
		{
			QueryParams.AddIgnoredActor(Node.AttachActor);
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

	if (UWorld* World = GetWorld())
	{
		World->LineTraceSingleByChannel(
			SurfaceHit,
			StartLocation,
			EndLocation,
			ECC_Visibility,
			QueryParams);
	}
}

bool URayRopeComponent::TryTraceBlockingHit(
	const FCollisionQueryParams& QueryParams,
	const FVector& StartLocation,
	const FVector& EndLocation,
	FHitResult& SurfaceHit) const
{
	TraceRopeLine(QueryParams, StartLocation, EndLocation, SurfaceHit);
	if (!SurfaceHit.bBlockingHit)
	{
		return false;
	}

	const bool bIsInitialHit =
		SurfaceHit.bStartPenetrating ||
		(SurfaceHit.Distance <= KINDA_SMALL_NUMBER && SurfaceHit.Time <= KINDA_SMALL_NUMBER);

	if (!bIsInitialHit)
	{
		return true;
	}

	FHitResult FallbackHit;
	TraceRopeLine(QueryParams, EndLocation, StartLocation, FallbackHit);

	const bool bHasFallbackHit = FallbackHit.bBlockingHit &&
		!FallbackHit.bStartPenetrating &&
		(FallbackHit.Distance > KINDA_SMALL_NUMBER || FallbackHit.Time > KINDA_SMALL_NUMBER);

	if (bHasFallbackHit)
	{
		SurfaceHit = FallbackHit;
		return true;
	}

	SurfaceHit = FHitResult();
	return false;
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

	const FRayRopeNode& PrevNode = Segment.Nodes[InsertIndex - 1];
	const FRayRopeNode& NextNode = Segment.Nodes[InsertIndex];
	if (AreEquivalentWrapNodes(Candidate, PrevNode) ||
		AreEquivalentWrapNodes(Candidate, NextNode))
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
		FirstNode.AttachActor == SecondNode.AttachActor;

	const bool bSameRedirect =
		FirstNode.NodeType == ENodeType::Redirect &&
		SecondNode.NodeType == ENodeType::Redirect &&
		(
			(
				FirstNode.AttachActor == SecondNode.AttachActor &&
				IsValid(FirstNode.AttachActor) &&
				FirstNode.bUseAttachActorOffset &&
				SecondNode.bUseAttachActorOffset &&
				FirstNode.AttachActorOffset.Equals(SecondNode.AttachActorOffset, WrapSolverEpsilon)
			) ||
			(
				!IsValid(FirstNode.AttachActor) &&
				!IsValid(SecondNode.AttachActor) &&
				FirstNode.WorldLocation.Equals(SecondNode.WorldLocation, WrapSolverEpsilon)
			)
		);

	return bSameAnchor || bSameRedirect;
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
	if (TryTraceBlockingHit(
		QueryParams,
		PrevNode.WorldLocation,
		NextNode.WorldLocation,
		SurfaceHit))
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

	return !TryTraceBlockingHit(
		QueryParams,
		CurrentNode.WorldLocation,
		ClosestPoint,
		SurfaceHit);
}

void URayRopeComponent::SplitSegmentOnAnchors(int32 SegmentIndex)
{
	if (!Segments.IsValidIndex(SegmentIndex))
	{
		return;
	}

	const TArray<FRayRopeNode>& Nodes = Segments[SegmentIndex].Nodes;
	if (Nodes.Num() < 3)
	{
		return;
	}

	TArray<FRayRopeSegment> SplitSegments;
	int32 StartIndex = 0;
	for (int32 NodeIndex = 1; NodeIndex < Nodes.Num(); ++NodeIndex)
	{
		const bool bShouldSplit =
			NodeIndex == Nodes.Num() - 1 ||
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

	Segments.RemoveAt(SegmentIndex, 1, EAllowShrinking::No);
	Segments.Reserve(Segments.Num() + SplitSegments.Num());

	for (int32 SplitIndex = 0; SplitIndex < SplitSegments.Num(); ++SplitIndex)
	{
		Segments.Insert(MoveTemp(SplitSegments[SplitIndex]), SegmentIndex + SplitIndex);
	}
}
