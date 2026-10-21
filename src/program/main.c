#include "raylib.h"
#include <mosquitto.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int rayInit();
int mqttInit();
void process(double *accum_x, double *accum_y);

double gx, gy, gz;
char txtGX[10] = "NaN", txtGY[10] = "NaN", txtGZ[10] = "NaN";
double accum_x, accum_y;
char payloadMessage[256] = "Waiting...";
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
        process(&accum_x, &accum_y);
        // Draw
        // ----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(RAYWHITE);
            // Draw the text above the circles
            DrawText(txtGX, 10, 0, 20, RED);
            DrawText(txtGY, 10, 20, 20, GREEN);
            DrawText(txtGZ, 10, 40, 20, BLUE);
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

void gyroParser(char* coord, double* gx, double* gy, double* gz, double *accum_x, double *accum_y){
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
    double sensitivityFactor = 0.08;
    double calibrated_gx = (raw_gx+162)*-1;
    double calibrated_gy = raw_gy+234;
    double calibrated_gz = raw_gz+421;
    *gx = (calibrated_gx >= 5 && calibrated_gx <= 20) ? 0 : calibrated_gx;
    *gy = (calibrated_gy >= 5 && calibrated_gy <= 20) ? 0 : calibrated_gy;
    *gz = (calibrated_gz >= 5 && calibrated_gz <= 20) ? 0 : calibrated_gz;
    int pDelta_x = (int)(*gx * sensitivityFactor);
    int pDelta_y = (int)(*gy * sensitivityFactor);
    *accum_x += pDelta_x;
    *accum_y += pDelta_y;
    double axis;
    char *txt;
    for (int i=0; i<3 ;i++){
        if (i==0){axis = *gx; txt = txtGX;}
        else if (i==1){axis = *gy; txt = txtGY;}
        else if (i==2){axis = *gz; txt = txtGZ;}

        int length = snprintf(NULL, 0, "%lf", axis);
        // Allocate memory (+1 for the null terminator '\0')
        snprintf(txt, length + 1, "%lf", axis);
    }
     
}

/* Callback called when a message is received from a subscribed topic. */
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
    if (!message->payload || message->payloadlen <= 0) return;

    int payloadsize = message->payloadlen < sizeof(payloadMessage) ? message->payloadlen : sizeof(payloadMessage);
    memcpy(&payloadMessage, (char *)message->payload, payloadsize);
    payloadMessage[payloadsize] = '\0';
    gyroParser(payloadMessage, &gx, &gy, &gz, &accum_x, &accum_y);
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

void process(double *accum_x, double *accum_y){
    CGEventRef event = CGEventCreate(NULL);
    CGPoint cursor_pos = CGEventGetLocation(event);

    CGWarpMouseCursorPosition(CGPointMake(cursor_pos.x+(int)*accum_x, cursor_pos.y+(int)*accum_y));
    *accum_x -= (int)*accum_x;
    *accum_y -= (int)*accum_x;
}
