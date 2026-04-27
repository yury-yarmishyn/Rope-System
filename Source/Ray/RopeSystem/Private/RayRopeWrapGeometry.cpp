#include "RayRopeWrapGeometry.h"

bool FRayRopeWrapGeometry::AreDirectionsNearlyCollinear(
	const FVector& FirstDirection,
	const FVector& SecondDirection,
	float Epsilon)
{
	return FVector::CrossProduct(
		FirstDirection.GetSafeNormal(),
		SecondDirection.GetSafeNormal()).SizeSquared() <= FMath::Square(Epsilon);
}

FVector FRayRopeWrapGeometry::CalculateRedirectLocation(
	const FRayRopeSpan& ValidSpan,
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit)
{
	const FVector FallbackLocation =
		CalculateProjectedPointOnHitPlane(ValidSpan, FrontSurfaceHit);

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
		ValidSpan.GetStartLocation(),
		ValidSpan.GetEndLocation(),
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

FVector FRayRopeWrapGeometry::CalculateRedirectOffset(
	const FRayRopeWrapSettings& WrapSettings,
	const FHitResult& FrontSurfaceHit,
	const FHitResult* BackSurfaceHit)
{
	if (BackSurfaceHit == nullptr)
	{
		return FrontSurfaceHit.ImpactNormal.GetSafeNormal() * WrapSettings.WrapOffset;
	}

	FVector OffsetDirection = FrontSurfaceHit.ImpactNormal + BackSurfaceHit->ImpactNormal;
	if (OffsetDirection.IsNearlyZero())
	{
		OffsetDirection = FrontSurfaceHit.ImpactNormal;
	}

	return OffsetDirection.GetSafeNormal() * WrapSettings.WrapOffset;
}

FVector FRayRopeWrapGeometry::CalculateProjectedPointOnHitPlane(
	const FRayRopeSpan& ValidSpan,
	const FHitResult& SurfaceHit)
{
	const FVector LineStart = ValidSpan.GetStartLocation();
	const FVector LineEnd = ValidSpan.GetEndLocation();
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

bool FRayRopeWrapGeometry::TryGetPlaneIntersectionLine(
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

	OutLinePoint = FVector::CrossProduct(
		FrontPlaneDistance * BackNormal - BackPlaneDistance * FrontNormal,
		RawLineDirection) / RawLineDirectionSizeSquared;

	OutLineDirection = RawLineDirection.GetSafeNormal();
	return true;
}

void FRayRopeWrapGeometry::FindClosestPointsOnSegmentToLine(
	const FVector& SegmentStart,
	const FVector& SegmentEnd,
	const FVector& LinePoint,
	const FVector& LineDirection,
	FVector& OutPointOnSegment,
	FVector& OutPointOnLine,
	float& OutDistanceSquared)
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
