#include "APathfinder.h"

#include "DrawDebugHelpers.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/SceneComponent.h"

AAPathfinder::AAPathfinder()
{
    PrimaryActorTick.bCanEverTick = true;

    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;
}

// начало игры и вызов функций
void AAPathfinder::BeginPlay()
{
    Super::BeginPlay();

    CreateGridCells();
    DrawGrid();
//    
    if (!StartPoint || !EndPoint)
    {
        UE_LOG(LogTemp, Error, TEXT("Начальная и конечная точки совпадают"));
        return;
    }

    const int32 StartIndex = FindClosestCellIndex(StartPoint->GetActorLocation());
    const int32 EndIndex   = FindClosestCellIndex(EndPoint->GetActorLocation());

    UE_LOG(LogTemp, Warning, TEXT("StartIndex=%d EndIndex=%d"), StartIndex, EndIndex);

    if (RunDijkstra(StartIndex, EndIndex))
    {
        TArray<int32> Path;
        if (BuildPath(EndIndex, Path))
        {
            UE_LOG(LogTemp, Warning, TEXT("Длина кратчайшего пути=%d"), Path.Num());
            DrawPath(Path);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("BuildPath failed"));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Dijkstra failed: no path"));
    }

    UE_LOG(LogTemp, Warning, TEXT("GridCells=%d GridSizeX=%d GridSizeY=%d CellSize=%.1f Origin=%s"),
        GridCells.Num(), GridSizeX, GridSizeY, CellSize, *GridOrigin.ToString());
}

void AAPathfinder::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// делаю трассировку, проверяю где клетки заняты, а где нет
bool AAPathfinder::IsCellBlockedByWall(const FVector& CellCenter) const
{
    const FVector HalfSize(CellSize * 0.45f, CellSize * 0.45f, 250.f);
    const FVector BoxCenter = CellCenter + FVector(0.f, 0.f, 250.f);

    TArray<AActor*> Ignore;
    Ignore.Add(const_cast<AAPathfinder*>(this));

    TArray<FHitResult> Hits;

    const bool bHitAnything = UKismetSystemLibrary::BoxTraceMulti(
        const_cast<AAPathfinder*>(this),
        BoxCenter,
        BoxCenter,
        HalfSize,
        FRotator::ZeroRotator,
        UEngineTypes::ConvertToTraceType(ECC_Visibility),
        false,
        Ignore,
        EDrawDebugTrace::None,
        Hits,
        true
    );

    if (!bHitAnything)
        return false;

    for (const FHitResult& H : Hits)
    {
        AActor* A = H.GetActor();
        if (!A) continue;

        if (A->ActorHasTag("Floor"))
            continue;

        if (A->IsA(AStaticMeshActor::StaticClass()))
            return true;
    }

    return false;
}

// создается поле клеток
void AAPathfinder::CreateGridCells()
{
    GridCells.Empty();
    GridCells.Reserve(GridSizeX * GridSizeY);

    FVector Origin = GetActorLocation();

    for (int32 X = 0; X < GridSizeX; X++)
    {
        for (int32 Y = 0; Y < GridSizeY; Y++)
        {
            FGridCell Cell;

            const FVector WorldPos = Origin + FVector(
                (X + 0.5f) * CellSize,
                (Y + 0.5f) * CellSize,
                0.f
            );
            Cell.X = X;
            Cell.Y = Y;

            Cell.WorldLocation = WorldPos;
            Cell.bIsWalkable = !IsCellBlockedByWall(WorldPos);

            GridCells.Add(Cell);
        }
    }
}

// отрисовываю поле с учетом проходимости/непроходимости клеток
void AAPathfinder::DrawGrid()
{
    UWorld* W = GetWorld();
    if (!W) return;

    const FVector Origin = GetActorLocation();

    const float Z = 10.f;
    const float Thickness = 4.f;

    auto Idx = [&](int32 X, int32 Y) -> int32
    {
        return X * GridSizeY + Y;
    };

    auto IsBlocked = [&](int32 X, int32 Y) -> bool
    {
        if (X < 0 || X >= GridSizeX || Y < 0 || Y >= GridSizeY)
        {
            return true;
        }
        return !GridCells.IsValidIndex(Idx(X, Y)) ? true : !GridCells[Idx(X, Y)].bIsWalkable;
    };

    auto EdgeColor = [&](int32 AX, int32 AY, int32 BX, int32 BY) -> FColor
    {
        const bool bA = IsBlocked(AX, AY);
        const bool bB = IsBlocked(BX, BY);
        return (bA || bB) ? FColor::Red : FColor::Green;
    };

    for (int32 X = 0; X <= GridSizeX; X++)
    {
        for (int32 Y = 0; Y < GridSizeY; Y++)
        {
            const FColor C = EdgeColor(X - 1, Y, X, Y);

            const FVector A = Origin + FVector(X * CellSize, Y * CellSize, Z+500.f);
            const FVector B = Origin + FVector(X * CellSize, (Y + 1) * CellSize, Z+500.f);

            DrawDebugLine(W, A, B, C, true, -1.f, 0, Thickness);
        }
    }

    for (int32 Y = 0; Y <= GridSizeY; Y++)
    {
        for (int32 X = 0; X < GridSizeX; X++)
        {
            const FColor C = EdgeColor(X, Y - 1, X, Y);

            const FVector A = Origin + FVector(X * CellSize, Y * CellSize, Z+500.f);
            const FVector B = Origin + FVector((X + 1) * CellSize, Y * CellSize, Z+500.f);

            DrawDebugLine(W, A, B, C, true, -1.f, 0, Thickness);
        }
    }
}

// поиск ближайших ячеек
int32 AAPathfinder::FindClosestCellIndex(const FVector& WorldPos) const
{
    const FVector Origin = GetActorLocation();

    const float LocalX = (WorldPos.X - Origin.X) / CellSize - 0.5f;
    const float LocalY = (WorldPos.Y - Origin.Y) / CellSize - 0.5f;

    const int32 X = FMath::Clamp(FMath::RoundToInt(LocalX), 0, GridSizeX - 1);
    const int32 Y = FMath::Clamp(FMath::RoundToInt(LocalY), 0, GridSizeY - 1);

    const int32 Index = Idx(X, Y);
    return GridCells.IsValidIndex(Index) ? Index : -1;
}

// собираю всех соседей
void AAPathfinder::GetNeighbors(int32 Index, TArray<int32>& Out) const
{
    Out.Reset();
    if (!GridCells.IsValidIndex(Index)) return;

    const int32 X = GridCells[Index].X;
    const int32 Y = GridCells[Index].Y;

    const int DX[4] = { 1, -1,  0,  0 };
    const int DY[4] = { 0,  0,  1, -1 };

    for (int i = 0; i < 4; ++i)
    {
        const int32 NX = X + DX[i];
        const int32 NY = Y + DY[i];

        if (!InBounds(NX, NY)) continue;

        const int32 NIndex = Idx(NX, NY);
        if (!GridCells.IsValidIndex(NIndex)) continue;
        if (!GridCells[NIndex].bIsWalkable) continue;

        Out.Add(NIndex);
    }
}

// запуск дейстры
bool AAPathfinder::RunDijkstra(int32 StartIndex, int32 EndIndex)
{
    if (!GridCells.IsValidIndex(StartIndex) || !GridCells.IsValidIndex(EndIndex))
        return false;

    if (!GridCells[StartIndex].bIsWalkable || !GridCells[EndIndex].bIsWalkable)
        return false;

    for (FGridCell& C : GridCells)
    {
        C.Dist = TNumericLimits<float>::Max();
        C.ParentIndex = -1;
        C.bVisited = false;
    }

    GridCells[StartIndex].Dist = 0.f;
// Дейкстра
    while (true)
    {
        int32 Current = -1;
        float Best = TNumericLimits<float>::Max();

        for (int32 i = 0; i < GridCells.Num(); ++i)
        {
            if (GridCells[i].bVisited) continue;
            if (!GridCells[i].bIsWalkable) continue;

            if (GridCells[i].Dist < Best)
            {
                Best = GridCells[i].Dist;
                Current = i;
            }
        }
        
        if (Current == -1) return false; // заново
        if (Current == EndIndex) return true; // нашли выход

        GridCells[Current].bVisited = true;

        TArray<int32> Neigh;
        GetNeighbors(Current, Neigh);

        for (int32 N : Neigh)
        {
            if (GridCells[N].bVisited) continue;

            const float NewDist = GridCells[Current].Dist + 1.f; // прибавляю вес
            if (NewDist < GridCells[N].Dist)
            {
                GridCells[N].Dist = NewDist;
                GridCells[N].ParentIndex = Current;
            }
        }
    }
}

// восстановление пути после Дейкстры
bool AAPathfinder::BuildPath(int32 EndIndex, TArray<int32>& OutPath) const
{
    OutPath.Reset();
    if (!GridCells.IsValidIndex(EndIndex)) return false;

    if (GridCells[EndIndex].Dist == 0.f)
    {
        OutPath.Add(EndIndex);
        return true;
    }

    if (GridCells[EndIndex].ParentIndex == -1) return false;

    int32 Cur = EndIndex;
    while (Cur != -1)
    {
        OutPath.Add(Cur);
        Cur = GridCells[Cur].ParentIndex;
    }

    Algo::Reverse(OutPath);
    return OutPath.Num() > 0;
}

// отрисовка пути
void AAPathfinder::DrawPath(const TArray<int32>& Path) const
{
    if (!bDrawPath) return;

    UWorld* W = GetWorld();
    if (!W) return;

    for (int32 i = 0; i + 1 < Path.Num(); ++i)
    {
        const FVector A = GridCells[Path[i]].WorldLocation + FVector(0, 0, PathZ);
        const FVector B = GridCells[Path[i + 1]].WorldLocation + FVector(0, 0, PathZ);

        DrawDebugLine(W, A, B, FColor::Blue, true, -1.f, 0, 12.f);
    }
}
