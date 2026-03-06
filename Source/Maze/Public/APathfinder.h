#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TargetPoint.h"
#include "APathfinder.generated.h"


USTRUCT(BlueprintType)
struct FGridCell
{
    GENERATED_BODY()

    UPROPERTY() FVector WorldLocation = FVector::ZeroVector;
    UPROPERTY() bool bIsWalkable = true;

    // Для Дейкстры
    UPROPERTY() int32 X = 0;
    UPROPERTY() int32 Y = 0;

    UPROPERTY() float Dist = TNumericLimits<float>::Max();
    UPROPERTY() int32 ParentIndex = -1;
    UPROPERTY() bool bVisited = false;
};

UCLASS()
class MAZE_API AAPathfinder : public AActor
{
    GENERATED_BODY()

public:
    AAPathfinder();

protected:
    virtual void BeginPlay() override;

public:
    UPROPERTY(EditAnywhere, Category="Grid")
    float CellSize = 500.f;

    UPROPERTY(EditAnywhere, Category="Grid", meta=(ClampMin="1"))
    int32 GridSizeX = 15;

    UPROPERTY(EditAnywhere, Category="Grid", meta=(ClampMin="1"))
    int32 GridSizeY = 15;

    UPROPERTY(EditAnywhere, Category="Grid")
    FVector GridOrigin = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, Category="Grid")
    float DrawZOffset = 50.f;
    UPROPERTY(EditAnywhere, Category="Grid|Debug")
    float FloorZ = 0.f;
    
    UPROPERTY(EditAnywhere, Category="Grid|Debug")
    float WallHeight = 500.f;

    UPROPERTY(EditAnywhere, Category="Grid|Debug")
    float DebugSurfaceOffset = 3.f;

    UPROPERTY(EditAnywhere, Category="Grid|Debug")
    float DebugLineThickness = 6.f;
    
    UPROPERTY(EditAnywhere, Category="Path")
    ATargetPoint* StartPoint = nullptr;

    UPROPERTY(EditAnywhere, Category="Path")
    ATargetPoint* EndPoint = nullptr;

    UPROPERTY(EditAnywhere, Category="Path|Debug")
    bool bDrawPath = true;

    UPROPERTY(EditAnywhere, Category="Path|Debug")
    float PathZ = 550.f;

private:
    UPROPERTY()
    TArray<FGridCell> GridCells;

    void CreateGridCells();
    void DrawGrid();

    bool IsCellBlockedByWall(const FVector& CellCenter) const;
    
    FORCEINLINE int32 Idx(int32 X, int32 Y) const { return X * GridSizeY + Y; }
    FORCEINLINE bool InBounds(int32 X, int32 Y) const { return X>=0 && X<GridSizeX && Y>=0 && Y<GridSizeY; }

    int32 FindClosestCellIndex(const FVector& WorldPos) const;
    void GetNeighbors(int32 Index, TArray<int32>& Out) const;

    bool RunDijkstra(int32 StartIndex, int32 EndIndex);
    bool BuildPath(int32 EndIndex, TArray<int32>& OutPath) const;
    void DrawPath(const TArray<int32>& Path) const;

public:
    virtual void Tick(float DeltaTime) override;
};

