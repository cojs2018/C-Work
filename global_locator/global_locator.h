/* This code will store the functions that will process the saving of a current
map after a certain time stamp looking at changes within the surrounding
environment from existing maps. It should also allow a drone to locate itself
within an environment. */
#include <iostream>
#include <cstdlib>
#include <Eigen/Dense>
#include <fstream>
#include <ctime>
#include <stdio.h>
#include <math.h>
#include <string.h>

#ifndef GLOBAL_LOCATOR_H
#define GLOBAL_LOCATOR_H

namespace globe
{
  class loc
  {
public:
    std::string dir_str = "./blam/external/bin/";

    //Checks if file is empty
    bool xyz_empty();

    //Saves map at each loop closure
    void xyz_save(std::string, bool);

    //save position of the robot
    void xyz_location(double, double, double, std::string);

    //Get existing maps
    Eigen::Matrix<double, 3, 1> xyz_get();
  };
}

#endif
