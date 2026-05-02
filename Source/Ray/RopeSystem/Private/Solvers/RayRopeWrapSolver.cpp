#include "RayRopeWrapSolver.h"

#include "CollisionQueryParams.h"
#include "RayRopeInterface.h"
#include "RayRopeNodeResolver.h"
#include "Solvers/RayRopeTopology.h"
#include "Helpers/RayRopeTrace.h"
#include "Helpers/RayRopeWrapGeometry.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
bool IsInvalidWrapEndpoint(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNode* Node)
{
	return Node == nullptr ||
		Node->WorldLocation.ContainsNaN() ||
		FRayRopeTrace::IsNodeInsideGeometry(TraceContext, *Node);
}

bool HasInvalidWrapEndpoint(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeSpan& Span)
{
	return !Span.IsValid() ||
		IsInvalidWrapEndpoint(TraceContext, Span.StartNode) ||
		IsInvalidWrapEndpoint(TraceContext, Span.EndNode);
}

bool IsValidRedirectCandidateLocation(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNode& CandidateNode)
{
	return !CandidateNode.WorldLocation.ContainsNaN() &&
		!FRayRopeTrace::IsPointInsideGeometry(TraceContext, CandidateNode.WorldLocation);
}
}

void FRayRopeWrapSolver::WrapSegment(
	const FRayRopeTraceSettings& TraceSettings,
	const FRayRopeWrapSettings& WrapSettings,
	FRayRopeSegment& Segment,
	const FRayRopeSegment& ReferenceSegment)
{
	const int32 ComparableNodeCount =
		FMath::Min(Segment.Nodes.Num(), ReferenceSegment.Nodes.Num());

	if (ComparableNodeCount < 2)
	{
		return;
	}

	const FRayRopeTraceContext TraceContext = FRayRopeTrace::MakeTraceContext(
		TraceSettings,
		FCollisionQueryParams(SCENE_QUERY_STAT(RayRopeTrace)));

	FRayRopePendingInsertionBuffer PendingInsertions;
	PendingInsertions.Reserve((ComparableNodeCount - 1) * 2);

	for (int32 NodeIndex = 0; NodeIndex < ComparableNodeCount - 1; ++NodeIndex)
	{
		FRayRopeWrapNodeBuffer NewNodes;
		if (!BuildWrapNodes(
			TraceContext,
			WrapSettings,
			NodeIndex,
			Segment.Nodes,
			ReferenceSegment.Nodes,
			NewNodes))
		{
			continue;
		}

		const int32 InsertIndex = NodeIndex + 1;
		for (FRayRopeNode& NewNode : NewNodes)
		{
			if (!CanInsertWrapNode(
				TraceContext,
				WrapSettings,
				InsertIndex,
				Segment,
				NewNode,
				PendingInsertions))
			{
				continue;
			}

			PendingInsertions.Emplace(InsertIndex, MoveTemp(NewNode));
		}
	}

	if (PendingInsertions.Num() > 0)
	{
		Segment.Nodes.Reserve(Segment.Nodes.Num() + PendingInsertions.Num());
	}

	for (int32 PendingIndex = PendingInsertions.Num() - 1; PendingIndex >= 0; --PendingIndex)
	{
		TPair<int32, FRayRopeNode>& PendingInsertion = PendingInsertions[PendingIndex];
		Segment.Nodes.Insert(MoveTemp(PendingInsertion.Value), PendingInsertion.Key);
	}
}

bool FRayRopeWrapSolver::BuildWrapNodes(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeWrapSettings& WrapSettings,
	int32 NodeIndex,
	TConstArrayView<FRayRopeNode> CurrentNodes,
	TConstArrayView<FRayRopeNode> ReferenceNodes,
	FRayRopeWrapNodeBuffer& OutNodes)
{
	OutNodes.Reset();

	FRayRopeSpan CurrentSpan;
	FRayRopeSpan ReferenceSpan;

	if (!FRayRopeTopology::TryGetNodeSpan(CurrentNodes, NodeIndex, CurrentSpan) ||
		!FRayRopeTopology::TryGetNodeSpan(ReferenceNodes, NodeIndex, ReferenceSpan))
	{
		return false;
	}

	if (HasInvalidWrapEndpoint(TraceContext, CurrentSpan) ||
		HasInvalidWrapEndpoint(TraceContext, ReferenceSpan))
	{
		return false;
	}

	FHitResult FrontSurfaceHit;
	if (!FRayRopeTrace::TryTraceSpan(TraceContext, CurrentSpan, FrontSurfaceHit))
	{
		return false;
	}

	AActor* HitActor = FrontSurfaceHit.GetActor();
	if (IsValid(HitActor) && HitActor->GetClass()->ImplementsInterface(URayRopeInterface::StaticClass()))
	{
		OutNodes.Add(FRayRopeNodeResolver::CreateAnchorNode(HitActor));
		return true;
	}

	if (!FRayRopeTrace::CanUseHitForRedirectWrap(
		FrontSurfaceHit,
		WrapSettings.bAllowWrapOnMovableObjects))
	{
		return false;
	}

	FRayWrapRedirectInput RedirectInput;
	if (!TryBuildWrapRedirectInputs(
		TraceContext,
		WrapSettings,
		CurrentSpan,
		ReferenceSpan,
		FrontSurfaceHit,
		RedirectInput))
	{
		return false;
	}

	AppendRedirectNodes(TraceContext, WrapSettings, RedirectInput, OutNodes);

	return OutNodes.Num() > 0;
}

bool FRayRopeWrapSolver::TryBuildWrapRedirectInputs(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeWrapSettings& WrapSettings,
	const FRayRopeSpan& CurrentSpan,
	const FRayRopeSpan& ReferenceSpan,
	const FHitResult& FrontSurfaceHit,
	FRayWrapRedirectInput& OutInput)
{
	OutInput = FRayWrapRedirectInput();

	if (!CurrentSpan.IsValid() || !ReferenceSpan.IsValid())
	{
		return false;
	}

	OutInput.ValidSpan = CurrentSpan;
	OutInput.FrontSurfaceHit = FrontSurfaceHit;

	const FRayRopeSpan ReversedCurrentSpan{CurrentSpan.EndNode, CurrentSpan.StartNode};
	FHitResult ReferenceHit;
	if (FRayRopeTrace::TryTraceSpan(TraceContext, ReferenceSpan, ReferenceHit))
	{
		if (FRayRopeTrace::TryTraceSpan(TraceContext, ReversedCurrentSpan, OutInput.BackSurfaceHit) &&
			FRayRopeTrace::CanUseHitForRedirectWrap(
				OutInput.BackSurfaceHit,
				WrapSettings.bAllowWrapOnMovableObjects))
		{
			OutInput.bHasBackSurfaceHit = true;
		}

		return true;
	}

	if (!TryFindBoundaryHit(
		TraceContext,
		WrapSettings,
		ReferenceSpan,
		CurrentSpan,
		OutInput.FrontSurfaceHit))
	{
		return false;
	}

	if (!FRayRopeTrace::CanUseHitForRedirectWrap(
		OutInput.FrontSurfaceHit,
		WrapSettings.bAllowWrapOnMovableObjects))
	{
		return false;
	}

	OutInput.ValidSpan = ReferenceSpan;

	const FRayRopeSpan ReversedReferenceSpan{ReferenceSpan.EndNode, ReferenceSpan.StartNode};
	if (TryFindBoundaryHit(
		TraceContext,
		WrapSettings,
		ReversedReferenceSpan,
		ReversedCurrentSpan,
		OutInput.BackSurfaceHit) &&
		FRayRopeTrace::CanUseHitForRedirectWrap(
			OutInput.BackSurfaceHit,
			WrapSettings.bAllowWrapOnMovableObjects))
	{
		OutInput.bHasBackSurfaceHit = true;
	}

	return true;
}

bool FRayRopeWrapSolver::TryFindBoundaryHit(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeWrapSettings& WrapSettings,
	const FRayRopeSpan& ValidSpan,
	const FRayRopeSpan& InvalidSpan,
	FHitResult& SurfaceHit)
{
	if (HasInvalidWrapEndpoint(TraceContext, ValidSpan) ||
		HasInvalidWrapEndpoint(TraceContext, InvalidSpan))
	{
		SurfaceHit = FHitResult();
		return false;
	}

	FRayRopeNode LastValidStartNode = *ValidSpan.StartNode;
	FRayRopeNode LastValidEndNode = *ValidSpan.EndNode;
	FRayRopeNode LastInvalidStartNode = *InvalidSpan.StartNode;
	FRayRopeNode LastInvalidEndNode = *InvalidSpan.EndNode;
	FRayRopeSpan LastValidSpan{&LastValidStartNode, &LastValidEndNode};
	FRayRopeSpan LastInvalidSpan{&LastInvalidStartNode, &LastInvalidEndNode};
	const float WrapSolverToleranceSquared = FMath::Square(WrapSettings.WrapSolverTolerance);

	for (int32 Iteration = 0; Iteration < WrapSettings.MaxWrapBinarySearchIterations; ++Iteration)
	{
		const bool bStartCloseEnough =
			FVector::DistSquared(LastValidSpan.GetStartLocation(), LastInvalidSpan.GetStartLocation()) <=
			WrapSolverToleranceSquared;

		const bool bEndCloseEnough =
			FVector::DistSquared(LastValidSpan.GetEndLocation(), LastInvalidSpan.GetEndLocation()) <=
			WrapSolverToleranceSquared;

		if (bStartCloseEnough && bEndCloseEnough)
		{
			break;
		}

		FRayRopeNode MidStartNode = LastValidStartNode;
		FRayRopeNode MidEndNode = LastValidEndNode;
		MidStartNode.WorldLocation =
			(LastValidSpan.GetStartLocation() + LastInvalidSpan.GetStartLocation()) * 0.5f;
		MidEndNode.WorldLocation =
			(LastValidSpan.GetEndLocation() + LastInvalidSpan.GetEndLocation()) * 0.5f;
		const FRayRopeSpan MidSpan{&MidStartNode, &MidEndNode};
		if (HasInvalidWrapEndpoint(TraceContext, MidSpan))
		{
			LastInvalidStartNode = MidStartNode;
			LastInvalidEndNode = MidEndNode;
			continue;
		}

		FHitResult MidSurfaceHit;
		if (FRayRopeTrace::TryTraceSpan(TraceContext, MidSpan, MidSurfaceHit))
		{
			LastInvalidStartNode = MidStartNode;
			LastInvalidEndNode = MidEndNode;
			continue;
		}

		LastValidStartNode = MidStartNode;
		LastValidEndNode = MidEndNode;
	}

	if (HasInvalidWrapEndpoint(TraceContext, LastInvalidSpan))
	{
		SurfaceHit = FHitResult();
		return false;
	}

	return FRayRopeTrace::TryTraceSpan(TraceContext, LastInvalidSpan, SurfaceHit);
}

bool FRayRopeWrapSolver::TryCreateRedirectNode(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeWrapSettings& WrapSettings,
	const FRayRopeSpan& ValidSpan,
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit,
	FRayRopeNode& OutNode)
{
	OutNode = FRayRopeNode();

	FRayRopeNode RedirectNode;
	RedirectNode.NodeType = ERayRopeNodeType::Redirect;
	RedirectNode.AttachedActor = ResolveRedirectAttachedActor(FrontSurfaceHit, BackSurfaceHit);
	const FVector RedirectLocation = FRayRopeWrapGeometry::CalculateRedirectLocation(
		ValidSpan,
		FrontSurfaceHit,
		BackSurfaceHit);
	const FVector RedirectOffset =
		FRayRopeWrapGeometry::CalculateRedirectOffset(
			WrapSettings,
			FrontSurfaceHit,
			BackSurfaceHit);
	RedirectNode.WorldLocation = RedirectLocation + RedirectOffset;
	if (RedirectNode.WorldLocation.ContainsNaN() ||
		FRayRopeTrace::IsPointInsideGeometry(TraceContext, RedirectNode.WorldLocation))
	{
		return false;
	}

	FRayRopeNodeResolver::CacheAttachedActorOffset(RedirectNode);
	OutNode = MoveTemp(RedirectNode);
	return true;
}

void FRayRopeWrapSolver::AppendRedirectNodes(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeWrapSettings& WrapSettings,
	const FRayWrapRedirectInput& RedirectInput,
	FRayRopeWrapNodeBuffer& OutNodes)
{
	// Redirect rules: one surface -> one redirect, corner -> one redirect, parallel planes -> two redirects.
	if (RedirectInput.GetBackSurfaceHitPtr() == nullptr ||
		!FRayRopeWrapGeometry::AreDirectionsNearlyCollinear(
			RedirectInput.FrontSurfaceHit.ImpactNormal,
			RedirectInput.GetBackSurfaceHitPtr()->ImpactNormal,
			WrapSettings.GeometryCollinearityTolerance))
	{
		FRayRopeNode RedirectNode;
		if (TryCreateRedirectNode(
			TraceContext,
			WrapSettings,
			RedirectInput.ValidSpan,
			RedirectInput.FrontSurfaceHit,
			RedirectInput.GetBackSurfaceHitPtr(),
			RedirectNode))
		{
			OutNodes.Add(MoveTemp(RedirectNode));
		}
		return;
	}

	FRayRopeNode FirstNode;
	if (!TryCreateRedirectNode(
		TraceContext,
		WrapSettings,
		RedirectInput.ValidSpan,
		RedirectInput.FrontSurfaceHit,
		nullptr,
		FirstNode))
	{
		return;
	}

	FRayRopeNode SecondNode;
	if (!TryCreateRedirectNode(
		TraceContext,
		WrapSettings,
		RedirectInput.ValidSpan,
		*RedirectInput.GetBackSurfaceHitPtr(),
		nullptr,
		SecondNode))
	{
		return;
	}

	if (FVector::DistSquared(RedirectInput.ValidSpan.GetStartLocation(), FirstNode.WorldLocation) >
		FVector::DistSquared(RedirectInput.ValidSpan.GetStartLocation(), SecondNode.WorldLocation))
	{
		Swap(FirstNode, SecondNode);
	}

	OutNodes.Add(MoveTemp(FirstNode));
	OutNodes.Add(MoveTemp(SecondNode));
}

AActor* FRayRopeWrapSolver::ResolveRedirectAttachedActor(
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

bool FRayRopeWrapSolver::CanInsertWrapNode(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeWrapSettings& WrapSettings,
	int32 InsertIndex,
	const FRayRopeSegment& Segment,
	const FRayRopeNode& Candidate,
	const FRayRopePendingInsertionBuffer& PendingInsertions)
{
	if (!Segment.Nodes.IsValidIndex(InsertIndex - 1) ||
		!Segment.Nodes.IsValidIndex(InsertIndex))
	{
		return false;
	}

	if (Candidate.NodeType == ERayRopeNodeType::Redirect &&
		!IsValidRedirectCandidateLocation(TraceContext, Candidate))
	{
		return false;
	}

	const FRayRopeNode& PrevNode = Segment.Nodes[InsertIndex - 1];
	const FRayRopeNode& NextNode = Segment.Nodes[InsertIndex];
	if (AreEquivalentWrapNodes(WrapSettings, Candidate, PrevNode) ||
		AreEquivalentWrapNodes(WrapSettings, Candidate, NextNode))
	{
		return false;
	}

	for (const TPair<int32, FRayRopeNode>& PendingInsertion : PendingInsertions)
	{
		if (PendingInsertion.Key == InsertIndex &&
			AreEquivalentWrapNodes(WrapSettings, PendingInsertion.Value, Candidate))
		{
			return false;
		}
	}

	return true;
}

bool FRayRopeWrapSolver::AreEquivalentWrapNodes(
	const FRayRopeWrapSettings& WrapSettings,
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

	const bool bBothAttached =
		FirstNode.AttachedActor == SecondNode.AttachedActor &&
		IsValid(FirstNode.AttachedActor) &&
		FirstNode.bUseAttachedActorOffset &&
		SecondNode.bUseAttachedActorOffset;

	if (bBothAttached)
	{
		return FirstNode.AttachedActorOffset.Equals(
			SecondNode.AttachedActorOffset,
			WrapSettings.WrapSolverTolerance
		);
	}

	const bool bBothWorldSpace =
		!IsValid(FirstNode.AttachedActor) &&
		!IsValid(SecondNode.AttachedActor);

	return bBothWorldSpace &&
		FirstNode.WorldLocation.Equals(SecondNode.WorldLocation, WrapSettings.WrapSolverTolerance);
}
