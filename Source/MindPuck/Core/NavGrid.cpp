#include "NavGrid.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace MindPuck
{
	FNavGrid::FNavGrid(const FLevel& Level)
	{
		Build(Level, Level.PuckRadius + 0.06);
	}

	FNavGrid::FNavGrid(const FLevel& Level, double Clearance)
	{
		Build(Level, Clearance);
	}

	void FNavGrid::Build(const FLevel& Level, double Clearance)
	{
		Cols = static_cast<int>(std::floor(Level.Width / Cell + 0.5));
		Rows = static_cast<int>(std::floor(Level.Height / Cell + 0.5));
		Blocked.assign(static_cast<size_t>(Cols) * Rows, 0);
		const double C2 = Clearance * Clearance;
		for (int Row = 0; Row < Rows; Row++)
		{
			for (int Col = 0; Col < Cols; Col++)
			{
				const double X = (Col + 0.5) * Cell;
				const double Y = (Row + 0.5) * Cell;
				bool bHit = X < Clearance || Y < Clearance || X > Level.Width - Clearance || Y > Level.Height - Clearance;
				for (size_t o = 0; !bHit && o < Level.Obstacles.size(); o++)
				{
					const FPolygon& Poly = Level.Obstacles[o];
					if (PointInPolygon(X, Y, Poly))
					{
						bHit = true;
					}
					const size_t Count = Poly.size();
					for (size_t i = 0, j = Count - 1; !bHit && i < Count; j = i++)
					{
						const FVec Q = ClosestOnSegment(X, Y, Poly[j].X, Poly[j].Y, Poly[i].X, Poly[i].Y);
						if ((X - Q.X) * (X - Q.X) + (Y - Q.Y) * (Y - Q.Y) < C2)
						{
							bHit = true;
						}
					}
				}
				if (bHit)
				{
					Blocked[Row * Cols + Col] = 1;
				}
			}
		}
	}

	int FNavGrid::IndexOf(double X, double Y) const
	{
		const int Col = (std::min)(Cols - 1, (std::max)(0, static_cast<int>(std::floor(X / Cell))));
		const int Row = (std::min)(Rows - 1, (std::max)(0, static_cast<int>(std::floor(Y / Cell))));
		return Row * Cols + Col;
	}

	FVec FNavGrid::Centre(int Index) const
	{
		return FVec{ ((Index % Cols) + 0.5) * Cell, ((Index / Cols) + 0.5) * Cell };
	}

	bool FNavGrid::IsFree(double X, double Y) const
	{
		return X >= 0 && Y >= 0 && X < Cols * Cell && Y < Rows * Cell && !Blocked[IndexOf(X, Y)];
	}

	int FNavGrid::NearestFree(double X, double Y) const
	{
		const int StartCell = IndexOf(X, Y);
		if (!Blocked[StartCell])
		{
			return StartCell;
		}
		std::vector<uint8_t> Seen(Blocked.size(), 0);
		std::vector<int> Queue;
		Queue.push_back(StartCell);
		Seen[StartCell] = 1;
		for (size_t Head = 0; Head < Queue.size(); Head++)
		{
			const int i = Queue[Head];
			if (!Blocked[i])
			{
				return i;
			}
			const int Col = i % Cols;
			const int Row = (i - Col) / Cols;
			for (int Dr = -1; Dr <= 1; Dr++)
			{
				for (int Dc = -1; Dc <= 1; Dc++)
				{
					const int C = Col + Dc;
					const int R = Row + Dr;
					if (C < 0 || R < 0 || C >= Cols || R >= Rows)
					{
						continue;
					}
					const int N = R * Cols + C;
					if (!Seen[N])
					{
						Seen[N] = 1;
						Queue.push_back(N);
					}
				}
			}
		}
		return -1;
	}

	bool FNavGrid::LineClear(const FVec& A, const FVec& B) const
	{
		const double Length = std::hypot(B.X - A.X, B.Y - A.Y);
		const int Steps = (std::max)(1, static_cast<int>(std::ceil(Length / (Cell * 0.5))));
		for (int i = 0; i <= Steps; i++)
		{
			const double T = static_cast<double>(i) / Steps;
			if (!IsFree(A.X + (B.X - A.X) * T, A.Y + (B.Y - A.Y) * T))
			{
				return false;
			}
		}
		return true;
	}

	bool FNavGrid::FindPath(const FVec& From, const FVec& To, std::vector<FVec>& OutPath) const
	{
		OutPath.clear();
		const int StartCell = NearestFree(From.X, From.Y);
		const int GoalCell = NearestFree(To.X, To.Y);
		if (StartCell < 0 || GoalCell < 0)
		{
			return false;
		}
		const size_t N = Blocked.size();
		std::vector<double> G(N, std::numeric_limits<double>::infinity());
		std::vector<int> Came(N, -1);
		std::vector<uint8_t> Closed(N, 0);
		const int GoalCol = GoalCell % Cols;
		const int GoalRow = (GoalCell - GoalCol) / Cols;
		const int GridCols = Cols;
		auto Heuristic = [GridCols, GoalCol, GoalRow](int i) -> double
		{
			const int Dc = std::abs((i % GridCols) - GoalCol);
			const int Dr = std::abs((i / GridCols) - GoalRow);
			return (std::max)(Dc, Dr) + (Sqrt2 - 1) * (std::min)(Dc, Dr);
		};

		// A binary heap of [cell, priority] pairs. It is written out by hand, as in the original,
		// so that ties are broken the same way and the routes match.
		std::vector<int> HeapCell;
		std::vector<double> HeapKey;
		auto HeapPush = [&HeapCell, &HeapKey](int InCell, double Key)
		{
			size_t i = HeapCell.size();
			HeapCell.push_back(InCell);
			HeapKey.push_back(Key);
			while (i > 0)
			{
				const size_t Parent = (i - 1) >> 1;
				if (HeapKey[Parent] <= Key)
				{
					break;
				}
				HeapCell[i] = HeapCell[Parent];
				HeapKey[i] = HeapKey[Parent];
				i = Parent;
			}
			HeapCell[i] = InCell;
			HeapKey[i] = Key;
		};
		auto HeapPop = [&HeapCell, &HeapKey]() -> int
		{
			const int Top = HeapCell[0];
			const int LastCell = HeapCell.back();
			const double Key = HeapKey.back();
			HeapCell.pop_back();
			HeapKey.pop_back();
			const size_t Size = HeapCell.size();
			if (Size > 0)
			{
				size_t i = 0;
				for (;;)
				{
					size_t Child = 2 * i + 1;
					if (Child >= Size)
					{
						break;
					}
					if (Child + 1 < Size && HeapKey[Child + 1] < HeapKey[Child])
					{
						Child++;
					}
					if (HeapKey[Child] >= Key)
					{
						break;
					}
					HeapCell[i] = HeapCell[Child];
					HeapKey[i] = HeapKey[Child];
					i = Child;
				}
				HeapCell[i] = LastCell;
				HeapKey[i] = Key;
			}
			return Top;
		};

		G[StartCell] = 0;
		HeapPush(StartCell, Heuristic(StartCell));
		while (!HeapCell.empty())
		{
			const int Current = HeapPop();
			if (Closed[Current])
			{
				continue;
			}
			Closed[Current] = 1;
			if (Current == GoalCell)
			{
				break;
			}
			const int Col = Current % Cols;
			const int Row = (Current - Col) / Cols;
			for (int Dr = -1; Dr <= 1; Dr++)
			{
				for (int Dc = -1; Dc <= 1; Dc++)
				{
					if (Dr == 0 && Dc == 0)
					{
						continue;
					}
					const int C = Col + Dc;
					const int R = Row + Dr;
					if (C < 0 || R < 0 || C >= Cols || R >= Rows)
					{
						continue;
					}
					const int Next = R * Cols + C;
					if (Blocked[Next] || Closed[Next])
					{
						continue;
					}
					if (Dr != 0 && Dc != 0 && (Blocked[Row * Cols + C] || Blocked[R * Cols + Col]))
					{
						continue;
					}
					const double Cost = G[Current] + (Dr != 0 && Dc != 0 ? Sqrt2 : 1.0);
					if (Cost < G[Next])
					{
						G[Next] = Cost;
						Came[Next] = Current;
						HeapPush(Next, Cost + Heuristic(Next));
					}
				}
			}
		}
		if (!Closed[GoalCell])
		{
			return false;
		}

		for (int i = GoalCell; i != -1; i = Came[i])
		{
			OutPath.push_back(Centre(i));
		}
		std::reverse(OutPath.begin(), OutPath.end());
		// The route ends exactly on the target when its own cell is free.
		if (IsFree(To.X, To.Y))
		{
			OutPath.back() = FVec{ To.X, To.Y };
		}
		return true;
	}
}
