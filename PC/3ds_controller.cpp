#define WIN32_LEAN_AND_MEAN

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <ViGEm/Client.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ViGEmClient.lib")

#define PORT 4242
#define MAGIC_3DSU 0x33445355  // '3DSU'

// ---- 3DS key masks ----
#define KEY_A       (1 << 0)
#define KEY_B       (1 << 1)
#define KEY_SELECT  (1 << 2)
#define KEY_START   (1 << 3)
#define KEY_DRIGHT  (1 << 4)
#define KEY_DLEFT   (1 << 5)
#define KEY_DUP     (1 << 6)
#define KEY_DDOWN   (1 << 7)
#define KEY_R       (1 << 8)
#define KEY_L       (1 << 9)
#define KEY_X       (1 << 10)
#define KEY_Y       (1 << 11)
#define KEY_ZL      (1 << 14)
#define KEY_ZR      (1 << 15)

// ---- Paquete UDP ----
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint32_t frame;
    uint32_t keys;
    int8_t   lx, ly;
    int8_t   rx, ry;
} InputState;
#pragma pack(pop)

static int16_t scaleStick(int8_t v)
{
    return (int16_t)((float)v / 127.0f * 32767.0f);
}

int main(void)
{
    // ===============================
    // WINSOCK
    // ===============================
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("Bind failed\n");
        return 1;
    }

    printf("Listening on UDP %d...\n", PORT);

    // ===============================
    // ViGEm INIT
    // ===============================
    PVIGEM_CLIENT client = vigem_alloc();
    if (!client) {
        printf("vigem_alloc failed\n");
        return 1;
    }

    if (vigem_connect(client) != VIGEM_ERROR_NONE) {
        printf("vigem_connect failed\n");
        return 1;
    }

    PVIGEM_TARGET pad = vigem_target_x360_alloc();

    VIGEM_ERROR err = vigem_target_add(client, pad);
    if (err != VIGEM_ERROR_NONE) {
        printf("vigem_target_add failed: %d\n", err);
        return 1;
    }

    printf("Xbox 360 virtual controller connected\n");

    // ===============================
    // MAIN LOOP
    // ===============================
    while (1) {
        InputState st;
        struct sockaddr_in from;
        int fromlen = sizeof(from);

        int ret = recvfrom(
            sock,
            (char*)&st,
            sizeof(st),
            0,
            (struct sockaddr*)&from,
            &fromlen
        );

        if (ret <= 0)
            continue;

        if (st.magic != MAGIC_3DSU)
            continue;

        // Debug mínimo
        printf("Frame %u | Keys %08X | LX %d LY %d\r",
               st.frame, st.keys, st.lx, st.ly);
        fflush(stdout);

        // -------------------------------
        // Construir reporte X360
        // -------------------------------
        XUSB_REPORT report;
        memset(&report, 0, sizeof(report));

        report.sThumbLX = scaleStick(st.lx);
        report.sThumbLY = scaleStick(st.ly);
        report.sThumbRX = scaleStick(st.rx);
        report.sThumbRY = scaleStick(st.ry);

        if (st.keys & KEY_A) report.wButtons |= XUSB_GAMEPAD_A;
        if (st.keys & KEY_B) report.wButtons |= XUSB_GAMEPAD_B;
        if (st.keys & KEY_X) report.wButtons |= XUSB_GAMEPAD_X;
        if (st.keys & KEY_Y) report.wButtons |= XUSB_GAMEPAD_Y;

        if (st.keys & KEY_L) report.wButtons |= XUSB_GAMEPAD_LEFT_SHOULDER;
        if (st.keys & KEY_R) report.wButtons |= XUSB_GAMEPAD_RIGHT_SHOULDER;

        report.bLeftTrigger  = (st.keys & KEY_ZL) ? 255 : 0;
        report.bRightTrigger = (st.keys & KEY_ZR) ? 255 : 0;

        if (st.keys & KEY_DUP)    report.wButtons |= XUSB_GAMEPAD_DPAD_UP;
        if (st.keys & KEY_DDOWN)  report.wButtons |= XUSB_GAMEPAD_DPAD_DOWN;
        if (st.keys & KEY_DLEFT)  report.wButtons |= XUSB_GAMEPAD_DPAD_LEFT;
        if (st.keys & KEY_DRIGHT) report.wButtons |= XUSB_GAMEPAD_DPAD_RIGHT;

        if (st.keys & KEY_START)  report.wButtons |= XUSB_GAMEPAD_START;
        if (st.keys & KEY_SELECT) report.wButtons |= XUSB_GAMEPAD_BACK;

        vigem_target_x360_update(client, pad, report);
    }
}

