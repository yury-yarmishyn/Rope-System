#include "RayRopeComponent.h"

#include "Solvers/RayRopeMoveSolver.h"
#include "Solvers/RayRopeNodeResolver.h"
#include "Solvers/RayRopePhysicsSolver.h"
#include "Solvers/RayRopeTopology.h"
#include "Solvers/RayRopeWrapSolver.h"

#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
FColor ToDebugColor(const FLinearColor& Color)
{
	return Color.ToFColor(true);
}

const TCHAR* GetRopeNodeTypeName(ERayRopeNodeType NodeType)
{
	switch (NodeType)
	{
	case ERayRopeNodeType::Anchor:
		return TEXT("Anchor");

	case ERayRopeNodeType::Redirect:
		return TEXT("Redirect");

	default:
		return TEXT("Unknown");
	}
}

float CalculateSegmentLength(const FRayRopeSegment& Segment)
{
	float SegmentLength = 0.f;
	for (int32 NodeIndex = 1; NodeIndex < Segment.Nodes.Num(); ++NodeIndex)
	{
		SegmentLength += FVector::Dist(
			Segment.Nodes[NodeIndex - 1].WorldLocation,
			Segment.Nodes[NodeIndex].WorldLocation);
	}

	return SegmentLength;
}

FVector CalculateSegmentCenter(const FRayRopeSegment& Segment)
{
	if (Segment.Nodes.Num() == 0)
	{
		return FVector::ZeroVector;
	}

	FVector Center = FVector::ZeroVector;
	for (const FRayRopeNode& Node : Segment.Nodes)
	{
		Center += Node.WorldLocation;
	}

	return Center / static_cast<float>(Segment.Nodes.Num());
}
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

	if (RopeSegments.Num() == 0)
	{
		TickDebug(TEXT("TickEmpty"));
		return;
	}

	if (!bIsRopeSolving)
	{
		SyncRopeNodes();
		RefreshRopeLength();
		if (ApplyRopeRuntimeEffects())
		{
			SyncRopeNodes();
			RefreshRopeLength();
			OnRopeSegmentsUpdated.Broadcast();
		}

		TickDebug(TEXT("TickIdle"));
		return;
	}

	SolveRope();

	const bool bAppliedRuntimeEffects = ApplyRopeRuntimeEffects();
	if (bAppliedRuntimeEffects)
	{
		SyncRopeNodes();
		RefreshRopeLength();
	}

	OnRopeSegmentsUpdated.Broadcast();
	TickDebug(TEXT("TickSolve"));
}

bool URayRopeComponent::TryStartRopeSolve(const TArray<AActor*>& AnchorActors)
{
	FRayRopeTraceSettings TraceSettings;
	TraceSettings.World = GetWorld();
	TraceSettings.OwnerActor = GetOwner();
	TraceSettings.TraceChannel = TraceChannel;
	TraceSettings.bTraceComplex = bTraceComplex;

	TArray<FRayRopeSegment> BaseSegments;
	if (!FRayRopeTopology::TryBuildBaseSegments(
		TraceSettings,
		AnchorActors,
		BaseSegments))
	{
		if (IsDebugLogEnabled())
		{
			UE_LOG(
				LogRayRope,
				Log,
				TEXT("[Debug] TryStartRopeSolve failed. Owner=%s AnchorActors=%d"),
				*GetNameSafe(GetOwner()),
				AnchorActors.Num());
		}

		LogDebugRopeState(TEXT("TryStartRopeSolveFailed"), true);
		return false;
	}

	const bool bWasSolvingRope = bIsRopeSolving;
	bIsRopeSolving = true;
	SetSegments(MoveTemp(BaseSegments));

	if (!bWasSolvingRope)
	{
		OnRopeSolveStarted.Broadcast();
	}

	LogDebugRopeState(TEXT("TryStartRopeSolve"), true);
	return true;
}

void URayRopeComponent::EndRopeSolve()
{
	if (!bIsRopeSolving)
	{
		return;
	}

	SyncRopeNodes();
	RefreshRopeLength();
	bIsRopeSolving = false;
	OnRopeSolveEnded.Broadcast();
	LogDebugRopeState(TEXT("EndRopeSolve"), true);
}

bool URayRopeComponent::BreakRopeOnSegment(int32 SegmentIndex)
{
	if (!RopeSegments.IsValidIndex(SegmentIndex))
	{
		if (IsDebugLogEnabled())
		{
			UE_LOG(
				LogRayRope,
				Log,
				TEXT("[Debug] BreakRopeOnSegment ignored invalid segment. Owner=%s SegmentIndex=%d RopeSegments=%d"),
				*GetNameSafe(GetOwner()),
				SegmentIndex,
				RopeSegments.Num());
		}

		return false;
	}

	RopeSegments.RemoveAt(SegmentIndex, 1, EAllowShrinking::No);

	const bool bHasSegments = RopeSegments.Num() > 0;
	const bool bWasSolvingRope = bIsRopeSolving;
	if (!bHasSegments)
	{
		bIsRopeSolving = false;
	}

	RefreshRopeLength();
	OnRopeSegmentsUpdated.Broadcast();
	OnRopeSegmentBroken.Broadcast(SegmentIndex);

	if (!bHasSegments)
	{
		if (bWasSolvingRope)
		{
			OnRopeSolveEnded.Broadcast();
		}

		OnRopeBroken.Broadcast();
	}

	LogDebugRopeState(TEXT("BreakRopeOnSegment"), true);
	return true;
}

void URayRopeComponent::BreakRope()
{
	const bool bHadSegments = RopeSegments.Num() > 0;
	const bool bWasSolvingRope = bIsRopeSolving;
	if (!bHadSegments && !bWasSolvingRope)
	{
		return;
	}

	RopeSegments.Reset();
	bIsRopeSolving = false;
	RefreshRopeLength();

	if (bHadSegments)
	{
		OnRopeSegmentsUpdated.Broadcast();
	}

	if (bWasSolvingRope)
	{
		OnRopeSolveEnded.Broadcast();
	}

	OnRopeBroken.Broadcast();
	LogDebugRopeState(TEXT("BreakRope"), true);
}

const TArray<FRayRopeSegment>& URayRopeComponent::GetSegments() const
{
	return RopeSegments;
}

void URayRopeComponent::SetSegments(TArray<FRayRopeSegment> NewSegments)
{
	RopeSegments = MoveTemp(NewSegments);
	SyncRopeNodes();
	RefreshRopeLength();
	OnRopeSegmentsUpdated.Broadcast();
	LogDebugRopeState(TEXT("SetSegments"), true);
}

void URayRopeComponent::SetRopeDebugEnabled(bool bEnabled)
{
	RopeDebugSettings.bDebugEnabled = bEnabled;
	NextDebugLogTimeSeconds = 0.f;

	TickDebug(bEnabled ? TEXT("SetRopeDebugEnabled") : TEXT("SetRopeDebugDisabled"));
}

bool URayRopeComponent::IsRopeDebugEnabled() const
{
	return RopeDebugSettings.bDebugEnabled;
}

void URayRopeComponent::SyncRopeNodes()
{
	for (FRayRopeSegment& Segment : RopeSegments)
	{
		FRayRopeNodeResolver::SyncSegmentNodes(Segment);
	}
}

void URayRopeComponent::RefreshRopeLength()
{
	CurrentRopeLength = FRayRopeTopology::CalculateRopeLength(RopeSegments);
}

bool URayRopeComponent::ApplyRopeRuntimeEffects()
{
	FRayRopePhysicsSettings PhysicsSettings;
	PhysicsSettings.CurrentRopeLength = CurrentRopeLength;
	PhysicsSettings.MaxAllowedRopeLength = MaxAllowedRopeLength;

	return FRayRopePhysicsSolver::Solve(
		GetOwner(),
		RopeSegments,
		PhysicsSettings);
}

void URayRopeComponent::SolveRope()
{
	if (!bIsRopeSolving || RopeSegments.Num() == 0)
	{
		return;
	}

	for (int32 SegmentIndex = 0; SegmentIndex < RopeSegments.Num(); ++SegmentIndex)
	{
		FRayRopeSegment& Segment = RopeSegments[SegmentIndex];
		if (Segment.Nodes.Num() < 2)
		{
			continue;
		}

		SolveSegment(Segment, SegmentIndex);
	}

	FinalizeSolve();
}

void URayRopeComponent::SolveSegment(FRayRopeSegment& Segment, int32 SegmentIndex) const
{
	FRayRopeTraceSettings TraceSettings;
	TraceSettings.World = GetWorld();
	TraceSettings.OwnerActor = GetOwner();
	TraceSettings.TraceChannel = TraceChannel;
	TraceSettings.bTraceComplex = bTraceComplex;

	FRayRopeWrapSettings WrapSettings;
	WrapSettings.bAllowWrapOnMovableObjects = bAllowWrapOnMovableObjects;
	WrapSettings.MaxWrapBinarySearchIterations = MaxWrapBinarySearchIterations;
	WrapSettings.WrapSolverTolerance = WrapSolverTolerance;
	WrapSettings.GeometryCollinearityTolerance = RelaxCollinearityTolerance;
	WrapSettings.WrapSurfaceOffset = WrapSurfaceOffset;

	FRayRopeMoveSettings MoveSettings;
	MoveSettings.MoveSolverTolerance = MoveSolverTolerance;
	MoveSettings.PlaneParallelTolerance = MovePlaneParallelTolerance;
	MoveSettings.EffectivePointSearchTolerance = MoveEffectivePointSearchTolerance;
	MoveSettings.MaxMoveIterations = MaxMoveIterations;
	MoveSettings.MaxEffectivePointSearchIterations = MaxEffectivePointSearchIterations;

	FRayRopeRelaxSettings RelaxSettings;
	RelaxSettings.RelaxSolverTolerance = RelaxSolverTolerance;
	RelaxSettings.RelaxCollinearityTolerance = RelaxCollinearityTolerance;
	RelaxSettings.MaxRelaxCollapseIterations = MaxRelaxCollapseIterations;

	const int32 InitialNodeCount = Segment.Nodes.Num();
	const FRayRopeSegment ReferenceSegment = Segment;
	FRayRopeNodeResolver::SyncSegmentNodes(Segment);
	FRayRopeMoveSolver::MoveSegment(TraceSettings, MoveSettings, Segment);
	FRayRopeWrapSolver::WrapSegment(TraceSettings, WrapSettings, Segment, ReferenceSegment);
	FRayRopeTopology::RelaxSegment(TraceSettings, RelaxSettings, Segment);

	if (IsDebugLogEnabled() && Segment.Nodes.Num() != InitialNodeCount)
	{
		UE_LOG(
			LogRayRope,
			Log,
			TEXT("[Debug] SolveSegment[%d] node count changed: %d -> %d"),
			SegmentIndex,
			InitialNodeCount,
			Segment.Nodes.Num());
	}
}

void URayRopeComponent::FinalizeSolve()
{
	FRayRopeTopology::SplitSegmentsOnAnchors(RopeSegments);
	RefreshRopeLength();
}

bool URayRopeComponent::IsDebugDrawEnabled() const
{
	return RopeDebugSettings.bDebugEnabled && RopeDebugSettings.bDrawDebugRope;
}

bool URayRopeComponent::IsDebugLogEnabled() const
{
	return RopeDebugSettings.bDebugEnabled && RopeDebugSettings.bLogDebugState;
}

void URayRopeComponent::TickDebug(const TCHAR* Context)
{
	DrawDebugRope();
	LogDebugRopeState(Context);
}

void URayRopeComponent::DrawDebugRope() const
{
	if (!IsDebugDrawEnabled())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	const float LifeTime = FMath::Max(0.f, RopeDebugSettings.DebugDrawLifetime);
	const int32 NodeSegments = FMath::Max(4, RopeDebugSettings.DebugNodeSphereSegments);
	const float NodeRadius = FMath::Max(0.f, RopeDebugSettings.DebugNodeRadius);
	const float SegmentThickness = FMath::Max(0.f, RopeDebugSettings.DebugSegmentThickness);
	const float AttachmentLinkThickness = FMath::Max(0.f, RopeDebugSettings.DebugAttachmentLinkThickness);
	const FColor OwnerColor = ToDebugColor(RopeDebugSettings.DebugOwnerColor);
	const FColor SegmentColor = ToDebugColor(RopeDebugSettings.DebugSegmentColor);
	const FColor AnchorNodeColor = ToDebugColor(RopeDebugSettings.DebugAnchorNodeColor);
	const FColor RedirectNodeColor = ToDebugColor(RopeDebugSettings.DebugRedirectNodeColor);
	const FColor AttachmentLinkColor = ToDebugColor(RopeDebugSettings.DebugAttachmentLinkColor);

	if (const AActor* OwnerActor = GetOwner())
	{
		const FVector OwnerLocation = OwnerActor->GetActorLocation();
		DrawDebugCoordinateSystem(
			World,
			OwnerLocation,
			OwnerActor->GetActorRotation(),
			FMath::Max(0.f, RopeDebugSettings.DebugOwnerAxisLength),
			false,
			LifeTime,
			0,
			SegmentThickness);

		DrawDebugSphere(
			World,
			OwnerLocation,
			FMath::Max(1.f, NodeRadius * 0.5f),
			NodeSegments,
			OwnerColor,
			false,
			LifeTime,
			0,
			SegmentThickness);

		if (RopeDebugSettings.bDrawDebugLabels)
		{
			DrawDebugString(
				World,
				OwnerLocation + FVector(0.f, 0.f, NodeRadius * 2.f),
				FString::Printf(
					TEXT("RopeComponent RopeSegments:%d Length:%.1f/%.1f %s"),
					RopeSegments.Num(),
					CurrentRopeLength,
					MaxAllowedRopeLength,
					bIsRopeSolving ? TEXT("Solving") : TEXT("Idle")),
				nullptr,
				OwnerColor,
				LifeTime,
				true);
		}
	}

	for (int32 SegmentIndex = 0; SegmentIndex < RopeSegments.Num(); ++SegmentIndex)
	{
		const FRayRopeSegment& Segment = RopeSegments[SegmentIndex];
		for (int32 NodeIndex = 1; NodeIndex < Segment.Nodes.Num(); ++NodeIndex)
		{
			DrawDebugLine(
				World,
				Segment.Nodes[NodeIndex - 1].WorldLocation,
				Segment.Nodes[NodeIndex].WorldLocation,
				SegmentColor,
				false,
				LifeTime,
				0,
				SegmentThickness);
		}

		if (RopeDebugSettings.bDrawDebugLabels && Segment.Nodes.Num() > 0)
		{
			DrawDebugString(
				World,
				CalculateSegmentCenter(Segment),
				FString::Printf(
					TEXT("Segment[%d] Nodes:%d Length:%.1f"),
					SegmentIndex,
					Segment.Nodes.Num(),
					CalculateSegmentLength(Segment)),
				nullptr,
				SegmentColor,
				LifeTime,
				true);
		}

		for (int32 NodeIndex = 0; NodeIndex < Segment.Nodes.Num(); ++NodeIndex)
		{
			const FRayRopeNode& Node = Segment.Nodes[NodeIndex];
			const FColor NodeColor = Node.NodeType == ERayRopeNodeType::Anchor
				? AnchorNodeColor
				: RedirectNodeColor;

			DrawDebugSphere(
				World,
				Node.WorldLocation,
				NodeRadius,
				NodeSegments,
				NodeColor,
				false,
				LifeTime,
				0,
				SegmentThickness);

			if (RopeDebugSettings.bDrawDebugAttachmentLinks && IsValid(Node.AttachedActor))
			{
				DrawDebugLine(
					World,
					Node.WorldLocation,
					Node.AttachedActor->GetActorLocation(),
					AttachmentLinkColor,
					false,
					LifeTime,
					0,
					AttachmentLinkThickness);
			}

			if (RopeDebugSettings.bDrawDebugLabels)
			{
				DrawDebugString(
					World,
					Node.WorldLocation + FVector(0.f, 0.f, NodeRadius),
					FString::Printf(
						TEXT("S%d N%d %s"),
						SegmentIndex,
						NodeIndex,
						GetRopeNodeTypeName(Node.NodeType)),
					nullptr,
					NodeColor,
					LifeTime,
					true);
			}
		}
	}
}

void URayRopeComponent::LogDebugRopeState(const TCHAR* Context, bool bForce)
{
	if (!IsDebugLogEnabled())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float CurrentTime = World != nullptr ? World->GetTimeSeconds() : 0.f;
	const float LogInterval = FMath::Max(0.f, RopeDebugSettings.DebugLogIntervalSeconds);
	if (!bForce && LogInterval > 0.f && CurrentTime < NextDebugLogTimeSeconds)
	{
		return;
	}

	NextDebugLogTimeSeconds = CurrentTime + LogInterval;

	const TCHAR* SafeContext = Context != nullptr ? Context : TEXT("Unknown");
	UE_LOG(
		LogRayRope,
		Log,
		TEXT("[Debug] %s Owner=%s Solving=%s RopeSegments=%d CurrentRopeLength=%.2f MaxAllowedRopeLength=%.2f"),
		SafeContext,
		*GetNameSafe(GetOwner()),
		bIsRopeSolving ? TEXT("true") : TEXT("false"),
		RopeSegments.Num(),
		CurrentRopeLength,
		MaxAllowedRopeLength);

	for (int32 SegmentIndex = 0; SegmentIndex < RopeSegments.Num(); ++SegmentIndex)
	{
		const FRayRopeSegment& Segment = RopeSegments[SegmentIndex];
		UE_LOG(
			LogRayRope,
			Log,
			TEXT("[Debug] %s Segment[%d] Nodes=%d Length=%.2f"),
			SafeContext,
			SegmentIndex,
			Segment.Nodes.Num(),
			CalculateSegmentLength(Segment));

		for (int32 NodeIndex = 0; NodeIndex < Segment.Nodes.Num(); ++NodeIndex)
		{
			const FRayRopeNode& Node = Segment.Nodes[NodeIndex];
			UE_LOG(
				LogRayRope,
				Log,
				TEXT("[Debug] %s Segment[%d].Node[%d] Type=%s Location=%s AttachedActor=%s UseOffset=%s Offset=%s"),
				SafeContext,
				SegmentIndex,
				NodeIndex,
				GetRopeNodeTypeName(Node.NodeType),
				*Node.WorldLocation.ToCompactString(),
				*GetNameSafe(Node.AttachedActor),
				Node.bUseAttachedActorOffset ? TEXT("true") : TEXT("false"),
				*Node.AttachedActorOffset.ToCompactString());
		}
	}
}
