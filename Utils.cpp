#include "includes/Utils.hpp"

#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

Utils::Utils()
{
}

Utils::~Utils()
{
}

void Utils::die(const std::string &msg)
{
  std::perror(msg.c_str());
  std::exit(1);
}

void Utils::spawn(const std::string &cmd)
{
  pid_t pid = fork();
  if (pid < 0) die("fork");
  if (pid == 0)
  {
    setsid();
    execl("/bin/sh", "sh", "-lc", cmd.c_str(), (char*)nullptr);
    _exit(127);
  }
}  
