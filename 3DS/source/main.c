#include <3ds.h>
#include <3ds/services/ac.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PC_IP   "0.0.0.0"   // YOUR IPv4 (PC / coming soon Wii U)
#define PC_PORT 4242

#define SOC_BUFFERSIZE 0x100000   // 1 MB

// Struct to send inputs
typedef struct {
    u32 magic;     // sanity check: '3DSU'
    u32 frame;     // contador de frames
    u32 keys;      // hidKeysHeld()
    s8  lx, ly;    // Circle Pad
    s8  rx, ry;    // reservado
} __attribute__((packed)) InputState;

int main(int argc, char **argv)
{
    // Initialize graphics and console
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    printf("\x1b[2;2H3DS Input Debug - Press START to exit");

    // --------------------------------
    // Initialize AC (WiFi)
    // --------------------------------
    Result ret = acInit();
    if (R_FAILED(ret)) {
        printf("\x1b[4;2HAC init failed: %08lX", ret);
        while (aptMainLoop()) {
            gspWaitForVBlank();
        }
    }
    printf("\x1b[4;2HAC initialized OK");

    // --------------------------------
    // Initialize SOC
    // --------------------------------
    void *soc_buffer = memalign(0x1000, SOC_BUFFERSIZE);
    if (!soc_buffer) {
        printf("\x1b[5;2HFailed to allocate SOC buffer");
        while (aptMainLoop()) {
            gspWaitForVBlank();
        }
    }

    ret = socInit(soc_buffer, SOC_BUFFERSIZE);
    if (R_FAILED(ret)) {
        printf("\x1b[5;2HSOC init failed: %08lX", ret);
        while (aptMainLoop()) {
            gspWaitForVBlank();
        }
    }
    printf("\x1b[5;2HSOC initialized OK");

    // --------------------------------
    // Crear socket UDP
    // --------------------------------
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("\x1b[6;2HError creating socket");
        while (aptMainLoop()) {
            gspWaitForVBlank();
        }
    }
    printf("\x1b[6;2HUDP socket created");

    struct sockaddr_in pc_addr;
    memset(&pc_addr, 0, sizeof(pc_addr));
    pc_addr.sin_family = AF_INET;
    pc_addr.sin_port   = htons(PC_PORT);
    inet_pton(AF_INET, PC_IP, &pc_addr.sin_addr);

    InputState st;
    u32 frameCounter = 0;

    // --------------------------------
    // Main loop
    // --------------------------------
    while (aptMainLoop()) {

        hidScanInput();

        // Exit with START
        if (hidKeysDown() & KEY_START)
            break;

        u32 keys = hidKeysHeld();

        // Circle Pad
        circlePosition cp;
        hidCircleRead(&cp);

        // Deadzone + escale
        s8 lx = (abs(cp.dx) < 5) ? 0 : (cp.dx / 2);
        s8 ly = (abs(cp.dy) < 5) ? 0 : (cp.dy / 2);

        // Build package
        st.magic = 0x33445355; // '3DSU'
        st.frame = frameCounter++;
        st.keys  = keys;
        st.lx    = lx;
        st.ly    = ly;
        st.rx    = 0;
        st.ry    = 0;

        // Debug in screen
        printf(
            "\x1b[8;2HFrame %lu | Keys:%08lX | LX:%+4d LY:%+4d   ",
            st.frame, st.keys, st.lx, st.ly
        );

        // Send UDP
        sendto(
            sock,
            &st,
            sizeof(st),
            0,
            (struct sockaddr*)&pc_addr,
            sizeof(pc_addr)
        );

        // VBlank
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    // --------------------------------
    // Cleaning
    // --------------------------------
    closesocket(sock);
    socExit();
    free(soc_buffer);
    acExit();

    gfxExit();
    return 0;
}

