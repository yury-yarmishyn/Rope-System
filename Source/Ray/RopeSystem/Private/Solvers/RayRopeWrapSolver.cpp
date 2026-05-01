#include "RayRopeWrapSolver.h"

#include "CollisionQueryParams.h"
#include "RayRopeInterface.h"
#include "RayRopeNodeResolver.h"
#include "Solvers/RayRopeTopology.h"
#include "Helpers/RayRopeTrace.h"
#include "Helpers/RayRopeWrapGeometry.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

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

	AppendRedirectNodes(WrapSettings, RedirectInput, OutNodes);

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
	if (!ValidSpan.IsValid() || !InvalidSpan.IsValid())
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
	const float WrapSolverEpsilonSquared = FMath::Square(WrapSettings.WrapSolverEpsilon);

	for (int32 Iteration = 0; Iteration < WrapSettings.MaxBinarySearchIteration; ++Iteration)
	{
		const bool bStartCloseEnough =
			FVector::DistSquared(LastValidSpan.GetStartLocation(), LastInvalidSpan.GetStartLocation()) <=
			WrapSolverEpsilonSquared;

		const bool bEndCloseEnough =
			FVector::DistSquared(LastValidSpan.GetEndLocation(), LastInvalidSpan.GetEndLocation()) <=
			WrapSolverEpsilonSquared;

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

	return FRayRopeTrace::TryTraceSpan(TraceContext, LastInvalidSpan, SurfaceHit);
}

FRayRopeNode FRayRopeWrapSolver::CreateRedirectNode(
	const FRayRopeWrapSettings& WrapSettings,
	const FRayRopeSpan& ValidSpan,
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit)
{
	FRayRopeNode RedirectNode;
	RedirectNode.NodeType = ERayRopeNodeType::Redirect;
	RedirectNode.AttachActor = ResolveRedirectAttachActor(FrontSurfaceHit, BackSurfaceHit);
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
	FRayRopeNodeResolver::CacheAttachActorOffset(RedirectNode);
	return RedirectNode;
}

void FRayRopeWrapSolver::AppendRedirectNodes(
	const FRayRopeWrapSettings& WrapSettings,
	const FRayWrapRedirectInput& RedirectInput,
	FRayRopeWrapNodeBuffer& OutNodes)
{
	// Redirect rules: one surface -> one redirect, corner -> one redirect, parallel planes -> two redirects.
	if (RedirectInput.GetBackSurfaceHitPtr() == nullptr ||
		!FRayRopeWrapGeometry::AreDirectionsNearlyCollinear(
			RedirectInput.FrontSurfaceHit.ImpactNormal,
			RedirectInput.GetBackSurfaceHitPtr()->ImpactNormal,
			WrapSettings.GeometryCollinearEpsilon))
	{
		OutNodes.Add(CreateRedirectNode(
			WrapSettings,
			RedirectInput.ValidSpan,
			RedirectInput.FrontSurfaceHit,
			RedirectInput.GetBackSurfaceHitPtr()));
		return;
	}

	FRayRopeNode FirstNode = CreateRedirectNode(
		WrapSettings,
		RedirectInput.ValidSpan,
		RedirectInput.FrontSurfaceHit,
		nullptr);

	FRayRopeNode SecondNode = CreateRedirectNode(
		WrapSettings,
		RedirectInput.ValidSpan,
		*RedirectInput.GetBackSurfaceHitPtr(),
		nullptr);

	if (FVector::DistSquared(RedirectInput.ValidSpan.GetStartLocation(), FirstNode.WorldLocation) >
		FVector::DistSquared(RedirectInput.ValidSpan.GetStartLocation(), SecondNode.WorldLocation))
	{
		Swap(FirstNode, SecondNode);
	}

	OutNodes.Add(MoveTemp(FirstNode));
	OutNodes.Add(MoveTemp(SecondNode));
}

AActor* FRayRopeWrapSolver::ResolveRedirectAttachActor(
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
		return FirstNode.AttachActor == SecondNode.AttachActor;
	}

	if (FirstNode.NodeType != ERayRopeNodeType::Redirect)
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
			WrapSettings.WrapSolverEpsilon
		);
	}

	const bool bBothWorldSpace =
		!IsValid(FirstNode.AttachActor) &&
		!IsValid(SecondNode.AttachActor);

	return bBothWorldSpace &&
		FirstNode.WorldLocation.Equals(SecondNode.WorldLocation, WrapSettings.WrapSolverEpsilon);
}
