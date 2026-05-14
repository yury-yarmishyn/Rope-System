#include "RayRopeNodeInsertionQueue.h"

#include "RayRopeNodeBuilder.h"

namespace
{
bool AreEquivalentNodes(
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
		// Actor-relative redirects should be compared in local space so moving geometry does not
		// produce duplicate nodes just because the actor moved between solver passes.
		return FirstNode.AttachedActorOffset.Equals(
			SecondNode.AttachedActorOffset,
			Settings.WrapSolverTolerance);
	}

	return FirstNode.WorldLocation.Equals(
		SecondNode.WorldLocation,
		Settings.WrapSolverTolerance);
}
}

bool FRayRopeNodeInsertionQueue::CanInsertNodes(
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

bool FRayRopeNodeInsertionQueue::CanInsertNodesInSegment(
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

void FRayRopeNodeInsertionQueue::AppendPendingInsertions(
	int32 InsertIndex,
	FRayRopeBuiltNodeBuffer& Nodes,
	FRayRopePendingNodeInsertionBuffer& PendingInsertions)
{
	for (FRayRopeNode& Node : Nodes)
	{
		PendingInsertions.Emplace(InsertIndex, MoveTemp(Node));
	}
}

void FRayRopeNodeInsertionQueue::SortPendingInsertions(
	FRayRopePendingNodeInsertionBuffer& PendingInsertions)
{
	if (PendingInsertions.Num() > 1)
	{
		PendingInsertions.StableSort(
			[](const TPair<int32, FRayRopeNode>& Left, const TPair<int32, FRayRopeNode>& Right)
			{
				return Left.Key < Right.Key;
			});
	}
}

int32 FRayRopeNodeInsertionQueue::ApplyPendingInsertions(
	FRayRopeSegment& Segment,
	FRayRopePendingNodeInsertionBuffer& PendingInsertions,
	bool bAlreadySorted)
{
	if (PendingInsertions.Num() == 0)
	{
		return 0;
	}

	const int32 AppliedInsertionCount = PendingInsertions.Num();
	if (!bAlreadySorted)
	{
		SortPendingInsertions(PendingInsertions);
	}

	TArray<FRayRopeNode> OriginalNodes = MoveTemp(Segment.Nodes);
	Segment.Nodes.Reserve(OriginalNodes.Num() + PendingInsertions.Num());

	int32 PendingIndex = 0;
	for (int32 NodeIndex = 0; NodeIndex < OriginalNodes.Num(); ++NodeIndex)
	{
		while (PendingInsertions.IsValidIndex(PendingIndex) &&
			PendingInsertions[PendingIndex].Key == NodeIndex)
		{
			Segment.Nodes.Add(MoveTemp(PendingInsertions[PendingIndex].Value));
			++PendingIndex;
		}

		Segment.Nodes.Add(MoveTemp(OriginalNodes[NodeIndex]));
	}

	while (PendingInsertions.IsValidIndex(PendingIndex))
	{
		Segment.Nodes.Add(MoveTemp(PendingInsertions[PendingIndex].Value));
		++PendingIndex;
	}

	PendingInsertions.Reset();
	return AppliedInsertionCount;
}
