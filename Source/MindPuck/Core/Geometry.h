#pragma once

#include <vector>

// Port of MindPuck/src/core/geometry.ts.

namespace MindPuck
{
	constexpr double Pi = 3.141592653589793;
	constexpr double Sqrt2 = 1.4142135623730951;

	struct FVec
	{
		double X = 0.0;
		double Y = 0.0;
	};

	/** A closed loop of vertices, in either winding. It may be concave. */
	using FPolygon = std::vector<FVec>;

	/** The point on segment a-b closest to p. */
	FVec ClosestOnSegment(double Px, double Py, double Ax, double Ay, double Bx, double By);

	bool PointInPolygon(double Px, double Py, const FPolygon& Poly);

	/** Distance from a point to a polygon's outline. Zero if the point is inside it. */
	double PointPolygonDistance(double Px, double Py, const FPolygon& Poly);

	bool SegmentsIntersect(const FVec& A, const FVec& B, const FVec& C, const FVec& D);

	/** Smallest gap between two polygons. Zero if they touch, cross or one contains the other. */
	double PolygonDistance(const FPolygon& P, const FPolygon& Q);

	double Dist(const FVec& A, const FVec& B);

	/** A rectangle with half-width Hw and half-height Hh, turned by Angle radians about its centre. */
	FPolygon RectPolygon(double Cx, double Cy, double Hw, double Hh, double Angle = 0.0);

	FPolygon RegularPolygon(double Cx, double Cy, double Radius, int Sides, double Angle = 0.0);

	/** An L: two arms of the given length and thickness, meeting at a corner near the centre. Concave. */
	FPolygon LPolygon(double Cx, double Cy, double Arm, double Thick, double Angle = 0.0);

	/** An irregular convex-ish rock: Radii are the distances of evenly spaced vertices from the centre. */
	FPolygon BlobPolygon(double Cx, double Cy, const std::vector<double>& Radii, double Angle = 0.0);
}
