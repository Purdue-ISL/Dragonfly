//#include <sys/stat.h>
#pragma warning(disable : 4996)
//#include <direct.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <folly/String.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#define SIZEX 12
#define SIZEY 12
#define stage1 19
#define CUT 29
#define SumUser 1
//#define cutting_threshold 7
using namespace std;
vector<string> rs;

double JND[SIZEX + 1][SIZEY + 1], JND0[SIZEX + 1][SIZEY + 1][SumUser + 1];
double avgtiles = 0, area = 0, SC[SIZEX + 1][SIZEY + 1][SIZEX + 1][SIZEY + 1],
       SC2[SIZEX + 1][SIZEY + 1][SIZEX + 1][SIZEY + 1],
       f[SIZEX + 1][SIZEY + 1][SIZEX + 1][SIZEY + 1][CUT + 1],
       g[SIZEX + 1][SIZEY + 1][SIZEX + 1][SIZEY + 1][CUT + 1];
int isfixed[SIZEX + 1][SIZEY + 1][SIZEX + 1][SIZEY + 1],
    st1dir[SIZEX + 1][SIZEY + 1][SIZEX + 1][SIZEY + 1],
    st1pos[SIZEX + 1][SIZEY + 1][SIZEX + 1][SIZEY + 1];
int viewport[SIZEX + 1][SIZEY + 1][SumUser + 1];
int direction[SIZEX + 1][SIZEY + 1][SIZEX + 1][SIZEY + 1][CUT + 1],
    position[SIZEX + 1][SIZEY + 1][SIZEX + 1][SIZEY + 1][CUT + 1],
    cutp[SIZEX + 1][SIZEY + 1][SIZEX + 1][SIZEY + 1][CUT + 1];
int direction2[SIZEX + 1][SIZEY + 1][SIZEX + 1][SIZEY + 1][CUT + 1],
    position2[SIZEX + 1][SIZEY + 1][SIZEX + 1][SIZEY + 1][CUT + 1],
    cutp2[SIZEX + 1][SIZEY + 1][SIZEX + 1][SIZEY + 1][CUT + 1];
int sum, tilemap[SIZEX + 1][SIZEY + 1];

double avgarea, avgbias;

struct mylist {
  int x1, y1, x2, y2;
} myl[1000];

double score(int x1, int y1, int x2, int y2) {
  if (x1 == x2 && y1 == y2)
    return 0;
  double avg = 0, bias = 0;
  for (int i = x1; i <= x2; ++i)
    for (int j = y1; j <= y2; ++j)
      avg += JND[i][j];
  avg = avg / (x2 - x1 + 1) / (y2 - y1 + 1);
  for (int i = x1; i <= x2; ++i)
    for (int j = y1; j <= y2; ++j)
      bias += (JND[i][j] - avg) * (JND[i][j] - avg);
  double dx = x2 - x1 + 1;
  double dy = y2 - y1 + 1;
  double k = max(dx, dy);
  // bias = (bias + 0.00001) * sqrt(max(dx / dy , dy / dx));
  return bias;
}

double score2(int x1, int y1, int x2, int y2) {
  int res = 0, flag = 0;

  for (int i = 1; i <= SumUser; ++i) {
    flag = 0;
    for (int j = x1; j <= x2; ++j) {
      for (int k = y1; k <= y2; ++k)
        if (viewport[j][k][i] == 0) {
          flag = 1;
          break;
        }
      if (flag)
        break;
    }
    res +=  flag * (x2 - x1 + 1) * (y2 - y1 + 1);
  }
  return res;
}

void dp(int x1, int y1, int x2, int y2, int k) {
  if (k == 0) {
    f[x1][y1][x2][y2][k] = SC[x1][y1][x2][y2];
    return;
  }
  double tmp;
  f[x1][y1][x2][y2][k] = 1000000;
  for (int i = x1 + 1; i <= x2; ++i)
    for (int k1 = 0; k1 < k; ++k1) {
      tmp = f[x1][y1][i - 1][y2][k1] + f[i][y1][x2][y2][k - 1 - k1];
      if (tmp < f[x1][y1][x2][y2][k]) {
        f[x1][y1][x2][y2][k] = tmp;
        direction[x1][y1][x2][y2][k] = 0;
        position[x1][y1][x2][y2][k] = i;
        cutp[x1][y1][x2][y2][k] = k1;
      }
    }
  for (int i = y1 + 1; i <= y2; ++i)
    for (int k1 = 0; k1 < k; ++k1) {
      tmp = f[x1][y1][x2][i - 1][k1] + f[x1][i][x2][y2][k - 1 - k1];
      if (tmp < f[x1][y1][x2][y2][k]) {
        f[x1][y1][x2][y2][k] = tmp;
        direction[x1][y1][x2][y2][k] = 1;
        position[x1][y1][x2][y2][k] = i;
        cutp[x1][y1][x2][y2][k] = k1;
      }
    }
}

void dp2(int x1, int y1, int x2, int y2, int k) {
  if (k == 0) {
    if (isfixed[x1][y1][x2][y2])
      g[x1][y1][x2][y2][k] = 1000000;
    else
      g[x1][y1][x2][y2][k] = SC2[x1][y1][x2][y2];
    return;
  }
  double tmp;
  g[x1][y1][x2][y2][k] = 1000000;
  for (int i = x1 + 1; i <= x2; ++i) {
    if (isfixed[x1][y1][x2][y2]) {
      if (st1dir[x1][y1][x2][y2] != 0)
        continue;
      if (st1pos[x1][y1][x2][y2] != i)
        continue;
    }
    for (int k1 = 0; k1 < k; ++k1) {
      tmp = g[x1][y1][i - 1][y2][k1] + g[i][y1][x2][y2][k - 1 - k1];
      if (tmp < g[x1][y1][x2][y2][k]) {
        g[x1][y1][x2][y2][k] = tmp;
        direction2[x1][y1][x2][y2][k] = 0;
        position2[x1][y1][x2][y2][k] = i;
        cutp2[x1][y1][x2][y2][k] = k1;
      }
    }
  }
  for (int i = y1 + 1; i <= y2; ++i) {
    if (isfixed[x1][y1][x2][y2]) {
      if (st1dir[x1][y1][x2][y2] != 1)
        continue;
      if (st1pos[x1][y1][x2][y2] != i)
        continue;
    }
    for (int k1 = 0; k1 < k; ++k1) {
      tmp = g[x1][y1][x2][i - 1][k1] + g[x1][i][x2][y2][k - 1 - k1];
      if (tmp < g[x1][y1][x2][y2][k]) {
        g[x1][y1][x2][y2][k] = tmp;
        direction2[x1][y1][x2][y2][k] = 1;
        position2[x1][y1][x2][y2][k] = i;
        cutp2[x1][y1][x2][y2][k] = k1;
      }
    }
  }
}

void maketile() {
  for (int i = 0; i < SIZEX; ++i)
    for (int j = 0; j < SIZEY; ++j)
      for (int i1 = i; i1 < SIZEX; ++i1)
        for (int j1 = j; j1 < SIZEY; ++j1) {
          SC2[i][j][i1][j1] = score(i, j, i1, j1);
          SC[i][j][i1][j1] = score2(i , j , i1 , j1);
        }
  for (int dx = 0; dx < SIZEX; ++dx)
    for (int dy = 0; dy < SIZEY; ++dy)
      for (int i = 0; i + dx < SIZEX; ++i)
        for (int j = 0; j + dy < SIZEY; ++j)
          for (int k = 0; k <= stage1; ++k)
            dp(i, j, i + dx, j + dy, k);
}

void tiling(int x1, int y1, int x2, int y2, int k) {
  if (k == 0) {
    /*myl[++sum].x1 = x1 + 1;
    myl[sum].y1 = y1 + 1;
    myl[sum].x2 = x2 + 1;
    myl[sum].y2 = y2 + 1;
    for (int i = x1; i <= x2; ++i)
        for (int j = y1; j <= y2; ++j)
            tilemap[i][j] = sum;*/
    return;
  }
  isfixed[x1][y1][x2][y2] = 1;
  st1dir[x1][y1][x2][y2] = direction[x1][y1][x2][y2][k];
  st1pos[x1][y1][x2][y2] = position[x1][y1][x2][y2][k];
  if (direction[x1][y1][x2][y2][k] == 0) {
    tiling(x1, y1, position[x1][y1][x2][y2][k] - 1, y2,
           cutp[x1][y1][x2][y2][k]);
    tiling(position[x1][y1][x2][y2][k], y1, x2, y2,
           k - 1 - cutp[x1][y1][x2][y2][k]);
  } else {
    tiling(x1, y1, x2, position[x1][y1][x2][y2][k] - 1,
           cutp[x1][y1][x2][y2][k]);
    tiling(x1, position[x1][y1][x2][y2][k], x2, y2,
           k - 1 - cutp[x1][y1][x2][y2][k]);
  }
}

void maketile2() {
  for (int dx = 0; dx < SIZEX; ++dx)
    for (int dy = 0; dy < SIZEY; ++dy)
      for (int i = 0; i + dx < SIZEX; ++i)
        for (int j = 0; j + dy < SIZEY; ++j)
          for (int k = 0; k <= CUT; ++k)
            dp2(i, j, i + dx, j + dy, k);
}

void tiling2(int x1, int y1, int x2, int y2, int k) {
  if (k == 0) {
    myl[++sum].x1 = x1 + 1;
    myl[sum].y1 = y1 + 1;
    myl[sum].x2 = x2 + 1;
    myl[sum].y2 = y2 + 1;
    for (int i = x1; i <= x2; ++i)
      for (int j = y1; j <= y2; ++j)
        tilemap[i][j] = sum;
    return;
  }

  if (direction2[x1][y1][x2][y2][k] == 0) {
    tiling2(x1, y1, position2[x1][y1][x2][y2][k] - 1, y2,
            cutp2[x1][y1][x2][y2][k]);
    tiling2(position2[x1][y1][x2][y2][k], y1, x2, y2,
            k - 1 - cutp2[x1][y1][x2][y2][k]);
  } else {
    tiling2(x1, y1, x2, position2[x1][y1][x2][y2][k] - 1,
            cutp2[x1][y1][x2][y2][k]);
    tiling2(x1, position2[x1][y1][x2][y2][k], x2, y2,
            k - 1 - cutp2[x1][y1][x2][y2][k]);
  }
}

void print(char outFile[]) {
  ofstream fq;
  fq.open(outFile); //输出

  for (int i = 1; i <= CUT + 1; ++i) {
    //std::cout << myl[i].x1 << "," << myl[i].x2 << "," << myl[i].y1 << ","
     //         << myl[i].y2 << std::endl;
    std::string out = std::to_string(i)+","+std::to_string(myl[i].x1)+","+std::to_string(myl[i].x2)+","+std::to_string(myl[i].y1)+","+std::to_string( myl[i].y2)+"\n";
    fq.write(out.c_str(),out.length());
  }
}

void read_write(char fileName[],char outFile[]) {

  float PSNR17[SIZEX + 1][SIZEY + 1];
  float PSNR50[SIZEX + 1][SIZEY + 1];
  ifstream infile;
  infile.open(fileName);

  std::string line;
  while (std::getline(infile, line)) {
    std::istringstream iss(line);

    int pos = line.find(":");
    std::string tileQP = line.substr(0, pos);
    int tileId = stoi(tileQP.substr(0, tileQP.find("-")));
    int qp = stoi(tileQP.substr(tileQP.find("-") + 1));
    line = line.substr(pos + 2);
    int i = (tileId - 1) / 12;
    int j = (tileId - 1) % 12;
    line.pop_back();
    std::vector<float> temp;
    folly::split(",", line, temp);
    if (qp == 17 || qp == 50) {
      float sum = 0;
      for (auto v : temp) {
        sum += v;
      }
      sum = sum / temp.size();
      if (qp == 17) {
        PSNR17[i][j] = sum;
      } else {
        PSNR50[i][j] = sum;
      }
    }
  }

  for (int j = 0; j < SIZEX; ++j) {
    std::string line = "";
    for (int k = 0; k < SIZEY; ++k) {
      JND[j][k] = (PSNR17[j][k] - PSNR50[j][k]);
      line += std::to_string((JND[j][k])) + " , ";
    }
    line.pop_back();
    std::cout << line << "\n";
  }
  sum = 0;
  memset(isfixed, 0, sizeof(isfixed));
  maketile();
  memset(tilemap, 0, sizeof(tilemap));
  tiling(0, 0, SIZEX - 1, SIZEY - 1, stage1);
  maketile2();
  tiling2(0, 0, SIZEX - 1, SIZEY - 1, CUT);

  print(outFile);
}

int main(int argc, char *argv[]){
  
  if (argc < 3){
    std::cout<<"Usage error: tileGrouping.o <psnr file> <videoName>\n";
    return -1;
  }
 
   read_write(argv[1],argv[2]); 
   return 1;
}
