#pragma once

#include "CoreMinimal.h"

/**
 * Inclusive range of span start indexes in a segment.
 *
 * A segment with N nodes has N - 1 spans, addressed by the index of each span's
 * first node.
 */
struct FRayRopeSpanIndexRange
{
	int32 FirstSpanIndex = 0;
	int32 LastSpanIndex = INDEX_NONE;

	bool IsValid() const
	{
		return FirstSpanIndex <= LastSpanIndex;
	}
};

using FRayRopeAffectedSpanRangeBuffer = TArray<FRayRopeSpanIndexRange, TInlineAllocator<8>>;

/**
 * Shared helpers for affected span range normalization.
 */
struct FRayRopeSpanIndexRangeUtils
{
	static bool TryClampRange(
		const FRayRopeSpanIndexRange& Range,
		int32 MaxSpanIndex,
		FRayRopeSpanIndexRange& OutRange)
	{
		OutRange = FRayRopeSpanIndexRange();
		if (MaxSpanIndex < 0)
		{
			return false;
		}

		OutRange.FirstSpanIndex = FMath::Clamp(Range.FirstSpanIndex, 0, MaxSpanIndex);
		OutRange.LastSpanIndex = FMath::Clamp(Range.LastSpanIndex, 0, MaxSpanIndex);
		return OutRange.IsValid();
	}

	static FRayRopeAffectedSpanRangeBuffer BuildMergedRanges(
		TConstArrayView<FRayRopeSpanIndexRange> SpanRanges,
		int32 MaxSpanIndex)
	{
		FRayRopeAffectedSpanRangeBuffer Ranges;
		Ranges.Reserve(SpanRanges.Num());
		for (const FRayRopeSpanIndexRange& Range : SpanRanges)
		{
			FRayRopeSpanIndexRange ClampedRange;
			if (TryClampRange(Range, MaxSpanIndex, ClampedRange))
			{
				Ranges.Add(ClampedRange);
			}
		}

		if (Ranges.Num() <= 1)
		{
			return Ranges;
		}

		Ranges.Sort(
			[](const FRayRopeSpanIndexRange& Left, const FRayRopeSpanIndexRange& Right)
			{
				return Left.FirstSpanIndex < Right.FirstSpanIndex;
			});

		FRayRopeAffectedSpanRangeBuffer MergedRanges;
		MergedRanges.Reserve(Ranges.Num());
		MergedRanges.Add(Ranges[0]);
		for (int32 RangeIndex = 1; RangeIndex < Ranges.Num(); ++RangeIndex)
		{
			FRayRopeSpanIndexRange& LastRange = MergedRanges.Last();
			const FRayRopeSpanIndexRange& CurrentRange = Ranges[RangeIndex];
			if (CurrentRange.FirstSpanIndex <= LastRange.LastSpanIndex + 1)
			{
				LastRange.LastSpanIndex = FMath::Max(
					LastRange.LastSpanIndex,
					CurrentRange.LastSpanIndex);
				continue;
			}

			MergedRanges.Add(CurrentRange);
		}

		return MergedRanges;
	}
};

/**
 * Transient result of a rope solve pass.
 *
 * This is intentionally tick-local. Persistent dirty flags on the component are
 * avoided so solver passes remain the source of truth for what changed.
 */
struct FRayRopeSolveResult
{
	bool bTopologyChanged = false;
	bool bNodeLocationsChanged = false;
	FRayRopeAffectedSpanRangeBuffer AffectedSpanRanges;

	bool DidChangeRope() const
	{
		return bTopologyChanged || bNodeLocationsChanged;
	}

	void MarkTopologyChanged()
	{
		bTopologyChanged = true;
	}

	void MarkNodeLocationsChanged()
	{
		bNodeLocationsChanged = true;
	}

	void AddAffectedSpanRange(int32 FirstSpanIndex, int32 LastSpanIndex)
	{
		FRayRopeSpanIndexRange Range;
		Range.FirstSpanIndex = FMath::Max(0, FirstSpanIndex);
		Range.LastSpanIndex = LastSpanIndex;
		if (Range.IsValid())
		{
			AffectedSpanRanges.Add(Range);
		}
	}

	void Merge(const FRayRopeSolveResult& Other)
	{
		bTopologyChanged |= Other.bTopologyChanged;
		bNodeLocationsChanged |= Other.bNodeLocationsChanged;
		AffectedSpanRanges.Append(Other.AffectedSpanRanges);
	}
};
