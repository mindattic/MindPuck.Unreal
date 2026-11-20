#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

/** The arrays one procedural mesh section is made from. */
struct FMindPuckMeshData
{
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FProcMeshTangent> Tangents;

	/** Add a triangle between three existing vertices, wound so that it faces along FaceNormal. */
	void AddTriangle(int32 A, int32 B, int32 C, const FVector& FaceNormal);

	/** Add a flat four-sided face. The corners go around the face in either direction. */
	void AddQuad(const FVector& P0, const FVector& P1, const FVector& P2, const FVector& P3, const FVector& FaceNormal);

	/**
	 * Add a prism: Outline (a closed loop in the XY plane, either winding, concave allowed) pushed
	 * up from Z0 to Z1, with a top face and side walls. There is no bottom face.
	 */
	void AddPrism(const TArray<FVector2D>& Outline, double Z0, double Z1);

	void CreateSection(UProceduralMeshComponent* Mesh, int32 SectionIndex) const;
};

namespace MindPuckMesh
{
	/** Twice the signed area of a loop. Positive when it runs counter-clockwise with X right and Y up. */
	double SignedArea2(const TArray<FVector2D>& Outline);

	/**
	 * Cut a simple polygon into triangles by ear clipping, which handles concave shapes such as
	 * the L. OutIndices receives three indices into Outline per triangle.
	 */
	void Triangulate(const TArray<FVector2D>& Outline, TArray<int32>& OutIndices);
}
