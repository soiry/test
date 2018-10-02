#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <iostream>
#include <sstream>

using namespace std;

int main(int argc, char* argv[])
{
  // router part
  int rowNum = atoi(argv[1]);
  int columnNum = atoi(argv[2]);
  cout << "\nrouter\n\n";
  int x = columnNum;
  int y = 1;
  for(int i=0; i<rowNum*columnNum; i++)
  {
    if ((i % rowNum == 0) && (i != 0))
    {
      x--;
    }
    cout << "Node" << i << "\tNA\t" << x << "\t" << y << "\n";
    y++;
    if (y > rowNum) y = 1;
  }

  // link part
  cout << "\nlink\n\n";
  for(int i = 0; i < columnNum; i++)
  {
    for(int j = 0; j < rowNum-1; j++)
    {
      cout << "Node" << j+(i*rowNum) << "\tNode" << j+(i*rowNum)+1 << "\t" << argv[3] << "Mbps\t" << argv[4] << "\t" << argv[5] << "ms\t" << argv[6] << "\n";
    }
  }
  for(int i = 0; i < columnNum-1; i++)
  {
    for(int j = 0; j < rowNum; j++)
    {
      cout << "Node" << j+(i*rowNum) << "\tNode" << j+((i+1)*rowNum) << "\t" << argv[3] << "Mbps\t" << argv[4] << "\t" << argv[5] << "ms\t" << argv[6] << "\n";
    }
  }

  return 0;
}

