#include <string>
#include <cstring>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <iostream>

using namespace std;

int main(int argc, char* argv[])
{
  ifstream ifs;
  ifs.open(argv[1]);

  char trash[30];
  char buf[30];
  char seps[] = ",-)";
  char *token;
  int packet;
  double byte;
  int from = 0;
  int to = 0;
  int packetArray[50][50];
  double byteArray[50][50];

  for(int i=0; i<50; i++) {
    for(int j=0; j<50; j++) {
      packetArray[i][j] = -1;
      byteArray[i][j] = -1;
    }
  }

  ifs >> trash;
  ifs >> trash;
  ifs >> trash;
  ifs >> trash;
  ifs >> trash;
  ifs >> trash;
  ifs >> trash;

  while(1)
  {
    ifs >> trash;
    if(ifs.fail()) break;
    ifs >> trash;
    ifs >> trash;
    ifs >> buf;
    strtok(buf, seps);
    token = strtok(NULL, seps);
    if(token == NULL)
    {
      ifs >> trash;
      ifs >> trash;
      ifs >> trash;
    }
    else
    {
      from = atoi(token);
      token = strtok(NULL, seps);
      to = atoi(token);
      ifs >> trash;
      ifs >> packet;
      ifs >> byte;
      cout << from << "\t" << to << "\t" << packet << "\t" << byte << endl;
      if(packetArray[from][to] == -1) packetArray[from][to] = 0;
      packetArray[from][to] += packet;
      if(byteArray[from][to] == -1) byteArray[from][to] = 0;
      byteArray[from][to] += byte;
    }
    memset(buf, 0, sizeof(char)*(30));
  }

  ofstream ofs;
  ofs.open(argv[2]);
  for(int i=0; i<50; i++) {
    for(int j=0; j<50; j++) {
      if(packetArray[i][j] != -1) {
        ofs << i << "\t" << j << "\t" << packetArray[i][j]/2 << "\t" << byteArray[i][j]/2 << "\n";
      }
    }
  }
}
