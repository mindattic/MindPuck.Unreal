#include "MindPuckMesh.h"

namespace MindPuckMesh
{
	static double Cross2(const FVector2D& O, const FVector2D& A, const FVector2D& B)
	{
		return (A.X - O.X) * (B.Y - O.Y) - (A.Y - O.Y) * (B.X - O.X);
	}

	double SignedArea2(const TArray<FVector2D>& Outline)
	{
		double Sum = 0.0;
		const int32 Count = Outline.Num();
		for (int32 i = 0, j = Count - 1; i < Count; j = i++)
		{
			Sum += Outline[j].X * Outline[i].Y - Outline[i].X * Outline[j].Y;
		}
		return Sum;
	}

	static bool PointInTriangle(const FVector2D& P, const FVector2D& A, const FVector2D& B, const FVector2D& C)
	{
		// A, B, C run counter-clockwise. Points on an edge count as outside, so that vertices
		// shared with the triangle do not block it.
		return Cross2(A, B, P) > 1e-12 && Cross2(B, C, P) > 1e-12 && Cross2(C, A, P) > 1e-12;
	}

	void Triangulate(const TArray<FVector2D>& Outline, TArray<int32>& OutIndices)
	{
		OutIndices.Reset();
		const int32 Count = Outline.Num();
		if (Count < 3)
		{
			return;
		}

		// Work on a list of the remaining vertex indices, counter-clockwise.
		TArray<int32> Remaining;
		Remaining.Reserve(Count);
		if (SignedArea2(Outline) >= 0.0)
		{
			for (int32 i = 0; i < Count; i++)
			{
				Remaining.Add(i);
			}
		}
		else
		{
			for (int32 i = Count - 1; i >= 0; i--)
			{
				Remaining.Add(i);
			}
		}

		while (Remaining.Num() > 3)
		{
			const int32 N = Remaining.Num();
			int32 Ear = INDEX_NONE;
			for (int32 i = 0; i < N; i++)
			{
				const int32 IPrev = Remaining[(i + N - 1) % N];
				const int32 ICur = Remaining[i];
				const int32 INext = Remaining[(i + 1) % N];
				const FVector2D& A = Outline[IPrev];
				const FVector2D& B = Outline[ICur];
				const FVector2D& C = Outline[INext];
				if (Cross2(A, B, C) <= 1e-12)
				{
					continue; // A reflex or flat corner cannot be an ear.
				}
				bool bContainsOther = false;
				for (int32 k = 0; k < N; k++)
				{
					const int32 IOther = Remaining[k];
					if (IOther == IPrev || IOther == ICur || IOther == INext)
					{
						continue;
					}
					if (PointInTriangle(Outline[IOther], A, B, C))
					{
						bContainsOther = true;
						break;
					}
				}
				if (!bContainsOther)
				{
					Ear = i;
					break;
				}
			}
			if (Ear == INDEX_NONE)
			{
				// A degenerate outline (repeated or collinear points). Clip any corner so the loop ends.
				Ear = 0;
			}
			OutIndices.Add(Remaining[(Ear + N - 1) % N]);
			OutIndices.Add(Remaining[Ear]);
			OutIndices.Add(Remaining[(Ear + 1) % N]);
			Remaining.RemoveAt(Ear);
		}
		OutIndices.Add(Remaining[0]);
		OutIndices.Add(Remaining[1]);
		OutIndices.Add(Remaining[2]);
	}
}

void FMindPuckMeshData::AddTriangle(int32 A, int32 B, int32 C, const FVector& FaceNormal)
{
	// The engine takes a triangle's front to be the side (P1 - P2) x (P0 - P2) points to (see
	// UKismetProceduralMeshLibrary::CalculateTangentsForMesh). Swap two corners if that is not
	// the side wanted.
	const FVector Facing = FVector::CrossProduct(Vertices[B] - Vertices[C], Vertices[A] - Vertices[C]);
	Triangles.Add(A);
	if (FVector::DotProduct(Facing, FaceNormal) >= 0.0)
	{
		Triangles.Add(B);
		Triangles.Add(C);
	}
	else
	{
		Triangles.Add(C);
		Triangles.Add(B);
	}
}

void FMindPuckMeshData::AddQuad(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const FVector& FaceNormal)
{
	const int32 Base = Vertices.Num();
	Vertices.Add(P0);
	Vertices.Add(P1);
	Vertices.Add(P2);
	Vertices.Add(P3);
	const FVector TangentDir = (P1 - P0).GetSafeNormal();
	for (int32 i = 0; i < 4; i++)
	{
		Normals.Add(FaceNormal);
		Tangents.Add(FProcMeshTangent(TangentDir, false));
	}
	UVs.Add(FVector2D(0.0, 0.0));
	UVs.Add(FVector2D(1.0, 0.0));
	UVs.Add(FVector2D(1.0, 1.0));
	UVs.Add(FVector2D(0.0, 1.0));
	AddTriangle(Base, Base + 1, Base + 2, FaceNormal);
	AddTriangle(Base, Base + 2, Base + 3, FaceNormal);
}

void FMindPuckMeshData::AddPrism(const TArray<FVector2D>& Outline, double Z0, double Z1)
{
	const int32 Count = Outline.Num();
	if (Count < 3)
	{
		return;
	}

	// The top face.
	const FVector Up(0.0, 0.0, 1.0);
	const int32 Base = Vertices.Num();
	for (const FVector2D& P : Outline)
	{
		Vertices.Add(FVector(P.X, P.Y, Z1));
		Normals.Add(Up);
		Tangents.Add(FProcMeshTangent(FVector(1.0, 0.0, 0.0), false));
		UVs.Add(P * 0.01);
	}
	TArray<int32> TopIndices;
	MindPuckMesh::Triangulate(Outline, TopIndices);
	for (int32 i = 0; i + 2 < TopIndices.Num(); i += 3)
	{
		AddTriangle(Base + TopIndices[i], Base + TopIndices[i + 1], Base + TopIndices[i + 2], Up);
	}

	// The walls. For a counter-clockwise loop the outside of edge A to B is (dy, -dx).
	const double Winding = MindPuckMesh::SignedArea2(Outline) >= 0.0 ? 1.0 : -1.0;
	for (int32 i = 0, j = Count - 1; i < Count; j = i++)
	{
		const FVector2D& A = Outline[j];
		const FVector2D& B = Outline[i];
		const FVector2D Edge = B - A;
		const FVector Outward = FVector(Edge.Y * Winding, -Edge.X * Winding, 0.0).GetSafeNormal();
		if (Outward.IsNearlyZero())
		{
			continue;
		}
		AddQuad(FVector(A.X, A.Y, Z0), FVector(B.X, B.Y, Z0), FVector(B.X, B.Y, Z1), FVector(A.X, A.Y, Z1), Outward);
	}
}

void FMindPuckMeshData::CreateSection(UProceduralMeshComponent* Mesh, int32 SectionIndex) const
{
	Mesh->CreateMeshSection_LinearColor(SectionIndex, Vertices, Triangles, Normals, UVs, TArray<FLinearColor>(), Tangents, false);
}
