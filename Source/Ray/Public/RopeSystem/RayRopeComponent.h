#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RayRopeComponent.generated.h"

UENUM(BlueprintType)
enum class ERopeNodeType : uint8
{
	Redirect,
	Attachment
};

USTRUCT(BlueprintType)
struct FRopeNode
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	ERopeNodeType NodeType = ERopeNodeType::Redirect;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVector WorldLocation = FVector::ZeroVector;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FVector SurfaceNormal = FVector::ZeroVector;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TObjectPtr<AActor> AttachActor = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	int32 AttachOrder = INDEX_NONE;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	FName AttachSocketName = NAME_None;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNodeAttached, TArray<FRopeNode>, RopeNodes);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RAY_API URayRopeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URayRopeComponent();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults|Rope")
	float CurrentLength = 100.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults|Rope")
	float MinLength = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults|Rope")
	float MaxLength = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults|Rope")
	float RopeResistance = 0.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults|Rope")
	bool bUseGravity = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Defaults|Rope")
	bool bUseDynamicLength = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Defaults")
	bool bUseActualRopeLengthAsCurrentLength = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Defaults")
	bool bUseFirstAttachDistanceAsMaxLength = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rope|Dynamic")
	TArray<FRopeNode> Nodes;
	
protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Rope|Defaults")
	void AttachRopeToTarget(bool& bResult);
	
	UFUNCTION(BlueprintCallable, Category = "Math|Geometry")
	static bool ReconstructCornerPoint();
};
