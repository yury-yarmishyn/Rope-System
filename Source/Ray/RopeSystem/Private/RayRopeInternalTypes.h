#pragma once

#include "CollisionQueryParams.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "RayRopeTypes.h"

class UWorld;

struct FRayRopeSpan
{
	const FRayRopeNode* StartNode = nullptr;
	const FRayRopeNode* EndNode = nullptr;

	bool IsValid() const
	{
		return StartNode != nullptr && EndNode != nullptr;
	}

	FVector GetStartLocation() const
	{
		return StartNode != nullptr ? StartNode->WorldLocation : FVector::ZeroVector;
	}

	FVector GetEndLocation() const
	{
		return EndNode != nullptr ? EndNode->WorldLocation : FVector::ZeroVector;
	}

	FVector GetDirection() const
	{
		return IsValid() ? GetEndLocation() - GetStartLocation() : FVector::ZeroVector;
	}

	float GetLengthSquared() const
	{
		return GetDirection().SizeSquared();
	}

	bool IsDegenerate(float Epsilon) const
	{
		return !IsValid() || GetLengthSquared() <= FMath::Square(Epsilon);
	}
};

struct FRayRopeTraceSettings
{
	UWorld* World = nullptr;
	const AActor* OwnerActor = nullptr;
	ECollisionChannel TraceChannel = ECC_Visibility;
};

struct FRayRopeTraceContext
{
	UWorld* World = nullptr;
	ECollisionChannel TraceChannel = ECC_Visibility;
	FCollisionQueryParams QueryParams;
};

struct FRayRopeWrapSettings
{
	bool bAllowWrapOnMovableObjects = false;
	int32 MaxBinarySearchIteration = 0;
	float WrapSolverEpsilon = 0.f;
	float GeometryCollinearEpsilon = 0.f;
	float RopePhysicalRadius = 0.f;
};
