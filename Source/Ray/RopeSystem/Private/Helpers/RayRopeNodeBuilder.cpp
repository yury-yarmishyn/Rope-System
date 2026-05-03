#include "RayRopeNodeBuilder.h"

#include "RayRopeInterface.h"
#include "RayRopeNodeSynchronizer.h"
#include "RayRopeSegmentTopology.h"
#include "RayRopeSurfaceGeometry.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"

namespace
{
struct FRayRopeRedirectBuildInput
{
	FRayRopeSpan RedirectBaseSpan;
	FHitResult FrontSurfaceHit;
	FHitResult BackSurfaceHit;
	bool bHasBackSurfaceHit = false;

	const FHitResult* GetBackSurfaceHitPtr() const
	{
		return bHasBackSurfaceHit ? &BackSurfaceHit : nullptr;
	}
};

bool TryBuildNodeSpanPair(
	const FRayRopeTraceContext& TraceContext,
	int32 NodeIndex,
	TConstArrayView<FRayRopeNode> CurrentNodes,
	TConstArrayView<FRayRopeNode> ReferenceNodes,
	FRayRopeSpan& OutCurrentSpan,
	FRayRopeSpan& OutReferenceSpan);

bool TryBuildAnchorNode(
	const FHitResult& FrontSurfaceHit,
	FRayRopeBuiltNodeBuffer& OutNodes);

bool TryBuildRedirectNodes(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeSpan& CurrentSpan,
	const FRayRopeSpan& ReferenceSpan,
	const FHitResult& FrontSurfaceHit,
	FRayRopeBuiltNodeBuffer& OutNodes);

bool TryBuildRedirectInput(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeSpan& CurrentSpan,
	const FRayRopeSpan& ReferenceSpan,
	const FHitResult& FrontSurfaceHit,
	FRayRopeRedirectBuildInput& OutInput);

bool TryBuildAlreadyBlockedRedirectInput(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeSpan& CurrentSpan,
	const FRayRopeSpan& ReversedCurrentSpan,
	const FHitResult& FrontSurfaceHit,
	FRayRopeRedirectBuildInput& OutInput);

bool TryBuildBoundaryRedirectInput(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeSpan& CurrentSpan,
	const FRayRopeSpan& ReferenceSpan,
	const FRayRopeSpan& ReversedCurrentSpan,
	FRayRopeRedirectBuildInput& OutInput);

bool TryFindBoundaryHit(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeSpan& ClearSpan,
	const FRayRopeSpan& BlockedSpan,
	FHitResult& SurfaceHit);

FVector CalculateRedirectOffset(
	const FRayRopeNodeBuildSettings& Settings,
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit = nullptr);

AActor* ResolveRedirectAttachedActor(
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit);

bool TryCreateRedirectNode(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeSpan& RedirectBaseSpan,
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit,
	FRayRopeNode& OutNode);

void AppendRedirectNodes(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeRedirectBuildInput& RedirectInput,
	FRayRopeBuiltNodeBuffer& OutNodes);

bool CanUseHitForRedirectNode(
	const FHitResult& SurfaceHit,
	bool bAllowWrapOnMovableObjects)
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

bool IsInvalidNodeBuilderEndpoint(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNode* Node)
{
	return Node == nullptr ||
		!FRayRopeTrace::IsValidFreeNode(TraceContext, *Node);
}

bool HasInvalidNodeBuilderEndpoint(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeSpan& Span)
{
	return !Span.IsValid() ||
		IsInvalidNodeBuilderEndpoint(TraceContext, Span.StartNode) ||
		IsInvalidNodeBuilderEndpoint(TraceContext, Span.EndNode);
}
}

bool FRayRopeNodeBuilder::BuildNodes(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	int32 NodeIndex,
	TConstArrayView<FRayRopeNode> CurrentNodes,
	TConstArrayView<FRayRopeNode> ReferenceNodes,
	FRayRopeBuiltNodeBuffer& OutNodes)
{
	OutNodes.Reset();

	FRayRopeSpan CurrentSpan;
	FRayRopeSpan ReferenceSpan;
	if (!TryBuildNodeSpanPair(
		TraceContext,
		NodeIndex,
		CurrentNodes,
		ReferenceNodes,
		CurrentSpan,
		ReferenceSpan))
	{
		return false;
	}

	return BuildNodesForSpanTransition(
		TraceContext,
		Settings,
		CurrentSpan,
		ReferenceSpan,
		OutNodes);
}

bool FRayRopeNodeBuilder::BuildNodesForSpanTransition(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeSpan& CurrentSpan,
	const FRayRopeSpan& ReferenceSpan,
	FRayRopeBuiltNodeBuffer& OutNodes)
{
	OutNodes.Reset();

	if (HasInvalidNodeBuilderEndpoint(TraceContext, CurrentSpan) ||
		HasInvalidNodeBuilderEndpoint(TraceContext, ReferenceSpan))
	{
		return false;
	}

	FHitResult FrontSurfaceHit;
	if (!FRayRopeTrace::TryTraceSpan(TraceContext, CurrentSpan, FrontSurfaceHit))
	{
		return false;
	}

	if (TryBuildAnchorNode(FrontSurfaceHit, OutNodes))
	{
		return true;
	}

	return TryBuildRedirectNodes(
		TraceContext,
		Settings,
		CurrentSpan,
		ReferenceSpan,
		FrontSurfaceHit,
		OutNodes);
}

bool FRayRopeNodeBuilder::CanInsertNodes(
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeNode& PrevNode,
	const FRayRopeNode& NextNode,
	int32 InsertIndex,
	TConstArrayView<FRayRopeNode> Candidates,
	const FRayRopePendingNodeInsertionBuffer& PendingInsertions)
{
	if (InsertIndex < 0 || Candidates.Num() == 0)
	{
		return false;
	}

	for (int32 CandidateIndex = 0; CandidateIndex < Candidates.Num(); ++CandidateIndex)
	{
		const FRayRopeNode& Candidate = Candidates[CandidateIndex];
		if (AreEquivalentNodes(Settings, Candidate, PrevNode) ||
			AreEquivalentNodes(Settings, Candidate, NextNode))
		{
			return false;
		}

		for (int32 OtherCandidateIndex = CandidateIndex + 1;
			OtherCandidateIndex < Candidates.Num();
			++OtherCandidateIndex)
		{
			if (AreEquivalentNodes(Settings, Candidate, Candidates[OtherCandidateIndex]))
			{
				return false;
			}
		}

		for (const TPair<int32, FRayRopeNode>& PendingInsertion : PendingInsertions)
		{
			const bool bNearbyInsertion = FMath::Abs(PendingInsertion.Key - InsertIndex) <= 1;
			if (bNearbyInsertion &&
				AreEquivalentNodes(Settings, PendingInsertion.Value, Candidate))
			{
				return false;
			}
		}
	}

	return true;
}

bool FRayRopeNodeBuilder::CanInsertNodesInSegment(
	const FRayRopeNodeBuildSettings& Settings,
	int32 InsertIndex,
	const FRayRopeSegment& Segment,
	TConstArrayView<FRayRopeNode> Candidates,
	const FRayRopePendingNodeInsertionBuffer& PendingInsertions)
{
	if (!Segment.Nodes.IsValidIndex(InsertIndex - 1) ||
		!Segment.Nodes.IsValidIndex(InsertIndex))
	{
		return false;
	}

	return CanInsertNodes(
		Settings,
		Segment.Nodes[InsertIndex - 1],
		Segment.Nodes[InsertIndex],
		InsertIndex,
		Candidates,
		PendingInsertions);
}

void FRayRopeNodeBuilder::AppendPendingInsertions(
	int32 InsertIndex,
	FRayRopeBuiltNodeBuffer& Nodes,
	FRayRopePendingNodeInsertionBuffer& PendingInsertions)
{
	for (FRayRopeNode& Node : Nodes)
	{
		PendingInsertions.Emplace(InsertIndex, MoveTemp(Node));
	}
}

void FRayRopeNodeBuilder::ApplyPendingInsertions(
	FRayRopeSegment& Segment,
	FRayRopePendingNodeInsertionBuffer& PendingInsertions)
{
	PendingInsertions.StableSort(
		[](const TPair<int32, FRayRopeNode>& Left, const TPair<int32, FRayRopeNode>& Right)
		{
			return Left.Key < Right.Key;
		});

	if (PendingInsertions.Num() > 0)
	{
		Segment.Nodes.Reserve(Segment.Nodes.Num() + PendingInsertions.Num());
	}

	for (int32 PendingIndex = PendingInsertions.Num() - 1; PendingIndex >= 0; --PendingIndex)
	{
		TPair<int32, FRayRopeNode>& PendingInsertion = PendingInsertions[PendingIndex];
		Segment.Nodes.Insert(MoveTemp(PendingInsertion.Value), PendingInsertion.Key);
	}

	PendingInsertions.Reset();
}

bool FRayRopeNodeBuilder::AreEquivalentNodes(
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeNode& FirstNode,
	const FRayRopeNode& SecondNode)
{
	if (FirstNode.NodeType != SecondNode.NodeType)
	{
		return false;
	}

	if (FirstNode.NodeType == ERayRopeNodeType::Anchor)
	{
		return FirstNode.AttachedActor == SecondNode.AttachedActor;
	}

	if (FirstNode.NodeType != ERayRopeNodeType::Redirect)
	{
		return false;
	}

	const bool bBothAttachedToSameValidActor =
		FirstNode.AttachedActor == SecondNode.AttachedActor &&
		IsValid(FirstNode.AttachedActor) &&
		FirstNode.bUseAttachedActorOffset &&
		SecondNode.bUseAttachedActorOffset;

	if (bBothAttachedToSameValidActor)
	{
		return FirstNode.AttachedActorOffset.Equals(
			SecondNode.AttachedActorOffset,
			Settings.WrapSolverTolerance
		);
	}

	return FirstNode.WorldLocation.Equals(
		SecondNode.WorldLocation,
		Settings.WrapSolverTolerance);
}

namespace
{
bool TryBuildNodeSpanPair(
	const FRayRopeTraceContext& TraceContext,
	int32 NodeIndex,
	TConstArrayView<FRayRopeNode> CurrentNodes,
	TConstArrayView<FRayRopeNode> ReferenceNodes,
	FRayRopeSpan& OutCurrentSpan,
	FRayRopeSpan& OutReferenceSpan)
{
	if (!FRayRopeSegmentTopology::TryGetNodeSpan(CurrentNodes, NodeIndex, OutCurrentSpan) ||
		!FRayRopeSegmentTopology::TryGetNodeSpan(ReferenceNodes, NodeIndex, OutReferenceSpan))
	{
		return false;
	}

	return !HasInvalidNodeBuilderEndpoint(TraceContext, OutCurrentSpan) &&
		!HasInvalidNodeBuilderEndpoint(TraceContext, OutReferenceSpan);
}

bool TryBuildAnchorNode(
	const FHitResult& FrontSurfaceHit,
	FRayRopeBuiltNodeBuffer& OutNodes)
{
	AActor* HitActor = FrontSurfaceHit.GetActor();
	if (!IsValid(HitActor) || !HitActor->GetClass()->ImplementsInterface(URayRopeInterface::StaticClass()))
	{
		return false;
	}

	OutNodes.Add(FRayRopeNodeSynchronizer::CreateAnchorNode(HitActor));
	return true;
}

bool TryBuildRedirectNodes(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeSpan& CurrentSpan,
	const FRayRopeSpan& ReferenceSpan,
	const FHitResult& FrontSurfaceHit,
	FRayRopeBuiltNodeBuffer& OutNodes)
{
	if (!CanUseHitForRedirectNode(
		FrontSurfaceHit,
		Settings.bAllowWrapOnMovableObjects))
	{
		return false;
	}

	FRayRopeRedirectBuildInput RedirectInput;
	if (!TryBuildRedirectInput(
		TraceContext,
		Settings,
		CurrentSpan,
		ReferenceSpan,
		FrontSurfaceHit,
		RedirectInput))
	{
		return false;
	}

	AppendRedirectNodes(TraceContext, Settings, RedirectInput, OutNodes);
	return OutNodes.Num() > 0;
}

bool TryBuildRedirectInput(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeSpan& CurrentSpan,
	const FRayRopeSpan& ReferenceSpan,
	const FHitResult& FrontSurfaceHit,
	FRayRopeRedirectBuildInput& OutInput)
{
	OutInput = FRayRopeRedirectBuildInput();

	if (!CurrentSpan.IsValid() || !ReferenceSpan.IsValid())
	{
		return false;
	}

	const FRayRopeSpan ReversedCurrentSpan{CurrentSpan.EndNode, CurrentSpan.StartNode};
	if (FRayRopeTrace::HasBlockingSpanHit(TraceContext, ReferenceSpan))
	{
		return TryBuildAlreadyBlockedRedirectInput(
			TraceContext,
			Settings,
			CurrentSpan,
			ReversedCurrentSpan,
			FrontSurfaceHit,
			OutInput);
	}

	return TryBuildBoundaryRedirectInput(
		TraceContext,
		Settings,
		CurrentSpan,
		ReferenceSpan,
		ReversedCurrentSpan,
		OutInput);
}

bool TryBuildAlreadyBlockedRedirectInput(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeSpan& CurrentSpan,
	const FRayRopeSpan& ReversedCurrentSpan,
	const FHitResult& FrontSurfaceHit,
	FRayRopeRedirectBuildInput& OutInput)
{
	OutInput.RedirectBaseSpan = CurrentSpan;
	OutInput.FrontSurfaceHit = FrontSurfaceHit;

	if (FRayRopeTrace::TryTraceSpan(TraceContext, ReversedCurrentSpan, OutInput.BackSurfaceHit) &&
		CanUseHitForRedirectNode(
			OutInput.BackSurfaceHit,
			Settings.bAllowWrapOnMovableObjects))
	{
		OutInput.bHasBackSurfaceHit = true;
	}

	return true;
}

bool TryBuildBoundaryRedirectInput(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeSpan& CurrentSpan,
	const FRayRopeSpan& ReferenceSpan,
	const FRayRopeSpan& ReversedCurrentSpan,
	FRayRopeRedirectBuildInput& OutInput)
{
	if (!TryFindBoundaryHit(
		TraceContext,
		Settings,
		ReferenceSpan,
		CurrentSpan,
		OutInput.FrontSurfaceHit))
	{
		return false;
	}

	if (!CanUseHitForRedirectNode(
		OutInput.FrontSurfaceHit,
		Settings.bAllowWrapOnMovableObjects))
	{
		return false;
	}

	OutInput.RedirectBaseSpan = ReferenceSpan;

	const FRayRopeSpan ReversedReferenceSpan{ReferenceSpan.EndNode, ReferenceSpan.StartNode};
	if (TryFindBoundaryHit(
		TraceContext,
		Settings,
		ReversedReferenceSpan,
		ReversedCurrentSpan,
		OutInput.BackSurfaceHit) &&
		CanUseHitForRedirectNode(
			OutInput.BackSurfaceHit,
			Settings.bAllowWrapOnMovableObjects))
	{
		OutInput.bHasBackSurfaceHit = true;
	}

	return true;
}

bool TryFindBoundaryHit(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeSpan& ClearSpan,
	const FRayRopeSpan& BlockedSpan,
	FHitResult& SurfaceHit)
{
	if (!ClearSpan.IsValid() || !BlockedSpan.IsValid())
	{
		SurfaceHit = FHitResult();
		return false;
	}

	FRayRopeNode LastClearStartNode = *ClearSpan.StartNode;
	FRayRopeNode LastClearEndNode = *ClearSpan.EndNode;
	FRayRopeNode LastBlockedStartNode = *BlockedSpan.StartNode;
	FRayRopeNode LastBlockedEndNode = *BlockedSpan.EndNode;
	FRayRopeSpan LastClearSpan{&LastClearStartNode, &LastClearEndNode};
	FRayRopeSpan LastBlockedSpan{&LastBlockedStartNode, &LastBlockedEndNode};
	const float WrapSolverToleranceSquared = FMath::Square(Settings.WrapSolverTolerance);

	const int32 MaxSearchIterations = FMath::Max(0, Settings.MaxWrapBinarySearchIterations);
	for (int32 Iteration = 0; Iteration < MaxSearchIterations; ++Iteration)
	{
		const bool bStartCloseEnough =
			FVector::DistSquared(LastClearSpan.GetStartLocation(), LastBlockedSpan.GetStartLocation()) <=
			WrapSolverToleranceSquared;

		const bool bEndCloseEnough =
			FVector::DistSquared(LastClearSpan.GetEndLocation(), LastBlockedSpan.GetEndLocation()) <=
			WrapSolverToleranceSquared;

		if (bStartCloseEnough && bEndCloseEnough)
		{
			break;
		}

		FRayRopeNode MidStartNode = LastClearStartNode;
		FRayRopeNode MidEndNode = LastClearEndNode;
		MidStartNode.WorldLocation =
			(LastClearSpan.GetStartLocation() + LastBlockedSpan.GetStartLocation()) * 0.5f;
		MidEndNode.WorldLocation =
			(LastClearSpan.GetEndLocation() + LastBlockedSpan.GetEndLocation()) * 0.5f;
		const FRayRopeSpan MidSpan{&MidStartNode, &MidEndNode};
		if (HasInvalidNodeBuilderEndpoint(TraceContext, MidSpan))
		{
			LastBlockedStartNode = MidStartNode;
			LastBlockedEndNode = MidEndNode;
			continue;
		}

		if (FRayRopeTrace::HasBlockingSpanHit(TraceContext, MidSpan))
		{
			LastBlockedStartNode = MidStartNode;
			LastBlockedEndNode = MidEndNode;
			continue;
		}

		LastClearStartNode = MidStartNode;
		LastClearEndNode = MidEndNode;
	}

	if (HasInvalidNodeBuilderEndpoint(TraceContext, LastBlockedSpan))
	{
		SurfaceHit = FHitResult();
		return false;
	}

	return FRayRopeTrace::TryTraceSpan(TraceContext, LastBlockedSpan, SurfaceHit);
}

FVector CalculateRedirectOffset(
	const FRayRopeNodeBuildSettings& Settings,
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit)
{
	if (BackSurfaceHit == nullptr)
	{
		return FrontSurfaceHit.ImpactNormal.GetSafeNormal() * Settings.WrapSurfaceOffset;
	}

	FVector OffsetDirection = FrontSurfaceHit.ImpactNormal + BackSurfaceHit->ImpactNormal;
	if (OffsetDirection.IsNearlyZero())
	{
		OffsetDirection = FrontSurfaceHit.ImpactNormal;
	}

	return OffsetDirection.GetSafeNormal() * Settings.WrapSurfaceOffset;
}

AActor* ResolveRedirectAttachedActor(
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit)
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

bool TryCreateRedirectNode(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeSpan& RedirectBaseSpan,
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit,
	FRayRopeNode& OutNode)
{
	OutNode = FRayRopeNode();

	FRayRopeNode RedirectNode;
	RedirectNode.NodeType = ERayRopeNodeType::Redirect;
	RedirectNode.AttachedActor = ResolveRedirectAttachedActor(FrontSurfaceHit, BackSurfaceHit);
	const FVector RedirectLocation = FRayRopeSurfaceGeometry::CalculateRedirectLocation(
		RedirectBaseSpan,
		FrontSurfaceHit,
		BackSurfaceHit);
	const FVector RedirectOffset =
		CalculateRedirectOffset(
			Settings,
			FrontSurfaceHit,
			BackSurfaceHit);
	RedirectNode.WorldLocation = RedirectLocation + RedirectOffset;
	if (!FRayRopeTrace::IsValidFreePoint(TraceContext, RedirectNode.WorldLocation))
	{
		return false;
	}

	FRayRopeNodeSynchronizer::CacheAttachedActorOffset(RedirectNode);
	OutNode = MoveTemp(RedirectNode);
	return true;
}

void AppendRedirectNodes(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeRedirectBuildInput& RedirectInput,
	FRayRopeBuiltNodeBuffer& OutNodes)
{
	const FHitResult* BackSurfaceHit = RedirectInput.GetBackSurfaceHitPtr();

	// Redirect rules: one surface -> one redirect, corner -> one redirect, parallel planes -> two redirects.
	if (BackSurfaceHit == nullptr ||
		!FRayRopeSurfaceGeometry::AreDirectionsNearlyCollinear(
			RedirectInput.FrontSurfaceHit.ImpactNormal,
			BackSurfaceHit->ImpactNormal,
			Settings.GeometryCollinearityTolerance))
	{
		FRayRopeNode RedirectNode;
		if (TryCreateRedirectNode(
			TraceContext,
			Settings,
			RedirectInput.RedirectBaseSpan,
			RedirectInput.FrontSurfaceHit,
			BackSurfaceHit,
			RedirectNode))
		{
			OutNodes.Add(MoveTemp(RedirectNode));
		}
		return;
	}

	FRayRopeNode FirstNode;
	if (!TryCreateRedirectNode(
		TraceContext,
		Settings,
		RedirectInput.RedirectBaseSpan,
		RedirectInput.FrontSurfaceHit,
		nullptr,
		FirstNode))
	{
		return;
	}

	FRayRopeNode SecondNode;
	if (!TryCreateRedirectNode(
		TraceContext,
		Settings,
		RedirectInput.RedirectBaseSpan,
		*BackSurfaceHit,
		nullptr,
		SecondNode))
	{
		return;
	}

	if (FVector::DistSquared(RedirectInput.RedirectBaseSpan.GetStartLocation(), FirstNode.WorldLocation) >
		FVector::DistSquared(RedirectInput.RedirectBaseSpan.GetStartLocation(), SecondNode.WorldLocation))
	{
		Swap(FirstNode, SecondNode);
	}

	OutNodes.Add(MoveTemp(FirstNode));
	OutNodes.Add(MoveTemp(SecondNode));
}
}
