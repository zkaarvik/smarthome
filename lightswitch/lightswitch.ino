#include "FS.h"
#include "ESP8266WiFi.h"
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>

#define SSID			"Why-Phi"
#define PASSWORD		"Pizz4Pizz4"

#define HUE_IP			"192.168.0.107"
#define API_KEY			"6IzGK0uoAXNRNTAces76VHl2zzM6ojNpq6OK3NWA"

TFT_eSPI tft = TFT_eSPI();

#define DEBUG			0

#define CALIBRATION_FILE	"/TouchCalData3"
#define REPEAT_CAL		false

#define NUM_ROOMS		6
#define ROOM_ID_MASTER		0
#define ROOM_ID_LIVING_ROOM	1
#define ROOM_ID_BEDROOM		2
#define ROOM_ID_DESK		3
#define ROOM_ID_DEN		5
#define ROOM_ID_HALLWAY		6

#define BUTTON_WIDTH		140
#define BUTTON_HEIGHT		70

#define TEXT_MARGIN		20

#define RED_WIDTH		BUTTON_WIDTH / 2
#define RED_HEIGHT		BUTTON_HEIGHT - TEXT_MARGIN
#define GREEN_WIDTH		BUTTON_WIDTH / 2
#define GREEN_HEIGHT		BUTTON_HEIGHT - TEXT_MARGIN

#define COLUMN			160
#define MARGIN_X		10
#define MARGIN_Y		7

#define POLL_INTERVAL		1000	// milliseconds
HTTPClient poll_http;
int poll_ms_cur;
int poll_ms_prev;

struct button {
	char name[64];
	boolean on;
	int x;
	int y;
	int r_x;
	int r_y;
	int g_x;
	int g_y;
	int hue_id;
};

struct button button_list[NUM_ROOMS];

// Comment out to stop drawing black spots
#define BLACK_SPOT

void init_button(int id, int x, int y, int hue_id, char *name)
{
	sprintf(button_list[id].name, name);
	button_list[id].on = false;
	button_list[id].x = x;
	button_list[id].y = y;
	button_list[id].r_x = x;
	button_list[id].r_y = y + TEXT_MARGIN;
	button_list[id].g_x = x + BUTTON_WIDTH / 2;
	button_list[id].g_y = y + TEXT_MARGIN;
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

	init_button(0, MARGIN_X, 1 * MARGIN_Y, ROOM_ID_MASTER, "Master");
	init_button(1, MARGIN_X, 2 * MARGIN_Y + 1 * BUTTON_HEIGHT, ROOM_ID_BEDROOM, "Bedroom");
	init_button(2, MARGIN_X, 3 * MARGIN_Y + 2 * BUTTON_HEIGHT, ROOM_ID_DESK, "Desk");
	init_button(3, COLUMN, 1 * MARGIN_Y, ROOM_ID_HALLWAY, "Hallway");
	init_button(4, COLUMN, 2 * MARGIN_Y + 1 * BUTTON_HEIGHT, ROOM_ID_LIVING_ROOM, "Living Room");
	init_button(5, COLUMN, 3 * MARGIN_Y + 2 * BUTTON_HEIGHT, ROOM_ID_DEN, "Den");

	draw_buttons();
}

void poll_state()
{
	int i;
	int diff;
	int httpCode;
	char req[2048];
	bool room_on;
	bool master_on;

	const size_t capacity = JSON_ARRAY_SIZE(0) + JSON_ARRAY_SIZE(1) +
		JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) +
		JSON_OBJECT_SIZE(8) + 150;

	poll_ms_cur = millis();
	diff = poll_ms_cur - poll_ms_prev;

	if (diff < POLL_INTERVAL)
		return;

	DynamicJsonBuffer jsonBuffer(capacity);
	poll_ms_prev = poll_ms_cur;

	// Turn on master switch if any rooms have lights on
	master_on = false;
	for (i = 1; i <= NUM_ROOMS - 1; i++) {
		sprintf(req, "http://%s/api/%s/groups/%d/",
			HUE_IP, API_KEY, button_list[i].hue_id);

		if (poll_http.begin(req)) {
			httpCode = poll_http.GET();

			if (httpCode > 0) {
				if (httpCode == HTTP_CODE_OK ||
				    httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
					JsonObject& root = jsonBuffer.parseObject(poll_http.getString());
					room_on = root["state"]["any_on"];

					if (room_on)
						master_on = true;

#if DEBUG
					Serial.printf("%s: %d\n", button_list[i].name, room_on);
#endif
					if (button_list[i].on != room_on) {
						if (room_on)
							draw_button_green(i);
						else
							draw_button_red(i);
					}

				}
			} else {
#if DEBUG
				Serial.println("HTTP request failed");
#endif
			}
			poll_http.end();
		}
	}

	if (master_on)
		draw_button_green(0);
	else
		draw_button_red(0);
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
		if ((x > btn->x && x < (btn->x + BUTTON_WIDTH))&&
		    (y > btn->y && y < (btn->y + BUTTON_HEIGHT))) {
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
	poll_state();
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
		BUTTON_WIDTH,
		BUTTON_HEIGHT,
		TFT_BLACK);
	tft.drawRect(button_list[id].x,
		button_list[id].y,
		BUTTON_WIDTH,
		TEXT_MARGIN,
		TFT_BLACK);
	tft.setTextColor(TFT_WHITE);
	tft.setTextSize(2);
	tft.setTextDatum(MC_DATUM);
	tft.drawString(button_list[id].name,
		button_list[id].x + BUTTON_WIDTH / 2,
		button_list[id].y + 10);
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

	if (http.begin(client, req)) {
#if DEBUG
		Serial.printf("Turning %s %s\n", button_list[id].name, state ? "on" : "off");
#endif

		int httpCode = http.sendRequest("PUT", data);

		if (httpCode > 0) {
			if (httpCode == HTTP_CODE_OK ||
			    httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
				Serial.println(http.getString());
			}
		} else {
#if DEBUG
			Serial.println("HTTP request failed");
#endif
		}
		http.end();
	}
}

void draw_button_red(int id)
{
	struct button *btn;

	btn = &button_list[id];

	tft.fillRect(btn->r_x, btn->r_y, RED_WIDTH, RED_HEIGHT, TFT_RED);
	tft.fillRect(btn->g_x, btn->g_y, GREEN_WIDTH, GREEN_HEIGHT, TFT_DARKGREY);
	draw_frame(id);
	btn->on = false;
}

void draw_button_green(int id)
{
	struct button *btn;

	btn = &button_list[id];

	tft.fillRect(btn->r_x, btn->r_y, RED_WIDTH, RED_HEIGHT, TFT_DARKGREY);
	tft.fillRect(btn->g_x, btn->g_y, GREEN_WIDTH, GREEN_HEIGHT, TFT_GREEN);
	draw_frame(id);
	btn->on = true;
}
