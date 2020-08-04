// EdgePairingTask.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "EdgePairingTask.h"
#include <vector>
#include <cmath>
#include <ctime>
#include <algorithm>
#include <utility>

int main()
{
	//Test0: Two arrays of the same size and is a direct signal
	/*size_t test_size0 = 72;
	edges::sets s0;
	for (int count = 0; count < test_size0; count++)
	{
		double r = rand();

		edges::leading l0;
		edges::trailing t0;

		l0.time = count + 1; l0.amplitude = r;
		t0.time = count + 1.5; t0.amplitude = -r;

		s0.leads.push_back(l0);
		s0.trails.push_back(t0);
	}
	s0.pair1();

	for (int i0 = 0; i0 < s0.pair_start_end.size(); i0++)
	{
		std::pair<edges::leading,edges::trailing> p0 = s0.pair_start_end.at(i0);
		std::cout << "Pair (" << i0 << "): start time = " << p0.first.time << ", amplitude = " << p0.first.amplitude
			<< ", end time = " << p0.second.time << std::endl;
	}*/

	//Test2: Two arrays of different sizes
	/*size_t test_size1 = 70;
	size_t test_size2 = 75; //Three missing values
	edges::sets s1;
	int countj = 0;
	for (int counti = 0; counti < std::max(test_size1, test_size2); counti++)
	{
		countj = counti;
		double r = rand();

		int b = rand() % 2 + 1;

		edges::leading l1;
		edges::trailing t1;

		l1.time = counti + clock(); l1.amplitude = r;
		s1.leads.push_back(l1);

		if (b != 1)
		{
			while (b != 1)
			{
				counti++;
				int r1 = rand();
				l1.time = counti + clock(); l1.amplitude = r1;
				s1.leads.push_back(l1);

				t1.time = countj + 1 + clock();
				if (countj < test_size2)
				{
					t1.amplitude = -r1;
					countj++;
				}
				s1.trails.push_back(t1);

				b = rand() % 2 + 1;
			}
		}

		t1.time = countj + 1 + clock();
		if (countj < test_size2)
		{
			t1.amplitude = -r;
			countj++;
		}
		s1.trails.push_back(t1);
	}

	for (int i = 0; i < 5; i++)
	{
		int k = rand() % test_size1;
		s1.leads.erase(s1.leads.begin() + k);
	}*/

	edges::sets s1;

	s1.leads = { { 200, 29 }, { 202, 16 } };

	s1.trails = { {205, -29}, {203, -16}, {207, 25} };

	s1.mergepairs(s1);
}


/*bool edges::sets::pair1()
{// Pairs edges together assuming chronological order
	size_t flooring = std::max(edges::sets::leads.size(), edges::sets::trails.size());

	int i = 0;
	while (i < flooring)
	{
		if (edges::sets::leads.at(i).amplitude == abs(edges::sets::trails.at(i).amplitude))
		{
			std::pair<leading, trailing> new_start_end(edges::sets::leads.at(i), edges::sets::trails.at(i));
			edges::sets::pair_start_end.push_back(new_start_end);

			edges::sets::leads.erase(edges::sets::leads.begin() + i);
			edges::sets::trails.erase(edges::sets::trails.begin() + i);
			flooring--;
		}
		else
		{
			i++;
		}
	}

	return true; // False return not needed
}

bool edges::sets::pair2()
{  Pair left over edges together that are equal but are not of equal time
	size_t flooring = std::max(edges::sets::leads.size(), edges::sets::trails.size());

	int i = 0; int j = 0;

	while (i < flooring)
	{
		while (edges::sets::leads.at(i).amplitude != abs(edges::sets::trails.at(j).amplitude))
		{
			j++;
			if (j >= flooring)
			{
				break;
			}
		}
		
		if (j < flooring)
		{
			std::pair<leading, trailing> new_start_end(edges::sets::leads.at(i), edges::sets::trails.at(j));
			edges::sets::pair_start_end.push_back(new_start_end);

			edges::sets::leads.erase(edges::sets::leads.begin() + i);
			edges::sets::trails.erase(edges::sets::trails.begin() + j);
			flooring--;
		}
		else
		{
			i++;
			if (i >= flooring)
			{
				return false;
			}
		}

		j = i;
	}
	return true;
}


bool edges::sets::pair3(char type)
{ 
	if (type == 't')
	{
		//Trailing edge is assumed missing, dummy value is applied
		edges::trailing t;
		t.time = clock() + edges::sets::leads.at(0).time; //Guess time
		t.amplitude = edges::sets::leads.at(0).amplitude;

		edges::sets::trails.insert(edges::sets::trails.begin(), t);
	}
	else
	{
		edges::leading l;
		l.time = edges::sets::trails.at(0).time - clock();
		l.amplitude = abs(edges::sets::trails.at(0).amplitude);

		edges::sets::leads.insert(edges::sets::leads.begin(), l);
	}

	std::pair<leading, trailing> new_start_end(edges::sets::leads.at(0), edges::sets::trails.at(0));
	edges::sets::pair_start_end.push_back(new_start_end);

	edges::sets::leads.erase(edges::sets::leads.begin());
	edges::sets::trails.erase(edges::sets::trails.begin());

	return true;
}*/