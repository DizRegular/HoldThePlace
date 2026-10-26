#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "raylib.h"
#include "win_mouse.h"

#include <unistd.h>
#include <time.h>

#define UDP_PORT 8080
#define BUFFER_SIZE 256

#define NoiseTheshold 100
//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int rayInit();
void gyroParser(char* coord, double* gx, double* gy, double* gz, double *accumX, double *accumY, int *isClick);

int isClick = 0;

double gx, gy, gz;

double accumX = 0.0;
double accumY = 0.0;
char lastMessage[BUFFER_SIZE] = "Waiting for data...";

long lastMessageTime = 0;
double latencyMS = 0;

int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    NetSocket sock;
    if (!net_init(&sock, 8080)) {
        printf("Network setup failed\n");
        return 1;
    }
    rayInit();

    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        //Logic
        // ----------------------------------------------------------------------------------
        
        char buffer[256];
        while (net_receive(sock, buffer, sizeof(buffer)) > 0) {
            gyroParser(buffer, &gx, & gy, &gz, &accumX, &accumY, &isClick);
            int stepX = (int)accumX;
            int stepY = (int)accumY;
            if (stepX != 0 || stepY != 0) {
                mouse_move_relative(stepX, stepY);
                accumX -= stepX;
                accumY -= stepY;
            }
            if (!isClick){
                mouse_left_click();
            }
        }
        
        // Draw
        // ----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(RAYWHITE);
            // Draw the text above the circles
            DrawText(TextFormat("%lf", gx), 10, 0, 20, RED);
            DrawText(TextFormat("%lf", gy), 10, 20, 20, GREEN);
            DrawText(TextFormat("%lf", gz), 10, 40, 20, BLUE);

            DrawText(TextFormat("%lf", accumX), 10, 80, 20, ORANGE);
            DrawText(TextFormat("%lf", accumY), 10, 100, 20, PURPLE);
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------
    net_close(sock);
    return 0;
}

int rayInit() {
    const int screenWidth = 800;
    const int screenHeight = 450;
    InitWindow(screenWidth, screenHeight, "raylib [core] example - delta time");

    int currentFps = 60;
    SetTargetFPS(currentFps);

    return 1;
}

void gyroParser(char* coord, double* gx, double* gy, double* gz, double *accumX, double *accumY, int *isClick){
    char *coordPtr = strtok(coord, ",");
    int calibrated_gx, calibrated_gy, calibrated_gz, isClicked;
    for (int i=0; i<4; i++){
        if (i==0){
           calibrated_gx = atof(coordPtr);
        } else if (i==1){
            calibrated_gy = atof(coordPtr);
        } else if (i==2){
            calibrated_gz = atof(coordPtr);
        } else {
            isClicked = atof(coordPtr);
        }
        coordPtr = strtok(NULL, ",");
    }
    *isClick = isClicked;
    //calibrated gyro readings
    double sensitivityFactor = 0.1;
    if (fabs(calibrated_gx) <= NoiseTheshold) {
        *gx = 0;
    } else if (calibrated_gx > 0) {
        *gx = calibrated_gx - NoiseTheshold;
    } else {
        *gx = calibrated_gx + NoiseTheshold;
    }
    if (fabs(calibrated_gy) <= NoiseTheshold){
        *gy = 0;
    }else if (calibrated_gy > 0) {
        *gy = calibrated_gy - NoiseTheshold;
    } else {
        *gy = calibrated_gy + NoiseTheshold;
    }
    if (fabs(calibrated_gz) <= NoiseTheshold ){
        *gz = 0;
    } else if (calibrated_gz > 0) {
        *gz = calibrated_gz - NoiseTheshold;
    } else  {
        *gz = calibrated_gz + NoiseTheshold;
    }
    double deg_sec_x = *gx / 131.0;
    double deg_sec_z = *gz / 131.0;

    double pDelta_x = -(deg_sec_x * sensitivityFactor);
    double pDelta_y = -(deg_sec_z * sensitivityFactor);

    *accumX += pDelta_y;
    *accumY += pDelta_x;
}


