#include "raylib.h"
#include <mosquitto.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int rayInit();
int mqttInit();

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
        // Draw
        // ----------------------------------------------------------------------------------
        BeginDrawing();
            ClearBackground(RAYWHITE);
            // Draw the text above the circles
            printf("%s\n", payloadMessage);
            DrawText(payloadMessage, 10, 0, 20, RED);

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

/* Callback called when a message is received from a subscribed topic. */
void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message) {
    if (!message->payload || message->payloadlen <= 0) return;

    int payloadsize = message->payloadlen < sizeof(payloadMessage) ? message->payloadlen : sizeof(payloadMessage);
    memcpy(&payloadMessage, (char *)message->payload, payloadsize);
    payloadMessage[payloadsize] = '\0';
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