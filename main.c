
#include "render_text.h"
#include "time_calc.h"
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringSerial.h>

#define PLANETS 8
#define PI 3.1415926535897932384626433832795
#define MY_FONT "/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf"
#define WIDTH 1920
#define HEIGHT 1080
int drawHeight = 0;

#define SPEED_MULTIPLIER 0.08
const int drawWidth = (WIDTH - (WIDTH / 6));
bool layout = false;
const int textWidth = drawWidth + 10;
double angle[8];
int plusCalc, minusCalc;
bool button = false;
bool setup = false;
int center[2];

typedef struct {
  const char *name;
  double distanceFromSun;
  double angle;
  double speed;
  int size;
  SDL_Color color;
} Planet;

void CircleFunction(SDL_Renderer *render, int x, int y, int r,
                    SDL_Color color) {
  SDL_SetRenderDrawColor(render, color.r, color.g, color.b, 255);
  for (int i = -r; i <= r; i++) {
    for (int j = -r; j <= r; j++) {
      if (i * i + j * j <= r * r) {
        SDL_RenderDrawPoint(render, x + i, j + y);
      }
    }
  }
}

void clearPrev(SDL_Renderer *renderer, TTF_Font *font, const char *current,
               const char *future, const char *past, int textWidth) {
  int horizontalLines[] = {270, 540, 810};
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

  SDL_RenderDrawLine(renderer, drawWidth, 1, drawWidth, HEIGHT);

  for (int i = 0; i < 3; i++) {
    SDL_RenderDrawLine(renderer, drawWidth, horizontalLines[i], WIDTH,
                       horizontalLines[i]);
  }
  CircleFunction(renderer, center[0], center[1], 25,
                 (SDL_Color){255, 255, 0, 255});
  RenderText(renderer, font, current, future, past, textWidth);

  return;
}

void updatePosition(Planet *p) {
  p->angle += p->speed;
  if (p->angle > 2 * PI)
    p->angle -= 2 * PI;
}

void planetDraw(SDL_Renderer *render, Planet p, int x, int y) {

  int cx = x + cos(p.angle) * p.distanceFromSun;
  int cy = y + sin(p.angle) * p.distanceFromSun;

  CircleFunction(render, cx, cy, p.size, p.color);
}

int openSerial() {
  int serial_port;

  if ((serial_port = serialOpen("/dev/ttyACM0", 115200)) < 0) {
    return 1;
  }
  if (wiringPiSetup() == -1) {
    return 1;
  }
  printf("Serial port opened successfully. Reading data ... \n");

  return serial_port;
}

int main() {
  int serial_port = openSerial();
  if (serial_port == 1) {
    return 1;
  }

  char buf[1000000];
  int bufIndex = 0;
  int tokenCount = 0;
  int i = 0;

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL Init Failed: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  SDL_Window *window = SDL_CreateWindow("Planetarium", SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED, WIDTH,
                                        HEIGHT, // Window width and height.
                                        SDL_WINDOW_FULLSCREEN);

  if (TTF_Init() < 0) {
    SDL_Quit();
    return EXIT_FAILURE;
  }
  SDL_Renderer *renderer =
      SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

  time_t currentDate;
  time(&currentDate);
  long long timeSinceEpoch = (long long)currentDate;
  int daysSinceEpoch = timeSinceEpoch / SECONDS_IN_DAY;

  Planet planets[] = {
      // Initialize the planets with distance from sun, initial angle (based
      // upon real time), orbit speed, size and colour.

      {"Mercury", 60, 0, 4.091, 4, {183, 184, 185, 255}},
      {"Venus", 90, 0, 1.6, 6, {248, 226, 176, 255}},
      {"Earth", 130, 0, 0.985626, 6, {0, 100, 255, 255}},
      {"Mars", 170, 0, 0.5263, 5, {173, 98, 60, 255}},
      {"Jupiter", 240, 0, 0.0831, 14, {227, 220, 209, 255}},
      {"Saturn", 310, 0, 0.03346, 12, {206, 184, 184, 255}},
      {"Uranus", 380, 0, 0.01173, 9, {175, 229, 238, 255}},
      {"Neptune", 450, 0, 0.00598, 9, {91, 93, 223, 255}}

  };

  for (int i = 0; i < PLANETS; i++) {
    // days since epoch * orbital speed
    double angleCalculated = fmod(daysSinceEpoch * planets[i].speed, 2 * PI);

    if (angleCalculated < 0)
      angleCalculated += 2 * PI;
    planets[i].angle = angleCalculated;
  }

  char current[25];
  char future[25];
  char past[25];
  char simulated[25];
  int plusDays, minusDays;

  plusDays = 1000;
  minusDays = 1000;

  timeCalculation(current, future, past, plusDays, minusDays);

  int simulatedDays = (timeSinceEpoch - (minusDays * SECONDS_IN_DAY));
  formatDate(simulatedDays, simulated);

  bool running = true;
  SDL_Event event;
  center[0] = drawWidth / 2;
  center[1] = HEIGHT / 2;

  TTF_Font *font = TTF_OpenFont(MY_FONT, 24);

  clearPrev(renderer, font, current, future, past, textWidth);

  while (running) {
    delay(1000); 
    if (serialDataAvail(serial_port)) {
        delay(1000); 
      bufIndex = 0;
      tokenCount = 0;
      while (serialDataAvail(serial_port)) {
        char c = serialGetchar(serial_port);
        if (c == '\n')
          break;
        buf[bufIndex++] = c;
      }
      buf[bufIndex] = '\0';
      printf("Received: %s\n", buf);
      delay(100);

      char *tokens[20];

      char *token;

      token = strtok(buf, "!");

      while (token != NULL) {
        tokens[tokenCount] = token;
        tokenCount++;
        token = strtok(NULL, "!");
      }
      if (tokenCount >= 2) {
        if (tokenCount == 3) {
          button = true;
        }
        minusDays = atoi(tokens[0]);
        plusDays = atoi(tokens[1]);
        timeCalculation(current, future, past, plusDays, minusDays);
      }
    }
    tokenCount = 0; 

    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (event.type == SDL_KEYDOWN) {
        if (event.key.keysym.sym == SDLK_q ||
            event.key.keysym.sym == SDLK_ESCAPE) {
          running = false;
        }
      }
    }
    clearPrev(renderer, font, current, future, past, textWidth);

    if (!font) {
      printf("Failed to load font: %s\n", TTF_GetError());
      return EXIT_FAILURE;
    }

    RenderText(renderer, font, current, future, past, textWidth);

    if (button == true) {
      int start = - minusDays;
      int finish = + plusDays;
      int simulatedDays = (daysSinceEpoch-minusDays) * SECONDS_IN_DAY;


      while (start < finish) {
              close(serial_port);
        clearPrev(renderer, font, current, future, past, textWidth);
        formatDate(simulatedDays, simulated);
        simulatedDate(renderer, font, simulated, textWidth);

        while (SDL_PollEvent(&event)) {
          if (event.type == SDL_QUIT) {
            running = false;
          } 
          else if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_q ||
                event.key.keysym.sym == SDLK_ESCAPE) {
              running = false;
            }
          }
        }

        if (!running)
          break; 

        for (int i = 0; i < PLANETS; i++) {
          double angleCalculated =
              fmod(start * planets[i].speed * SPEED_MULTIPLIER, 2 * PI);
          if (angleCalculated < 0)
            angleCalculated += 2 * PI;
          if (angleCalculated < 0) {
            angleCalculated += 2 * PI;
          }
          planets[i].angle = angleCalculated;
          planetDraw(renderer, planets[i], center[0], center[1]);
        }
        start++;

        SDL_RenderPresent(renderer);
        SDL_Delay(1);
        simulatedDays = simulatedDays + SECONDS_IN_DAY;
      }
      delay(100);
      openSerial();

      serialPuts(serial_port, "Hi");
      delay(100);
      buf[0] = '\0'; // clears the buffer. need to do this at end
      tcflush(serial_port, TCIOFLUSH);
      bufIndex = 0;
      tokenCount = 0;
      button = false;
 
    }
    if (button == false) {
      clearPrev(renderer, font, current, future, past, textWidth);
      //simulatedDate(renderer, font, simulated, textWidth);

      for (int i = 0; i < PLANETS; i++) {
        double angleCalculated = fmod(daysSinceEpoch * planets[i].speed, 360);
        if (angleCalculated < 0) {
          angleCalculated += 2 * PI;
        }
        planets[i].angle = angleCalculated;
        planetDraw(renderer, planets[i], center[0], center[1]);
      }
    }

    SDL_RenderPresent(renderer);
    SDL_Delay(1); // 16 will give ~60fps if we are having issues can cut to 33
                  // for 30fps and it will still be fairly smooth.
    delay(1000);
    buf[0] = '\0'; // clears the buffer. need to do this at end
    tcflush(serial_port, TCIOFLUSH);
    i = 0;
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  TTF_CloseFont(font);
  TTF_Quit();

  return 0;
}





// MAKE FILE

/*
# A simple Makefile for compiling small SDL projects

# set the compiler
CC := gcc

# set the compiler flags
CFLAGS := `sdl2-config --libs --cflags` -ggdb3 -O0 --std=c99 -Wall -lSDL2_image
-lSDL2_ttf -lm

# add header files here
HDRS :=

# add source files here
<<<<<<< HEAD
SRCS := Graphics.c
=======
SRCS := Graphics.c time_calc.c
>>>>>>> 570d761 (Added function that calculats +/- x days)

# generate names of object files
OBJS := $(SRCS:.c=.o)

# name of executable
EXEC := planetarium

# default recipe
all: $(EXEC)

# recipe for building the final executable
$(EXEC): $(OBJS) $(HDRS) makefile
        $(CC) -o $@ $(OBJS) $(CFLAGS)

# recipe for building object files
#$(OBJS): $(@:.o=.c) $(HDRS) makefile
#	$(CC) -o $@ $(@:.o=.c) -c $(CFLAGS)

# recipe to clean the workspace
clean:
        rm -f $(EXEC) $(OBJS)

.PHONY: all clean
*/

/*OLD PLANET SCALING FOR TESTING WHERE ITS NOT GOING AT 47,000 KM/H

    Planet planets[] ={ //Initialize the planets with distance from sun, initial
angle (will change when adding real time), speed around the sun, size and
colour.
        {"Mercury", 60, 0, 0.05, 4, {183, 184, 185, 255}},
        {"Venus", 90, 0, 0.01667, 6 ,{248, 226, 176, 255}},
        {"Earth", 130, 0, 0.01, 6, {0, 100, 255, 255}},
        {"Mars", 170, 0,  0.005, 5, {173,98, 60, 255}},
        {"Jupiter", 240, 0, 0.00084, 14, {227, 220, 209, 255}},
        {"Saturn", 310, 0, 0.0003898, 12, {206, 184, 184, 255}},
        {"Uranus", 380, 0, 0.0001905, 9, {175, 229, 238, 255}},
        {"Neptune", 450,0, 0.00006068, 9, {91, 93, 223, 255}}
    };

*CURRENT PLANET SCALING FOR ACTUAL PROJECT.
*     Planet planets[] ={ //Initialize the planets with distance from sun,
initial angle (will change when adding real time), speed around the sun, size
and colour.
        {"Mercury", 60, 0, 4.091, 4, {183, 184, 185, 255}},
        {"Venus", 90, 0, 1.6, 6 ,{248, 226, 176, 255}},
        {"Earth", 130, 0, 0.985626, 6, {0, 100, 255, 255}},
        {"Mars", 170, 0, 0.5263, 5,{173,98, 60, 255}},
        {"Jupiter", 240, 0, 0.0831, 14, {227, 220, 209, 255}},
        {"Saturn", 310, 0, 0.03346, 12, {206, 184, 184, 255}},
        {"Uranus", 380, 0, 0.01173, 9, {175, 229, 238, 255}},
        {"Neptune", 450,0, 0.00598, 9, {91, 93, 223, 255}}
    };
    *
    * */
