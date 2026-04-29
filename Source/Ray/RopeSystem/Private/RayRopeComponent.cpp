#include "RayRopeComponent.h"

#include "RayRopeMoveSolver.h"
#include "RayRopeNodeResolver.h"
#include "RayRopeTopology.h"
#include "RayRopeWrapSolver.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Actor.h"

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

	if (Segments.Num() == 0)
	{
		return;
	}

	if (!bShouldSolveRope)
	{
		SyncRopeNodes();
		RefreshRopeLength();
		if (EnforceMaxRopeLength())
		{
			SyncRopeNodes();
			RefreshRopeLength();
			OnSegmentsSet.Broadcast();
		}

		return;
	}

	SolveRope();

	if (EnforceMaxRopeLength())
	{
		SyncRopeNodes();
		RefreshRopeLength();
		OnSegmentsSet.Broadcast();
	}
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
		return false;
	}

	const bool bWasSolvingRope = bShouldSolveRope;
	bShouldSolveRope = true;
	SetSegments(MoveTemp(BaseSegments));

	if (!bWasSolvingRope)
	{
		OnRopeSolveStarted.Broadcast();
	}

	return true;
}

void URayRopeComponent::EndRopeSolve()
{
	if (!bShouldSolveRope)
	{
		return;
	}

	SyncRopeNodes();
	RefreshRopeLength();
	bShouldSolveRope = false;
	OnRopeSolveEnded.Broadcast();
}

bool URayRopeComponent::BreakRopeOnSegment(int32 SegmentIndex)
{
	if (!Segments.IsValidIndex(SegmentIndex))
	{
		return false;
	}

	Segments.RemoveAt(SegmentIndex, 1, EAllowShrinking::No);

	const bool bHasSegments = Segments.Num() > 0;
	const bool bWasSolvingRope = bShouldSolveRope;
	if (!bHasSegments)
	{
		bShouldSolveRope = false;
	}

	RefreshRopeLength();
	OnSegmentsSet.Broadcast();
	OnRopeSegmentBroken.Broadcast(SegmentIndex);

	if (!bHasSegments)
	{
		if (bWasSolvingRope)
		{
			OnRopeSolveEnded.Broadcast();
		}

		OnRopeBroken.Broadcast();
	}

	return true;
}

void URayRopeComponent::BreakRope()
{
	const bool bHadSegments = Segments.Num() > 0;
	const bool bWasSolvingRope = bShouldSolveRope;
	if (!bHadSegments && !bWasSolvingRope)
	{
		return;
	}

	Segments.Reset();
	bShouldSolveRope = false;
	RefreshRopeLength();

	if (bHadSegments)
	{
		OnSegmentsSet.Broadcast();
	}

	if (bWasSolvingRope)
	{
		OnRopeSolveEnded.Broadcast();
	}

	OnRopeBroken.Broadcast();
}

const TArray<FRayRopeSegment>& URayRopeComponent::GetSegments() const
{
	return Segments;
}

void URayRopeComponent::SetSegments(TArray<FRayRopeSegment> NewSegments)
{
	Segments = MoveTemp(NewSegments);
	SyncRopeNodes();
	RefreshRopeLength();
	OnSegmentsSet.Broadcast();
}

bool URayRopeComponent::EnforceMaxRopeLength()
{
	if (MaxRopeLength <= 0.f || RopeLength <= MaxRopeLength)
	{
		return false;
	}

	const FRayRopeNode* OwnerNode = nullptr;
	const FRayRopeNode* AdjacentNode = nullptr;
	if (!TryGetOwnerTerminalNodes(OwnerNode, AdjacentNode) ||
		OwnerNode == nullptr ||
		AdjacentNode == nullptr)
	{
		return false;
	}

	return ClampOwnerAnchorToMaxRopeLength(*OwnerNode, *AdjacentNode);
}

bool URayRopeComponent::TryGetOwnerTerminalNodes(
	const FRayRopeNode*& OutOwnerNode,
	const FRayRopeNode*& OutAdjacentNode) const
{
	OutOwnerNode = nullptr;
	OutAdjacentNode = nullptr;

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || Segments.Num() == 0)
	{
		return false;
	}

	const FRayRopeSegment& FirstSegment = Segments[0];
	if (FirstSegment.Nodes.Num() >= 2)
	{
		const FRayRopeNode& FirstNode = FirstSegment.Nodes[0];
		if (FirstNode.NodeType == ENodeType::Anchor &&
			FirstNode.AttachActor == OwnerActor)
		{
			OutOwnerNode = &FirstNode;
			OutAdjacentNode = &FirstSegment.Nodes[1];
			return true;
		}
	}

	const FRayRopeSegment& LastSegment = Segments.Last();
	if (LastSegment.Nodes.Num() >= 2)
	{
		const int32 LastNodeIndex = LastSegment.Nodes.Num() - 1;
		const FRayRopeNode& LastNode = LastSegment.Nodes[LastNodeIndex];
		if (LastNode.NodeType == ENodeType::Anchor &&
			LastNode.AttachActor == OwnerActor)
		{
			OutOwnerNode = &LastNode;
			OutAdjacentNode = &LastSegment.Nodes[LastNodeIndex - 1];
			return true;
		}
	}

	return false;
}

bool URayRopeComponent::ClampOwnerAnchorToMaxRopeLength(
	const FRayRopeNode& OwnerNode,
	const FRayRopeNode& AdjacentNode) const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return false;
	}

	const FVector OwnerAnchorLocation = OwnerNode.WorldLocation;
	const FVector AdjacentLocation = AdjacentNode.WorldLocation;
	const FVector TowardAdjacent = AdjacentLocation - OwnerAnchorLocation;
	const float TerminalSpanLength = TowardAdjacent.Size();
	if (TerminalSpanLength <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float ExcessLength = RopeLength - MaxRopeLength;
	if (ExcessLength <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector OutwardDirection = -TowardAdjacent / TerminalSpanLength;
	RemoveOwnerOutwardVelocity(OutwardDirection);

	const float ClampDistance = FMath::Min(ExcessLength, TerminalSpanLength);
	const FVector TargetAnchorLocation =
		OwnerAnchorLocation + TowardAdjacent.GetSafeNormal() * ClampDistance;
	const FVector ActorDelta = TargetAnchorLocation - OwnerAnchorLocation;
	if (ActorDelta.IsNearlyZero())
	{
		return false;
	}

	const FVector StartActorLocation = OwnerActor->GetActorLocation();
	FHitResult SweepHit;
	OwnerActor->SetActorLocation(
		StartActorLocation + ActorDelta,
		true,
		&SweepHit,
		ETeleportType::None);

	return !OwnerActor->GetActorLocation().Equals(StartActorLocation, KINDA_SMALL_NUMBER);
}

void URayRopeComponent::RemoveOwnerOutwardVelocity(const FVector& OutwardDirection) const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || OutwardDirection.IsNearlyZero())
	{
		return;
	}

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(OwnerActor))
	{
		if (UCharacterMovementComponent* CharacterMovement = OwnerCharacter->GetCharacterMovement())
		{
			const float OutwardSpeed =
				FVector::DotProduct(CharacterMovement->Velocity, OutwardDirection);
			if (OutwardSpeed > 0.f)
			{
				CharacterMovement->Velocity -= OutwardDirection * OutwardSpeed;
			}
		}
	}

	UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
	if (!IsValid(RootPrimitive))
	{
		return;
	}

	if (RootPrimitive->IsSimulatingPhysics())
	{
		const FVector LinearVelocity = RootPrimitive->GetPhysicsLinearVelocity();
		const float OutwardSpeed = FVector::DotProduct(LinearVelocity, OutwardDirection);
		if (OutwardSpeed > 0.f)
		{
			RootPrimitive->AddImpulse(
				-OutwardDirection * OutwardSpeed * RootPrimitive->GetMass(),
				NAME_None,
				true);
		}

		return;
	}

	const FVector LinearVelocity = RootPrimitive->GetComponentVelocity();
	const float OutwardSpeed = FVector::DotProduct(LinearVelocity, OutwardDirection);
	if (OutwardSpeed > 0.f)
	{
		RootPrimitive->ComponentVelocity = LinearVelocity - OutwardDirection * OutwardSpeed;
	}
}

void URayRopeComponent::SyncRopeNodes()
{
	for (FRayRopeSegment& Segment : Segments)
	{
		FRayRopeNodeResolver::SyncSegmentNodes(Segment);
	}
}

void URayRopeComponent::RefreshRopeLength()
{
	RopeLength = FRayRopeTopology::CalculateRopeLength(Segments);
}

void URayRopeComponent::SolveRope()
{
	if (!bShouldSolveRope || Segments.Num() == 0)
	{
		return;
	}

	for (int32 SegmentIndex = 0; SegmentIndex < Segments.Num(); ++SegmentIndex)
	{
		FRayRopeSegment& Segment = Segments[SegmentIndex];
		if (Segment.Nodes.Num() < 2)
		{
			continue;
		}

		SolveSegment(Segment);
	}

	FinalizeSolve();
}

void URayRopeComponent::SolveSegment(FRayRopeSegment& Segment) const
{
	FRayRopeTraceSettings TraceSettings;
	TraceSettings.World = GetWorld();
	TraceSettings.OwnerActor = GetOwner();
	TraceSettings.TraceChannel = TraceChannel;
	TraceSettings.bTraceComplex = bTraceComplex;

	FRayRopeWrapSettings WrapSettings;
	WrapSettings.bAllowWrapOnMovableObjects = bAllowWrapOnMovableObjects;
	WrapSettings.MaxBinarySearchIteration = MaxBinarySearchIteration;
	WrapSettings.WrapSolverEpsilon = WrapSolverEpsilon;
	WrapSettings.GeometryCollinearEpsilon = RelaxCollinearEpsilon;
	WrapSettings.WrapOffset = WrapOffset;

	FRayRopeMoveSettings MoveSettings;

	FRayRopeRelaxSettings RelaxSettings;
	RelaxSettings.RelaxSolverEpsilon = RelaxSolverEpsilon;
	RelaxSettings.RelaxCollinearEpsilon = RelaxCollinearEpsilon;

	const FRayRopeSegment ReferenceSegment = Segment;
	FRayRopeNodeResolver::SyncSegmentNodes(Segment);
	FRayRopeMoveSolver::MoveSegment(
		TraceSettings,
		MoveSettings,
		Segment);
	FRayRopeWrapSolver::WrapSegment(
		TraceSettings,
		WrapSettings,
		Segment,
		ReferenceSegment);
	FRayRopeTopology::RelaxSegment(
		TraceSettings,
		RelaxSettings,
		Segment);
}

void URayRopeComponent::FinalizeSolve()
{
	FRayRopeTopology::SplitSegmentsOnAnchors(Segments);
	RefreshRopeLength();
	OnSegmentsSet.Broadcast();
}
