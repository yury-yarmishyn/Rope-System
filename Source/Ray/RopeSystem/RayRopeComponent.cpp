#include "RayRopeComponent.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "RayRopeInterface.h"

bool IsInitialTraceHit(const FHitResult& Hit)
{
	return Hit.bStartPenetrating ||
		(Hit.Distance <= KINDA_SMALL_NUMBER && Hit.Time <= KINDA_SMALL_NUMBER);
}

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
		SyncSegmentNodes(Segment);
		MoveSegment(Segment);
		WrapSegment(Segment, ReferenceSegment);
		RelaxSegment(Segment);
		SplitSegmentOnAnchors(SegmentIndex);
	}

	OnSegmentsSet.Broadcast();
}

void URayRopeComponent::SyncSegmentNodes(FRayRopeSegment& Segment) const
{
	for (FRayRopeNode& Node : Segment.Nodes)
	{
		SyncNode(Node);
		
	}
}

void URayRopeComponent::SyncNode(FRayRopeNode& Node) const
{
	if (!IsValid(Node.AttachActor))
	{
		return;
	}

	if (Node.NodeType == ENodeType::Anchor)
	{
		Node.WorldLocation = GetAnchorWorldLocation(Node);
	}
	if (Node.NodeType == ENodeType::Redirect)
	{
		SyncRedirectNode(Node);
	}
}

void URayRopeComponent::SyncRedirectNode(FRayRopeNode& Node) const
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

void URayRopeComponent::CacheAttachActorOffset(FRayRopeNode& Node) const
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

	TArray<TPair<int32, FRayRopeNode>> PendingInsertions;
	PendingInsertions.Reserve((ComparableNodeCount - 1) * 2);

	for (int32 NodeIndex = 0; NodeIndex < ComparableNodeCount - 1; ++NodeIndex)
	{
		TArray<FRayRopeNode> NewNodes;
		if (!BuildWrapNodes(NodeIndex, Segment, ReferenceSegment, QueryParams, NewNodes))
		{
			continue;
		}

		const int32 InsertIndex = NodeIndex + 1;
		for (FRayRopeNode& NewNode : NewNodes)
		{
			if (!CanInsertWrapNode(InsertIndex, Segment, NewNode, PendingInsertions))
			{
				continue;
			}

			PendingInsertions.Emplace(InsertIndex, MoveTemp(NewNode));
		}
	}

	for (int32 PendingIndex = PendingInsertions.Num() - 1; PendingIndex >= 0; --PendingIndex)
	{
		TPair<int32, FRayRopeNode>& PendingInsertion = PendingInsertions[PendingIndex];
		Segment.Nodes.Insert(MoveTemp(PendingInsertion.Value), PendingInsertion.Key);
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
		if (CurrentNode.NodeType != ENodeType::Redirect)
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

bool URayRopeComponent::TryGetSegmentSpan(
	const FRayRopeSegment& Segment,
	int32 NodeIndex,
	const FRayRopeNode*& OutStartNode,
	const FRayRopeNode*& OutEndNode) const
{
	OutStartNode = nullptr;
	OutEndNode = nullptr;

	if (!Segment.Nodes.IsValidIndex(NodeIndex) || !Segment.Nodes.IsValidIndex(NodeIndex + 1))
	{
		return false;
	}

	OutStartNode = &Segment.Nodes[NodeIndex];
	OutEndNode = &Segment.Nodes[NodeIndex + 1];
	return true;
}

bool URayRopeComponent::BuildWrapNodes(
	int32 NodeIndex,
	const FRayRopeSegment& CurrentSegment,
	const FRayRopeSegment& ReferenceSegment,
	const FCollisionQueryParams& QueryParams,
	TArray<FRayRopeNode>& OutNodes) const
{
	OutNodes.Reset();

	const FRayRopeNode* CurrentStartNode = nullptr;
	const FRayRopeNode* CurrentEndNode = nullptr;
	const FRayRopeNode* ReferenceStartNode = nullptr;
	const FRayRopeNode* ReferenceEndNode = nullptr;
	
	if (!TryGetSegmentSpan(CurrentSegment, NodeIndex, CurrentStartNode, CurrentEndNode) ||
		!TryGetSegmentSpan(ReferenceSegment, NodeIndex, ReferenceStartNode, ReferenceEndNode))
	{
		return false;
	}

	FHitResult FrontSurfaceHit;
	if (!TryTraceSpan(*CurrentStartNode, *CurrentEndNode, QueryParams, FrontSurfaceHit))
	{
		return false;
	}

	AActor* HitActor = FrontSurfaceHit.GetActor();
	if (IsValid(HitActor) && HitActor->GetClass()->ImplementsInterface(URayRopeInterface::StaticClass()))
	{
		OutNodes.Add(CreateAnchorNode(HitActor));
		return true;
	}

	if (!CanUseHitForRedirectWrap(FrontSurfaceHit))
	{
		return false;
	}

	const FRayRopeNode* ValidStartNode = nullptr;
	const FRayRopeNode* ValidEndNode = nullptr;
	FHitResult ResolvedFrontSurfaceHit;
	FHitResult ResolvedBackSurfaceHit;
	const FHitResult* BackSurfaceHit = nullptr;
	if (!TryBuildWrapRedirectInputs(
		*CurrentStartNode,
		*CurrentEndNode,
		*ReferenceStartNode,
		*ReferenceEndNode,
		QueryParams,
		FrontSurfaceHit,
		ValidStartNode,
		ValidEndNode,
		ResolvedFrontSurfaceHit,
		ResolvedBackSurfaceHit,
		BackSurfaceHit))
	{
		return false;
	}

	AppendRedirectNodes(
		*ValidStartNode,
		*ValidEndNode,
		ResolvedFrontSurfaceHit,
		BackSurfaceHit,
		OutNodes);

	return OutNodes.Num() > 0;
}

bool URayRopeComponent::TryBuildWrapRedirectInputs(
	const FRayRopeNode& CurrentStartNode,
	const FRayRopeNode& CurrentEndNode,
	const FRayRopeNode& ReferenceStartNode,
	const FRayRopeNode& ReferenceEndNode,
	const FCollisionQueryParams& QueryParams,
	const FHitResult& FrontSurfaceHit,
	const FRayRopeNode*& OutValidStartNode,
	const FRayRopeNode*& OutValidEndNode,
	FHitResult& OutResolvedFrontSurfaceHit,
	FHitResult& OutResolvedBackSurfaceHit,
	const FHitResult*& OutBackSurfaceHit) const
{
	OutValidStartNode = &CurrentStartNode;
	OutValidEndNode = &CurrentEndNode;
	OutResolvedFrontSurfaceHit = FrontSurfaceHit;
	OutResolvedBackSurfaceHit = FHitResult();
	OutBackSurfaceHit = nullptr;

	FHitResult ReferenceHit;
	if (TryTraceSpan(ReferenceStartNode, ReferenceEndNode, QueryParams, ReferenceHit))
	{
		if (TryTraceSpan(CurrentEndNode, CurrentStartNode, QueryParams, OutResolvedBackSurfaceHit) &&
			CanUseHitForRedirectWrap(OutResolvedBackSurfaceHit))
		{
			OutBackSurfaceHit = &OutResolvedBackSurfaceHit;
		}

		return true;
	}

	if (!TryFindBoundaryHit(
		ReferenceStartNode,
		ReferenceEndNode,
		CurrentStartNode,
		CurrentEndNode,
		QueryParams,
		OutResolvedFrontSurfaceHit))
	{
		return false;
	}

	if (!CanUseHitForRedirectWrap(OutResolvedFrontSurfaceHit))
	{
		return false;
	}

	OutValidStartNode = &ReferenceStartNode;
	OutValidEndNode = &ReferenceEndNode;

	if (TryFindBoundaryHit(
		ReferenceEndNode,
		ReferenceStartNode,
		CurrentEndNode,
		CurrentStartNode,
		QueryParams,
		OutResolvedBackSurfaceHit) &&
		CanUseHitForRedirectWrap(OutResolvedBackSurfaceHit))
	{
		OutBackSurfaceHit = &OutResolvedBackSurfaceHit;
	}

	return true;
}

bool URayRopeComponent::TryFindBoundaryHit(
	const FRayRopeNode& ValidStartNode,
	const FRayRopeNode& ValidEndNode,
	const FRayRopeNode& InvalidStartNode,
	const FRayRopeNode& InvalidEndNode,
	const FCollisionQueryParams& QueryParams,
	FHitResult& SurfaceHit) const
{
	FRayRopeNode LastValidLineStart = ValidStartNode;
	FRayRopeNode LastValidLineEnd = ValidEndNode;
	FRayRopeNode LastInvalidLineStart = InvalidStartNode;
	FRayRopeNode LastInvalidLineEnd = InvalidEndNode;

	for (int32 Iteration = 0; Iteration < MaxBinarySearchIteration; ++Iteration)
	{
		const bool bStartCloseEnough =
			FVector::Dist(LastValidLineStart.WorldLocation, LastInvalidLineStart.WorldLocation) <=
			WrapSolverEpsilon;

		const bool bEndCloseEnough =
			FVector::Dist(LastValidLineEnd.WorldLocation, LastInvalidLineEnd.WorldLocation) <=
			WrapSolverEpsilon;

		if (bStartCloseEnough && bEndCloseEnough)
		{
			break;
		}

		FRayRopeNode MidLineStart = LastValidLineStart;
		FRayRopeNode MidLineEnd = LastValidLineEnd;
		MidLineStart.WorldLocation =
			(LastValidLineStart.WorldLocation + LastInvalidLineStart.WorldLocation) * 0.5f;
		MidLineEnd.WorldLocation =
			(LastValidLineEnd.WorldLocation + LastInvalidLineEnd.WorldLocation) * 0.5f;

		FHitResult MidSurfaceHit;
		if (TryTraceBlockingHit(
			QueryParams,
			MidLineStart.WorldLocation,
			MidLineEnd.WorldLocation,
			MidSurfaceHit))
		{
			LastInvalidLineStart = MidLineStart;
			LastInvalidLineEnd = MidLineEnd;
			continue;
		}

		LastValidLineStart = MidLineStart;
		LastValidLineEnd = MidLineEnd;
	}

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
	const FRayRopeNode& ValidStartNode,
	const FRayRopeNode& ValidEndNode,
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit) const
{
	FRayRopeNode RedirectNode;
	RedirectNode.NodeType = ENodeType::Redirect;
	RedirectNode.AttachActor = ResolveRedirectAttachActor(FrontSurfaceHit, BackSurfaceHit);
	const FVector RedirectLocation = CalculateRedirectLocation(
		ValidStartNode,
		ValidEndNode,
		FrontSurfaceHit,
		BackSurfaceHit);
	const FVector RedirectOffset = CalculateRedirectOffset(FrontSurfaceHit, BackSurfaceHit);
	RedirectNode.WorldLocation = RedirectLocation + RedirectOffset;
	CacheAttachActorOffset(RedirectNode);
	return RedirectNode;
}

void URayRopeComponent::AppendRedirectNodes(
	const FRayRopeNode& ValidStartNode,
	const FRayRopeNode& ValidEndNode,
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit,
	TArray<FRayRopeNode>& OutNodes) const
{
	// Redirect rules: one surface -> one redirect, corner -> one redirect, parallel planes -> two redirects.
	if (BackSurfaceHit == nullptr ||
		!AreDirectionsNearlyCollinear(
			FrontSurfaceHit.ImpactNormal,
			BackSurfaceHit->ImpactNormal,
			RelaxCollinearEpsilon))
	{
		OutNodes.Add(CreateRedirectNode(
			ValidStartNode,
			ValidEndNode,
			FrontSurfaceHit,
			BackSurfaceHit));
		return;
	}

	FRayRopeNode FirstNode = CreateRedirectNode(
		ValidStartNode,
		ValidEndNode,
		FrontSurfaceHit,
		nullptr);

	FRayRopeNode SecondNode = CreateRedirectNode(
		ValidStartNode,
		ValidEndNode,
		*BackSurfaceHit,
		nullptr);

	if (FVector::DistSquared(ValidStartNode.WorldLocation, FirstNode.WorldLocation) >
		FVector::DistSquared(ValidStartNode.WorldLocation, SecondNode.WorldLocation))
	{
		Swap(FirstNode, SecondNode);
	}

	OutNodes.Add(MoveTemp(FirstNode));
	OutNodes.Add(MoveTemp(SecondNode));
}

FVector URayRopeComponent::CalculateProjectedPointOnHitPlane(
	const FRayRopeNode& ValidStartNode,
	const FRayRopeNode& ValidEndNode,
	const FHitResult& SurfaceHit) const
{
	const FVector LineStart = ValidStartNode.WorldLocation;
	const FVector LineEnd = ValidEndNode.WorldLocation;
	const FVector LineDirection = LineEnd - LineStart;
	const float Denominator = FVector::DotProduct(SurfaceHit.ImpactNormal, LineDirection);

	if (FMath::IsNearlyZero(Denominator))
	{
		return FVector::DistSquared(SurfaceHit.ImpactPoint, LineStart) <=
			FVector::DistSquared(SurfaceHit.ImpactPoint, LineEnd)
			? LineStart
			: LineEnd;
	}

	const float T = FMath::Clamp(
		FVector::DotProduct(
			SurfaceHit.ImpactPoint - LineStart,
			SurfaceHit.ImpactNormal) / Denominator,
		0.f,
		1.f);

	return LineStart + LineDirection * T;
}

FVector URayRopeComponent::CalculateRedirectLocation(
	const FRayRopeNode& ValidStartNode,
	const FRayRopeNode& ValidEndNode,
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit) const
{
	const FVector FallbackLocation =
		CalculateProjectedPointOnHitPlane(ValidStartNode, ValidEndNode, FrontSurfaceHit);

	if (BackSurfaceHit == nullptr)
	{
		return FallbackLocation;
	}

	FVector PlaneIntersectionPoint = FVector::ZeroVector;
	FVector PlaneIntersectionDirection = FVector::ZeroVector;
	if (!TryGetPlaneIntersectionLine(
		FrontSurfaceHit,
		*BackSurfaceHit,
		PlaneIntersectionPoint,
		PlaneIntersectionDirection))
	{
		return FallbackLocation;
	}

	FVector ClosestPointOnValidSpan = FVector::ZeroVector;
	FVector ClosestPointOnIntersectionLine = FVector::ZeroVector;
	float DistanceToIntersectionLineSquared = 0.f;
	FindClosestPointsOnSegmentToLine(
		ValidStartNode.WorldLocation,
		ValidEndNode.WorldLocation,
		PlaneIntersectionPoint,
		PlaneIntersectionDirection,
		ClosestPointOnValidSpan,
		ClosestPointOnIntersectionLine,
		DistanceToIntersectionLineSquared);

	if (ClosestPointOnIntersectionLine.ContainsNaN())
	{
		return FallbackLocation;
	}

	return ClosestPointOnIntersectionLine;
}

FVector URayRopeComponent::CalculateRedirectOffset(
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit) const
{
	if (BackSurfaceHit == nullptr)
	{
		return FrontSurfaceHit.ImpactNormal.GetSafeNormal() * RopePhysicalRadius;
	}

	FVector OffsetDirection = FrontSurfaceHit.ImpactNormal + BackSurfaceHit->ImpactNormal;
	if (OffsetDirection.IsNearlyZero())
	{
		OffsetDirection = FrontSurfaceHit.ImpactNormal;
	}

	return OffsetDirection.GetSafeNormal() * RopePhysicalRadius;
}
bool URayRopeComponent::AreDirectionsNearlyCollinear(
	const FVector& FirstDirection,
	const FVector& SecondDirection,
	float Epsilon) const
{
	return FVector::CrossProduct(
		FirstDirection.GetSafeNormal(),
		SecondDirection.GetSafeNormal()).SizeSquared() <= FMath::Square(Epsilon);
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

void URayRopeComponent::FindClosestPointsOnSegmentToLine(
	const FVector& SegmentStart,
	const FVector& SegmentEnd,
	const FVector& LinePoint,
	const FVector& LineDirection,
	FVector& OutPointOnSegment,
	FVector& OutPointOnLine,
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

		OutPointOnLine = LinePoint + LineDirection * T;
		OutDistanceSquared = FVector::DistSquared(OutPointOnSegment, OutPointOnLine);
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
	OutPointOnLine = LinePoint + LineDirection * LineT;
	OutPointOnSegment = SegmentStart + SegmentDirection * SegmentT;
	OutDistanceSquared = FVector::DistSquared(OutPointOnSegment, OutPointOnLine);
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

AActor* URayRopeComponent::ResolveRedirectAttachActor(
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit) const
{
	AActor* FrontActor = FrontSurfaceHit.GetActor();
	if (!IsValid(FrontActor))
	{
		return nullptr;
	}

	if (BackSurfaceHit == nullptr)
	{
		return FrontActor;
	}

	AActor* BackActor = BackSurfaceHit->GetActor();
	return FrontActor == BackActor ? FrontActor : nullptr;
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
		if (Node.NodeType != ENodeType::Anchor || !IsValid(Node.AttachActor))
		{
			continue;
		}

		QueryParams.AddIgnoredActor(Node.AttachActor);
	}
}

bool URayRopeComponent::TryTraceSpan(
	const FRayRopeNode& StartNode,
	const FRayRopeNode& EndNode,
	const FCollisionQueryParams& QueryParams,
	FHitResult& SurfaceHit) const
{
	return TryTraceBlockingHit(
		QueryParams,
		StartNode.WorldLocation,
		EndNode.WorldLocation,
		SurfaceHit);
}

bool URayRopeComponent::TryTraceBlockingHit(
	const FCollisionQueryParams& QueryParams,
	const FVector& StartLocation,
	const FVector& EndLocation,
	FHitResult& SurfaceHit) const
{
	SurfaceHit = FHitResult();

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	World->LineTraceSingleByChannel(
		SurfaceHit,
		StartLocation,
		EndLocation,
		ECC_Visibility,
		QueryParams);

	if (!SurfaceHit.bBlockingHit)
	{
		return false;
	}

	const bool bAcceptForwardHit =
		!IsInitialTraceHit(SurfaceHit) ||
		IsTraceEnteringHitSurface(StartLocation, EndLocation, SurfaceHit);

	if (bAcceptForwardHit)
	{
		return true;
	}

	FHitResult FallbackHit;
	World->LineTraceSingleByChannel(
		FallbackHit,
		EndLocation,
		StartLocation,
		ECC_Visibility,
		QueryParams);

	if (FallbackHit.bBlockingHit)
	{
		const bool bAcceptFallbackHit =
			!IsInitialTraceHit(FallbackHit) ||
			IsTraceEnteringHitSurface(
			EndLocation,
			StartLocation,
			FallbackHit);

		if (bAcceptFallbackHit)
		{
			SurfaceHit = FallbackHit;
			return true;
		}
	}

	SurfaceHit = FHitResult();
	return false;
}

bool URayRopeComponent::CanUseHitForRedirectWrap(const FHitResult& SurfaceHit) const
{
	if (!SurfaceHit.bBlockingHit)
	{
		return false;
	}

	if (bAllowWrapOnMovableObjects)
	{
		return true;
	}

	const UPrimitiveComponent* HitComponent = SurfaceHit.GetComponent();
	if (IsValid(HitComponent))
	{
		if (HitComponent->Mobility == EComponentMobility::Movable ||
			HitComponent->IsSimulatingPhysics())
		{
			return false;
		}
	}

	const AActor* HitActor = SurfaceHit.GetActor();
	if (!IsValid(HitActor))
	{
		return HitComponent != nullptr;
	}

	const USceneComponent* RootComponent = HitActor->GetRootComponent();
	return !IsValid(RootComponent) || RootComponent->Mobility != EComponentMobility::Movable;
}

bool URayRopeComponent::CanInsertWrapNode(
	int32 InsertIndex,
	const FRayRopeSegment& Segment,
	const FRayRopeNode& Candidate,
	const TArray<TPair<int32, FRayRopeNode>>& PendingInsertions) const
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

	for (const TPair<int32, FRayRopeNode>& PendingInsertion : PendingInsertions)
	{
		if (PendingInsertion.Key == InsertIndex &&
			AreEquivalentWrapNodes(PendingInsertion.Value, Candidate))
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
	if (FirstNode.NodeType != SecondNode.NodeType)
	{
		return false;
	}

	if (FirstNode.NodeType == ENodeType::Anchor)
	{
		return FirstNode.AttachActor == SecondNode.AttachActor;
	}

	if (FirstNode.NodeType != ENodeType::Redirect)
	{
		return false;
	}

	const bool bBothAttached =
		FirstNode.AttachActor == SecondNode.AttachActor &&
		IsValid(FirstNode.AttachActor) &&
		FirstNode.bUseAttachActorOffset &&
		SecondNode.bUseAttachActorOffset;

	if (bBothAttached)
	{
		return FirstNode.AttachActorOffset.Equals(
			SecondNode.AttachActorOffset,
			WrapSolverEpsilon
		);
	}

	const bool bBothWorldSpace =
		!IsValid(FirstNode.AttachActor) &&
		!IsValid(SecondNode.AttachActor);

	return bBothWorldSpace &&
		FirstNode.WorldLocation.Equals(SecondNode.WorldLocation, WrapSolverEpsilon);
}

bool URayRopeComponent::IsTraceEnteringHitSurface(
	const FVector& StartLocation,
	const FVector& EndLocation,
	const FHitResult& SurfaceHit) const
{
	const FVector TraceDirection = (EndLocation - StartLocation).GetSafeNormal();
	const FVector SurfaceNormal = SurfaceHit.ImpactNormal.GetSafeNormal();
	if (TraceDirection.IsNearlyZero() || SurfaceNormal.IsNearlyZero())
	{
		return false;
	}

	return FVector::DotProduct(TraceDirection, SurfaceNormal) < -KINDA_SMALL_NUMBER;
}

bool URayRopeComponent::CanRemoveRelaxNode(
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& CurrentNode,
	const FRayRopeNode& NextNode,
	const FCollisionQueryParams& QueryParams) const
{
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
	const bool bHasDegenerateBend =
		FirstDirection.IsNearlyZero(RelaxSolverEpsilon) ||
		SecondDirection.IsNearlyZero(RelaxSolverEpsilon);

	const bool bHasCollinearBend =
		AreDirectionsNearlyCollinear(FirstDirection, SecondDirection, RelaxCollinearEpsilon);

	if (bHasDegenerateBend || bHasCollinearBend)
	{
		return true;
	}

	const FVector ShortcutDirection = NextNode.WorldLocation - PrevNode.WorldLocation;
	const float ShortcutLengthSquared = ShortcutDirection.SizeSquared();
	const float T = ShortcutLengthSquared <= KINDA_SMALL_NUMBER
		? 0.f
		: FVector::DotProduct(
			CurrentNode.WorldLocation - PrevNode.WorldLocation,
			ShortcutDirection) / ShortcutLengthSquared;

	const FVector ClosestPoint = PrevNode.WorldLocation + ShortcutDirection * T;
	return CurrentNode.WorldLocation.Equals(ClosestPoint, RelaxSolverEpsilon) ||
		!TryTraceBlockingHit(
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
