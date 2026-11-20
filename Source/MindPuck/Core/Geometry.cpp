#include "Geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace MindPuck
{
	FVec ClosestOnSegment(double Px, double Py, double Ax, double Ay, double Bx, double By)
	{
		const double Dx = Bx - Ax;
		const double Dy = By - Ay;
		const double Len2 = Dx * Dx + Dy * Dy;
		double T = Len2 > 0.0 ? ((Px - Ax) * Dx + (Py - Ay) * Dy) / Len2 : 0.0;
		T = T < 0.0 ? 0.0 : T > 1.0 ? 1.0 : T;
		return FVec{ Ax + Dx * T, Ay + Dy * T };
	}

	bool PointInPolygon(double Px, double Py, const FPolygon& Poly)
	{
		bool bInside = false;
		const size_t Count = Poly.size();
		for (size_t i = 0, j = Count - 1; i < Count; j = i++)
		{
			const FVec& A = Poly[i];
			const FVec& B = Poly[j];
			if ((A.Y > Py) != (B.Y > Py) && Px < ((B.X - A.X) * (Py - A.Y)) / (B.Y - A.Y) + A.X)
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	double PointPolygonDistance(double Px, double Py, const FPolygon& Poly)
	{
		if (PointInPolygon(Px, Py, Poly))
		{
			return 0.0;
		}
		double Best = std::numeric_limits<double>::infinity();
		const size_t Count = Poly.size();
		for (size_t i = 0, j = Count - 1; i < Count; j = i++)
		{
			const FVec Q = ClosestOnSegment(Px, Py, Poly[j].X, Poly[j].Y, Poly[i].X, Poly[i].Y);
			const double D = std::hypot(Px - Q.X, Py - Q.Y);
			if (D < Best)
			{
				Best = D;
			}
		}
		return Best;
	}

	static double GeometryCross(double Ox, double Oy, double Ax, double Ay, double Bx, double By)
	{
		return (Ax - Ox) * (By - Oy) - (Ay - Oy) * (Bx - Ox);
	}

	bool SegmentsIntersect(const FVec& A, const FVec& B, const FVec& C, const FVec& D)
	{
		const double D1 = GeometryCross(C.X, C.Y, D.X, D.Y, A.X, A.Y);
		const double D2 = GeometryCross(C.X, C.Y, D.X, D.Y, B.X, B.Y);
		const double D3 = GeometryCross(A.X, A.Y, B.X, B.Y, C.X, C.Y);
		const double D4 = GeometryCross(A.X, A.Y, B.X, B.Y, D.X, D.Y);
		return ((D1 > 0 && D2 < 0) || (D1 < 0 && D2 > 0)) && ((D3 > 0 && D4 < 0) || (D3 < 0 && D4 > 0));
	}

	double Dist(const FVec& A, const FVec& B)
	{
		return std::hypot(A.X - B.X, A.Y - B.Y);
	}

	double PolygonDistance(const FPolygon& P, const FPolygon& Q)
	{
		if (PointInPolygon(P[0].X, P[0].Y, Q) || PointInPolygon(Q[0].X, Q[0].Y, P))
		{
			return 0.0;
		}
		double Best = std::numeric_limits<double>::infinity();
		const size_t PCount = P.size();
		const size_t QCount = Q.size();
		for (size_t i = 0, j = PCount - 1; i < PCount; j = i++)
		{
			for (size_t k = 0, l = QCount - 1; k < QCount; l = k++)
			{
				if (SegmentsIntersect(P[j], P[i], Q[l], Q[k]))
				{
					return 0.0;
				}
				const double D = (std::min)(
					Dist(P[i], ClosestOnSegment(P[i].X, P[i].Y, Q[l].X, Q[l].Y, Q[k].X, Q[k].Y)),
					Dist(Q[k], ClosestOnSegment(Q[k].X, Q[k].Y, P[j].X, P[j].Y, P[i].X, P[i].Y)));
				if (D < Best)
				{
					Best = D;
				}
			}
		}
		return Best;
	}

	static FPolygon PlacePoints(const std::vector<FVec>& Points, double Cx, double Cy, double Angle)
	{
		const double C = std::cos(Angle);
		const double S = std::sin(Angle);
		FPolygon Out;
		Out.reserve(Points.size());
		for (const FVec& P : Points)
		{
			Out.push_back(FVec{ Cx + P.X * C - P.Y * S, Cy + P.X * S + P.Y * C });
		}
		return Out;
	}

	FPolygon RectPolygon(double Cx, double Cy, double Hw, double Hh, double Angle)
	{
		return PlacePoints({ { -Hw, -Hh }, { Hw, -Hh }, { Hw, Hh }, { -Hw, Hh } }, Cx, Cy, Angle);
	}

	FPolygon RegularPolygon(double Cx, double Cy, double Radius, int Sides, double Angle)
	{
		std::vector<FVec> Points;
		Points.reserve(Sides);
		for (int i = 0; i < Sides; i++)
		{
			const double A = (static_cast<double>(i) / Sides) * Pi * 2;
			Points.push_back(FVec{ std::cos(A) * Radius, std::sin(A) * Radius });
		}
		return PlacePoints(Points, Cx, Cy, Angle);
	}

	FPolygon LPolygon(double Cx, double Cy, double Arm, double Thick, double Angle)
	{
		const double O = Arm / 2;
		return PlacePoints(
			{
				{ -O, -O },
				{ Arm - O, -O },
				{ Arm - O, Thick - O },
				{ Thick - O, Thick - O },
				{ Thick - O, Arm - O },
				{ -O, Arm - O },
			},
			Cx, Cy, Angle);
	}

	FPolygon BlobPolygon(double Cx, double Cy, const std::vector<double>& Radii, double Angle)
	{
		std::vector<FVec> Points;
		Points.reserve(Radii.size());
		for (size_t i = 0; i < Radii.size(); i++)
		{
			const double A = (static_cast<double>(i) / static_cast<double>(Radii.size())) * Pi * 2;
			Points.push_back(FVec{ std::cos(A) * Radii[i], std::sin(A) * Radii[i] });
		}
		return PlacePoints(Points, Cx, Cy, Angle);
	}
}
