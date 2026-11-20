#pragma once

#include "Geometry.h"
#include "Level.h"

#include <cstdint>
#include <vector>

// Port of MindPuck.Web/src/core/navgrid.ts.

namespace MindPuck
{
	/**
	 * The table as a grid of cells the puck's centre may occupy: a cell is free when the whole puck
	 * fits there. Answers "is there a route from here to there" and gives the route.
	 */
	class FNavGrid
	{
	public:
		static constexpr double Cell = 0.25;

		int Cols = 0;
		int Rows = 0;

		/** The clearance is the puck's radius plus a margin of 0.06. */
		explicit FNavGrid(const FLevel& Level);

		/** Clearance is how far the puck's centre must stay from every wall. */
		FNavGrid(const FLevel& Level, double Clearance);

		bool IsFree(double X, double Y) const;

		/** True when the puck can travel in a straight line between the two points. */
		bool LineClear(const FVec& A, const FVec& B) const;

		/**
		 * A* over the free cells, eight directions, never cutting a corner. Fills OutPath with the
		 * route as points from From to To and returns true, or returns false when the goal cannot
		 * be reached.
		 */
		bool FindPath(const FVec& From, const FVec& To, std::vector<FVec>& OutPath) const;

	private:
		std::vector<uint8_t> Blocked;

		void Build(const FLevel& Level, double Clearance);
		int IndexOf(double X, double Y) const;
		FVec Centre(int Index) const;
		/** The free cell nearest to a point: the point's own cell, or the closest one if that is blocked. */
		int NearestFree(double X, double Y) const;
	};
}
