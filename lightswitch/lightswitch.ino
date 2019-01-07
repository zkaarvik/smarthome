#include "FS.h"
#include "ESP8266WiFi.h"
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#define SSID			"Why-Phi"
#define PASSWORD		"Pizz4Pizz4"

#define HUE_IP			"192.168.0.107"
#define API_KEY			"6IzGK0uoAXNRNTAces76VHl2zzM6ojNpq6OK3NWA"

TFT_eSPI tft = TFT_eSPI();

#define CALIBRATION_FILE	"/TouchCalData3"
#define REPEAT_CAL		false

#define NUM_ROOMS		5
#define ROOM_ID_LIVING_ROOM	1
#define ROOM_ID_BEDROOM		2
#define ROOM_ID_DESK		3
#define ROOM_ID_DEN		5
#define ROOM_ID_HALLWAY		6


struct button {
	boolean on;
	int x;
	int y;
	int width;
	int height;
	int r_x;
	int r_y;
	int r_width;
	int r_height;
	int g_x;
	int g_y;
	int g_width;
	int g_height;
	int hue_id;
};

struct button button_list[NUM_ROOMS];

// Comment out to stop drawing black spots
#define BLACK_SPOT

void init_button(int id, int x, int y, int width, int height, int hue_id)
{
	button_list[id].on = false;
	button_list[id].x = x;
	button_list[id].y = y;
	button_list[id].width = width;
	button_list[id].height = height;
	button_list[id].r_x = x;
	button_list[id].r_y = y;
	button_list[id].r_width = width / 2;
	button_list[id].r_height = height;
	button_list[id].g_x = x + width / 2;
	button_list[id].g_y = y;
	button_list[id].g_width = width / 2;
	button_list[id].g_height = height;
	button_list[id].hue_id = hue_id;
}

void setup(void)
{
	Serial.begin(115200);

	WiFi.begin(SSID, PASSWORD);

	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print("*");
	}

	Serial.println("");
	Serial.println("WiFi connection Successful");
	Serial.print("ESP8266 IP: ");
	Serial.print(WiFi.localIP());

	tft.init();
	tft.setRotation(1);

	uint16_t calData[5] = {267, 3375, 475, 3141, 1};
	tft.setTouch(calData);

	tft.fillScreen(TFT_BLACK);

	init_button(0, 50, 30, 150, 30, ROOM_ID_BEDROOM);
	init_button(1, 50, 70, 150, 30, ROOM_ID_DESK);
	init_button(2, 50, 110, 150, 30, ROOM_ID_HALLWAY);
	init_button(3, 50, 150, 150, 30, ROOM_ID_LIVING_ROOM);
	init_button(4, 50, 190, 150, 30, ROOM_ID_DEN);

	Serial.println("Initializing buttons");

	draw_buttons();
	Serial.println("Drew buttons");
}

void draw_buttons(void)
{
	int i;
	struct button *btn;

	for (i = 0; i < NUM_ROOMS; i++) {
		btn = &button_list[i];

		if (btn->on)
			draw_button_green(i);
		else
			draw_button_red(i);
	}
}

/*
 * Check if the coordinates pressed correspond to the location of a button
 * @param x	x coordinate
 * @param y	y coordinate
 * @ret		-1: no buttons pressed, otherwise the ID of the button
 */
int button_pressed(int x, int y)
{
	int i;
	struct button *btn;

	for (i = 0; i < NUM_ROOMS; i++) {
		btn = &button_list[i];
		if ((x > btn->x && x < (btn->x + btn->width))&&
		    (y > btn->y && y < (btn->y + btn->height))) {
			return i;
		}
	}

	return -1;
}

void loop()
{
	uint16_t x, y;
	int id;

	if (tft.getTouch(&x, &y)) {
#ifdef BLACK_SPOT
		tft.fillCircle(x, y, 2, TFT_BLACK);
#endif

		id = button_pressed(x, y);
		if (id >= 0) {
			if (button_list[id].on) {
				toggle_light(id, 0);
				draw_button_red(id);
			} else {
				toggle_light(id, 1);
				draw_button_green(id);
			}
		}
	}
}

void touch_calibrate()
{
	uint16_t calData[5];
	uint8_t calDataOK = 0;

	// check file system exists
	if (!SPIFFS.begin()) {
		Serial.println("Formating file system");
		SPIFFS.format();
		SPIFFS.begin();
	}

	// check if calibration file exists and size is correct
	if (SPIFFS.exists(CALIBRATION_FILE)) {
		if (REPEAT_CAL) {
			// Delete if we want to re-calibrate
			SPIFFS.remove(CALIBRATION_FILE);
		} else {
			File f = SPIFFS.open(CALIBRATION_FILE, "r");
			if (f) {
				if (f.readBytes((char *)calData, 14) == 14)
					calDataOK = 1;
				f.close();
			}
		}
	}

	if (calDataOK && !REPEAT_CAL) {
		// calibration data valid
		tft.setTouch(calData);
	} else {
		// data not valid so recalibrate
		tft.fillScreen(TFT_BLACK);
		tft.setCursor(20, 0);
		tft.setTextFont(2);
		tft.setTextSize(1);
		tft.setTextColor(TFT_WHITE, TFT_BLACK);

		tft.println("Touch corners as indicated");

		tft.setTextFont(1);
		tft.println();

		if (REPEAT_CAL) {
			tft.setTextColor(TFT_RED, TFT_BLACK);
			tft.println("Set REPEAT_CAL to false to stop this running again!");
		}

		tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

		tft.setTextColor(TFT_GREEN, TFT_BLACK);
		tft.println("Calibration complete!");

		// store data
		File f = SPIFFS.open(CALIBRATION_FILE, "w");
		if (f) {
			f.write((const unsigned char *)calData, 14);
			f.close();
		}
	}
}

void draw_frame(int id)
{
	tft.drawRect(button_list[id].x,
		button_list[id].y,
		button_list[id].width,
		button_list[id].height,
		TFT_BLACK);
}


void toggle_light(int id, int state)
{
	char req[2048];
	char data[256];
	WiFiClient client;
	HTTPClient http;

	sprintf(req, "http://%s/api/%s/groups/%d/action",
		HUE_IP, API_KEY, button_list[id].hue_id);
	sprintf(data, "{\"on\":%s}", state ? "true" : "false");

	Serial.println("Starting HTTP");
	if (http.begin(client, req)) {
		Serial.println("Sending PUT request");

		int httpCode = http.sendRequest("PUT", data);

		if (httpCode > 0) {
			Serial.printf("Get code: %d\n", httpCode);

			if (httpCode == HTTP_CODE_OK ||
			    httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
				Serial.println(http.getString());
			}
		} else {
			Serial.println("HTTP request failed");
		}
		http.end();
	}
}

void draw_button_red(int id)
{
	struct button *btn;

	btn = &button_list[id];

	tft.fillRect(btn->r_x, btn->r_y, btn->r_width, btn->r_height, TFT_RED);
	tft.fillRect(btn->g_x, btn->g_y, btn->g_width, btn->g_height, TFT_DARKGREY);
	draw_frame(id);
	tft.setTextColor(TFT_WHITE);
	tft.setTextSize(2);
	tft.setTextDatum(MC_DATUM);
//	tft.drawString("On", B1_G_X + (B1_G_W / 2), B1_G_Y + (B1_G_H / 2));
	tft.drawString("On", btn->g_x, btn->g_y);
	btn->on = false;
}

void draw_button_green(int id)
{
	struct button *btn;

	btn = &button_list[id];

	tft.fillRect(btn->r_x, btn->r_y, btn->r_width, btn->r_height, TFT_DARKGREY);
	tft.fillRect(btn->g_x, btn->g_y, btn->g_width, btn->g_height, TFT_GREEN);
	draw_frame(id);
	tft.setTextColor(TFT_WHITE);
	tft.setTextSize(2);
	tft.setTextDatum(MC_DATUM);
//	tft.drawString("Off", B1_R_X + (B1_R_W / 2) + 1, B1_R_Y + (B1_R_H / 2));
	tft.drawString("Off", btn->r_x + btn->r_width / 2, btn->r_y + btn->r_height / 2);
	btn->on = true;
}
