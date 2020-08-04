#include <iostream>
#include <fstream>
#include <ros/ros.h>
#include "/home/lunet/cojs9/Documents/code/blam/internal/src/global_locator/global_locator.h"
#include <ctime>
#include <stdio.h>
#include <Eigen/Dense>
#include <math.h>
#include <string.h>

using namespace globe;

bool loc::xyz_empty()
{
  std::ifstream mFile = std::ifstream file(strcat(loc::dir_str,"*.pcap"));

  if(is_empty(mFile))
  {
    ROS_WARNING("Warning: no files of type *.pcap in path %s.", loc::dir);
    return true;
  }
  
  return false;
}

void loc::xyz_save(std::string mfile, bool emp)
{
  //Save file will be in format "map-x-y-z-yyyy-mm-dd-hh-mm-ss.pcap"
  
  if(!(emp == true))//If not empty delete existing file
  { 
    std::string cmd = "rm ";
    system(cmd.append(strcat(loc::dir_str,"*.pcap")).c_str());
  }
  
  //Now add a new file
  std::ofstream out(strcat(loc::dir_str, mfile), ofstream::app);
  out.close();

}

void loc::xyz_location(double x, double y, double z, std::string t) //save location of lidar
{
  std::string locat_str = "[New Map] location:" + to_string(x) + "," + to_string(y) + ",", to_string(z) + "; at time:" + t + ".\n"; //Find way keeping point at 4sf
  
  std::ofstream out(strcat(loc::dir_str,"saveStream.txt");
  out << locat_str;
  out.flush();
  out.close();

}

Eigen::Matrix<double, 3, 1> loc::xyz_get() //Get map file and last location, work out vector between last and new location and get new location from that
{
  //File for map
  std::ifstream mFile = std::ifstream file(strcat(loc::dir_str,"*.pcap"));
  mfile.open();

  //Read text file for location
  std::ifstream inp("saveStream.txt");
  std::sring loc_str;
  for(std::string line; getline(inp, line))
  {
    if(line != "")
    {
      loc_str = line;
    }
  }
  
  double x; double y; double z;
  Eigen::Matrix<double, 3, 1> last_location;
  
  //Numbers should exist at points 20, 26 and 32 and are of 4sf
  
  std::string x_str = loc_str.substr(20, 4);
  std::string y_str = loc_str.substr(26, 4);
  std::string z_str = loc_str.substr(32, 4);
  
  std::stringstream x_stream(x_str);
  std::stringstream y_stream(y_str);
  std::stringstream z_stream(z_str);
  
  x_stream >> x;
  y_stream >> y;
  z_stream >> z;

  last_location << x,
		   y,
		   z;
  
  return last_location;
}
