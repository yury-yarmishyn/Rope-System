#include "RayRopeSurfaceGeometry.h"

#include "Engine/HitResult.h"

namespace
{
FVector CalculateProjectedPointOnHitPlane(
	const FRayRopeSpan& Span,
	const FHitResult& SurfaceHit)
{
	const FVector LineStart = Span.GetStartLocation();
	const FVector LineEnd = Span.GetEndLocation();
	const FVector LineDirection = LineEnd - LineStart;
	const float Denominator = FVector::DotProduct(SurfaceHit.ImpactNormal, LineDirection);

	if (FMath::IsNearlyZero(Denominator))
	{
		// The span is parallel to the hit plane, so projection is underconstrained. Pick the endpoint
		// nearest the impact point to keep redirect placement stable.
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

bool TryGetPlaneIntersectionLine(
	const FHitResult& FrontSurfaceHit,
	const FHitResult& BackSurfaceHit,
	FVector& OutLinePoint,
	FVector& OutLineDirection)
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

	// Closed-form point on the intersection line of two planes, using normalized plane equations.
	OutLinePoint = FVector::CrossProduct(
		FrontPlaneDistance * BackNormal - BackPlaneDistance * FrontNormal,
		RawLineDirection) / RawLineDirectionSizeSquared;

	OutLineDirection = RawLineDirection.GetSafeNormal();
	return true;
}

FVector FindClosestPointOnLineToClampedSegment(
	const FVector& SegmentStart,
	const FVector& SegmentEnd,
	const FVector& LinePoint,
	const FVector& LineDirection)
{
	const FVector SegmentDirection = SegmentEnd - SegmentStart;
	const float SegmentLengthSquared = SegmentDirection.SizeSquared();
	const float LineLengthSquared = LineDirection.SizeSquared();

	if (SegmentLengthSquared <= KINDA_SMALL_NUMBER ||
		LineLengthSquared <= KINDA_SMALL_NUMBER)
	{
		const float T = LineLengthSquared <= KINDA_SMALL_NUMBER
			? 0.f
			: FVector::DotProduct(SegmentStart - LinePoint, LineDirection) / LineLengthSquared;

		return LinePoint + LineDirection * T;
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

	// Clamp only the rope segment parameter; the plane-intersection line remains unbounded.
	SegmentT = FMath::Clamp(SegmentT, 0.f, 1.f);

	const float LineT = (E + B * SegmentT) / C;
	return LinePoint + LineDirection * LineT;
}
}

bool FRayRopeSurfaceGeometry::AreDirectionsNearlyCollinear(
	const FVector& FirstDirection,
	const FVector& SecondDirection,
	float Epsilon)
{
	return FVector::CrossProduct(
		FirstDirection.GetSafeNormal(),
		SecondDirection.GetSafeNormal()).SizeSquared() <= FMath::Square(Epsilon);
}

FVector FRayRopeSurfaceGeometry::CalculateRedirectLocation(
	const FRayRopeSpan& Span,
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit)
{
	const FVector FallbackLocation =
		CalculateProjectedPointOnHitPlane(Span, FrontSurfaceHit);

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

	const FVector ClosestPointOnIntersectionLine = FindClosestPointOnLineToClampedSegment(
		Span.GetStartLocation(),
		Span.GetEndLocation(),
		PlaneIntersectionPoint,
		PlaneIntersectionDirection);

	if (ClosestPointOnIntersectionLine.ContainsNaN())
	{
		return FallbackLocation;
	}

	return ClosestPointOnIntersectionLine;
}

FVector FRayRopeSurfaceGeometry::CalculateSurfaceOffsetDirection(
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit)
{
	if (BackSurfaceHit == nullptr)
	{
		return FrontSurfaceHit.ImpactNormal.GetSafeNormal();
	}

	FVector OffsetDirection = FrontSurfaceHit.ImpactNormal + BackSurfaceHit->ImpactNormal;
	if (OffsetDirection.IsNearlyZero())
	{
		OffsetDirection = FrontSurfaceHit.ImpactNormal;
	}

	return OffsetDirection.GetSafeNormal();
}
