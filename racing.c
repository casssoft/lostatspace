#include <stdio.h>      /* printf, scanf, puts, NULL */
#include <stdlib.h>     /* srand, rand */

#define MAPX 1000
#define MAPY 1000

#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <time.h>       /* time */
#include <math.h>

#include "OpenSimplexNoise.h"
#define TRUE 1
#define FALSE 0

#define MAXSHOTS 64
#define MAXENEMIES 32
#define SHOTSPEED 1

#define SHOT_ACTIVE 1
#define SHOT_ENEMY 2
#define ENEMY_ACTIVE 1
#define ENEMY_WILL_COLLIDE 2
typedef struct {
  double x;
  double y;
  double dir;
  unsigned int f;
  char flags;
} Shot;

typedef struct {
  double x;
  double y;
  double vx;
  double vy;
  double dir;
  unsigned int fuel;
  unsigned int f;
  char flags;
} Enemy;

struct termios orig_termios;

static char hasreset = 0;
void reset_terminal_mode()
{
    if (!hasreset) {
      tcsetattr(0, TCSANOW, &orig_termios);
      hasreset = 1;
    }
}

void set_conio_terminal_mode()
{
    struct termios new_termios;

    /* take two copies - one for now, one for later */
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    /* register cleanup handler, and set the new terminal mode */
    atexit(reset_terminal_mode);
    cfmakeraw(&new_termios);
    new_termios.c_iflag &= ~IXOFF;
    tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

int getch()
{
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    } else {
        return c;
    }
}
#define PI 3.14159265
#define TILE_DIRTY 1
static unsigned int sWidth = 0;
static unsigned int sHeight = 0;
static int d_cx;
static int d_cy;
static unsigned char* tileflags = NULL;
static unsigned int fuel = 100;
static unsigned int numEnies = 0;
static unsigned int score = 0;
static unsigned char hasLag = FALSE;
static char dead = FALSE;
static char controlType = 0;

void markDirty(int x, int y, int cx, int cy) {
  if (x - cx >= 0 && x - cx < sWidth && y - cy >= 0 && y - cy < sHeight) {
    tileflags[x - cx + (y - cy)* sWidth] |= TILE_DIRTY;
  }
}


void drawEntity(int ex, int ey, int cx, int cy, double dir, int color, char c) {
  if (ex - cx >= 0 && ex - cx < sWidth && ey - cy >= 0 && ey - cy < sHeight) {
    //printf("CASFASFSF");
    printf("%c[%d;%dH%c", 0x1B, ey - cy + 1, ex - cx + 1, c); // move to row col and print O
    tileflags[ex - cx + (ey - cy) * sWidth] |= TILE_DIRTY;
  }
  //while (dir < 0) {
  //  dir += PI * 2;
  //}
  //while (dir > PI * 2) {
  //  dir -= PI * 2;
  //}
  int headX = (int)llround(ex + cos(dir));
  int headY = (int)llround(ey + sin(dir));
  if (headX - cx >= 0 && headX - cx < sWidth && headY - cy >= 0 && headY - cy < sHeight) {
    //printf("CASFASFSF");
    printf("%c[%d;%dHo", 0x1B, headY - cy + 1, headX - cx + 1); // move to row col and print o
    tileflags[headX - cx + (headY - cy) * sWidth] |= TILE_DIRTY;
  }
}
void draw(int* tilemap, Shot* shots, Enemy* enies, int cx, int cy, double x, double y, double dir) {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

  if (tileflags == NULL || sWidth != w.ws_col || sHeight != w.ws_row) {
    sWidth = w.ws_col;
    sHeight = w.ws_row;
    if (tileflags != NULL) free(tileflags);
    tileflags = (unsigned char*) malloc(sWidth * sHeight * sizeof(unsigned char));
    int p;
    for(p = 0; p < sWidth * sHeight; ++p) {
      tileflags[p] = TILE_DIRTY;
    }
    d_cx = cx;
    d_cy = cy;
  } else if (d_cx != cx || d_cy != cy) {
    int p;
    for(p = 0; p < sWidth * sHeight; ++p) {
      tileflags[p] = TILE_DIRTY;
    }
    d_cx = cx;
    d_cy = cy;
  }

  printf("%c[%d;%dH", 0x1B, 1, 1); // position cursor to 1,1
  // Tilemap
  int row,col;
  for (row = 0; row < sHeight; ++row) {
    for (col = 0; col < sWidth; ++col) {
      if (tileflags[col + row * sWidth] & TILE_DIRTY) {
        printf("%c[%d;%dH", 0x1B, row  + 1, col + 1);
        if (col + cx >= 0 && col + cx < MAPX && row + cy >= 0 && row + cy < MAPY) {
          switch(tilemap[(col + cx) + (row + cy) * MAPX]) {
            case 0:
              printf("$");
              break;
            case 1:
              printf("%c[%dmX%c[%dm", 0x1B, 32, 0x1B, 0); // green X
              break;
            case 2:
              printf("%c[%dmQ%c[%dm", 0x1B, 36, 0x1B, 0); // cyan Q
              break;
            default:
              printf(" ");
          }
        } else {
          printf(" ");
        }
        tileflags[col + row * sWidth] &= ~TILE_DIRTY;
      }
    }
    //printf("\n");
    //printf("%c[%d;%dH", 0x1B, row + 1, 1);
  }
  //Shots
  int i;
  for (i = 0; i < MAXSHOTS; ++i) {
    if (shots[i].flags & SHOT_ACTIVE) {
      int shotX = (int)shots[i].x;
      int shotY = (int)shots[i].y;
      if (shotX - cx >= 0 && shotX - cx < sWidth && shotY - cy >= 0 && shotY - cy < sHeight) {
        if (shots[i].flags & SHOT_ENEMY) {
          printf("%c[%d;%dH%c[%dm@%c[%dm", 0x1B, shotY - cy + 1, shotX - cx + 1, 0x1B, 32, 0x1B, 0); // Green shot
        } else {
          printf("%c[%d;%dH@", 0x1B, shotY - cy + 1, shotX - cx + 1);
        }
        tileflags[shotX - cx + (shotY - cy) * sWidth] = TILE_DIRTY;
      }
    }
  }

  // character
  drawEntity(x, y, cx, cy, dir, -1, 'O');
  for (i = 0; i < MAXENEMIES; ++i) {
    if (enies[i].flags & ENEMY_ACTIVE) {
      if (enies[i].flags & ENEMY_WILL_COLLIDE || enies[i].fuel == 0) {
        drawEntity(enies[i].x, enies[i].y, cx, cy, enies[i].dir, 6, '#');
      } else {
        drawEntity(enies[i].x, enies[i].y, cx, cy, enies[i].dir, 6, '%');
      }
    }
  }
  //int bodyX = (int)x;
  //int bodyY = (int)y;
  //if (bodyX - cx >= 0 && bodyX - cx < sWidth && bodyY - cy >= 0 && bodyY - cy < sHeight) {
  //  //printf("CASFASFSF");
  //  printf("%c[%d;%dHO", 0x1B, bodyY - cy + 1, bodyX - cx + 1); // move to row col and print O
  //  tileflags[bodyX - cx + (bodyY - cy) * sWidth] |= TILE_DIRTY;
  //}
  ////while (dir < 0) {
  ////  dir += PI * 2;
  ////}
  ////while (dir > PI * 2) {
  ////  dir -= PI * 2;
  ////}
  //int headX;
  //int headY;
  //headX = (int)llround(bodyX + cos(dir));
  //headY = (int)llround(bodyY + sin(dir));
  //if (headX - cx >= 0 && headX - cx < sWidth && headY - cy >= 0 && headY - cy < sHeight) {
  //  //printf("CASFASFSF");
  //  printf("%c[%d;%dHo", 0x1B, headY - cy + 1, headX - cx + 1); // move to row col and print o
  //  tileflags[headX - cx + (headY - cy) * sWidth] |= TILE_DIRTY;
  //}

  //printf("%c[%d;%dH", 0x1B, sHeight, sWidth); // position cursor to 1,1
  printf("%c[%d;%dHFUEL: %5i ", 0x1B, 1, 1, fuel); // position cursor to 1,1
  printf("%c[%d;%dHSCORE: %4i ", 0x1B, 2, 1, score); // position cursor to 1,1
  printf("%c[%d;%dHENEMIES: %2i ", 0x1B, 3, 1, numEnies); // position cursor to 1,1
  if (hasLag) {
    printf("%c[%d;%dHLAG", 0x1B, 4, 1); // position cursor to 1,1
    markDirty(0 , 3, 0, 0);
    markDirty(1 , 3, 0, 0);
    markDirty(2 , 3, 0, 0);
  }
  if (dead) {
    printf("%c[%d;%dHYOU HAVE DIED AT SPACE. PRESS R TO RESTART", 0x1B, sHeight/2 + 1, sWidth/2 - 12 + 1);
  }
  fflush(stdout);
}


int fireShot(int sx, int sy, double dir, char flags, Shot* shots) {
  int i = 0;
  while (i < MAXSHOTS && shots[i].flags & SHOT_ACTIVE) {
    ++i;
  }
  if (i != MAXSHOTS) {
    shots[i].flags = flags;
    shots[i].x = sx + .5;
    shots[i].y = sy + .5;
    shots[i].f = 0;
    shots[i].dir = dir;
    return 1;
  }
  return 0;
}

void explodeBox(int ex, int ey, int tile, int* tilemap, Enemy* enies, char points) {
  int i,j;
  for (j = ey - 3; j <= ey + 3; ++j) {
    for (i = ex - 3; i <= ex + 3; ++i) {
      if (i >= 0 && i < MAPX && j >= 0 && j < MAPY) {
        tilemap[i + j * MAPX] = tile;
        markDirty(i, j, d_cx, d_cy);
      }
    }
  }
  // points pretty much just means if it was our shot or not. No enemy friendly fire cause the player doesn't have to worry about it
  if (points) {
    for (i = 0; i < MAXENEMIES; ++i) {
      if (enies[i].flags & ENEMY_ACTIVE) {
        if (enies[i].x >= ex - 3 && enies[i].x <= ex + 3 && enies[i].y >= ey - 3 && enies[i].y <= ey + 3) {
          enies[i].flags = 0;
          numEnies -= 1;
          explodeBox(enies[i].x, enies[i].y, tile, tilemap, enies, 0);
          score += 3;
        }
      }
    }
  }
}

char shouldSteerAway(int * tilemap, double x, double y, double vx, double vy, unsigned int frames) {
  int lastx = x;
  int lasty = y;
  while (frames > 0) {
    x += vx;
    y += vy;
    int ex = x;
    int ey = y;
    if (ex != lastx || ey != lasty) {
      if (ex >= 0 && ex < MAPX && ey >= 0 && ey < MAPY) {
        if (tilemap[ex + ey * MAPX] == 1) {
          return TRUE;
        }
      }
      lastx = ex;
      lasty = ey;
    }
    frames--;
  }
  return FALSE;
}

double interpolateAngle(double* angle, double dirTo) {
    while (*angle > PI * 2) *angle -= PI * 2;
    while (*angle < 0) *angle += PI * 2;
    // angle is now normalized
    if (*angle == dirTo) return TRUE;

    double distanceMore;
    double distanceLess;
    if (*angle < dirTo) {
      distanceMore = dirTo - *angle;
      distanceLess = *angle + PI * 2 - dirTo;
    } else {
      distanceMore = dirTo + PI * 2 - *angle;
      distanceLess = *angle - dirTo;
    }
    if (distanceLess < distanceMore) return -1 * distanceLess;
    return distanceMore;
}

void takeAdjacentFuel(int* tilemap, int ex, int ey) {
  int i,j;
  for (j = ey - 1; j <= ey + 1; ++j) {
    for (i = ex - 1; i <= ex + 1; ++i) {
      if (i >= 0 && i < MAPX && j >= 0 && j < MAPY) {
        if (tilemap[i + j * MAPX] ==  0) {
          fuel += 10;
          tilemap[i + j * MAPX] = -1;
          markDirty(i, j, d_cx, d_cy);
          takeAdjacentFuel(tilemap, i, j);
        }
      }
    }
  }
}

int makeEnemy(int sx, int sy, double dir, char flags, Enemy* enies) {
  int i = 0;
  while (i < MAXENEMIES && enies[i].flags & ENEMY_ACTIVE) {
    ++i;
  }
  if (i != MAXENEMIES) {
    enies[i].flags = flags;
    enies[i].x = sx + .5;
    enies[i].y = sy + .5;
    enies[i].vx = 0;
    enies[i].vy = 0;
    enies[i].f = 0;
    enies[i].fuel = 100;
    enies[i].dir = dir;
    numEnies += 1;
    return 1;
  }
  return 0;
}
static OpenSimplexNoise osn;
void reset(int*tilemap, Shot*shots, Enemy*enies, double*x, double* y, int *cx, int *cy, double *vx, double *vy, double*dir) {
  dead = FALSE;
  fuel = 100;
  score = 0;
  *x = (int)(MAPX/2) + .5;
  *y = (int)(MAPY/2) + .5;
  *cx = *x - sWidth/2;
  d_cx = -1; // invalidate tileflags array
  *cy = *y - sHeight/2;
  *vx = 0;
  *vy = 0;
  *dir = 0;
  int i;
  for (i = 0; i < MAXSHOTS; ++i) {
    shots[i].flags = 0;
  }
  for (i = 0; i < MAXENEMIES; ++i) {
    enies[i].flags = 0;
  }
  numEnies = 0;
  int j;
  OSN_init(&osn, time(NULL));
  for (j = 0; j < MAPY; ++j) {
    for (i = 0; i < MAPX; ++i) {
      if (.2 * OSN_eval(&osn, i/5.0, j/5.0) + .8 * OSN_eval(&osn, (double)i/15.0, (double)j/15.0) > .1) {
        tilemap[i + j * MAPX] = 1;
      } else {
        if (rand() % 4000 == 0) {
          tilemap[i + j * MAPX] = 0;
          if (i - 1 >= 0) {
            tilemap[i - 1 + j * MAPX] = 0;
            if (j - 1 >= 0) {
              tilemap[i + (j - 1) * MAPX] = 0;
              tilemap[i - 1 + (j - 1)* MAPX] = 0;
            }
          }
        } else {
          tilemap[i + j * MAPX] = -1;
        }
      }
    }
  }
  explodeBox(*x, *y, -1, tilemap, enies, 0);
}
int main() {
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

  sWidth = w.ws_col;
  sHeight = w.ws_row;
  unsigned int topscore = 0;
  int cx = 0;
  int cy = 0;
  double x = 10.5;
  double y = 10.5;
  double vx = 0;
  double vy = 0;
  double dir = 0;
  srand(time(NULL));
  int tilemap[MAPX * MAPY];
  Shot shots[MAXSHOTS];
  Enemy enies[MAXENEMIES];
  reset(tilemap, shots, enies, &x, &y, &cx, &cy, &vx, &vy, &dir);

  //for (i = 0; i < MAPX * MAPY; ++i) {
  //  int num = (rand()%100000);
  //  if (num < 200) {
  //    tilemap[i] = 0;
  //    continue;
  //  }
  //  num -= 2000;
  //  if (num < 2000) {
  //    tilemap[i] = 1;
  //    continue;
  //  }
  //  num -= 2000;
  //  if (num < 5) {
  //    tilemap[i] = 2;
  //    continue;
  //  }
  //  num -= 5;
  //  tilemap[i] = -1;
  //}

  //set proper buffer mode
  //struct winsize w;
  //ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  //if (setvbuf(stdout, NULL, _IOFBF, w.ws_col * w.ws_row * 10) != 0) {
  //  fprintf(stderr, "Error setting buffering type for stdout\n");
  //  return 0;
  //}
  int i;
  printf("%c[?%dl", 0x1B, 25);
  set_conio_terminal_mode();
  printf("%c[%dJ", 0x1B, 2); // clear screen
  char quit = FALSE;


  struct timeval prevTime, curTime;

  long int mtimeDiff;

  while (!quit) {
    gettimeofday(&prevTime, NULL);
    while (kbhit()) {
      int c = getch();
      if (c == 'q') {
        quit = TRUE;
        break;
      }
      if (c == 'r') {
        reset(tilemap, shots, enies, &x, &y, &cx, &cy, &vx, &vy, &dir);
      }
      if (c == '1') {
        controlType = 0;
      }
      if (c == '2') {
        controlType = 1;
      }
      if (!dead) {
        if (controlType == 0) {
          if (fuel != 0) {
            switch (c) {
              case 'a':
                vx -= .03;
                fuel--;
                break;
              case 'd':
                vx += .03;
                fuel--;
                break;
              case 'w':
                vy -= .03;
                fuel--;
                break;
              case 's':
                vy += .03;
                fuel--;
                break;
            }
          }
          if (c == 'j') {
            dir -= PI/4;
          }
          if (c == 'l') {
            dir += PI/4;
          }
        } else if (controlType == 1) {
          if (c == 'a') {
            dir -= PI/4;
          }
          if (c == 'd') {
            dir += PI/4;
          }
          if (c == 'w' && fuel != 0) {
            vx += cos(dir) * .03;
            vy += sin(dir) * .03;
            --fuel;
          }
          if (c == 's' && fuel != 0) {
            vx -= cos(dir) * .03;
            vy -= sin(dir) * .03;
            --fuel;
          }
        }
        if (c == ' ' && fuel >= 2) {
          if (fireShot((int)x, (int)y, dir, SHOT_ACTIVE, shots)) {
            fuel -= 2;
          }
        }
      }
    }
    if (!dead) {
      x += vx;
      y += vy;
      if (rand() % 700 == 0) {
        int newex = cx + rand() % sWidth;
        int newey = cy + rand() % sHeight;
        explodeBox(newex, newey, -1, tilemap, enies, 0);
        makeEnemy(newex, newey, 0, ENEMY_ACTIVE, enies);
      }
      //vx *= .995;
      //vy *= .995;
      int bX = x;
      int bY = y;
      for (i = 0; i < MAXENEMIES; ++i) {
        if (enies[i].flags & ENEMY_ACTIVE) {

          if (enies[i].flags & ENEMY_WILL_COLLIDE) {
            //if (enies[i].fuel == 0) {
            //  score++;
            //}
            if (rand() % 5 == 0) {
              if (shouldSteerAway(tilemap, enies[i].x, enies[i].y, enies[i].vx, enies[i].vy, 120)) {
                if (interpolateAngle(&(enies[i].dir), atan2(-1 * enies[i].vy, -1 * enies[i].vx)) < 0) {
                  enies[i].dir -= PI/2;
                } else {
                  enies[i].dir += PI/2;
                }
                if (enies[i].fuel > 0) {
                  enies[i].vx += cos(enies[i].dir) * .03;
                  enies[i].vy += sin(enies[i].dir) * .03;
                  enies[i].fuel--;
                }
                if (enies[i].fuel > 0) {
                  enies[i].vx += cos(enies[i].dir) * .03;
                  enies[i].vy += sin(enies[i].dir) * .03;
                  enies[i].fuel--;
                }
                enies[i].flags |= ENEMY_WILL_COLLIDE;
              } else {
                enies[i].flags &= ~ENEMY_WILL_COLLIDE;
              }
            }

          } else {

            if (rand() % 20 == 0) {
              double dirTo = atan2(y - enies[i].y, x - enies[i].x);

              if (interpolateAngle(&(enies[i].dir), dirTo) < 0) {
                enies[i].dir -= PI/4;
              } else {
                enies[i].dir += PI/4;
              }
            }

            char steerCheck = FALSE;
            if (enies[i].fuel > 0 && rand() % 70 == 0) {
              enies[i].vx += cos(enies[i].dir) * .03;
              enies[i].vy += sin(enies[i].dir) * .03;
              enies[i].fuel--;
              steerCheck = TRUE;
            }

            if (steerCheck || enies[i].f % 100 == 0) {
              if (shouldSteerAway(tilemap, enies[i].x, enies[i].y, enies[i].vx, enies[i].vy, 120)) {
                if (interpolateAngle(&(enies[i].dir), atan2(-1 * enies[i].vy, -1 * enies[i].vx)) < 0) {
                  enies[i].dir -= PI/2;
                } else {
                  enies[i].dir += PI/2;
                }
                if (enies[i].fuel > 0) {
                  enies[i].vx += cos(enies[i].dir) * .03;
                  enies[i].vy += sin(enies[i].dir) * .03;
                  enies[i].fuel--;
                }
                if (enies[i].fuel > 0) {
                  enies[i].vx += cos(enies[i].dir) * .03;
                  enies[i].vy += sin(enies[i].dir) * .03;
                  enies[i].fuel--;
                }
                enies[i].flags |= ENEMY_WILL_COLLIDE;
              } else {
                enies[i].flags &= ~ENEMY_WILL_COLLIDE;
              }
            }
            if (enies[i].fuel >= 2 && rand() % 300 == 0) {
              if (fireShot(enies[i].x, enies[i].y, enies[i].dir, SHOT_ACTIVE | SHOT_ENEMY, shots)) {
                enies[i].fuel -= 2;
              }
            }
          }

          //if (rand() % 10000 == 0) {
          //  enies[i].fuel += 20;
          //}
          enies[i].x += enies[i].vx;
          enies[i].y += enies[i].vy;

          int eniesX = enies[i].x;
          int eniesY = enies[i].y;
          enies[i].f += 1;
          if (eniesX >= 0 && eniesX < MAPX && eniesY >= 0 && eniesY < MAPY) {
            switch(tilemap[eniesX + eniesY * MAPX]) {
                case 1:
                  enies[i].flags = 0;
                  numEnies -= 1;
                  explodeBox(eniesX, eniesY, -1, tilemap, enies, 0);
                  break;
            }
          }
        }
      }

      for (i = 0; i < MAXSHOTS; ++i) {
        if (shots[i].flags & SHOT_ACTIVE) {
          shots[i].x += cos(shots[i].dir) * SHOTSPEED;
          shots[i].y += sin(shots[i].dir) * SHOTSPEED;
          int shotX = (int) shots[i].x;
          int shotY = (int) shots[i].y;
          // Ways to explode shot:
          // Collision with Xs
          if (shotX >= 0 && shotX < MAPX && shotY >= 0 && shotY < MAPY) {
            if (tilemap[shotX + shotY * MAPX] == 1) {
              shots[i].flags = 0;
              explodeBox(shotX, shotY, -1, tilemap, enies, !(shots[i].flags & SHOT_ENEMY));
              continue;
            }
          }
          // Enemy shot collision with self
          if (shots[i].flags & SHOT_ENEMY) {
            if (bX == shotX && bY == shotY) {
              dead = TRUE;
              if (topscore < score) {
                topscore = score;
              }
              continue;
            }
          }
          // Non enemy shot collision with enemy
          if (!(shots[i].flags & SHOT_ENEMY)) {
            int e;
            for (e = 0; e < MAXENEMIES; ++e) {
              if (enies[e].flags & ENEMY_ACTIVE) {
                if ((int)enies[e].x == shotX && (int)enies[e].y == shotY) {
                  enies[e].flags = 0;
                  numEnies -= 1;
                  shots[i].flags = 0;
                  explodeBox(shotX, shotY, -1, tilemap, enies, 1);
                  score += 3;
                  continue;
                }
              }
            }
          }
          // Fade out
          shots[i].f += 1;
          if (shots[i].f > 500) {
            shots[i].flags = 0;
            continue;
          }
        }
      }
      if (bX >= 0 && bX < MAPX && bY >= 0 && bY < MAPY) {
        switch(tilemap[bX + bY * MAPX]) {
          case 0:
            score++;
            fuel += 10;
            tilemap[bX + bY * MAPX] = -1;
            markDirty(bX, bY, cx, cy);
            takeAdjacentFuel(tilemap, bX, bY);
            break;
          case 1:
            dead = TRUE;
            if (topscore < score) topscore = score;
            break;
        }
      }
      if (x < cx + 8) {
        cx -= sWidth/2;
      }
      if (x > cx + sWidth - 8) {
        cx += sWidth/2;
      }
      if (y < cy + 8) {
        cy -= sHeight/2;
      }
      if (y > cy + sHeight - 8) {
        cy += sHeight/2;
      }
    }
    draw(tilemap, shots, enies, cx, cy, x, y, dir);


    gettimeofday(&curTime, NULL);

    long seconds  = curTime.tv_sec  - prevTime.tv_sec;
    long useconds = curTime.tv_usec - prevTime.tv_usec;

    mtimeDiff = -1 * (((seconds) * 1000 + useconds/1000.0) + 0.5);
    mtimeDiff += 16;
    if (mtimeDiff > 0) {
      hasLag = FALSE;
      usleep(mtimeDiff * 1000);
    } else {
      hasLag = TRUE;
    }
  }
  printf("%c[%dJ", 0x1B, 2);
  printf("%c[?%dh", 0x1B, 25);
  printf("%c[%d;%dH", 0x1B, 1, 1);
  reset_terminal_mode();
  printf("Top Score: %d\n", topscore);
  return 0;
}


