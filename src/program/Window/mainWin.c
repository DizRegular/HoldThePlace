#include "raylib.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#include "win_mouse.h"
#include "serial_win.h"
#include "localip_win.h"

#include <unistd.h>
#include <time.h>

#define NoiseTheshold 100

//------------------------------------------------------------------------------------
// Theme
//------------------------------------------------------------------------------------

int isClick = 0;

#define COL_BG        (Color){ 244, 246, 250, 255 }
#define COL_CARD      (Color){ 255, 255, 255, 255 }
#define COL_BORDER    (Color){ 220, 224, 232, 255 }
#define COL_TEXT      (Color){ 33, 37, 41, 255 }
#define COL_MUTED     (Color){ 108, 117, 125, 255 }
#define COL_ACCENT    (Color){ 37, 99, 235, 255 }
#define COL_ACCENT_HV (Color){ 29, 78, 190, 255 }
#define COL_FOCUS_BG  (Color){ 235, 242, 255, 255 }
#define COL_OK        (Color){ 22, 163, 74, 255 }
#define COL_ERR       (Color){ 220, 38, 38, 255 }

//------------------------------------------------------------------------------------
// Declarations
//------------------------------------------------------------------------------------
int rayInit(void);
void gyroParser(char* coord, double* gx, double* gy, double* gz, double* accumX, double* accumY, int *isClick);
void DrawCustomSlider(Rectangle bounds, float *value, float minVal, float maxVal);
void DrawTextBox(Rectangle bounds, char *text, bool *isFocused, bool masked);
void DrawCard(Rectangle r, const char *title);
void UIText(const char *text, float x, float y, float size, Color color);
void AppendChar(char *buf, int maxLen, int key);

//------------------------------------------------------------------------------------
// Global Variables
//------------------------------------------------------------------------------------
Font uiFont;

double gx, gy, gz;
double accumX = 0.0;
double accumY = 0.0;

float sensitivityFactor = 0.1f;
bool isDraggingSlider = false;

char wifiSSID[64] = "";
char wifiPass[64] = "";
char comPort[16] = "COM3";
char hostIP[32] = "";

int isClick = 0;
int prevClick = -1;
int idleClick = -1;          // button level when NOT pressed (learned from first packet)
char lastRaw[64] = "";       // last raw UDP packet, for debugging
unsigned long clickCount = 0;
bool ssidFocused = false;
bool passFocused = false;
bool comFocused = false;
bool ipFocused = false;
char portList[96] = "";

unsigned long packetCount = 0;
double lastPacketTime = -1.0;

char statusMessage[128] = "Ready to send credentials";
Color statusColor = COL_MUTED;

bool sendPending = false;

// Turns the board's printed status lines into one short message for the status bar
static void ShowSendResult(void) {
    if (!SerialLastOk()) {
        unsigned long err = SerialLastError();
        const char *why =
            (err == 2)  ? "port not found - check COM number" :
            (err == 5)  ? "port busy - close Serial Monitor/other app" :
            (err == 31) ? "device not working - check driver/cable" :
            (err == 121)? "timeout - check cable/board" :
                          "see error code";
        snprintf(statusMessage, sizeof(statusMessage), "%s failed (err %lu): %s", comPort, err, why);
        statusColor = COL_ERR;
        return;
    }

    const char *r = SerialLastReply();
    if (strstr(r, "ERROR")) {
        snprintf(statusMessage, sizeof(statusMessage), "Board rejected the format (avoid ; in name/password)");
        statusColor = COL_ERR;
    } else if (strstr(r, "Connected!")) {
        char ip[20] = "";
        const char *p = strstr(r, "Device IP:");
        if (p) {
            p += 10;
            while (*p == ' ') p++;
            int i = 0;
            while ((isdigit((unsigned char)*p) || *p == '.') && i < 15) ip[i++] = *p++;
            ip[i] = '\0';
        }
        snprintf(statusMessage, sizeof(statusMessage), "Wi-Fi OK%s%s - sending UDP",
                 ip[0] ? ", board IP " : "", ip);
        statusColor = COL_OK;
    } else if (strstr(r, "failed")) {
        snprintf(statusMessage, sizeof(statusMessage), "Wi-Fi failed - check name/password (2.4 GHz only)");
        statusColor = COL_ERR;
    } else if (strstr(r, "saved")) {
        snprintf(statusMessage, sizeof(statusMessage), "Saved on board - watch Packets for data");
        statusColor = COL_MUTED;
    } else if (r[0]) {
        snprintf(statusMessage, sizeof(statusMessage), "Board says: %.40s", r);
        statusColor = COL_MUTED;
    } else {
        snprintf(statusMessage, sizeof(statusMessage), "No reply from %s (read err %lu) - wrong COM port?",
                 comPort, SerialLastReadError());
        statusColor = COL_ERR;
    }
}

int main(void)
{
    NetSocket sock;
    if (!net_init(&sock, 8080)) {
        printf("Network setup failed\n");
        return 1;
    }
    rayInit();

    // Auto-detect this PC's IP (no manual input needed)
    if (!GetLocalIP(hostIP, sizeof(hostIP))) hostIP[0] = '\0';

    // Auto-detect the board's COM port
    FindComPort(comPort, sizeof(comPort));
    ListComPorts(portList, sizeof(portList));

    float backspaceTimer = 0.0f;

    while (!WindowShouldClose())
    {
        // 1. UDP -> Gyro Mouse
        char buffer[256];
        int n;
        while ((n = net_receive(sock, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[n] = '\0';
            packetCount++;
            lastPacketTime = GetTime();
            snprintf(lastRaw, sizeof(lastRaw), "%s", buffer);   // keep a copy (gyroParser's strtok modifies buffer)
            gyroParser(buffer, &gx, & gy, &gz, &accumX, &accumY, &isClick);
            int stepX = (int)accumX;
            int stepY = (int)accumY;
            if (stepX != 0 || stepY != 0) {
                mouse_move_relative(stepX, stepY);
                accumX -= stepX;
                accumY -= stepY;
            }
            // Click once per press. The first packet defines the idle (not pressed) level,
            // so this works for both active-low and active-high buttons.
            if (idleClick < 0) idleClick = isClick;
            if (prevClick == idleClick && isClick != idleClick) {
                mouse_left_click();
                clickCount++;
            }
            prevClick = isClick;
        }

        // 2. Typing
        int key = GetCharPressed();
        while (key > 0) {
            if ((key >= 32) && (key <= 126)) {
                if (ssidFocused)      AppendChar(wifiSSID, 63, key);
                else if (passFocused) AppendChar(wifiPass, 63, key);
                else if (comFocused)  AppendChar(comPort, 15, key);
                else if (ipFocused)   AppendChar(hostIP, 31, key);
            }
            key = GetCharPressed();
        }

        // Backspace (hold to delete repeatedly)
        bool doDelete = false;
        if (IsKeyPressed(KEY_BACKSPACE)) {
            doDelete = true;
            backspaceTimer = 0.4f;
        } else if (IsKeyDown(KEY_BACKSPACE)) {
            backspaceTimer -= GetFrameTime();
            if (backspaceTimer <= 0.0f) {
                doDelete = true;
                backspaceTimer = 0.04f;
            }
        }

        if (doDelete) {
            if (ssidFocused && strlen(wifiSSID) > 0) wifiSSID[strlen(wifiSSID) - 1] = '\0';
            if (passFocused && strlen(wifiPass) > 0) wifiPass[strlen(wifiPass) - 1] = '\0';
            if (comFocused  && strlen(comPort)  > 0) comPort[strlen(comPort) - 1] = '\0';
            if (ipFocused   && strlen(hostIP)   > 0) hostIP[strlen(hostIP) - 1] = '\0';
        }

        // 3. Render
        BeginDrawing();
            ClearBackground(COL_BG);

            // Header
            UIText("Air Mouse", 20, 12, 28, COL_TEXT);
            UIText("Gyro mouse control & Wi-Fi setup", 150, 20, 16, COL_MUTED);

            // ---- Card 1: Gyro ----
            DrawCard((Rectangle){ 20, 60, 760, 130 }, "Gyro Mouse");

            UIText(TextFormat("GX: %7.2f      GY: %7.2f      GZ: %7.2f", gx, gy, gz), 40, 96, 18, COL_TEXT);
            UIText(TextFormat("Accum X: %6.2f   Accum Y: %6.2f   Btn: %d   Clicks: %lu", accumX, accumY, isClick, clickCount), 40, 120, 16, COL_MUTED);

            UIText(TextFormat("Raw: %s", lastRaw[0] ? lastRaw : "(none)"), 40, 172, 14, COL_MUTED);
            UIText("Sensitivity", 40, 152, 18, COL_TEXT);
            DrawCustomSlider((Rectangle){ 160, 160, 300, 8 }, &sensitivityFactor, 0.01f, 1.0f);
            UIText(TextFormat("%.3f", sensitivityFactor), 490, 152, 18, COL_ACCENT);

            // Packet status: tells you whether UDP data is reaching the PC
            double since = (lastPacketTime < 0) ? -1.0 : GetTime() - lastPacketTime;
            bool receiving = (since >= 0 && since < 2.0);
            UIText(TextFormat("Packets: %lu", packetCount), 560, 96, 18, COL_TEXT);
            UIText(receiving ? "Receiving data" : (packetCount ? "No data (stopped)" : "No data yet"),
                   560, 120, 16, receiving ? COL_OK : COL_ERR);

            Rectangle fwBtn = { 560, 146, 200, 32 };
            bool fwHover = CheckCollisionPointRec(GetMousePosition(), fwBtn);
            DrawRectangleRounded(fwBtn, 0.25f, 8, fwHover ? COL_ACCENT_HV : COL_ACCENT);
            UIText("Open Firewall (UDP 8080)", fwBtn.x + 12, fwBtn.y + 6, 16, WHITE);
            if (fwHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (open_firewall_udp(8080)) {
                    snprintf(statusMessage, sizeof(statusMessage), "Firewall request sent - accept the Windows prompt");
                    statusColor = COL_OK;
                } else {
                    snprintf(statusMessage, sizeof(statusMessage), "Could not open firewall prompt");
                    statusColor = COL_ERR;
                }
            }

            // ---- Card 2: Wi-Fi ----
            DrawCard((Rectangle){ 20, 206, 760, 270 }, "Send Wi-Fi Credentials via USB");

            UIText("COM Port",     40, 249, 18, COL_TEXT);
            UIText("Wi-Fi Name",   40, 294, 18, COL_TEXT);
            UIText("Password",     40, 339, 18, COL_TEXT);
            UIText("PC IP", 40, 384, 18, COL_TEXT);

            DrawTextBox((Rectangle){ 160, 242, 320, 34 }, comPort,  &comFocused,  false);
            UIText(TextFormat("Ports: %s", portList[0] ? portList : "none"), 570, 251, 15, COL_MUTED);

            // Auto-detect button
            Rectangle autoBtn = { 490, 242, 70, 34 };
            bool autoHover = CheckCollisionPointRec(GetMousePosition(), autoBtn);
            DrawRectangleRounded(autoBtn, 0.25f, 8, autoHover ? COL_ACCENT_HV : COL_ACCENT);
            UIText("Auto", autoBtn.x + 16, autoBtn.y + 7, 18, WHITE);
            if (autoHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                ListComPorts(portList, sizeof(portList));
                if (FindComPort(comPort, sizeof(comPort))) {
                    snprintf(statusMessage, sizeof(statusMessage), "Found %s", comPort);
                    statusColor = COL_OK;
                } else {
                    snprintf(statusMessage, sizeof(statusMessage), "No COM port found - plug in the board");
                    statusColor = COL_ERR;
                }
            }
            DrawTextBox((Rectangle){ 160, 287, 320, 34 }, wifiSSID, &ssidFocused, false);
            DrawTextBox((Rectangle){ 160, 332, 320, 34 }, wifiPass, &passFocused, true);

            // PC IP: auto-detected, but editable (VPN / virtual adapters can fool auto-detect)
            DrawTextBox((Rectangle){ 160, 377, 320, 34 }, hostIP, &ipFocused, false);

            Rectangle ipBtn = { 490, 377, 70, 34 };
            bool ipHover = CheckCollisionPointRec(GetMousePosition(), ipBtn);
            DrawRectangleRounded(ipBtn, 0.25f, 8, ipHover ? COL_ACCENT_HV : COL_ACCENT);
            UIText("Auto", ipBtn.x + 16, ipBtn.y + 7, 18, WHITE);
            if (ipHover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (GetLocalIP(hostIP, sizeof(hostIP))) {
                    snprintf(statusMessage, sizeof(statusMessage), "PC IP: %s (check it matches ipconfig Wi-Fi)", hostIP);
                    statusColor = COL_OK;
                } else {
                    hostIP[0] = '\0';
                    snprintf(statusMessage, sizeof(statusMessage), "IP not found - type it manually");
                    statusColor = COL_ERR;
                }
            }

            Rectangle sendBtn = { 160, 425, 150, 36 };
            bool isHovered = CheckCollisionPointRec(GetMousePosition(), sendBtn);
            DrawRectangleRounded(sendBtn, 0.25f, 8, isHovered ? COL_ACCENT_HV : COL_ACCENT);
            Vector2 tsz = MeasureTextEx(uiFont, "Send to USB", 18, 0.5f);
            UIText("Send to USB", sendBtn.x + (sendBtn.width - tsz.x) / 2, sendBtn.y + (sendBtn.height - tsz.y) / 2, 18, WHITE);

            if (isHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !SerialBusy()) {
                // Only auto-fill the IP if the box is empty, so a manual IP is never overwritten
                if (hostIP[0] == '\0') GetLocalIP(hostIP, sizeof(hostIP));

                if (wifiSSID[0] == '\0' || wifiPass[0] == '\0') {
                    snprintf(statusMessage, sizeof(statusMessage), "Enter Wi-Fi name AND password (firmware needs both)");
                    statusColor = COL_ERR;
                } else if (hostIP[0] == '\0') {
                    snprintf(statusMessage, sizeof(statusMessage), "PC IP is empty - type it manually");
                    statusColor = COL_ERR;
                } else {
                    char payload[200];
                    snprintf(payload, sizeof(payload), "%s;%s;%s\n", wifiSSID, wifiPass, hostIP);
                    if (SerialSendStart(comPort, payload)) {
                        sendPending = true;
                        snprintf(statusMessage, sizeof(statusMessage), "Sending to %s, waiting for board...", comPort);
                        statusColor = COL_MUTED;
                    } else {
                        snprintf(statusMessage, sizeof(statusMessage), "Could not start sending");
                        statusColor = COL_ERR;
                    }
                }
            }

            // Background send finished -> show the outcome
            if (sendPending && !SerialBusy()) {
                sendPending = false;
                ShowSendResult();
            }

            UIText(statusMessage, 330, 434, 17, statusColor);

        EndDrawing();
    }

    if (uiFont.texture.id != GetFontDefault().texture.id) UnloadFont(uiFont);
    CloseWindow();
    net_close(sock);
    return 0;
}

// ------------------------------------------------------------------------------------
// Helper Functions
// ------------------------------------------------------------------------------------

void AppendChar(char *buf, int maxLen, int key) {
    int len = (int)strlen(buf);
    if (len < maxLen) {
        buf[len] = (char)key;
        buf[len + 1] = '\0';
    }
}

void UIText(const char *text, float x, float y, float size, Color color) {
    DrawTextEx(uiFont, text, (Vector2){ x, y }, size, 0.5f, color);
}

void DrawCard(Rectangle r, const char *title) {
    DrawRectangleRounded(r, 0.04f, 8, COL_CARD);
    DrawRectangleRoundedLines(r, 0.04f, 8, 1.0f, COL_BORDER);
    UIText(title, r.x + 20, r.y + 14, 20, COL_ACCENT);
}

void DrawTextBox(Rectangle bounds, char *text, bool *isFocused, bool masked) {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        *isFocused = CheckCollisionPointRec(GetMousePosition(), bounds);
    }

    DrawRectangleRounded(bounds, 0.2f, 8, *isFocused ? COL_FOCUS_BG : COL_CARD);
    DrawRectangleRoundedLines(bounds, 0.2f, 8, *isFocused ? 2.0f : 1.0f, *isFocused ? COL_ACCENT : COL_BORDER);

    // Password is shown as *
    char shown[64];
    if (masked) {
        int len = (int)strlen(text);
        for (int i = 0; i < len; i++) shown[i] = '*';
        shown[len] = '\0';
    } else {
        strcpy(shown, text);
    }

    float fontSize = 18;
    Vector2 sz = MeasureTextEx(uiFont, shown, fontSize, 0.5f);
    float innerW = bounds.width - 20;
    float offset = (sz.x > innerW - 6) ? (sz.x - (innerW - 6)) : 0;   // scroll to show end of text

    BeginScissorMode((int)bounds.x + 4, (int)bounds.y, (int)bounds.width - 8, (int)bounds.height);
        UIText(shown, bounds.x + 10 - offset, bounds.y + (bounds.height - sz.y) / 2, fontSize, COL_TEXT);

        // Blinking cursor
        if (*isFocused && ((int)(GetTime() * 2) % 2 == 0)) {
            float cx = bounds.x + 10 - offset + sz.x + 2;
            DrawRectangle((int)cx, (int)bounds.y + 7, 2, (int)bounds.height - 14, COL_ACCENT);
        }
    EndScissorMode();
}

int rayInit(void) {
    InitWindow(800, 500, "Air Mouse");
    SetTargetFPS(60);

    // Load Segoe UI, fall back to raylib default font
    const char *fontPath = "C:/Windows/Fonts/segoeui.ttf";
    if (FileExists(fontPath)) {
        uiFont = LoadFontEx(fontPath, 48, NULL, 0);
        SetTextureFilter(uiFont.texture, TEXTURE_FILTER_BILINEAR);
    } else {
        uiFont = GetFontDefault();
    }
    return 1;
}

void DrawCustomSlider(Rectangle bounds, float *value, float minVal, float maxVal) {
    Vector2 mousePos = GetMousePosition();
    Rectangle clickBounds = { bounds.x - 12, bounds.y - 12, bounds.width + 24, bounds.height + 24 };

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mousePos, clickBounds)) isDraggingSlider = true;
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) isDraggingSlider = false;

    if (isDraggingSlider) {
        float normalized = (mousePos.x - bounds.x) / bounds.width;
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;
        *value = minVal + normalized * (maxVal - minVal);
    }

    float progress = (*value - minVal) / (maxVal - minVal);
    float handleX = bounds.x + progress * bounds.width;

    DrawRectangleRounded(bounds, 1.0f, 8, COL_BORDER);
    DrawRectangleRounded((Rectangle){ bounds.x, bounds.y, progress * bounds.width, bounds.height }, 1.0f, 8, COL_ACCENT);
    DrawCircle((int)handleX, (int)(bounds.y + bounds.height / 2), 10, WHITE);
    DrawCircleLines((int)handleX, (int)(bounds.y + bounds.height / 2), 10, isDraggingSlider ? COL_ACCENT_HV : COL_ACCENT);
}

void gyroParser(char* coord, double* gx, double* gy, double* gz, double *accumX, double* accumY, int* isClick){
    char* coordPtr = strtok(coord, ",");
    double calibrated_gx = 0, calibrated_gy = 0, calibrated_gz = 0, isClicked = 1;

    for (int i = 0; i < 4; i++){
        if (coordPtr == NULL) break;
        if (i == 0) calibrated_gx = atof(coordPtr);
        else if (i == 1) calibrated_gy = atof(coordPtr);
        else if (i == 2) calibrated_gz = atof(coordPtr);
        else isClicked = atof(coordPtr);
        coordPtr = strtok(NULL, ",");
    }
     *isClick = (isClicked >= 0.5) ? 1 : 0;

    if (fabs(calibrated_gx) <= NoiseTheshold) *gx = 0;
    else if (calibrated_gx > 0) *gx = calibrated_gx - NoiseTheshold;
    else *gx = calibrated_gx + NoiseTheshold;

    if (fabs(calibrated_gy) <= NoiseTheshold) *gy = 0;
    else if (calibrated_gy > 0) *gy = calibrated_gy - NoiseTheshold;
    else *gy = calibrated_gy + NoiseTheshold;

    if (fabs(calibrated_gz) <= NoiseTheshold) *gz = 0;
    else if (calibrated_gz > 0) *gz = calibrated_gz - NoiseTheshold;
    else *gz = calibrated_gz + NoiseTheshold;

    double deg_sec_x = *gx / 131.0;
    double deg_sec_z = *gz / 131.0;

    double pDelta_x = -(deg_sec_x * (double)sensitivityFactor);
    double pDelta_y = -(deg_sec_z * (double)sensitivityFactor);

    *accumX += pDelta_y;
    *accumY += pDelta_x;
}