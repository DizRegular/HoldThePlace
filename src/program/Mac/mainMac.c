#include "raylib.h"
#include <mosquitto.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>
#include <time.h>
//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int rayInit();
int mqttInit();
void process(double *accumX, double *accumY);

double gx, gy, gz;
char txtGX[10] = "NaN", txtGY[10] = "NaN", txtGZ[10] = "NaN";
double accumX = 0, accumY = 0;
char txtAccumX[11] = "NaN", txtAccumY[11] = "NaN";
char payloadMessage[256] = "Waiting...";
long lastMessageTime = 0;
double latencyMS = 0;
char txtPing[20] = "0";
struct mosquitto *mosqClient = NULL;
int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    rayInit();
    while (!mqttInit()){}
    
    //--------------------------------------------------------------------------------------



    // Main game loop
    while (!WindowShouldClose()) // Detect window close button or ESC key
    {
        //Logic
        // ----------------------------------------------------------------------------------
        process(&accumX, &accumY);
        // Draw
        // ----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(RAYWHITE);
            // Draw the text above the circles
            DrawText(txtGX, 10, 0, 20, RED);
            DrawText(txtGY, 10, 20, 20, GREEN);
            DrawText(txtGZ, 10, 40, 20, BLUE);

            DrawText(txtAccumX, 10, 80, 20, ORANGE);
            DrawText(txtAccumY, 10, 100, 20, PURPLE);
            DrawText(txtPing, 10, 120, 20, GREEN);
        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

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

void on_connect(struct mosquitto *mosq, void *obj, int reason_code) {
    printf("on_connect: %s\n", mosquitto_connack_string(reason_code));
    if (reason_code == 0) {
        /* Subscribe to the topic upon successful connection */
        int rc = mosquitto_subscribe(mosq, NULL, "HoldThePlaceIotDevice", 1);
        if (rc != MOSQ_ERR_SUCCESS) {
            fprintf(stderr, "Error subscribing: %s\n", mosquitto_strerror(rc));
        }
    } else {
        mosquitto_disconnect(mosq);
    }
}

void gyroParser(char* coord, double* gx, double* gy, double* gz, double *accumX, double *accumY){
    char *coordPtr = strtok(coord, ",");
    int raw_gx, raw_gy, raw_gz;
    for (int i=0; i<3; i++){
        if (i==0){
           raw_gx = atof(coordPtr);
        } else if (i==1){
            raw_gy = atof(coordPtr);
        } else {
            raw_gz = atof(coordPtr);
        }
        coordPtr = strtok(NULL, ",");
    }
    //calibrated gyro readings
    double sensitivityFactor = 0.001;
    double calibrated_gx = (raw_gx+172);
    double calibrated_gy = (raw_gy+233);
    double calibrated_gz = (raw_gz+416);
    if (fabs(calibrated_gx) <= 10) {
        *gx = 0;
    } else if (calibrated_gx > 0) {
        *gx = calibrated_gx - 10;
    } else {
        *gx = calibrated_gx + 10;
    }
    if (fabs(calibrated_gy) <= 10){
        *gy = 0;
    }else if (calibrated_gy > 0) {
        *gy = calibrated_gy - 10;
    } else {
        *gy = calibrated_gy + 10;
    }
    if (fabs(calibrated_gz) <= 10 ){
        *gz = 0;
    } else if (calibrated_gz > 0) {
        *gz = calibrated_gz - 10;
    } else  {
        *gz = calibrated_gz + 10;
    }
    double pDelta_x = -(*gx * sensitivityFactor*10 );
    double pDelta_y = -(*gz * sensitivityFactor*10 );
    *accumX += pDelta_y;
    *accumY += pDelta_x;
    double axis;
    char *txt;
    for (int i=0; i<3 ;i++){
        if (i==0){axis = *gx; txt = txtGX;}
        else if (i==1){axis = *gy; txt = txtGY;}
        else if (i==2){axis = *gz; txt = txtGZ;}

        int length = snprintf(NULL, 0, "%.2lf", axis);
        // Allocate memory (+1 for the null terminator '\0')
        snprintf(txt, length + 1, "%.2lf", axis);
    }
    snprintf(txtAccumX, 11, "%.2lf", *accumX);
    snprintf(txtAccumY, 11, "%.2lf", *accumY);
}

/* Callback called when a message is received from a subscribed topic. */
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
    if (!message->payload || message->payloadlen <= 0) return;

    int payloadsize = message->payloadlen < sizeof(payloadMessage) ? message->payloadlen : sizeof(payloadMessage);
    memcpy(&payloadMessage, (char *)message->payload, payloadsize);
    payloadMessage[payloadsize] = '\0';
    gyroParser(payloadMessage, &gx, &gy, &gz, &accumX, &accumY);
    
    double receivedTime = GetTime();
    latencyMS = (receivedTime - lastMessageTime)*1000;
    lastMessageTime = receivedTime;
    snprintf(txtPing, 20, "%.2lf", latencyMS);
}

int mqttInit() {
    int connectionResult;
    mosquitto_lib_init();

    mosqClient = mosquitto_new(NULL, true, NULL);
    if (!mosqClient) {
        fprintf(stderr, "Error: Out of memory.\n");
        mosquitto_lib_cleanup();
        return 0;
    }

    /* Assign our defined callback functions */
    mosquitto_connect_callback_set(mosqClient, on_connect);
    mosquitto_message_callback_set(mosqClient, on_message);

    /* Connect to a public test broker on port 1883 with a keepalive of 60 seconds */
    connectionResult = mosquitto_connect(mosqClient, "mqtt-dashboard.com", 1883, 60);
    if (connectionResult != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Unable to connect: %s\n", mosquitto_strerror(connectionResult));
        mosquitto_destroy(mosqClient);
        mosquitto_lib_cleanup();
        return 0;
    }
    mosquitto_loop_start(mosqClient);
    printf("Connecting to broker... Press Ctrl+C to exit.\n");

    return 1;

}

void process(double *accumX, double *accumY){
    CGEventRef event = CGEventCreate(NULL);
    CGPoint cursor_pos = CGEventGetLocation(event);

    CGWarpMouseCursorPosition(CGPointMake(cursor_pos.x+(int)*accumX, cursor_pos.y+(int)*accumY));
    *accumX -= (int)*accumX;
    *accumY -= (int)*accumY;
}
