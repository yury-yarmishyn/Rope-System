#include "RayRopeNodeBuilder.h"

#include "Geometry/RayRopeSurfaceGeometry.h"
#include "Debug/RayRopeDebugContext.h"
#include "Interfaces/RayRopeInterface.h"
#include "RayRopeNodeSynchronizer.h"
#include "Topology/RayRopeSegmentTopology.h"
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

struct FRayRopeBoundarySearchBounds
{
	FRayRopeNode ClearStartNode;
	FRayRopeNode ClearEndNode;
	FRayRopeNode BlockedStartNode;
	FRayRopeNode BlockedEndNode;

	bool IsResolved(float ToleranceSquared) const
	{
		return FVector::DistSquared(ClearStartNode.WorldLocation, BlockedStartNode.WorldLocation) <=
				ToleranceSquared &&
			FVector::DistSquared(ClearEndNode.WorldLocation, BlockedEndNode.WorldLocation) <=
				ToleranceSquared;
	}

	void BuildMidSpan(FRayRopeNode& OutStartNode, FRayRopeNode& OutEndNode) const
	{
		OutStartNode = ClearStartNode;
		OutEndNode = ClearEndNode;
		OutStartNode.WorldLocation =
			(ClearStartNode.WorldLocation + BlockedStartNode.WorldLocation) * 0.5f;
		OutEndNode.WorldLocation =
			(ClearEndNode.WorldLocation + BlockedEndNode.WorldLocation) * 0.5f;
	}

	void MarkBlocked(const FRayRopeNode& StartNode, const FRayRopeNode& EndNode)
	{
		BlockedStartNode = StartNode;
		BlockedEndNode = EndNode;
	}

	void MarkClear(const FRayRopeNode& StartNode, const FRayRopeNode& EndNode)
	{
		ClearStartNode = StartNode;
		ClearEndNode = EndNode;
	}

	FRayRopeSpan GetBlockedSpan() const
	{
		return FRayRopeSpan{&BlockedStartNode, &BlockedEndNode};
	}
};

bool TryBuildNodeSpanPair(
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
	const FRayRopeSpan& BlockedSpan,
	const FRayRopeSpan& ReferenceSpan,
	const FRayRopeSpan& ReversedCurrentSpan,
	FRayRopeRedirectBuildInput& OutInput);

bool TryFindBoundaryHit(
	const FRayRopeTraceContext& TraceContext,
	const FRayRopeNodeBuildSettings& Settings,
	const FRayRopeSpan& ClearSpan,
	const FRayRopeSpan& BlockedSpan,
	FHitResult& SurfaceHit);

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

bool IsMovableRedirectComponent(const UPrimitiveComponent* Component)
{
	return IsValid(Component) &&
		(Component->Mobility == EComponentMobility::Movable ||
			Component->IsSimulatingPhysics());
}

bool IsMovableRedirectActor(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}

	const USceneComponent* RootComponent = Actor->GetRootComponent();
	return IsValid(RootComponent) && RootComponent->Mobility == EComponentMobility::Movable;
}

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
	if (IsMovableRedirectComponent(HitComponent))
	{
		return false;
	}

	const AActor* HitActor = SurfaceHit.GetActor();
	if (!IsValid(HitActor))
	{
		return HitComponent != nullptr;
	}

	return !IsMovableRedirectActor(HitActor);
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

bool HasInvalidNodeBuilderSpanLocation(const FRayRopeSpan& Span)
{
	return !Span.IsValid() ||
		Span.GetStartLocation().ContainsNaN() ||
		Span.GetEndLocation().ContainsNaN();
}

void RecordNodeBuilderEvent(
	const FRayRopeTraceContext& TraceContext,
	const FString& Message)
{
	if (TraceContext.DebugContext == nullptr)
	{
		return;
	}

	TraceContext.DebugContext->RecordSolverEvent(TEXT("NodeBuilder"), Message);
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
		NodeIndex,
		CurrentNodes,
		ReferenceNodes,
		CurrentSpan,
		ReferenceSpan))
	{
		RecordNodeBuilderEvent(
			TraceContext,
			FString::Printf(
				TEXT("Span[%d] rejected: current/reference span pair unavailable CurrentNodes=%d ReferenceNodes=%d"),
				NodeIndex,
				CurrentNodes.Num(),
				ReferenceNodes.Num()));
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

	if (HasInvalidNodeBuilderSpanLocation(CurrentSpan) ||
		HasInvalidNodeBuilderSpanLocation(ReferenceSpan))
	{
		RecordNodeBuilderEvent(
			TraceContext,
			TEXT("Rejected: invalid current or reference span location"));
		return false;
	}

	FHitResult FrontSurfaceHit;
	if (!FRayRopeTrace::TryTraceSpan(TraceContext, CurrentSpan, FrontSurfaceHit))
	{
		RecordNodeBuilderEvent(
			TraceContext,
			TEXT("Rejected: current span is clear"));
		return false;
	}

	if (HasInvalidNodeBuilderEndpoint(TraceContext, CurrentSpan) ||
		HasInvalidNodeBuilderEndpoint(TraceContext, ReferenceSpan))
	{
		RecordNodeBuilderEvent(
			TraceContext,
			TEXT("Rejected: current or reference endpoint is invalid/overlapping geometry"));
		return false;
	}

	if (TryBuildAnchorNode(FrontSurfaceHit, OutNodes))
	{
		RecordNodeBuilderEvent(
			TraceContext,
			FString::Printf(
				TEXT("Built anchor node from hit actor %s"),
				*GetNameSafe(FrontSurfaceHit.GetActor())));
		return true;
	}

	const bool bBuiltRedirectNodes = TryBuildRedirectNodes(
		TraceContext,
		Settings,
		CurrentSpan,
		ReferenceSpan,
		FrontSurfaceHit,
		OutNodes);
	if (!bBuiltRedirectNodes)
	{
		RecordNodeBuilderEvent(
			TraceContext,
			FString::Printf(
				TEXT("Rejected: failed to build redirect from hit actor %s component %s"),
				*GetNameSafe(FrontSurfaceHit.GetActor()),
				*GetNameSafe(FrontSurfaceHit.GetComponent())));
	}

	return bBuiltRedirectNodes;
}

namespace
{
bool TryBuildNodeSpanPair(
	int32 NodeIndex,
	TConstArrayView<FRayRopeNode> CurrentNodes,
	TConstArrayView<FRayRopeNode> ReferenceNodes,
	FRayRopeSpan& OutCurrentSpan,
	FRayRopeSpan& OutReferenceSpan)
{
	return FRayRopeSegmentTopology::TryGetNodeSpan(CurrentNodes, NodeIndex, OutCurrentSpan) &&
		FRayRopeSegmentTopology::TryGetNodeSpan(ReferenceNodes, NodeIndex, OutReferenceSpan);
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
		RecordNodeBuilderEvent(
			TraceContext,
			FString::Printf(
				TEXT("Redirect rejected: hit actor %s is movable/simulating and movable wrapping is disabled"),
				*GetNameSafe(FrontSurfaceHit.GetActor())));
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
		RecordNodeBuilderEvent(
			TraceContext,
			TEXT("Redirect rejected: could not build boundary or already-blocked redirect input"));
		return false;
	}

	AppendRedirectNodes(TraceContext, Settings, RedirectInput, OutNodes);
	if (TraceContext.DebugContext != nullptr)
	{
		for (const FRayRopeNode& Node : OutNodes)
		{
			TraceContext.DebugContext->DrawSolverPoint(
				ERayRopeDebugDrawFlags::Wrap,
				Node.WorldLocation,
				TraceContext.DebugContext->GetSettings().DebugCandidateColor,
				TEXT("Redirect"));
		}
	}
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

	const FRayRopeSpan ReversedCurrentSpan{CurrentSpan.EndNode, CurrentSpan.StartNode};
	// If the reference span is already blocked, there is no clear boundary to recover. Use the
	// current hit directly and optionally pair it with a reverse hit for corner/parallel handling.
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
	const FRayRopeSpan& BlockedSpan,
	const FRayRopeSpan& ReferenceSpan,
	const FRayRopeSpan& ReversedCurrentSpan,
	FRayRopeRedirectBuildInput& OutInput)
{
	if (!TryFindBoundaryHit(
		TraceContext,
		Settings,
		ReferenceSpan,
		BlockedSpan,
		OutInput.FrontSurfaceHit))
	{
		RecordNodeBuilderEvent(
			TraceContext,
			TEXT("Boundary redirect rejected: no front boundary hit"));
		return false;
	}

	if (!CanUseHitForRedirectNode(
		OutInput.FrontSurfaceHit,
		Settings.bAllowWrapOnMovableObjects))
	{
		RecordNodeBuilderEvent(
			TraceContext,
			FString::Printf(
				TEXT("Boundary redirect rejected: front hit actor %s cannot receive redirect"),
				*GetNameSafe(OutInput.FrontSurfaceHit.GetActor())));
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
		RecordNodeBuilderEvent(
			TraceContext,
			TEXT("Boundary search rejected: invalid clear or blocked span"));
		return false;
	}

	FRayRopeBoundarySearchBounds Bounds{
		*ClearSpan.StartNode,
		*ClearSpan.EndNode,
		*BlockedSpan.StartNode,
		*BlockedSpan.EndNode
	};
	const float WrapSolverToleranceSquared = FMath::Square(Settings.WrapSolverTolerance);

	// Binary-search both endpoints between the last known clear span and the first blocked span.
	// The final blocked span gives a stable surface hit close to the moment wrapping became necessary.
	const int32 MaxSearchIterations = FMath::Max(0, Settings.MaxWrapBinarySearchIterations);
	for (int32 Iteration = 0; Iteration < MaxSearchIterations; ++Iteration)
	{
		if (Bounds.IsResolved(WrapSolverToleranceSquared))
		{
			break;
		}

		FRayRopeNode MidStartNode;
		FRayRopeNode MidEndNode;
		Bounds.BuildMidSpan(MidStartNode, MidEndNode);
		const FRayRopeSpan MidSpan{&MidStartNode, &MidEndNode};
		if (HasInvalidNodeBuilderEndpoint(TraceContext, MidSpan))
		{
			RecordNodeBuilderEvent(
				TraceContext,
				FString::Printf(
					TEXT("Boundary search iteration %d marked blocked: midpoint endpoint invalid"),
					Iteration));
			Bounds.MarkBlocked(MidStartNode, MidEndNode);
			continue;
		}

		if (FRayRopeTrace::HasBlockingSpanHit(TraceContext, MidSpan))
		{
			Bounds.MarkBlocked(MidStartNode, MidEndNode);
			continue;
		}

		Bounds.MarkClear(MidStartNode, MidEndNode);
	}

	const FRayRopeSpan LastBlockedSpan = Bounds.GetBlockedSpan();
	if (HasInvalidNodeBuilderEndpoint(TraceContext, LastBlockedSpan))
	{
		SurfaceHit = FHitResult();
		RecordNodeBuilderEvent(
			TraceContext,
			TEXT("Boundary search rejected: final blocked span endpoint invalid"));
		return false;
	}

	return FRayRopeTrace::TryTraceSpan(TraceContext, LastBlockedSpan, SurfaceHit);
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
		FRayRopeSurfaceGeometry::CalculateSurfaceOffsetDirection(FrontSurfaceHit, BackSurfaceHit) *
		Settings.WrapSurfaceOffset;
	RedirectNode.WorldLocation = RedirectLocation + RedirectOffset;
	if (!FRayRopeTrace::IsValidFreePoint(TraceContext, RedirectNode.WorldLocation))
	{
		RecordNodeBuilderEvent(
			TraceContext,
			FString::Printf(
				TEXT("Redirect rejected: candidate point overlaps geometry Location=%s"),
				*RedirectNode.WorldLocation.ToCompactString()));
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

	// A single surface or corner can be represented by one redirect. Parallel opposing planes need
	// two ordered redirects so the rope runs around both sides instead of cutting through the gap.
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
