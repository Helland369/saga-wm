#pragma once

#include <string>

class Utils
{
private:
public:

  Utils();
  ~Utils();

  static void die(const std::string &msg);

  static void spawn(const std::string &cmd);
};  
