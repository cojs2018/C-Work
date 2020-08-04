#pragma once
#include <vector>
#include <utility>
#include <ctime>

namespace edges
{
	/* Two data strucures, one a list of start edges, one a list of end edges.
	 * Each consist of a time and amplitude.
	 */

	struct leading
	{
		clock_t time;
		double amplitude;
	};

	struct trailing
	{
		clock_t time;
		double amplitude;
	};

	

	static class sets
	{
	public: //NOTE: Edge lists are of unknown size
		std::vector<leading> leads;
		std::vector<trailing> trails;

		std::vector <std::pair<leading, trailing>> pair_start_end; //Output structure

		/*bool pair1();
		bool pair2();
		bool pair3(char);*/

		std::vector<leading> mergeleadssort(int l, int r);
		std::vector<leading> mergeleads(int l, int r);

		std::vector<trailing> mergetrailssort(int l, int r);
		std::vector<trailing> mergetrails(int l, int r);

		void mergepairs(sets s);

		void sortpairsbystarttime();
	};
}