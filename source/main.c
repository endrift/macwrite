#include <nds.h>
#include <stdio.h>

#define CRC_OFFSET 0x02A
#define MAC_OFFSET 0x036

#define CSI(X) "\033[" X
#define BRIGHT CSI("39m")
#define DIM CSI("38m")

struct WifiHeader {
	u8 header[CRC_OFFSET];
	u16 crc16;
	u16 length;
	u8 pad[8];
	u8 mac[6];
	u8 data[0x1C4];
} wifiHeader;
static_assert(sizeof(struct WifiHeader) == 0x200, "Invalid size");
static_assert(offsetof(struct WifiHeader, crc16) == CRC_OFFSET, "Invalid offset");
static_assert(offsetof(struct WifiHeader, mac) == MAC_OFFSET, "Invalid offset");

void run(void) {
	int nybble = 0;

	readFirmware(0, &wifiHeader, sizeof(struct WifiHeader));
	if (wifiHeader.length > sizeof(struct WifiHeader) - CRC_OFFSET - 2) {
		iprintf("Header size %d exceeds limit\n", wifiHeader.length);
		return;
	}
	if (wifiHeader.length == 0) {
		iprintf("Failed to read firmware.\nNot doing anything.\nPlease restart and try again.\n");
		return;
	}

	u8 mac[6];
	memcpy(mac, &wifiHeader.mac, sizeof(mac));

	iprintf("MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	u16 crc = swiCRC16(0, &wifiHeader.length, wifiHeader.length);

	if (crc != wifiHeader.crc16) {
		iprintf("Expected %04X, got %04X.\nExisting CRC16 doesn't match.\nNot doing anything.\nPlease restart and try again.\n", crc, wifiHeader.crc16);
		return;
	}

	iprintf("Press A to flash new MAC addressor START to exit instead.\n\n\n\n");
	while (1) {
		swiWaitForVBlank();

		iprintf(CSI("2K") CSI("2A") "\r");

		iprintf("  ");
		int spaces = nybble * 3 / 2;
		int i;
		for (i = 0; i < spaces; ++i) {
			iprintf(" ");
		}
		iprintf("^");
		for (i = 0; i < 18 - spaces; ++i) {
			iprintf(" ");
		}
		iprintf("\n");
		iprintf("< " DIM);
		for (i = 0; i < 12; ++i) {
			u8 value = wifiHeader.mac[i / 2];
			if (i & 1) {
				value &= 0xF;
			} else {
				value >>= 4;
			}
			if (i == nybble) {
				iprintf(BRIGHT);
			}
			iprintf("%X", value);
			if (i != 11 && (i & 1)) {
				iprintf(BRIGHT ":");
			}
			iprintf(DIM);
		}
		iprintf(BRIGHT " >\n  ");
		for (i = 0; i < spaces; ++i) {
			iprintf(" ");
		}
		iprintf("v");
		for (i = 0; i < 18 - spaces; ++i) {
			iprintf(" ");
		}

		scanKeys();
		int pressed = keysDown();
		if (pressed & KEY_START) {
			exit(0);
		}
		if (pressed & KEY_LEFT) {
			nybble = (nybble + 11) % 12;
		}
		if (pressed & KEY_RIGHT) {
			nybble = (nybble + 1) % 12;
		}
		if (pressed & KEY_UP) {
			u8 value = wifiHeader.mac[nybble / 2];
			if (nybble & 1) {
				value = (value & 0xF0) | ((value + 1) & 0xF);
			} else {
				value += 0x10;
			}
			wifiHeader.mac[nybble / 2] = value;
		}
		if (pressed & KEY_DOWN) {
			u8 value = wifiHeader.mac[nybble / 2];
			if (nybble & 1) {
				value = (value & 0xF0) | ((value - 1) & 0xF);
			} else {
				value -= 0x10;
			}
			wifiHeader.mac[nybble / 2] = value;
		}
		if (pressed & KEY_A) {
			break;
		}
	}
	printf("\n\n");

	if (memcmp(&wifiHeader.mac, mac, sizeof(mac)) == 0) {
		iprintf("MAC address left unchanged.\n");
		return;
	}

	wifiHeader.crc16 = swiCRC16(0, &wifiHeader.length, wifiHeader.length);

	int res = writeFirmware(0, &wifiHeader, 0x100);
	if (res < 0) {
		iprintf("Flashing failed!\n");
		return;
	}

	readFirmware(0, &wifiHeader, sizeof(struct WifiHeader));
	if (memcmp(&wifiHeader.mac, mac, sizeof(mac)) == 0) {
		iprintf("Flashing didn't take.\nIs SL1 bridged?\n");
		return;
	}
	iprintf("Flashing succeeded!\n");
}

int main(void) {
	consoleDemoInit();
	run();
	iprintf("Press START to exit\n");
	while (1) {
		swiWaitForVBlank();
		scanKeys();
		int pressed = keysDown();
		if (pressed & KEY_START) break;
	}

}
