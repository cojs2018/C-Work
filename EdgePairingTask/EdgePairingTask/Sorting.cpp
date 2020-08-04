#include <iostream>
#include "EdgePairingTask.h"
#include <vector>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <utility>

using namespace edges;

void sets::mergepairs(sets s)
{
	//Perform merge sort on each set
	s.leads = s.mergeleadssort(0, s.leads.size() - 1);
	s.trails = s.mergetrailssort(0, s.trails.size() - 1);

	/* if both equal at i, make pair */
	for (int i = 0; i < std::min(s.leads.size(), s.trails.size()); i++)
	{
		int j = i;
		bool b0 = s.leads.at(i).amplitude == abs(s.trails.at(i).amplitude);
		bool b1 = isnan(s.leads.at(i).amplitude) || isnan(s.trails.at(i).amplitude);
		if (i == 0 && b0 == false) //Leading value is missing
		{
			leading l;
			l.amplitude = abs(s.trails.at(i).amplitude);
			// Estimate time halfway between trails at i+1 and i
			l.time = s.trails.at(i).time - 2; //Two secon estimate
			std::pair<leading, trailing> new_pair(l, s.trails.at(i));
			s.pair_start_end.push_back(new_pair);
			s.trails.at(i).amplitude = NAN;
		}

		if (b0 == true && b1 == false)
		{
			std::pair<leading, trailing> new_pair(s.leads.at(i), s.trails.at(i));
			s.pair_start_end.push_back(new_pair);

			//set value in leading and trailing sets to null
			s.leads.at(i).amplitude = NAN;
			s.trails.at(i).amplitude = NAN;
		}
		else
		{
			/* These structures have been ordered by sort */
			while (s.leads.at(i).amplitude < s.trails.at(j).amplitude)
			{
				b0 = s.leads.at(i).amplitude == abs(s.trails.at(j).amplitude);
				b1 = isnan(s.leads.at(i).amplitude) || isnan(s.trails.at(j).amplitude);
				if (j < std::min(s.leads.size()-1, s.trails.size()-1))
				{
					j++;
				}
				else 
				{
					break;
				}
			}
			if (b0 == true && b1 == false) // nearest trail exists
			{
				std::pair<leading, trailing> new_pair(s.leads.at(i), s.trails.at(j));
				s.pair_start_end.push_back(new_pair);

				//set value in leading and trailing sets to null
				s.leads.at(i).amplitude = NAN;
				s.trails.at(j).amplitude = NAN;
				
			}
			else if (b1 == false && b0 == false)
			{
				if (i < std::min(s.leads.size()-1, s.trails.size()-1))
				{
					/* Noise present so estimate time
					 * if lead at i+1 equal to trail at j then lead is missing trail
					 * then add new trail.
					 * else trail is missing lead so add new lead.*/
					if (s.leads.at(i + 1).amplitude == s.trails.at(j).amplitude)
					{
						trailing t;
						t.amplitude = -1 * s.leads.at(i).amplitude;
						// Estimate time halfway between trails at j-1 and j
						t.time = s.leads.at(i).time + (abs(s.trails.at(j).time - s.trails.at(j-1).time) / 2);
						std::pair<leading, trailing> new_pair(s.leads.at(i), t);
						s.pair_start_end.push_back(new_pair);
						s.leads.at(i).amplitude = NAN;
					}
					/*else
					{
						leading l;
						l.amplitude = abs(s.trails.at(i).amplitude);
						// Estimate time halfway between trails at i+1 and i
						l.time = s.trails.at(j).time - abs((s.leads.at(i + 1).time - s.leads.at(i).time) / 2);
						std::pair<leading, trailing> new_pair(l, s.trails.at(j));
						s.pair_start_end.push_back(new_pair);
						s.trails.at(j).amplitude = NAN;
					}*/
				}
			}
		}
	}

	//If any elements are left over create a dummy pair for each
	if (s.pair_start_end.size() < std::max(s.leads.size(), s.trails.size()))
	{
		for (int k = 0; k < std::max(s.leads.size(), s.trails.size()); k++)
		{
			bool b3 = (k >= s.leads.size());
			bool b2;
			if (b3 == false)
			{
				b2 = isnan(s.leads.at(k).amplitude);
				if (b2 == false)
				{
					trailing t;
					t.amplitude = -1 * s.leads.at(k).amplitude;
					t.time = s.leads.at(k).time + 0.5;
					std::pair<leading, trailing> new_pair(s.leads.at(k), t);
					s.pair_start_end.push_back(new_pair);
					s.leads.at(k).amplitude = NAN;
				}
			}
			b3 = (k >= s.trails.size());
			if (b3 == false)
			{
				b2 = isnan(s.trails.at(k).amplitude);
				if (b2 == false)
				{
					leading l;
					l.amplitude = abs(s.trails.at(k).amplitude);
					l.time = s.trails.at(k).time - 2; //Two second estimate
					std::pair<leading, trailing> new_pair(l, s.trails.at(k));
					s.pair_start_end.push_back(new_pair);
					s.trails.at(k).amplitude = NAN;
				}
			}
		}
	}

	s.sortpairsbystarttime();

	//Output result
	for (int i0 = 0; i0 < s.pair_start_end.size(); i0++)
	{
		std::pair<edges::leading, edges::trailing> p0 = s.pair_start_end.at(i0);
		std::cout << "Pair (" << i0 << "): start time = " << double(p0.first.time) << ", amplitude = " << p0.first.amplitude
			<< ", end time = " << double(p0.second.time) << std::endl;
	}
}

std::vector<trailing> sets::mergetrails(int l, int r)
{
	int i, j; //Counters;
	for (i = l; i <= r - 1; i++)
	{
		for (j = i+1; j <= r; j++)
		{
			/* absolutely greater than since ampiltude is negative in the trailing set. */
			double ai = abs(this->trails.at(i).amplitude);
			double aj = abs(this->trails.at(j).amplitude);
			if (ai > aj)
			{
				/* insertion procedure */
				trailing trail_at_j = this->trails.at(j);

				this->trails.erase(this->trails.begin() + j);
				this->trails.insert(this->trails.begin() + i, trail_at_j);
			}
			else if (this->trails.at(i).amplitude == this->trails.at(j).amplitude)
			{
				if (this->trails.at(i).time > this->trails.at(j).time)
				{
					/* insertion procedure */
					trailing trail_at_j = this->trails.at(j);

					this->trails.erase(this->trails.begin() + j);
					this->trails.insert(this->trails.begin() + i, trail_at_j);
				}
			}
		}
	}

	return this->trails;
}

std::vector<trailing> sets::mergetrailssort(int l, int r)
{
	int m;
	if ((r - l) > 2)// if set size is greater than 2
	{   //Divide set and repeat
		m = ceil((r - l) / 2);
		sets::mergetrailssort(l, m);
		sets::mergetrailssort(m + 1, l);
	}
	return sets::mergetrails(l, r);
}


std::vector<leading> sets::mergeleads(int l,int r)
{
	int i, j; //Counters;
	for (i = l; i <= r - 1; i++)
	{
		for (j = 1; j <= r; j++)
		{
			if (this->leads.at(i).amplitude > this->leads.at(j).amplitude && j > i)
			{
				/* insertion procedure */
				leading lead_at_j = this->leads.at(j);

				this->leads.erase(this->leads.begin() + j);
				this->leads.insert(this->leads.begin()+i,lead_at_j);
			}
			else if (this->leads.at(i).amplitude == this->leads.at(j).amplitude && j > i)
			{
				if (this->leads.at(i).time > this->leads.at(j).time)
				{
					/* insertion procedure */
					leading lead_at_j = this->leads.at(j);

					this->leads.erase(this->leads.begin() + j);
					this->leads.insert(this->leads.begin() + i, lead_at_j);
				}
			}
			/*if (j < i)
			{
				k = i; i = j; j = k;
			}*/
		}
	}

	return this->leads;
}

std::vector<leading> sets::mergeleadssort(int l, int r)
{
	int m;
	if ((r-l) > 2) // if set size is greater than 2
	{   //Divide set and repeat
		m = ceil((r - l) / 2);
		sets::mergeleadssort(l, m);
		sets::mergeleadssort(m + 1, l);
	}
	return sets::mergeleads(l, r);
}

void sets::sortpairsbystarttime()
{
	int r = this->pair_start_end.size();
	int i, j;
	for (i = 0; i < r - 1; i++)
	{
		for (j = i; j < r; j++)
		{
			if (this->pair_start_end.at(i).first.time > this->pair_start_end.at(j).first.time)
			{
				/* Swap procedure */
				std::pair<leading, trailing> pi = this->pair_start_end.at(i);
				std::pair<leading, trailing> pj = this->pair_start_end.at(j);

				this->pair_start_end.at(i) = pj;
				this->pair_start_end.at(j) = pi;
			}
		}
	}
}