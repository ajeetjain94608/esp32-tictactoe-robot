        // Pin assignments of ESP32-CAM for Robot's Camera PCB (Green PCB)
        //       
        // TFT Display Connections (Driver: ST7789V, Resolution: 320x240) :-
        // TFT_CS GPIO12
        // TFT_DIN GPIO15
        // TFT_CLK GPIO2
        // TFT_DC GPIO01 TXD0    After disabling TFT (using TFT_CS), acts as Soft Tx pin (Tx only no Rx)
        // PCA_OE GPIO03 RXD0 :  Red_Led and PCA9685 enable (Blue PCB) Active Low
        //
        // FLASH   GPIO4  [Solder Pad]
        //
        // To connect PCA9685 (Blue PCB) with I2C bus
        // I2C_SDA GPIO13
        // I2C-SCL GPIO14
        //
        // Connect a push button here to make OTA easy for Robot Software
        // If the button is pressed at the time or Reset (or in a few seconds),
        // the user can switch between 
        // - WiFI Browser mode
        //   and
        // - Normal robot software running mode
        

        // A count-down is displayed on TFT after reset is pressed on ESP32-CAM
        // During this time if the button is pressed, the mode can be switched
        // 
        // WiFI Browser Mode: Upload, Download, Delete or List file, Update Binary code OTA
        // Robot Software Mode: Run the TicTacToc Robot plaing software
        // I2C_SDA GPIO13
        // CSI_MCLK_XCLK GPIO0 [Solder Pad]
        // Connect a push button on the above two pins.
        // Note: 
        //       1) Since GPIO13 is connected on I2C_SDA, it should only be used before
        //          I2C bus is initialized
        //       2) In this project, GPIO0 is used for FOUR different thing at different times:
        //          GPIO0: LOW for flashing: When you press the small-button, near RST on ESP32-CAM, 
        //                 it connects to GND and new program can be flased using USB and Upload button on Arduino IDE
        //          GPIO0: must be HIGH for normal booting (a pull-up resistance is soldered on ESP32-CAM PCB at factory)
        //                 When you only press the RST button, ESP32-CAM boots normally and runs setup()
        //          GPIO0: It is also connected to another switch on camera PCB (Mode select: WiFi Browsing OR Robot Software)
        //                 After booting when setup() runs, a logic LOW is sent on GPIO13
        //                 If the switch is pressed, the WiFi Browsing mode is started
        //                 else the normal loop() for Robot Software is started
        //                 After this stage is passed (count-down on TFT) the mode cannot be switched, until next RESET
        //          GPIO0: Provides clock signal to the OV-camera: CSI_MCLK_XCLK,  
        //                 Should not be used elsewhere once the camera is initialized
        //       3) TRICK: The Mode Select Switch on Camera PCB has two terminals: 
        //                 One is connected to GPIO0 and another to GPIO13 (and not directly to GND)
        //                 GPIO13 can provide LOW only after booting is over and setup() runs digitalWrite()
        //                 Hence GPIO0 can get GND/no-GND (pull-up) only after booting is done.
        //                 Disclaimer: However if GPIO0 is not used for mode selection, the above trick is not required        
        //                             This could have been done in much simpler way by simply connecting
        //                             GPIO13 (or any other GPIO) to GND via mode selection switch. 
        //                             It will be optmised in future version.
        //                             All this was done with a plan to solder a switch on PCB easily.

#define CAMERA_MODEL_AI_THINKER // Has PSRAM

#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include <Adafruit_PWMServoDriver.h>
//#include <WebServer.h>
#include <ESP32WebServer.h>    // https://github.com/Pedroalbuquerque/ESP32WebServer download and place in your Libraries folder
#include <ESPmDNS.h>
#include <FS.h>
//#include <SD.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>       // Hardware-specific library
#include <Update.h>

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiMulti.h>
#include <Wire.h>
//#include "ESP_Process_Image_1.h"

//#include "file_services.c"

#include "camera_pins.h"
#include "web_styles.h"
#include "esp_camera.h"
#include "serial_utils.c"
// web_page.h removed - OTA page now uses SHARED_CSS with other pages

#define TEXT_SIZE   2 // Avoid changing. Try, NOT to change

// #define SERVOMIN   150 // This is the 'minimum' pulse length count (out of 4096)
// #define SERVOMAX   600 // This is the 'maximum' pulse length count (out of 4096)
#define USMIN      600 // This is the rounded 'minimum' microsecond length based on the minimum pulse of 150
#define USMAX     2400 // This is the rounded 'maximum' microsecond length based on the maximum pulse of 600
#define SERVO_FREQ  50 // Analog servos run at ~50 Hz updates

uint8_t servonum = 0;

#define SDA_PIN                    13  // Fixed for TL_RAN PCB
#define SCL_PIN                    14  // Fixed for TL_RAN PCB
#define PCA_ENABLE_PIN_NO           3  //  Fixed for TL_RAN PCB
#define LED_RED_INBUILD_PIN_NO     33  //  Fixed for ESP32-CAM
#define BUTTON_PIN                  0  // Push button (GPIO0 - shared with camera clock) 

#define PWM_CHANNEL_NO_LASER  6  // On PCA PCB
#define PWM_CHANNEL_NO_EM     7  // On PCA PCB
#define IMAGE_WIDTH 240
#define IMAGE_HEIGHT 240

// Servo speed for pickup/place movements (1=slowest, 10=fastest)
#define GAMMA_SPEED 1

// Number of consistent readings needed to confirm human move (prevents false detection)
#define MOVE_CONFIRM_COUNT 3
#define EMPTY ' '
#define X 'X'
#define O 'O'

TFT_eSPI tft = TFT_eSPI();  // Invoke custom library
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver(); // Default I2C address 0x40
//WebServer server(80);
ESP32WebServer  server(80);

const char* host = "esp32";

// WiFi Manager - configurable credentials
#define WIFI_CFG_PATH "/wifi.cfg"
#define AP_SSID "TicTacToe-Robot"
#define AP_PASSWORD "12345678"
#define WIFI_CONNECT_TIMEOUT 15000  // 15 seconds timeout for WiFi connection

char wifi_ssid[33] = "";      // Max SSID length is 32 chars + null
char wifi_password[65] = "";  // Max password length is 64 chars + null
bool apModeActive = false;

bool    SPIFFS_present = false;
//bool    SD_present = false; // DO not change, as no SD card in robot

int channel = 0;
int value = 0;
int position_uS;
long count = 0; // Rack piece counter (0-11)
int lcd_row = 0;  // Initialize to prevent undefined behavior
const int lcd_row_incrementer = 20;
char buffer_temp[30];
const int length=65;
const int dis=24;

// ============ COLOR DETECTION THRESHOLDS ============
#define COLOR_RED_MIN       120   // Minimum red value for red pawn detection
#define COLOR_DIFF_RED      30    // Red must exceed green/blue by this amount
#define COLOR_BLUE_MIN      100   // Minimum blue value for blue pawn detection
#define COLOR_DIFF_BLUE     20    // Blue must exceed red/green by this amount
#define COLOR_GREEN_MIN     100   // Minimum green value for empty cell detection
#define COLOR_DIFF_GREEN    20    // Green must exceed red/blue by this amount
#define DETECTION_THRESHOLD_DIV 7 // Divide total pixels by this for threshold (~14%)

// Maximum rack positions available
#define MAX_RACK_POSITIONS  12

int bestMove[2]={-1, -1};
int humanMoveRow = -1;
int humanMoveCol = -1;

int player=0;
bool humanStartsNext = true;  // Track who starts next game (human starts first game, so toggle to false for robot next)

// Internal game state (source of truth for game logic)
char board[5][5]={ {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                   {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                   {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                   {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                   {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY} };

// Previous state for change detection
char previousState[5][5]={ {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                           {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                           {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                           {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                           {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY} };

// Camera detected state (temporary, used for human move detection only)
char cameraBoard[5][5]={ {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                         {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                         {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                         {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY},
                         {EMPTY, EMPTY, EMPTY, EMPTY, EMPTY} };

// ============ CHEATING DETECTION ============
#define CHEAT_NONE           0
#define CHEAT_MULTI_MOVE     1  // Placed more than one piece
#define CHEAT_REMOVED_HUMAN  2  // Removed own piece (and maybe moved it)
#define CHEAT_MOVED_PIECE    3  // Moved piece to different location
#define CHEAT_REPLACED_ROBOT 4  // Replaced robot's piece with own

#define CHEAT_CONFIRM_COUNT  3  // Must detect cheating this many times before warning

int lastCheatType = CHEAT_NONE;
int cheatConfirmCount = 0;      // Confirmation counter for cheating
int lastCheatDetected = CHEAT_NONE;  // Last detected cheat type
int cheatMsgIndex = 0;

// Funny cheating messages
const char* cheatMessages[] = {
  "Nice try, cheater!",
  "I have cameras, duh!",
  "I'm not THAT dumb!",
  "Really? Cheating?",
  "Put it back, human!",
  "I SAW that!",
  "Sneaky sneaky...",
  "Not on my watch!",
  "Try again, fairly!",
  "Cheaters never win!",
  "I'm watching you!",
  "Fix it, NOW!",
};
#define NUM_CHEAT_MESSAGES 12

// ============ GAME UI SETTINGS ============
// Score tracking
int humanWins = 0;
int robotWins = 0;
int gameNumber = 1;

// UI Layout constants
#define SCORE_BAR_HEIGHT    35
#define BOARD_START_Y       40
#define BOARD_SIZE          180
#define CELL_SIZE           36    // BOARD_SIZE / 5
#define BOARD_START_X       30    // Center the board (240 - 180) / 2
#define STATUS_BAR_Y        225

// Colors for game UI
#define COLOR_HUMAN     TFT_RED
#define COLOR_ROBOT     TFT_BLUE
#define COLOR_GRID      TFT_WHITE
#define COLOR_HIGHLIGHT TFT_YELLOW
#define COLOR_BG        TFT_BLACK

// Last move tracking for highlight
int lastMoveRow = -1;
int lastMoveCol = -1;
bool isHumanLastMove = false;

// Animation state
int animDots = 0;
unsigned long lastAnimTime = 0;

// Rack positions remapped: 0=old11, 1=old10, 2=old5, 3=old9, 4=old4, 5=old8,
//                          6=old3, 7=old7, 8=old2, 9=old6, 10=old1, 11=old0
const int rack_freq[12][2] = {
    {1370, 840},   // 0 (was 11)
    {1640, 1150},  // 1 (was 10)
    {1530, 760},   // 2 (was 5)
    {1820, 1100},  // 3 (was 9)
    {1890, 1430},  // 4 (was 4)
    {2050, 1470},  // 5 (was 8)
    {1940, 1160},  // 6 (was 3)
    {2240, 1550},  // 7 (was 7)
    {2090, 1250},  // 8 (was 2)
    {2400, 1460},  // 9 (was 6)
    {2230, 1210},  // 10 (was 1)
    {2260, 1090}   // 11 (was 0)
};

const int board_alpha_freq[5][5] = {
    {1100, 1100, 1000, 870, 650},
    {1430, 1360, 1250, 1060, 880},
    {1650, 1570, 1400, 1190, 950},
    {1880, 1780, 1540, 1230, 950},
    {2100, 2050, 1700, 1170, 840}
};

const int board_beta_freq[5][5] = {
    {600, 800, 850, 810, 600},
    {1090, 1200, 1270, 1200, 1100},
    {1370, 1490, 1540, 1500, 1370},
    {1580, 1700, 1750, 1700, 1590},
    {1700, 1890, 1980, 1880, 1710}
};





// ROI calibration for 5x5 grid detection (hardcoded values)
int roi_start_x = 50;    // X position of first dot center (top-left)
int roi_start_y = 50;    // Y position of first dot center (top-left)
int roi_spacing_x = 35;  // Horizontal spacing between dot centers
int roi_spacing_y = 35;  // Vertical spacing between dot centers
int roi_size = 32;       // Size of detection box

// ROI array will be calculated at runtime
static int rois[25][4];

// Grid calibration mode flag
static bool gridCalibrationMode = false;

// Pose storage (persisted to SPIFFS) so you can calibrate from phone and keep values.
static const char* POSES_CFG_PATH = "/poses.cfg";

// Home position (microseconds) for the 3 PCA9685 servo channels (0..2)
// Defaults (will be overridden by SPIFFS poses if present).
int home_alpha = 2400;
int home_beta  = 1460;
int home_gamma = 600;

// Gamma heights
int gamma_hover = 1200;
int gamma_pickup = 710;
int gamma_place = 850;


// Camera/board viewing pose (microseconds).
// Defaults (will be overridden by SPIFFS poses if present).
int board_view_alpha = 1740;
int board_view_beta  = 1200;
int board_view_gamma = 1200;

static int current_servo_us[3] = {2400, 1500, 1200};

static bool cameraReady = false;
static bool pcaReady = false;
static bool pcaOutputsEnabled = false;
static bool calibrationActive = false;
static bool calibrationPreviewEnabled = true;
static bool emOn = false;
static bool laserOn = false;

// Current calibration cell/slot being edited
static int calCellRow = -1;
static int calCellCol = -1;
static int calRackSlot = -1;

static uint32_t lastPreviewMs = 0;
String  webpage = "";
char winner;

static bool baselineCaptured = false;

// Forward declarations
void process_image_direct(uint16_t* data, int width, int height);
char Winner(char b[5][5]);
int checkIfEmpty(char b[5][5]);
void recalculateROIs();
void resetGame();
bool checkGameOver();
void loadGridCalibration();
void saveGridCalibration();
bool findHumanMove();
int findHumanMoveWithCheatDetection();
int detectCheating();
void displayCheatWarning(int cheatType);
void displayCameraBoard();

// Game UI forward declarations
void drawPiece(int row, int col, char piece, bool highlight);
void drawGameGrid();
void drawGameBoard();
void drawScoreBar();
void drawStatusBar(const char* message, uint16_t color);
void drawAnimatedStatus(const char* message, uint16_t color);
void drawScanningIndicator();
void drawGameScreen();
void drawStartupScreen();
void drawResultScreen(char winner);
void drawHumanTurnScreen();
void drawRobotThinkingScreen();
void drawConfirmProgress(int count, int total);
static void move_servo_smooth(uint8_t ch, int target_us, int speed);

// WiFi Manager forward declarations
bool loadWiFiCredentials();
bool saveWiFiCredentials(const char* ssid, const char* pass);
void clearWiFiCredentials();
bool connectToWiFi(const char* ssid, const char* pass, int timeoutMs);
void startAPMode();
void WiFiSetupPage();
void ApiWiFiScan();
void ApiWiFiConnect();
void ApiWiFiStatus();
void ApiWiFiReset();

static void GridCalibrationPage();
static void ApiGridUpdate();
static void ApiGridSave();
static void ApiEM();
static void ApiLaser();
static void ApiHeight();
static void ApiGotoBoard();
static void ApiSaveBoard();
static void ApiGotoRack();
static void ApiSaveRack();
static void ApiSaveAll();
static void ApiExport();
static void setEM(bool on);
static void setLaser(bool on);

static void set_pca_outputs(bool enable)
{
  pcaOutputsEnabled = enable;
  digitalWrite(PCA_ENABLE_PIN_NO, enable ? LOW : HIGH);
}

static void move_servo_us(uint8_t ch, int microseconds)
{
  if (ch > 2) return;
  current_servo_us[ch] = constrain(microseconds, USMIN, USMAX);
  servonum = ch;
  position_uS = current_servo_us[ch];
  pwm.writeMicroseconds(servonum, position_uS);
}

// Smooth servo movement with speed control
// speed: 1 = slowest, 10 = fastest, 0 = instant (no smoothing)
static void move_servo_smooth(uint8_t ch, int target_us, int speed)
{
  if (ch > 2) return;
  if (speed <= 0) {
    move_servo_us(ch, target_us);
    return;
  }

  int current = current_servo_us[ch];
  int target = constrain(target_us, USMIN, USMAX);

  // Calculate step size based on speed (higher speed = bigger steps)
  int step = speed * 5;  // 5-50 microseconds per step
  int delay_ms = 15;     // Delay between steps

  if (current < target) {
    // Moving up
    while (current < target) {
      current += step;
      if (current > target) current = target;
      move_servo_us(ch, current);
      delay(delay_ms);
    }
  } else {
    // Moving down
    while (current > target) {
      current -= step;
      if (current < target) current = target;
      move_servo_us(ch, current);
      delay(delay_ms);
    }
  }
}

// Electromagnet control (same as old code)
static void setEM(bool on) {
  emOn = on;
  if (on) {
    pwm.setPWM(PWM_CHANNEL_NO_EM, 4096, 0);  // EM ON
  } else {
    pwm.setPWM(PWM_CHANNEL_NO_EM, 0, 4096);  // EM OFF
  }
}

// Laser control (same as old code)
static void setLaser(bool on) {
  laserOn = on;
  if (on) {
    pwm.setPWM(PWM_CHANNEL_NO_LASER, 0, 4096);  // LASER ON
  } else {
    pwm.setPWM(PWM_CHANNEL_NO_LASER, 4096, 0);  // LASER OFF
  }
}

// Recalculate ROI positions based on current grid parameters
void recalculateROIs() {
  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 5; col++) {
      int idx = row * 5 + col;
      rois[idx][0] = roi_start_x - roi_size/2 + col * roi_spacing_x;  // x
      rois[idx][1] = roi_start_y - roi_size/2 + row * roi_spacing_y;  // y
      rois[idx][2] = roi_size;  // width
      rois[idx][3] = roi_size;  // height
    }
  }
}

// Load grid calibration from SPIFFS
void loadGridCalibration() {
  if (!SPIFFS.exists("/grid.cfg")) return;
  File f = SPIFFS.open("/grid.cfg", "r");
  if (!f) return;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.startsWith("START_X=")) roi_start_x = line.substring(8).toInt();
    else if (line.startsWith("START_Y=")) roi_start_y = line.substring(8).toInt();
    else if (line.startsWith("SPACING_X=")) roi_spacing_x = line.substring(10).toInt();
    else if (line.startsWith("SPACING_Y=")) roi_spacing_y = line.substring(10).toInt();
    else if (line.startsWith("SIZE=")) roi_size = line.substring(5).toInt();
  }
  f.close();
}

// Save grid calibration to SPIFFS
void saveGridCalibration() {
  File f = SPIFFS.open("/grid.cfg", "w");
  if (!f) return;
  f.printf("START_X=%d\n", roi_start_x);
  f.printf("START_Y=%d\n", roi_start_y);
  f.printf("SPACING_X=%d\n", roi_spacing_x);
  f.printf("SPACING_Y=%d\n", roi_spacing_y);
  f.printf("SIZE=%d\n", roi_size);
  f.close();
}

// ============================================================
// SHARED CSS - Used by all web pages (stored in PROGMEM)
// ============================================================
const char SHARED_CSS[] PROGMEM = R"rawliteral(
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',Arial,sans-serif;background:linear-gradient(135deg,#0f0c29,#302b63,#24243e);min-height:100vh;color:#fff;padding:20px}
.container{max-width:800px;margin:0 auto}
h1{font-size:28px;margin-bottom:10px;color:#00d9ff;text-shadow:0 0 10px rgba(0,217,255,0.5)}
h2{font-size:22px;color:#00ff88;margin:20px 0 10px}
h3{font-size:18px;color:#ff6b6b;margin:15px 0 8px}
.subtitle{color:#888;font-size:14px;margin-bottom:25px}
.card{background:rgba(255,255,255,0.05);backdrop-filter:blur(10px);border:1px solid rgba(255,255,255,0.1);border-radius:15px;padding:20px;margin:15px 0;transition:transform 0.2s,box-shadow 0.2s}
.card:hover{transform:translateY(-2px);box-shadow:0 10px 40px rgba(0,0,0,0.3)}
.card-title{font-size:16px;font-weight:bold;color:#00d9ff;margin-bottom:10px;display:flex;align-items:center;gap:10px}
.card-title svg{width:24px;height:24px}
.grid{display:grid;gap:15px}
.grid-2{grid-template-columns:repeat(2,1fr)}
.grid-3{grid-template-columns:repeat(3,1fr)}
.grid-4{grid-template-columns:repeat(4,1fr)}
@media(max-width:600px){.grid-2,.grid-3,.grid-4{grid-template-columns:1fr}}
.btn{display:inline-flex;align-items:center;justify-content:center;gap:8px;padding:12px 24px;border:none;border-radius:10px;font-size:14px;font-weight:600;cursor:pointer;transition:all 0.2s;text-decoration:none}
.btn-primary{background:linear-gradient(135deg,#00d9ff,#00a8cc);color:#000}
.btn-success{background:linear-gradient(135deg,#00ff88,#00cc6a);color:#000}
.btn-danger{background:linear-gradient(135deg,#ff6b6b,#ee5a5a);color:#fff}
.btn-warning{background:linear-gradient(135deg,#ffd93d,#ffb800);color:#000}
.btn-secondary{background:rgba(255,255,255,0.1);color:#fff;border:1px solid rgba(255,255,255,0.2)}
.btn:hover{transform:scale(1.05);box-shadow:0 5px 20px rgba(0,0,0,0.3)}
.btn:active{transform:scale(0.98)}
.btn-sm{padding:8px 16px;font-size:12px}
.btn-lg{padding:16px 32px;font-size:16px}
.input{width:100%;padding:12px 15px;border:1px solid rgba(255,255,255,0.2);border-radius:8px;background:rgba(0,0,0,0.3);color:#fff;font-size:14px}
.input:focus{outline:none;border-color:#00d9ff;box-shadow:0 0 10px rgba(0,217,255,0.3)}
.input-sm{width:80px;text-align:center}
.row{display:flex;align-items:center;gap:10px;margin:8px 0;flex-wrap:wrap}
.label{min-width:80px;font-weight:600;color:#aaa}
.status{padding:15px;background:rgba(0,0,0,0.4);border-radius:10px;font-family:'Courier New',monospace;font-size:13px;margin:15px 0}
.nav{display:flex;gap:10px;flex-wrap:wrap;margin:20px 0;padding:15px;background:rgba(0,0,0,0.2);border-radius:10px}
.tag{display:inline-block;padding:4px 10px;border-radius:20px;font-size:11px;font-weight:bold}
.tag-on{background:#00ff88;color:#000}
.tag-off{background:#ff6b6b;color:#fff}
.divider{height:1px;background:linear-gradient(90deg,transparent,rgba(255,255,255,0.2),transparent);margin:20px 0}
)rawliteral";

// ============ WIFI MANAGER FUNCTIONS ============

// Load WiFi credentials from SPIFFS
bool loadWiFiCredentials() {
  if (!SPIFFS_present) return false;
  if (!SPIFFS.exists(WIFI_CFG_PATH)) return false;

  File f = SPIFFS.open(WIFI_CFG_PATH, "r");
  if (!f) return false;

  wifi_ssid[0] = '\0';
  wifi_password[0] = '\0';

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    int eq = line.indexOf('=');
    if (eq > 0) {
      String key = line.substring(0, eq);
      String val = line.substring(eq + 1);
      if (key == "SSID") {
        strncpy(wifi_ssid, val.c_str(), sizeof(wifi_ssid) - 1);
      } else if (key == "PASS") {
        strncpy(wifi_password, val.c_str(), sizeof(wifi_password) - 1);
      }
    }
  }
  f.close();

  return strlen(wifi_ssid) > 0;
}

// Save WiFi credentials to SPIFFS
bool saveWiFiCredentials(const char* ssid, const char* pass) {
  File f = SPIFFS.open(WIFI_CFG_PATH, "w");
  if (!f) return false;

  f.printf("SSID=%s\n", ssid);
  f.printf("PASS=%s\n", pass);
  f.close();

  strncpy(wifi_ssid, ssid, sizeof(wifi_ssid) - 1);
  strncpy(wifi_password, pass, sizeof(wifi_password) - 1);

  return true;
}

// Clear saved WiFi credentials
void clearWiFiCredentials() {
  if (SPIFFS.exists(WIFI_CFG_PATH)) {
    SPIFFS.remove(WIFI_CFG_PATH);
  }
  wifi_ssid[0] = '\0';
  wifi_password[0] = '\0';
}

// Try to connect to WiFi with timeout
bool connectToWiFi(const char* ssid, const char* pass, int timeoutMs) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - startTime > timeoutMs) {
      WiFi.disconnect();
      return false;
    }
    delay(500);
    ss_putc('.');
  }
  return true;
}

// Start AP mode for WiFi configuration
void startAPMode() {
  apModeActive = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  IPAddress apIP = WiFi.softAPIP();

  ss_printf("\r\n AP Mode Started!");
  ss_printf("\r\n SSID: %s", AP_SSID);
  ss_printf("\r\n Password: %s", AP_PASSWORD);
  ss_printf("\r\n Config URL: http://%s", apIP.toString().c_str());

  // Display on TFT - WiFi setup instructions
  tft.setRotation(0);  // Portrait mode (240 wide x 320 tall)
  tft.fillScreen(TFT_BLACK);

  int y = 10;

  // Title
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("WiFi Setup", 50, y); y += 35;

  // Instructions box
  tft.drawRect(5, y, 230, 85, TFT_CYAN);
  y += 10;

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Connect your phone to WiFi:", 15, y); y += 18;

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString(AP_SSID, 15, y); y += 25;

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Password:", 15, y);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(AP_PASSWORD, 85, y); y += 35;

  // Browser instructions
  tft.drawRect(5, y, 230, 60, TFT_YELLOW);
  y += 10;

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Then open browser:", 15, y); y += 18;

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("192.168.4.1", 40, y); y += 40;

  // Additional info
  tft.setTextSize(1);
  tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tft.drawString("Select your home WiFi", 15, y); y += 12;
  tft.drawString("network on the webpage", 15, y);

  tft.setTextSize(TEXT_SIZE);
}

// WiFi Setup Page (served in AP mode)
void WiFiSetupPage() {
  server.sendHeader("Connection", "close");

  String html;
  html.reserve(4000);
  html += F("<!DOCTYPE html><html><head><meta charset='UTF-8'>");
  html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>WiFi Setup</title><style>");
  html += FPSTR(SHARED_CSS);
  html += F("</style></head><body><div class='container'>");

  html += F("<h1>WiFi Setup</h1>");
  html += F("<p class='subtitle'>Connect TicTacToe Robot to your WiFi network</p>");

  // Scan networks card
  html += F("<div class='card'><div class='card-title'>");
  html += F("<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' style='width:24px;height:24px'><path d='M5 12.55a11 11 0 0114 0'/><path d='M1.42 9a16 16 0 0121.16 0'/><path d='M8.53 16.11a6 6 0 016.95 0'/><circle cx='12' cy='20' r='1'/></svg>");
  html += F("Available Networks</div>");
  html += F("<div id='networks'>Scanning...</div>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='scan()' style='margin-top:10px'>Rescan</button>");
  html += F("</div>");

  // Manual entry card
  html += F("<div class='card'><div class='card-title'>Enter WiFi Credentials</div>");
  html += F("<form id='wifiForm'>");
  html += F("<div class='row'><span class='label'>SSID:</span>");
  html += F("<input type='text' id='ssid' class='input' placeholder='WiFi Network Name' required></div>");
  html += F("<div class='row'><span class='label'>Password:</span>");
  html += F("<input type='password' id='pass' class='input' placeholder='WiFi Password'></div>");
  html += F("<div class='row' style='margin-top:20px'>");
  html += F("<button type='submit' class='btn btn-success btn-lg'>Connect & Save</button>");
  html += F("</div></form></div>");

  // Status
  html += F("<div class='status' id='status'>Enter your WiFi credentials above</div>");

  // Instructions
  html += F("<div class='card'><div class='card-title'>Instructions</div>");
  html += F("<ol style='color:#aaa;font-size:14px;padding-left:20px;line-height:1.8'>");
  html += F("<li>Select a network from the list or enter SSID manually</li>");
  html += F("<li>Enter the WiFi password</li>");
  html += F("<li>Click 'Connect & Save'</li>");
  html += F("<li>Robot will restart and connect to your network</li>");
  html += F("<li>Check the TFT display for the new IP address</li></ol></div>");

  html += F("</div><script>");

  // Scan function
  html += F("async function scan(){");
  html += F("document.getElementById('networks').innerHTML='Scanning...';");
  html += F("const r=await fetch('/scan');const nets=await r.json();");
  html += F("let h='';for(let n of nets){");
  html += F("h+='<div class=\"row\" style=\"cursor:pointer\" onclick=\"selectNet(\\''+n.ssid+'\\')\"><span style=\"color:#00d9ff\">'+n.ssid+'</span>';");
  html += F("h+='<span style=\"color:#888;font-size:12px\">'+n.rssi+'dBm '+(n.secure?'🔒':'')+'</span></div>';}");
  html += F("document.getElementById('networks').innerHTML=h||'<p style=\"color:#888\">No networks found</p>';}");

  // Select network
  html += F("function selectNet(ssid){document.getElementById('ssid').value=ssid;}");

  // Form submit
  html += F("document.getElementById('wifiForm').onsubmit=async function(e){");
  html += F("e.preventDefault();");
  html += F("const ssid=document.getElementById('ssid').value;");
  html += F("const pass=document.getElementById('pass').value;");
  html += F("document.getElementById('status').textContent='Connecting to '+ssid+'...';");
  html += F("const r=await fetch('/connect?ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass));");
  html += F("const j=await r.json();");
  html += F("if(j.success){document.getElementById('status').innerHTML='<span style=\"color:#00ff88\">Connected! Restarting...</span>';}");
  html += F("else{document.getElementById('status').innerHTML='<span style=\"color:#ff6b6b\">Failed: '+j.error+'</span>';}};");

  // Initial scan
  html += F("scan();");
  html += F("</script></body></html>");

  server.send(200, "text/html", html);
}

// API: Scan for WiFi networks
void ApiWiFiScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN) + "}";
  }
  json += "]";
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

// API: Connect to WiFi and save credentials
void ApiWiFiConnect() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");

  if (ssid.length() == 0) {
    server.send(200, "application/json", "{\"success\":false,\"error\":\"SSID required\"}");
    return;
  }

  // Save credentials
  saveWiFiCredentials(ssid.c_str(), pass.c_str());

  // Send success response before restarting
  server.send(200, "application/json", "{\"success\":true}");
  delay(1000);

  // Restart to connect with new credentials
  ESP.restart();
}

// API: Get current WiFi status
void ApiWiFiStatus() {
  String json = "{";
  json += "\"connected\":" + String(WiFi.status() == WL_CONNECTED) + ",";
  json += "\"ssid\":\"" + String(wifi_ssid) + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"apMode\":" + String(apModeActive);
  json += "}";
  server.send(200, "application/json", json);
}

// API: Reset WiFi (clear credentials and restart in AP mode)
void ApiWiFiReset() {
  clearWiFiCredentials();
  server.send(200, "application/json", "{\"success\":true}");
  delay(1000);
  ESP.restart();
}

static bool loadPosesFromFS()
{
  if (!SPIFFS_present) return false;
  File f = SPIFFS.open(POSES_CFG_PATH, "r");
  if (!f) return false;

  bool loadedAny = false;
  while (f.available())
  {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int a, b, c;
    if (line.startsWith("HOME="))
    {
      if (sscanf(line.c_str(), "HOME=%d,%d,%d", &a, &b, &c) == 3)
      {
        home_alpha = a;
        home_beta = b;
        home_gamma = c;
        loadedAny = true;
      }
    }
    else if (line.startsWith("BOARD="))
    {
      if (sscanf(line.c_str(), "BOARD=%d,%d,%d", &a, &b, &c) == 3)
      {
        board_view_alpha = a;
        board_view_beta = b;
        board_view_gamma = c;
        loadedAny = true;
      }
    }
  }
  f.close();

  // Keep current servo values in sync with (possibly updated) home pose.
  current_servo_us[0] = constrain(home_alpha, USMIN, USMAX);
  current_servo_us[1] = constrain(home_beta, USMIN, USMAX);
  current_servo_us[2] = constrain(home_gamma, USMIN, USMAX);
  return loadedAny;
}

static bool savePosesToFS()
{
  if (!SPIFFS_present) return false;
  File f = SPIFFS.open(POSES_CFG_PATH, "w");
  if (!f) return false;
  f.printf("HOME=%d,%d,%d\n", home_alpha, home_beta, home_gamma);
  f.printf("BOARD=%d,%d,%d\n", board_view_alpha, board_view_beta, board_view_gamma);
  f.close();
  return true;
}

static bool init_camera_if_needed()
{
  if (cameraReady) return true;

  ss_puts("\r\n Starting Camera Configuration ....  ");
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_240X240;
  config.fb_count = 1;

  ss_puts("\r\n About to init camera ...  ");
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    ss_puts("\r\n ERROR: camera init failed");
    return false;
  }
  cameraReady = true;
  return true;
}

static bool init_pca_if_needed()
{
  if (pcaReady) return true;
  Wire.begin(SDA_PIN, SCL_PIN);
  pwm.begin();
  pwm.setOscillatorFrequency(27000000);
  pwm.setPWMFreq(SERVO_FREQ);
  pcaReady = true;
  return true;
}

// --- Calibration web UI handlers ---
static void CalibrationPage();
static void ApiServo();
static void ApiStatus();
static void ApiPcaEnable();
static void ApiSavePose();
static void ApiLoadPoses();
static void ApiGoto();

#define   servername "fileserver"  // Set your server's logical name here e.g. if 'myserver' then address is http://myserver.local/
// IPAddress local_IP(192, 168, 0, 150); // Set your server's fixed IP address here
// IPAddress gateway(192, 168, 0, 1);    // Set your network Gateway usually your Router base address
// IPAddress subnet(255, 255, 255, 0);   // Set your network sub-network mask here
// IPAddress dns(192,168,0,1);           // Set your network DNS usually your Router base address

//#define EM_ON  0
//#define EM_OFF 1

#define LED_INBUILD_ON  0
#define LED_INBUILD_OFF 1

void hang(void);
   
void setup()
{
  software_serial_configure();
  ss_puts("\r\n\r\n =================  RESET ESP32-CAM-CH340  =================\r\n");

  pinMode(PCA_ENABLE_PIN_NO, OUTPUT);       
  digitalWrite(PCA_ENABLE_PIN_NO, HIGH); // Disable PCA module: So that servo motors do not move
  
  pinMode(4, OUTPUT);                       digitalWrite(4, LOW); // Flash light OFF
  pinMode(LED_RED_INBUILD_PIN_NO, OUTPUT);  digitalWrite(LED_RED_INBUILD_PIN_NO, LED_INBUILD_OFF); 
  
 // Mode Select:
 //               -WiFi Browsing Mode
 //               -Normal Robot Software

  #define PRODUCER 13
  #define CONSUMER 0

  pinMode(PRODUCER, OUTPUT);   digitalWrite(PRODUCER, LOW); 
  pinMode(CONSUMER, INPUT);                        
    
  ss_puts("\r\n TFT Resolution in UserSteup file 320 x 240");
  tft.begin(); // Resolution in UserSteup file 320 x 240
  tft.setRotation(0);	// 0 & 2 Portrait. 1 & 3 landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(TEXT_SIZE);
  
  tft.setTextColor(TFT_WHITE, TFT_BLACK); //  text, background
  tft.drawString("ESP32-CAM ST7789V", 0, lcd_row); lcd_row += lcd_row_incrementer;
  
  tft.setTextColor(TFT_WHITE, TFT_RED); //  text, background
  tft.drawString(" R E S E T", 0, lcd_row); lcd_row += lcd_row_incrementer;
  
  if (!SPIFFS.begin(true)) {
    ss_puts("\r\n SPIFFS initialisation failed...");
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString("No SPIFFS", 0, lcd_row);  lcd_row += lcd_row_incrementer;
    SPIFFS_present = false;
  } else {
    ss_puts("\r\n SPIFFS initialised... file access enabled...");
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("SPIFFS: Okay", 0, lcd_row);  lcd_row += lcd_row_incrementer;
    SPIFFS_present = true;
  }

  // DISABLED: Load grid calibration from SPIFFS (using hardcoded values instead)
  // loadGridCalibration();

  // Initialize ROI grid for board detection (uses hardcoded values)
  recalculateROIs();

  // Load calibrated poses (if available) early so both WiFi calibration and robot mode use them.
  loadPosesFromFS();

  // Count-down for mode selection
  for(count=50; count >=1; count--)
  {
      tft.setTextColor(TFT_YELLOW, TFT_BLACK); //  text, background
      tft.drawString("Press WiFi Key: ", 0, lcd_row);

      tft.setTextColor(TFT_ORANGE, TFT_BLACK); //  text, background
      tft.drawNumber(count/10,     200, lcd_row);

    if(digitalRead(CONSUMER) == LOW) // Switch Pressed for WiFi / Web-Browser mode
      {
        tft.setRotation(1);	// 0 & 2 Portrait. 1 & 3 landscape
        tft.fillScreen(TFT_BLACK);
        lcd_row = 0;
        tft.setTextColor(TFT_WHITE, TFT_BLUE); //  text, background
        tft.drawString("WiFi Browser Mode", 0, lcd_row);  lcd_row += lcd_row_incrementer;
        ss_puts("\r\n Key pressed: Now in WiFi / Browser Mode");
        digitalWrite(4, HIGH); delay(2); digitalWrite(4, LOW);

        // Try to load saved WiFi credentials
        bool connected = false;
        if (loadWiFiCredentials() && strlen(wifi_ssid) > 0) {
          ss_printf("\r\n Trying saved WiFi: %s", wifi_ssid);
          tft.setTextColor(TFT_YELLOW, TFT_BLACK);
          tft.drawString("Connecting to:", 0, lcd_row); lcd_row += lcd_row_incrementer;
          tft.drawString(wifi_ssid, 0, lcd_row); lcd_row += lcd_row_incrementer;

          connected = connectToWiFi(wifi_ssid, wifi_password, WIFI_CONNECT_TIMEOUT);
        }

        if (connected) {
          // Successfully connected to saved WiFi
          IPAddress ip = WiFi.localIP();
          String ipString = ip.toString();

          ss_printf("\r\n Connected to: %s", wifi_ssid);
          ss_printf("\r\n IP address: %s", ipString.c_str());
          ss_printf("\r\nhttp://%s\r\n", ipString.c_str());

          tft.fillScreen(TFT_BLACK);
          lcd_row = 0;
          tft.setTextColor(TFT_GREEN, TFT_BLACK);
          tft.drawString("WiFi Connected!", 0, lcd_row); lcd_row += lcd_row_incrementer;
          tft.setTextColor(TFT_BLUE, TFT_BLACK);
          tft.drawString("SSID: ", 0, lcd_row); tft.drawString(wifi_ssid, 70, lcd_row); lcd_row += lcd_row_incrementer;
          tft.drawString("IP: ", 0, lcd_row); lcd_row += lcd_row_incrementer;

          tft.setTextColor(TFT_SKYBLUE, TFT_BLACK);
          tft.setTextSize(TEXT_SIZE + 1);
          tft.drawString(ipString, 0, lcd_row); lcd_row = lcd_row + (lcd_row_incrementer + lcd_row_incrementer / 2);
          tft.setTextSize(TEXT_SIZE);

          tft.setTextColor(TFT_BLUE, TFT_BLACK);
          tft.drawString("Open Browser", 0, lcd_row); lcd_row += lcd_row_incrementer;
          sprintf(buffer_temp, "http://%s", ipString.c_str());
          tft.drawString(buffer_temp, 0, lcd_row); lcd_row += lcd_row_incrementer;

          // Setup mDNS
          if (!MDNS.begin(host)) {
            ss_puts("\r\n Error setting up MDNS responder!");
            tft.setTextColor(TFT_RED, TFT_BLACK);
            tft.drawString("ERROR: MDNS", 0, lcd_row); lcd_row += lcd_row_incrementer;
          } else {
            ss_puts("\r\n mDNS responder started");
            tft.drawString("MDNS: Okay", 0, lcd_row); lcd_row += lcd_row_incrementer;
          }

          // Setup normal routes
          server.on("/", HomePage);

          // Calibration endpoints (servo jogging + pose save/load)
          server.on("/cal", CalibrationPage);
          server.on("/api/status", ApiStatus);
          server.on("/api/servo", ApiServo);
          server.on("/api/pca", ApiPcaEnable);
          server.on("/api/pose/save", ApiSavePose);
          server.on("/api/pose/load", ApiLoadPoses);
          server.on("/api/goto", ApiGoto);

          // EM, Laser, Heights, Board, Rack calibration
          server.on("/api/em", ApiEM);
          server.on("/api/laser", ApiLaser);
          server.on("/api/height", ApiHeight);
          server.on("/api/gotoBoard", ApiGotoBoard);
          server.on("/api/saveBoard", ApiSaveBoard);
          server.on("/api/gotoRack", ApiGotoRack);
          server.on("/api/saveRack", ApiSaveRack);
          server.on("/api/saveAll", ApiSaveAll);
          server.on("/api/export", ApiExport);

          // Grid calibration endpoints
          server.on("/grid", GridCalibrationPage);
          server.on("/api/grid", ApiGridUpdate);
          server.on("/api/grid/save", ApiGridSave);

          server.on("/ota", OTAPage);
          server.on("/files", FileManagerPage);

          /*handling uploading firmware file */
          server.on("/update", HTTP_POST, []() {
            server.sendHeader("Connection", "close");
            server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
            delay(500);
            ESP.restart();
          }, []() {
            HTTPUpload& upload = server.upload();
            if (upload.status == UPLOAD_FILE_START) {
              esp_camera_deinit();  // Free camera memory for OTA
              Update.begin(UPDATE_SIZE_UNKNOWN);
            } else if (upload.status == UPLOAD_FILE_WRITE) {
              Update.write(upload.buf, upload.currentSize);
            } else if (upload.status == UPLOAD_FILE_END) {
              Update.end(true);
            }
          });

          server.on("/download", File_Download);
          server.on("/upload",   File_Upload);
          server.on("/fupload",  HTTP_POST,[](){ server.send(200);}, handleFileUpload);
          server.on("/stream",   File_Stream);
          server.on("/delete",   File_Delete);
          server.on("/dir",      SPIFFS_dir);

          // WiFi manager routes (for reconfiguring WiFi)
          server.on("/wifi", WiFiSetupPage);
          server.on("/scan", ApiWiFiScan);
          server.on("/connect", ApiWiFiConnect);
          server.on("/api/wifi/status", ApiWiFiStatus);
          server.on("/api/wifi/reset", ApiWiFiReset);

          server.begin();
        } else {
          // No saved credentials or connection failed - start AP mode
          ss_puts("\r\n Starting AP mode for WiFi setup...");
          startAPMode();

          // AP mode routes - only WiFi setup
          server.on("/", WiFiSetupPage);
          server.on("/scan", ApiWiFiScan);
          server.on("/connect", ApiWiFiConnect);

          server.begin();
        }
          ss_printf("\r\n Waiting for request from web-browser");
          while(1)
          {
            server.handleClient();            
            digitalWrite(LED_RED_INBUILD_PIN_NO, ~digitalRead(LED_RED_INBUILD_PIN_NO)); // Toggle the state of inbuild LED

            // Optional live camera preview on TFT while calibrating from phone.
            if ((calibrationActive || gridCalibrationMode) && calibrationPreviewEnabled)
            {
              uint32_t now = millis();
              if ((now - lastPreviewMs) > 200)
              {
                lastPreviewMs = now;
                if (init_camera_if_needed())
                {
                  camera_fb_t *frame = esp_camera_fb_get();
                  if (frame)
                  {
                    tft.pushImage(0, 0, frame->width, frame->height, (uint16_t*)frame->buf);
                    esp_camera_fb_return(frame);

                    // Draw grid overlay during grid calibration
                    if (gridCalibrationMode)
                    {
                      for (int i = 0; i < 25; i++)
                      {
                        tft.drawRect(rois[i][0], rois[i][1], rois[i][2], rois[i][3], TFT_GREEN);
                      }
                      // Draw center crosshair on first cell for reference
                      int cx = roi_start_x;
                      int cy = roi_start_y;
                      tft.drawLine(cx - 5, cy, cx + 5, cy, TFT_RED);
                      tft.drawLine(cx, cy - 5, cx, cy + 5, TFT_RED);
                    }
                  }
                }
              }
            }
            delay(10);
          }

      }


    if(count%10==0){ 
      ss_printf("\r\n Countdown:  %3d  %d   ", count, digitalRead(CONSUMER)); }
      delay(100);    
    } // for(count=100; count >=1; count--)

////////////////////////////////////////////////////////////////////////////////////
// Normal Robot Software Run Mode:    
  lcd_row += lcd_row_incrementer;

  while(digitalRead(CONSUMER) == LOW)  // Let the user release the WiFi key
    ;
  delay(1000);
  pinMode(CONSUMER, OUTPUT);   

  tft.setTextColor(TFT_GREEN, TFT_BLACK); //  text, background
  tft.drawString("Robot Run Mode", 0, lcd_row); lcd_row += lcd_row_incrementer;
  
  ss_puts("\r\n Robot Run Mode  ");

  if (!init_camera_if_needed())
  {
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.setTextSize(TEXT_SIZE);
    tft.drawString("CAMERA: ERROR", 0, lcd_row); lcd_row += lcd_row_incrementer;
    tft.drawString("Press Reset  ", 0, lcd_row); lcd_row += lcd_row_incrementer;
    tft.drawString("to try again ", 0, lcd_row); lcd_row += lcd_row_incrementer;
    hang();
    return;
  }

  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("CAMERA: Init Okay", 0, lcd_row); lcd_row += lcd_row_incrementer;
  ss_puts("\r\n CAMERA: Init Okay");
  delay(50);

  init_pca_if_needed();

  
  // Servo Motor positions while assembling the robot:
  // Bring motors to these positions first and then "join" the link joints together.
  // Keep the circuit / motors powered while assembling
  // Please note that plastic gears are delicate so handel them carefully.
  //
  // Positions during robot assembly:
  // Servo 1 : 180'   Link-1 in Extreme Left (RIght handed human seated on human side of checker board)
  // Servo 2 : Middle Link-2A in middle position
  // Servo 3 : 0' Link-2B in "pick the checker" position (extream downward)
  //
  //
  // First connect only M1 to Blue PCB 
  // once it is assembled
  // then addtionally connect Link2A's motor M2 to Blue PCB
  // once it is assembled
  // then addtionally connect Link2B's motor M3 to Blue PCB
  // once it is assembled
  // then addtionally connect Laser (and ELectromagnet) driver PCB to Blue Board.
  //
  // the Laser diode should project the Red spot on the calibration point.
  //



   //// NOTE: To command any device connected to Blue PCB (PCA9685), the Blue PCBs output should be anabled.
   ////       A LOW on OE pin on Blue PCB (Blue Wire) is output enable
   ////       Devices such as Servo Motors, Electro Magnet, Laser and Buzzer are connected to Blue PCB  (PCA9685)
     
    // Set servo positions BEFORE enabling PCA (prevents initial jump)
    pwm.writeMicroseconds(0, board_view_alpha);
    pwm.writeMicroseconds(1, board_view_beta);
    pwm.writeMicroseconds(2, board_view_gamma);
    current_servo_us[0] = board_view_alpha;
    current_servo_us[1] = board_view_beta;
    current_servo_us[2] = board_view_gamma;

    set_pca_outputs(true); // Now enable - servos go directly to board view position

    // ss_printf("\r\n Laser is ON (EM is OFF)...");
    // tft.drawString("Laser: ON",   0, lcd_row);  lcd_row += lcd_row_incrementer; 
    // pwm.setPWM(PWM_CHANNEL_NO_LASER, 0, 4096); // LASER ON
    
    // tft.drawString("EM: OFF",   0, lcd_row);  lcd_row += lcd_row_incrementer; 
    // pwm.setPWM(PWM_CHANNEL_NO_EM, 0, 4096); // EM OFF
    
    
  ////while(1);

////// TESTs LASER and Electro Magnet (EM)   ONLY  (Note: Infinite loop):-  
  //       digitalWrite(PCA_ENABLE_PIN_NO, LOW); // When this pin is low all pins are enabled.
  //         //while(1) 
  //         {
  //         ss_printf("\r\n Laser and EM are ON ...");
  //         pwm.setPWM(PWM_CHANNEL_NO_EM, 4096, 0); // EM ON
  //         pwm.setPWM(PWM_CHANNEL_NO_LASER, 0, 4096); // LASER ON
  //         delay(5000);
          
  //         ss_printf("\r\n Laser OFF ...");
  //         pwm.setPWM(PWM_CHANNEL_NO_EM, 0, 4096);  // EM OFF
  //         pwm.setPWM(PWM_CHANNEL_NO_LASER, 4096, 0);  // LASER OFF
  //         delay(3000);
  //       }

  // tft.setTextColor(TFT_GREEN, TFT_BLACK); //  text, background
  // tft.drawString(" ALL - OK ", 0, lcd_row); lcd_row += lcd_row_incrementer;
  // delay(10);

  // int angle_rad, angle_deg;

  //   for(int i=0; i<7; i++)
  // {
  //   for(int j=0; j<2; j++)
  //   {
  //     angle_rad = calc_angle_alpha(i, j, 0);
  //     angle_deg = angle_rad * 180 / 3.14;
  //     position_uS = ((angle_deg+17.28) * 10) + 560;
  //     servonum = 0;
  //     pwm.writeMicroseconds(servonum, position_uS);    
  //     tft.drawString("Servo# ",   0, lcd_row); tft.drawNumber(servonum,  100, lcd_row); tft.drawNumber(position_uS,  140, lcd_row);
  //     lcd_row += lcd_row_incrementer; // Draw integer using current font       
  //     delay(1000); // Wait for few seconds

  //     angle_rad = calc_angle_beta(i, j, 0);
  //     angle_deg = angle_rad * 180 / 3.14;
  //     position_uS = ((180-angle_deg) * 10) + 530;
  //     servonum = 1;
  //     pwm.writeMicroseconds(servonum, position_uS);    
  //     tft.drawString("Servo# ",   0, lcd_row); tft.drawNumber(servonum,  100, lcd_row); tft.drawNumber(position_uS,  140, lcd_row);
  //     lcd_row += lcd_row_incrementer; // Draw integer using current font       
  //     delay(5000); // Wait for few seconds
  //     lcd_row -= (2*lcd_row_incrementer);
  //   }
  // }

  // for(int i=0; i<5; i++)
  // {
  //   for(int j=0; j<5; j++)
  //   {
  //     angle_rad = calc_angle_alpha(i, j, 1);
  //     angle_deg = angle_rad * 180 / 3.14;
  //     position_uS = ((angle_deg+17.28) * 10) + 560;
  //     servonum = 0;
  //     pwm.writeMicroseconds(servonum, position_uS);    
  //     tft.drawString("Servo# ",   0, lcd_row); tft.drawNumber(servonum,  100, lcd_row); tft.drawNumber(position_uS,  140, lcd_row);
  //     lcd_row += lcd_row_incrementer; // Draw integer using current font       
  //     delay(1000); // Wait for few seconds

  //     angle_rad = calc_angle_beta(i, j, 1);
  //     angle_deg = angle_rad * 180 / 3.14;
  //     position_uS = ((180-angle_deg) * 10) + 530;
  //     servonum = 1;
  //     pwm.writeMicroseconds(servonum, position_uS);    
  //     tft.drawString("Servo# ",   0, lcd_row); tft.drawNumber(servonum,  100, lcd_row); tft.drawNumber(position_uS,  140, lcd_row);
  //     lcd_row += lcd_row_incrementer; // Draw integer using current font       
  //     delay(5000); // Wait for few seconds
  //     lcd_row -= (2*lcd_row_incrementer);
  //   }
  // }

  tft.fillScreen(TFT_BLACK);
  count=0;
  deleteFile(SPIFFS, "/image.rgb");

  // Show startup screen
  drawStartupScreen();
  delay(2000);

  // Countdown to start
  tft.fillRect(50, 250, 150, 40, COLOR_BG);
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN, COLOR_BG);
  for (int i = 3; i >= 1; i--) {
    tft.fillRect(100, 260, 50, 30, COLOR_BG);
    tft.drawNumber(i, 110, 260);
    delay(800);
  }
  tft.fillRect(90, 260, 80, 30, COLOR_BG);
  tft.drawString("GO!", 100, 260);
  delay(500);

}
////////////////////////////////////////////////////////////////////////////////////////////
camera_fb_t * fb;
// NOTE: Removed unused global variables c and count1

static void move_to_board_view()
{
  // FIRST: Raise gamma to hover for safe travel
  move_servo_smooth(2, gamma_hover, GAMMA_SPEED);
  delay(100);

  // Move alpha and beta while gamma is at hover - OPTIMIZED: reduced from 700ms
  move_servo_us(0, board_view_alpha);
  delay(400);

  move_servo_us(1, board_view_beta);
  delay(400);

  // Lower gamma to board view position (for camera)
  move_servo_smooth(2, board_view_gamma, GAMMA_SPEED);
  delay(100);
}

static bool capture_write_and_process(bool drawPreview)
{
  // Trick: capture twice to avoid stale buffered frame
  camera_fb_t *frame = esp_camera_fb_get();
  if (frame) {
    esp_camera_fb_return(frame);
  }

  frame = esp_camera_fb_get();
  if (!frame) {
    delay(250);
    frame = esp_camera_fb_get();
  }
  if (!frame) {
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("ERROR: fb_get()", 0, lcd_row);
    lcd_row += lcd_row_incrementer;
    return false;
  }

  if (drawPreview) {
    tft.pushImage(0, 0, frame->width, frame->height, (uint16_t*)frame->buf);
  }

  // Process frame buffer directly (faster, no SPIFFS needed)
  process_image_direct((uint16_t*)frame->buf, frame->width, frame->height);

  esp_camera_fb_return(frame);
  return true;
}

// ============ GAME UI DRAWING FUNCTIONS ============

// Draw a single piece (X or O) at grid position with optional highlight
// Mirror the board so display matches physical board from human's view
void drawPiece(int row, int col, char piece, bool highlight) {
  // Mirror: flip both row and col for correct orientation
  int mirrorRow = 4 - row;
  int mirrorCol = 4 - col;
  int centerX = BOARD_START_X + mirrorCol * CELL_SIZE + CELL_SIZE / 2;
  int centerY = BOARD_START_Y + mirrorRow * CELL_SIZE + CELL_SIZE / 2;
  int radius = CELL_SIZE / 2 - 4;

  if (piece == O) {
    // Human piece - Red filled circle
    if (highlight) {
      tft.fillCircle(centerX, centerY, radius + 2, COLOR_HIGHLIGHT);
    }
    tft.fillCircle(centerX, centerY, radius, COLOR_HUMAN);
    tft.drawCircle(centerX, centerY, radius, TFT_DARKGREY);
  }
  else if (piece == X) {
    // Robot piece - Blue X
    if (highlight) {
      tft.fillRect(centerX - radius - 2, centerY - radius - 2,
                   radius * 2 + 4, radius * 2 + 4, COLOR_HIGHLIGHT);
    }
    int offset = radius - 2;
    // Draw thick X
    for (int t = -2; t <= 2; t++) {
      tft.drawLine(centerX - offset, centerY - offset + t,
                   centerX + offset, centerY + offset + t, COLOR_ROBOT);
      tft.drawLine(centerX + offset, centerY - offset + t,
                   centerX - offset, centerY + offset + t, COLOR_ROBOT);
    }
  }
}

// Draw the 5x5 game grid
void drawGameGrid() {
  // Draw grid lines
  tft.setTextColor(COLOR_GRID);

  // Vertical lines
  for (int i = 0; i <= 5; i++) {
    int x = BOARD_START_X + i * CELL_SIZE;
    tft.drawFastVLine(x, BOARD_START_Y, BOARD_SIZE, COLOR_GRID);
    if (i < 5) {
      tft.drawFastVLine(x + 1, BOARD_START_Y, BOARD_SIZE, COLOR_GRID);
    }
  }

  // Horizontal lines
  for (int i = 0; i <= 5; i++) {
    int y = BOARD_START_Y + i * CELL_SIZE;
    tft.drawFastHLine(BOARD_START_X, y, BOARD_SIZE, COLOR_GRID);
    if (i < 5) {
      tft.drawFastHLine(BOARD_START_X, y + 1, BOARD_SIZE, COLOR_GRID);
    }
  }
}

// Draw all pieces on the game board
void drawGameBoard() {
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      if (board[i][j] != EMPTY) {
        bool highlight = (i == lastMoveRow && j == lastMoveCol);
        drawPiece(i, j, board[i][j], highlight);
      }
    }
  }
}

// Draw the score bar at the top
void drawScoreBar() {
  // Background
  tft.fillRect(0, 0, 240, SCORE_BAR_HEIGHT, TFT_DARKGREY);

  // Human score (left side)
  tft.setTextSize(2);
  tft.setTextColor(COLOR_HUMAN, TFT_DARKGREY);
  tft.drawString("YOU:", 5, 8);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawNumber(humanWins, 55, 8);

  // VS in center
  tft.setTextColor(COLOR_HIGHLIGHT, TFT_DARKGREY);
  tft.drawString("vs", 105, 8);

  // Robot score (right side)
  tft.setTextColor(COLOR_ROBOT, TFT_DARKGREY);
  tft.drawString("BOT:", 140, 8);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawNumber(robotWins, 190, 8);
}

// Draw status bar at bottom
void drawStatusBar(const char* message, uint16_t color) {
  tft.fillRect(0, STATUS_BAR_Y, 240, 95, COLOR_BG);
  tft.setTextSize(2);
  tft.setTextColor(color, COLOR_BG);

  // Center the message
  int textWidth = strlen(message) * 12;  // Approximate width at size 2
  int x = (240 - textWidth) / 2;
  if (x < 5) x = 5;

  tft.drawString(message, x, STATUS_BAR_Y + 10);
}

// Draw animated status (with dots animation)
void drawAnimatedStatus(const char* message, uint16_t color) {
  // Update animation
  if (millis() - lastAnimTime > 400) {
    animDots = (animDots + 1) % 4;
    lastAnimTime = millis();
  }

  tft.fillRect(0, STATUS_BAR_Y, 240, 95, COLOR_BG);
  tft.setTextSize(2);
  tft.setTextColor(color, COLOR_BG);

  // Draw message
  tft.drawString(message, 20, STATUS_BAR_Y + 10);

  // Draw animated dots
  String dots = "";
  for (int i = 0; i < animDots; i++) dots += ".";
  tft.drawString(dots.c_str(), 180, STATUS_BAR_Y + 10);
}

// Draw scanning indicator (small, non-intrusive)
void drawScanningIndicator() {
  // Small scanning dot in corner
  static int scanDot = 0;
  scanDot = (scanDot + 1) % 3;

  int baseX = 210;
  int baseY = STATUS_BAR_Y + 70;

  // Clear previous
  tft.fillRect(baseX, baseY, 30, 10, COLOR_BG);

  // Draw dots
  for (int i = 0; i < 3; i++) {
    uint16_t col = (i == scanDot) ? TFT_GREEN : TFT_DARKGREY;
    tft.fillCircle(baseX + i * 8, baseY + 4, 3, col);
  }
}

// Draw the complete game screen
void drawGameScreen() {
  tft.fillScreen(COLOR_BG);
  drawScoreBar();
  drawGameGrid();
  drawGameBoard();
}

// Draw startup/title screen
void drawStartupScreen() {
  tft.fillScreen(COLOR_BG);

  // Title
  tft.setTextSize(3);
  tft.setTextColor(COLOR_HIGHLIGHT, COLOR_BG);
  tft.drawString("TIC TAC TOE", 15, 30);

  // Subtitle
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.drawString("Robot vs Human", 35, 70);

  // Draw a sample board
  tft.setTextSize(1);
  tft.drawString("5x5 Edition", 80, 100);

  // Robot icon (simple)
  tft.setTextColor(COLOR_ROBOT, COLOR_BG);
  tft.setTextSize(4);
  tft.drawString("[X]", 30, 150);

  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.setTextSize(3);
  tft.drawString("vs", 100, 155);

  tft.setTextColor(COLOR_HUMAN, COLOR_BG);
  tft.setTextSize(4);
  tft.drawString("(O)", 150, 150);

  // Game number
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN, COLOR_BG);
  tft.drawString("Game #", 60, 220);
  tft.drawNumber(gameNumber, 150, 220);

  // Starting message
  tft.setTextColor(TFT_CYAN, COLOR_BG);
  tft.drawString("Get Ready!", 60, 260);
}

// Draw win/lose/draw result screen
void drawResultScreen(char winner) {
  tft.fillScreen(COLOR_BG);

  tft.setTextSize(3);

  if (winner == O) {
    // Human wins
    tft.setTextColor(COLOR_HUMAN, COLOR_BG);
    tft.drawString("YOU WIN!", 40, 30);
  }
  else if (winner == X) {
    // Robot wins
    tft.setTextColor(COLOR_ROBOT, COLOR_BG);
    tft.drawString("ROBOT WINS!", 20, 30);
  }
  else {
    // Draw
    tft.setTextColor(COLOR_HIGHLIGHT, COLOR_BG);
    tft.drawString("DRAW!", 70, 30);
  }

  // Draw mini board representation (centered, smaller)
  int miniStartX = 60;
  int miniStartY = 70;
  int miniCellSize = 24;

  // Draw mini grid
  tft.setTextColor(TFT_DARKGREY);
  for (int i = 0; i <= 5; i++) {
    tft.drawFastVLine(miniStartX + i * miniCellSize, miniStartY, miniCellSize * 5, TFT_DARKGREY);
    tft.drawFastHLine(miniStartX, miniStartY + i * miniCellSize, miniCellSize * 5, TFT_DARKGREY);
  }

  // Draw pieces on mini board (mirrored to match physical board)
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      // Mirror coordinates
      int mi = 4 - i;
      int mj = 4 - j;
      int cx = miniStartX + mj * miniCellSize + miniCellSize / 2;
      int cy = miniStartY + mi * miniCellSize + miniCellSize / 2;
      if (board[i][j] == O) {
        tft.fillCircle(cx, cy, 8, COLOR_HUMAN);
      } else if (board[i][j] == X) {
        // Draw small X
        for (int t = -1; t <= 1; t++) {
          tft.drawLine(cx - 6, cy - 6 + t, cx + 6, cy + 6 + t, COLOR_ROBOT);
          tft.drawLine(cx + 6, cy - 6 + t, cx - 6, cy + 6 + t, COLOR_ROBOT);
        }
      }
    }
  }

  // Score display
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.drawString("SCORE", 80, 210);

  // Human score
  tft.setTextColor(COLOR_HUMAN, COLOR_BG);
  tft.setTextSize(3);
  tft.drawNumber(humanWins, 50, 240);

  // VS
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.setTextSize(2);
  tft.drawString("-", 110, 245);

  // Robot score
  tft.setTextColor(COLOR_ROBOT, COLOR_BG);
  tft.setTextSize(3);
  tft.drawNumber(robotWins, 140, 240);

  // Instructions
  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN, COLOR_BG);
  tft.drawString("Clear board & press button", 30, 290);
}

// Draw "Your Turn" screen
void drawHumanTurnScreen() {
  drawGameScreen();
  drawStatusBar("YOUR TURN!", COLOR_HUMAN);

  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.drawString("Place your RED piece", 50, STATUS_BAR_Y + 40);
}

// Draw "Robot Thinking" screen
void drawRobotThinkingScreen() {
  drawAnimatedStatus("THINKING", COLOR_ROBOT);
}

// Draw move confirmation progress
void drawConfirmProgress(int count, int total) {
  int barWidth = 150;
  int barHeight = 15;
  int barX = 45;
  int barY = STATUS_BAR_Y + 50;

  // Background bar
  tft.drawRect(barX, barY, barWidth, barHeight, TFT_WHITE);

  // Fill progress
  int fillWidth = (barWidth - 4) * count / total;
  tft.fillRect(barX + 2, barY + 2, fillWidth, barHeight - 4, TFT_GREEN);

  // Text
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, COLOR_BG);
  tft.drawString("Confirming move...", 60, barY + 20);
}

// Display the internal board state on TFT (left side)
void displayBoard() {
    tft.fillRect(0, 240, 240, 80, TFT_BLACK);
    lcd_row = 245;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("INT:", 0, lcd_row);  // Internal board label
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            char c = board[i][j];
            if (c == X) {
                tft.setTextColor(TFT_BLUE, TFT_BLACK);
                tft.drawChar('X', 30 + j * 12, lcd_row);
            } else if (c == O) {
                tft.setTextColor(TFT_RED, TFT_BLACK);
                tft.drawChar('O', 30 + j * 12, lcd_row);
            } else {
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.drawChar('.', 30 + j * 12, lcd_row);
            }
        }
        lcd_row += 10;
    }
    tft.setTextSize(2);
}

// Check if game is over (win or draw)
bool gameOver = false;

// Reset the game state for a new game
void resetGame() {
    // Reset board to empty
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            board[i][j] = EMPTY;
            previousState[i][j] = EMPTY;
            cameraBoard[i][j] = EMPTY;
        }
    }

    // Reset game variables
    gameOver = false;
    baselineCaptured = false;
    count = 0;  // Reset rack position for robot pieces

    // Reset last move tracking
    lastMoveRow = -1;
    lastMoveCol = -1;

    // Alternate who starts
    humanStartsNext = !humanStartsNext;
    player = humanStartsNext ? 0 : 1;  // 0 = human, 1 = robot
}

// Returns true if game was reset (new game starting)
bool checkGameOver() {
    char winner = Winner(board);
    if (winner == X) {
        gameOver = true;
        robotWins++;
    } else if (winner == O) {
        gameOver = true;
        humanWins++;
    } else if (!checkIfEmpty(board)) {
        winner = 'D';  // Draw marker
        gameOver = true;
    }

    if (gameOver) {
        // Show result screen
        drawResultScreen(winner);

        return_to_home();

        // Disable camera to free GPIO0 for button
        esp_camera_deinit();
        cameraReady = false;
        delay(100);

        // Configure GPIO0 as input with pull-up for button
        pinMode(BUTTON_PIN, INPUT_PULLUP);
        // Set GPIO13 LOW to provide ground for button circuit
        pinMode(SDA_PIN, OUTPUT);
        digitalWrite(SDA_PIN, LOW);

        // Wait for button press (button is LOW when pressed)
        while (digitalRead(BUTTON_PIN) == HIGH) {
            digitalWrite(LED_RED_INBUILD_PIN_NO, !digitalRead(LED_RED_INBUILD_PIN_NO));
            delay(300);
        }

        // Wait for button release with proper debounce
        delay(100);  // OPTIMIZED: increased debounce for reliability
        while (digitalRead(BUTTON_PIN) == LOW) {
            delay(20);
        }
        delay(100);  // OPTIMIZED: final debounce

        // Increment game number
        gameNumber++;

        // Show next game startup screen
        tft.fillScreen(TFT_BLACK);
        tft.setTextSize(3);
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.drawString("GAME", 80, 30);
        tft.drawString("#", 70, 70);
        tft.drawNumber(gameNumber, 100, 70);

        tft.setTextSize(2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        if (!humanStartsNext) {  // Will be toggled in resetGame, so show opposite
            tft.setTextColor(COLOR_HUMAN, TFT_BLACK);
            tft.drawString("YOU start!", 60, 130);
        } else {
            tft.setTextColor(COLOR_ROBOT, TFT_BLACK);
            tft.drawString("ROBOT starts!", 45, 130);
        }

        // Countdown
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        for (int i = 3; i >= 1; i--) {
            tft.fillRect(90, 180, 60, 40, TFT_BLACK);
            tft.setTextSize(4);
            tft.drawNumber(i, 105, 180);
            delay(1000);
        }

        tft.fillRect(60, 180, 120, 40, TFT_BLACK);
        tft.setTextSize(2);
        tft.drawString("GO!", 100, 190);
        delay(300);

        // Show "Preparing" screen while camera initializes
        tft.fillScreen(COLOR_BG);
        tft.setTextSize(3);
        tft.setTextColor(TFT_CYAN, COLOR_BG);
        tft.drawString("PREPARING", 30, 100);
        tft.setTextSize(2);
        tft.setTextColor(TFT_WHITE, COLOR_BG);
        tft.drawString("Get ready...", 55, 150);

        // Reinitialize I2C for servos (GPIO13 was used for button ground)
        Wire.begin(SDA_PIN, SCL_PIN);
        delay(100);

        // Reinitialize camera silently with retry logic
        int cameraRetries = 0;
        const int MAX_CAMERA_RETRIES = 5;
        int dotAnim = 0;

        while (!init_camera_if_needed() && cameraRetries < MAX_CAMERA_RETRIES) {
            cameraRetries++;

            // Show animated dots (no technical details)
            tft.fillRect(150, 150, 50, 20, COLOR_BG);
            tft.setTextColor(TFT_WHITE, COLOR_BG);
            String dots = "";
            for (int d = 0; d < (dotAnim % 4); d++) dots += ".";
            tft.drawString(dots.c_str(), 155, 150);
            dotAnim++;

            delay(400);

            // Try deinit and reinit
            esp_camera_deinit();
            cameraReady = false;
            delay(200);
        }

        // If camera failed, keep trying silently
        while (!cameraReady) {
            // Update display with friendly message
            tft.fillRect(30, 180, 200, 30, COLOR_BG);
            tft.setTextSize(1);
            tft.setTextColor(TFT_YELLOW, COLOR_BG);
            tft.drawString("Hold on, almost ready...", 40, 185);

            // Retry loop
            for (int retry = 0; retry < MAX_CAMERA_RETRIES && !cameraReady; retry++) {
                esp_camera_deinit();
                cameraReady = false;
                delay(300);
                init_camera_if_needed();

                // Animate dots
                tft.fillRect(150, 150, 50, 20, COLOR_BG);
                tft.setTextSize(2);
                tft.setTextColor(TFT_WHITE, COLOR_BG);
                String dots = "";
                for (int d = 0; d < (dotAnim % 4); d++) dots += ".";
                tft.drawString(dots.c_str(), 155, 150);
                dotAnim++;
            }

            if (!cameraReady) {
                delay(500);  // Brief pause before retrying again
            }
        }

        tft.fillScreen(TFT_BLACK);

        // Reset the game
        resetGame();
        return true;  // Game was reset
    }
    return false;  // Game continues
}

void loop()
{
    lcd_row = 240;
    digitalWrite(LED_RED_INBUILD_PIN_NO, !digitalRead(LED_RED_INBUILD_PIN_NO));

    // Human's turn (player == 0)
    if (player == 0) {
        // SAFETY: Ensure gamma is at hover (prevents hitting pawns)
        move_servo_smooth(2, gamma_hover, GAMMA_SPEED);

        // Initialize board at game start
        if (!baselineCaptured) {
            // All cells start EMPTY
            for (int i = 0; i < 5; i++) {
                for (int j = 0; j < 5; j++) {
                    board[i][j] = EMPTY;
                }
            }
            baselineCaptured = true;
            lastMoveRow = -1;
            lastMoveCol = -1;
        }

        // Draw the game UI
        drawHumanTurnScreen();

        move_to_board_view();

        // Wait for human to place a RED piece (with confirmation to avoid false detection)
        bool humanMoved = false;
        int confirmCount = 0;
        int lastDetectedRow = -1;
        int lastDetectedCol = -1;

        while (!humanMoved) {
            // Scan in background - no camera display
            if (!capture_write_and_process(false)) {
                delay(500);
                continue;
            }

            // Update scanning indicator
            drawScanningIndicator();

            // Check if human placed a RED piece in an empty cell (with cheat detection)
            int moveResult = findHumanMoveWithCheatDetection();

            if (moveResult == -1) {
                // CHEATING DETECTED! Show warning and wait for correction
                displayCheatWarning(lastCheatType);

                // Wait in background until cheating is resolved
                int fixConfirmCount = 0;
                bool cheatingResolved = false;
                unsigned long cheatStartTime = millis();
                bool showingBoard = false;
                const unsigned long SHOW_BOARD_AFTER = 5000;  // 5 seconds

                while (!cheatingResolved) {
                    delay(500);

                    // After 5 seconds, toggle between cheat warning and board display
                    unsigned long elapsed = millis() - cheatStartTime;
                    if (elapsed > SHOW_BOARD_AFTER && !showingBoard) {
                        // Show expected board state so human can match it
                        tft.fillScreen(COLOR_BG);
                        tft.setTextSize(2);
                        tft.setTextColor(TFT_YELLOW, COLOR_BG);
                        tft.drawString("CORRECT BOARD:", 35, 5);

                        // Draw the expected board state
                        drawGameGrid();
                        drawGameBoard();

                        tft.setTextSize(1);
                        tft.setTextColor(TFT_CYAN, COLOR_BG);
                        tft.drawString("Match your board to this!", 40, STATUS_BAR_Y + 10);
                        tft.setTextColor(TFT_GREEN, COLOR_BG);
                        tft.drawString("Then make ONE move", 50, STATUS_BAR_Y + 30);

                        showingBoard = true;
                    }
                    // Toggle back to warning every 8 seconds
                    else if (elapsed > SHOW_BOARD_AFTER + 8000 && showingBoard) {
                        displayCheatWarning(lastCheatType);
                        cheatStartTime = millis();  // Reset timer
                        showingBoard = false;
                    }

                    // Capture and process WITHOUT displaying to screen
                    if (!capture_write_and_process(false)) {
                        continue;
                    }

                    // Check if cheating is still happening
                    int cheatCheck = detectCheating();

                    if (cheatCheck == CHEAT_NONE) {
                        // Board looks good - confirm it
                        fixConfirmCount++;

                        // Update waiting text with progress
                        int progressY = showingBoard ? STATUS_BAR_Y + 55 : 280;
                        tft.fillRect(30, progressY, 200, 25, COLOR_BG);
                        tft.setTextColor(TFT_MAGENTA, COLOR_BG);
                        tft.setTextSize(2);
                        tft.drawString("Checking...", 30, progressY);
                        tft.drawNumber(fixConfirmCount, 160, progressY);
                        tft.drawString("/3", 185, progressY);

                        if (fixConfirmCount >= 3) {
                            cheatingResolved = true;
                        }
                    } else {
                        // Still cheating - reset
                        fixConfirmCount = 0;
                    }
                }

                // Cheating resolved! Show success message
                tft.fillScreen(TFT_BLACK);
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.setTextSize(4);
                tft.drawString("GOOD!", 60, 40);
                tft.setTextSize(2);
                tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                tft.drawString("That's better!", 40, 100);
                tft.drawString("Good human!", 50, 130);
                tft.setTextSize(2);
                tft.setTextColor(TFT_CYAN, TFT_BLACK);
                tft.drawString("Now make your", 30, 170);
                tft.drawString("move properly", 30, 195);
                delay(2000);

                // Reset confirmation and continue
                confirmCount = 0;
                lastDetectedRow = -1;
                lastDetectedCol = -1;

                // Redraw the game screen
                drawHumanTurnScreen();
            }
            else if (moveResult == 1) {
                // Valid move detected - confirm it
                if (humanMoveRow == lastDetectedRow && humanMoveCol == lastDetectedCol) {
                    confirmCount++;
                    // Show confirmation progress bar
                    drawConfirmProgress(confirmCount, MOVE_CONFIRM_COUNT);
                } else {
                    // New position detected, reset confirmation
                    confirmCount = 1;
                    lastDetectedRow = humanMoveRow;
                    lastDetectedCol = humanMoveCol;

                    // Show potential move on board (preview)
                    drawGameScreen();
                    drawPiece(humanMoveRow, humanMoveCol, O, true);  // Preview with highlight
                    drawStatusBar("Move detected!", COLOR_HIGHLIGHT);
                }

                // Confirmed enough times?
                if (confirmCount >= MOVE_CONFIRM_COUNT) {
                    humanMoved = true;

                    // Update internal board with human's move
                    board[humanMoveRow][humanMoveCol] = O;
                    lastMoveRow = humanMoveRow;
                    lastMoveCol = humanMoveCol;
                    isHumanLastMove = true;

                    // Show confirmed move
                    drawGameScreen();
                    drawStatusBar("Good move!", TFT_GREEN);
                    delay(800);
                }
            } else {
                // No move detected yet, reset confirmation
                if (confirmCount > 0) {
                    // Was detecting something, now lost - redraw
                    drawHumanTurnScreen();
                }
                confirmCount = 0;
                lastDetectedRow = -1;
                lastDetectedCol = -1;
            }

            delay(400);  // OPTIMIZED: reduced from 800ms, still stable
        }

        delay(300);

        // Check if human won
        if (checkGameOver()) return;  // Game reset, start fresh loop
    }
    // Robot's turn (player == 1)
    else {
        // SAFETY: Raise gamma to hover before thinking (prevents hitting pawns)
        move_servo_smooth(2, gamma_hover, GAMMA_SPEED);

        // Initialize board at game start (when robot goes first)
        if (!baselineCaptured) {
            for (int i = 0; i < 5; i++) {
                for (int j = 0; j < 5; j++) {
                    board[i][j] = EMPTY;
                }
            }
            baselineCaptured = true;
            lastMoveRow = -1;
            lastMoveCol = -1;
        }

        // Show game board with thinking status
        drawGameScreen();
        drawAnimatedStatus("THINKING", COLOR_ROBOT);

        // Reset bestMove
        bestMove[0] = -1;
        bestMove[1] = -1;

        // Find best move using internal board state
        FindBestMove(board);

        if (bestMove[0] >= 0 && bestMove[1] >= 0) {
            // Show which move robot chose
            drawGameScreen();
            drawStatusBar("My move!", COLOR_ROBOT);

            // Preview the move with blinking effect
            for (int blink = 0; blink < 3; blink++) {
                drawPiece(bestMove[0], bestMove[1], X, true);
                delay(200);
                // Redraw grid to "erase" piece (use mirrored coordinates like drawPiece)
                int mirrorRow = 4 - bestMove[0];
                int mirrorCol = 4 - bestMove[1];
                int x = BOARD_START_X + mirrorCol * CELL_SIZE;
                int y = BOARD_START_Y + mirrorRow * CELL_SIZE;
                tft.fillRect(x + 2, y + 2, CELL_SIZE - 3, CELL_SIZE - 3, COLOR_BG);
                delay(200);
            }

            // makeMove() physically places piece and updates board[bestMove] = X
            makeMove();

            // Update last move tracking
            lastMoveRow = bestMove[0];
            lastMoveCol = bestMove[1];
            isHumanLastMove = false;

            // Show updated board
            drawGameScreen();
            drawStatusBar("Done!", TFT_GREEN);
            delay(800);

            // Check if robot won (using internal board state)
            if (checkGameOver()) return;  // Game reset, start fresh loop
        } else {
            // No valid move found
            drawStatusBar("No move?!", TFT_YELLOW);
            delay(1000);
        }
    }

    // Switch player (only if game continues)
    player = !player;
    delay(300);
} // loop()
//////////////////////////////////////////////////////////////////////////////////////////////////

void makeMove()
{
    pick_checker();

    // SAFETY: Ensure gamma is at hover before horizontal movement
    move_servo_smooth(2, gamma_hover, GAMMA_SPEED);
    delay(100);  // OPTIMIZED: reduced from 200ms

    // Move alpha SLOWLY to board position (pawn in hand - must be gentle)
    move_servo_smooth(0, board_alpha_freq[bestMove[0]][bestMove[1]], GAMMA_SPEED);
    delay(100);  // OPTIMIZED: reduced from 200ms

    // Move beta SLOWLY to board position (pawn in hand - must be gentle)
    move_servo_smooth(1, board_beta_freq[bestMove[0]][bestMove[1]], GAMMA_SPEED);
    delay(100);  // OPTIMIZED: reduced from 200ms

    // Lower gamma SLOWLY to place height
    move_servo_smooth(2, gamma_place, GAMMA_SPEED);
    delay(150);  // OPTIMIZED: slight pause for stability

    // Release piece
    pwm.setPWM(PWM_CHANNEL_NO_EM, 0, 4096);  // EM OFF
    delay(200);  // OPTIMIZED: reduced from 300ms

    // Raise gamma SLOWLY back to hover
    move_servo_smooth(2, gamma_hover, GAMMA_SPEED);
    delay(100);  // OPTIMIZED: reduced from 200ms

    // Update internal board state
    board[bestMove[0]][bestMove[1]] = X;

    // Increment piece counter with bounds check (wrap around if needed)
    count++;
    if (count >= MAX_RACK_POSITIONS) {
        count = 0;  // Wrap around to prevent array overflow
    }

    // Go directly to board view after placing (no home position)
    move_to_board_view();
}
void return_to_home()
{
  // FIRST: Raise gamma to hover for safe travel
  move_servo_smooth(2, gamma_hover, GAMMA_SPEED);
  delay(100);

  // Move alpha and beta while gamma is at hover - OPTIMIZED: reduced from 700ms each
  move_servo_us(0, home_alpha);
  delay(400);

  move_servo_us(1, home_beta);
  delay(400);

  // KEEP gamma at hover (don't lower - prevents hitting pawns)
  // Gamma stays at hover position
}

int checkChange()
{
  for(int i=0; i<5; i++)
  {
    for(int j=0; j<5; j++)
    {
      if(board[i][j]!=previousState[i][j])
        return 1;
    }
  }
  return 0;
}

void updatePrevious()
{
  for(int i=0; i<5; i++)
  {
    for(int j=0; j<5; j++)
    {
      previousState[i][j]=board[i][j];
    }
  }
}

// ============ CHEATING DETECTION FUNCTIONS ============

// Count human (red/O) pieces on camera board vs expected board
// Checks human pieces and detects replacing robot pieces
// Returns CHEAT_NONE if valid, otherwise returns cheat type
int detectCheating()
{
  int newHumanPieces = 0;      // New O pieces in empty cells
  int missingHumanPieces = 0;  // Human O pieces that disappeared
  int replacedRobotPieces = 0; // Robot X pieces replaced with human O

  // Count expected human pieces and actual human pieces
  int expectedHumanCount = 0;
  int actualHumanCount = 0;

  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      char expected = board[i][j];
      char actual = cameraBoard[i][j];

      // Count expected human pieces
      if (expected == O) {
        expectedHumanCount++;
      }

      // Count actual human pieces on camera
      if (actual == O) {
        actualHumanCount++;
      }

      // Check for specific situations
      if (expected == O && actual != O) {
        // Human's old piece is missing (moved or removed)
        missingHumanPieces++;
      }
      else if (expected == EMPTY && actual == O) {
        // New human piece in empty cell
        newHumanPieces++;
      }
      else if (expected == X && actual == O) {
        // Robot piece replaced with human piece - big cheat!
        replacedRobotPieces++;
      }
    }
  }

  // Check for robot piece replacement first (most serious)
  if (replacedRobotPieces > 0) {
    return CHEAT_REPLACED_ROBOT;
  }

  // Multiple new pieces placed at once
  if (newHumanPieces > 1) {
    return CHEAT_MULTI_MOVE;
  }

  // Moved a piece: old piece missing AND new piece appeared
  if (missingHumanPieces > 0 && newHumanPieces > 0) {
    return CHEAT_MOVED_PIECE;
  }

  // Removed own piece without placing new one (taking back a move)
  if (missingHumanPieces > 0 && newHumanPieces == 0) {
    return CHEAT_REMOVED_HUMAN;
  }

  return CHEAT_NONE;
}

// Display cheating warning with funny message
void displayCheatWarning(int cheatType)
{
  tft.fillScreen(TFT_BLACK);

  // Big warning - extra large
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(4);
  tft.drawString("HEY!", 70, 10);

  // Funny message (cycle through different ones)
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString(cheatMessages[cheatMsgIndex], 5, 55);
  cheatMsgIndex = (cheatMsgIndex + 1) % NUM_CHEAT_MESSAGES;

  // Specific cheat description - bigger text
  tft.setTextSize(2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  switch (cheatType) {
    case CHEAT_MULTI_MOVE:
      tft.drawString("Extra pieces!", 10, 100);
      tft.drawString("Remove them!", 10, 125);
      break;
    case CHEAT_REMOVED_HUMAN:
      tft.drawString("Piece removed!", 10, 100);
      tft.drawString("No take-backs!", 10, 125);
      break;
    case CHEAT_MOVED_PIECE:
      tft.drawString("Piece moved!", 10, 100);
      tft.drawString("Put it back!", 10, 125);
      break;
    case CHEAT_REPLACED_ROBOT:
      tft.drawString("MY piece gone!", 10, 100);
      tft.drawString("Put BLUE back!", 10, 125);
      break;
  }

  // Instructions - bigger
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Fix board &", 10, 170);
  tft.drawString("make 1 move", 10, 195);

  tft.setTextColor(TFT_MAGENTA, TFT_BLACK);
  tft.setTextSize(3);
  tft.drawString("Waiting...", 30, 240);

  tft.setTextSize(TEXT_SIZE);
}

// Find human's move with cheating detection
// Returns: 0 = no move yet, 1 = valid move found, -1 = cheating CONFIRMED
int findHumanMoveWithCheatDetection()
{
  humanMoveRow = -1;
  humanMoveCol = -1;

  // Check for cheating (only human pieces)
  int cheatType = detectCheating();

  if (cheatType != CHEAT_NONE) {
    // Cheating detected - but need to confirm multiple times
    if (cheatType == lastCheatDetected) {
      cheatConfirmCount++;
    } else {
      // Different cheat type or first detection
      cheatConfirmCount = 1;
      lastCheatDetected = cheatType;
    }

    // Only flag as confirmed cheating after multiple detections
    if (cheatConfirmCount >= CHEAT_CONFIRM_COUNT) {
      lastCheatType = cheatType;
      cheatConfirmCount = 0;  // Reset for next time
      lastCheatDetected = CHEAT_NONE;
      return -1;  // Cheating CONFIRMED!
    }

    // Not confirmed yet - keep checking
    return 0;
  }

  // No cheating detected - reset cheat confirmation
  cheatConfirmCount = 0;
  lastCheatDetected = CHEAT_NONE;

  // Look for valid move (exactly one new O piece)
  int newMoves = 0;
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 5; j++) {
      // Human placed a piece: internal board is EMPTY, camera sees RED (O)
      if (board[i][j] == EMPTY && cameraBoard[i][j] == O) {
        humanMoveRow = i;
        humanMoveCol = j;
        newMoves++;
      }
    }
  }

  if (newMoves == 1) {
    return 1;  // Valid single move found
  }

  return 0;  // No move detected yet (or still confirming)
}

// Legacy function for compatibility
bool findHumanMove()
{
  return findHumanMoveWithCheatDetection() == 1;
}

// Display camera detected board (right side, for debugging)
void displayCameraBoard() {
    lcd_row = 245;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    tft.drawString("CAM:", 120, lcd_row);  // Camera board label (right side)
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            char c = cameraBoard[i][j];
            if (c == X) {
                tft.setTextColor(TFT_BLUE, TFT_BLACK);
                tft.drawChar('X', 150 + j * 12, lcd_row);
            } else if (c == O) {
                tft.setTextColor(TFT_RED, TFT_BLACK);
                tft.drawChar('O', 150 + j * 12, lcd_row);
            } else {
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.drawChar('.', 150 + j * 12, lcd_row);
            }
        }
        lcd_row += 10;
    }
    tft.setTextSize(2);
}

void pick_checker()
{
        // FIRST: Raise gamma to hover height for safe horizontal travel
        move_servo_smooth(2, gamma_hover, GAMMA_SPEED);
        delay(100);

        // Move alpha to rack position (gamma at hover) - OPTIMIZED: reduced from 800ms
        move_servo_us(0, rack_freq[count][0]);
        delay(400);

        // Move beta to rack position (gamma still at hover) - OPTIMIZED: reduced from 800ms
        move_servo_us(1, rack_freq[count][1]);
        delay(400);

        // Turn on electromagnet BEFORE lowering (pre-magnetize while above pawn)
        pwm.setPWM(PWM_CHANNEL_NO_EM, 4096, 0); // EM ON
        delay(250);  // OPTIMIZED: EM magnetizes quickly, reduced from 500ms

        // NOW lower gamma SLOWLY to pickup height (EM already magnetized)
        move_servo_smooth(2, gamma_pickup, GAMMA_SPEED);
        delay(500);  // OPTIMIZED: reduced from 1000ms, still ensures grip

        // Raise gamma SLOWLY back to hover (EM still ON, holding pawn)
        move_servo_smooth(2, gamma_hover, GAMMA_SPEED);
        delay(100);
}

// NOTE: Removed custom min/max functions - use Arduino's built-in versions

// ============================================================
// OPTIMIZED TIC-TAC-TOE AI FOR ESP32
// Combines strategic shortcuts with adjacent-cell pruning
// ============================================================

#define BOARD_SIZE 5
#define WIN_LENGTH 4
#define MAX_CANDIDATES 25

// Adjacent cell candidates for pruned search
static int candidates[MAX_CANDIDATES];
static int candidateCount = 0;

// Check if position is valid
static inline bool isValidPos(int r, int c) {
    return r >= 0 && r < BOARD_SIZE && c >= 0 && c < BOARD_SIZE;
}

// Check for 4 in a row on a 5x5 board
char Winner(char b[5][5])
{
    // Check all rows for 4 consecutive
    for (int i = 0; i < 5; i++) {
        // Check positions 0-3
        if (b[i][0] != EMPTY && b[i][0] == b[i][1] && b[i][1] == b[i][2] && b[i][2] == b[i][3])
            return b[i][0];
        // Check positions 1-4
        if (b[i][1] != EMPTY && b[i][1] == b[i][2] && b[i][2] == b[i][3] && b[i][3] == b[i][4])
            return b[i][1];
    }

    // Check all columns for 4 consecutive
    for (int j = 0; j < 5; j++) {
        // Check positions 0-3
        if (b[0][j] != EMPTY && b[0][j] == b[1][j] && b[1][j] == b[2][j] && b[2][j] == b[3][j])
            return b[0][j];
        // Check positions 1-4
        if (b[1][j] != EMPTY && b[1][j] == b[2][j] && b[2][j] == b[3][j] && b[3][j] == b[4][j])
            return b[1][j];
    }

    // Check main diagonals (top-left to bottom-right)
    // Diagonal starting at (0,0): positions 0-3
    if (b[0][0] != EMPTY && b[0][0] == b[1][1] && b[1][1] == b[2][2] && b[2][2] == b[3][3])
        return b[0][0];
    // Diagonal starting at (0,0): positions 1-4
    if (b[1][1] != EMPTY && b[1][1] == b[2][2] && b[2][2] == b[3][3] && b[3][3] == b[4][4])
        return b[1][1];
    // Diagonal starting at (0,1)
    if (b[0][1] != EMPTY && b[0][1] == b[1][2] && b[1][2] == b[2][3] && b[2][3] == b[3][4])
        return b[0][1];
    // Diagonal starting at (1,0)
    if (b[1][0] != EMPTY && b[1][0] == b[2][1] && b[2][1] == b[3][2] && b[3][2] == b[4][3])
        return b[1][0];

    // Check anti-diagonals (top-right to bottom-left)
    // Diagonal starting at (0,4): positions 0-3
    if (b[0][4] != EMPTY && b[0][4] == b[1][3] && b[1][3] == b[2][2] && b[2][2] == b[3][1])
        return b[0][4];
    // Diagonal starting at (0,4): positions 1-4
    if (b[1][3] != EMPTY && b[1][3] == b[2][2] && b[2][2] == b[3][1] && b[3][1] == b[4][0])
        return b[1][3];
    // Diagonal starting at (0,3)
    if (b[0][3] != EMPTY && b[0][3] == b[1][2] && b[1][2] == b[2][1] && b[2][1] == b[3][0])
        return b[0][3];
    // Diagonal starting at (1,4)
    if (b[1][4] != EMPTY && b[1][4] == b[2][3] && b[2][3] == b[3][2] && b[3][2] == b[4][1])
        return b[1][4];

    return EMPTY;
}

int checkIfEmpty(char b[5][5])
{
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            if (b[i][j] == EMPTY)
                return 1;
        }
    }
    return 0;
}

// ============ OPTIMIZED HELPER FUNCTIONS ============

// Find winning move for player - O(25) check
bool findWinningMove(char b[5][5], char player, int &row, int &col) {
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            if (b[i][j] == EMPTY) {
                b[i][j] = player;
                if (Winner(b) == player) {
                    b[i][j] = EMPTY;
                    row = i;
                    col = j;
                    return true;
                }
                b[i][j] = EMPTY;
            }
        }
    }
    return false;
}

// Count threats (3-in-a-row with 1 empty) - optimized single pass
int countThreats(char b[5][5], char player) {
    int threats = 0;

    // Horizontal
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j <= 1; j++) {
            int cnt = 0, emp = 0;
            for (int k = 0; k < 4; k++) {
                if (b[i][j+k] == player) cnt++;
                else if (b[i][j+k] == EMPTY) emp++;
            }
            if (cnt == 3 && emp == 1) threats++;
        }
    }
    // Vertical
    for (int i = 0; i <= 1; i++) {
        for (int j = 0; j < 5; j++) {
            int cnt = 0, emp = 0;
            for (int k = 0; k < 4; k++) {
                if (b[i+k][j] == player) cnt++;
                else if (b[i+k][j] == EMPTY) emp++;
            }
            if (cnt == 3 && emp == 1) threats++;
        }
    }
    // Diagonal down-right
    for (int i = 0; i <= 1; i++) {
        for (int j = 0; j <= 1; j++) {
            int cnt = 0, emp = 0;
            for (int k = 0; k < 4; k++) {
                if (b[i+k][j+k] == player) cnt++;
                else if (b[i+k][j+k] == EMPTY) emp++;
            }
            if (cnt == 3 && emp == 1) threats++;
        }
    }
    // Diagonal down-left
    for (int i = 0; i <= 1; i++) {
        for (int j = 3; j < 5; j++) {
            int cnt = 0, emp = 0;
            for (int k = 0; k < 4; k++) {
                if (b[i+k][j-k] == player) cnt++;
                else if (b[i+k][j-k] == EMPTY) emp++;
            }
            if (cnt == 3 && emp == 1) threats++;
        }
    }
    return threats;
}

// Find fork move (creates 2+ threats)
bool findForkMove(char b[5][5], char player, int &row, int &col) {
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            if (b[i][j] == EMPTY) {
                b[i][j] = player;
                if (countThreats(b, player) >= 2) {
                    b[i][j] = EMPTY;
                    row = i;
                    col = j;
                    return true;
                }
                b[i][j] = EMPTY;
            }
        }
    }
    return false;
}

// ============ ADJACENT CELL PRUNING (KEY OPTIMIZATION) ============

// Get candidate moves - only cells adjacent to played pieces
void getCandidates(char b[5][5]) {
    candidateCount = 0;
    bool hasPlayed = false;
    bool visited[5][5] = {false};

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            if (b[i][j] != EMPTY) {
                hasPlayed = true;
                // Add adjacent empty cells
                for (int di = -1; di <= 1; di++) {
                    for (int dj = -1; dj <= 1; dj++) {
                        int ni = i + di;
                        int nj = j + dj;
                        if (isValidPos(ni, nj) && b[ni][nj] == EMPTY && !visited[ni][nj]) {
                            visited[ni][nj] = true;
                            candidates[candidateCount++] = ni * 5 + nj;
                        }
                    }
                }
            }
        }
    }

    // Empty board - return center
    if (!hasPlayed) {
        candidates[0] = 12; // Center (2,2)
        candidateCount = 1;
    }
}

// ============ SIMPLIFIED HEURISTIC (SINGLE PASS) ============

int evaluate(char b[5][5]) {
    char w = Winner(b);
    if (w == X) return 10000;
    if (w == O) return -10000;

    int score = 0;

    // Single pass through all lines
    // Horizontal
    for (int i = 0; i < 5; i++) {
        for (int j = 0; j <= 1; j++) {
            int xCnt = 0, oCnt = 0, eCnt = 0;
            for (int k = 0; k < 4; k++) {
                if (b[i][j+k] == X) xCnt++;
                else if (b[i][j+k] == O) oCnt++;
                else eCnt++;
            }
            if (oCnt == 0) { // X can use this line
                if (xCnt == 3) score += 100;
                else if (xCnt == 2) score += 10;
            }
            if (xCnt == 0) { // O can use this line
                if (oCnt == 3) score -= 100;
                else if (oCnt == 2) score -= 10;
            }
        }
    }

    // Vertical
    for (int i = 0; i <= 1; i++) {
        for (int j = 0; j < 5; j++) {
            int xCnt = 0, oCnt = 0;
            for (int k = 0; k < 4; k++) {
                if (b[i+k][j] == X) xCnt++;
                else if (b[i+k][j] == O) oCnt++;
            }
            if (oCnt == 0) {
                if (xCnt == 3) score += 100;
                else if (xCnt == 2) score += 10;
            }
            if (xCnt == 0) {
                if (oCnt == 3) score -= 100;
                else if (oCnt == 2) score -= 10;
            }
        }
    }

    // Diagonal down-right
    for (int i = 0; i <= 1; i++) {
        for (int j = 0; j <= 1; j++) {
            int xCnt = 0, oCnt = 0;
            for (int k = 0; k < 4; k++) {
                if (b[i+k][j+k] == X) xCnt++;
                else if (b[i+k][j+k] == O) oCnt++;
            }
            if (oCnt == 0) {
                if (xCnt == 3) score += 100;
                else if (xCnt == 2) score += 10;
            }
            if (xCnt == 0) {
                if (oCnt == 3) score -= 100;
                else if (oCnt == 2) score -= 10;
            }
        }
    }

    // Diagonal down-left
    for (int i = 0; i <= 1; i++) {
        for (int j = 3; j < 5; j++) {
            int xCnt = 0, oCnt = 0;
            for (int k = 0; k < 4; k++) {
                if (b[i+k][j-k] == X) xCnt++;
                else if (b[i+k][j-k] == O) oCnt++;
            }
            if (oCnt == 0) {
                if (xCnt == 3) score += 100;
                else if (xCnt == 2) score += 10;
            }
            if (xCnt == 0) {
                if (oCnt == 3) score -= 100;
                else if (oCnt == 2) score -= 10;
            }
        }
    }

    // Center bonus
    if (b[2][2] == X) score += 50;
    else if (b[2][2] == O) score -= 50;

    return score;
}

// ============ MINIMAX WITH ADJACENT PRUNING ============

int minimax(char b[5][5], int depth, bool isMax, int alpha, int beta) {
    yield(); // ESP32 watchdog

    char w = Winner(b);
    if (w == X) return 10000 + depth;
    if (w == O) return -10000 - depth;

    // Check draw
    bool hasEmpty = false;
    for (int i = 0; i < 5 && !hasEmpty; i++) {
        for (int j = 0; j < 5 && !hasEmpty; j++) {
            if (b[i][j] == EMPTY) hasEmpty = true;
        }
    }
    if (!hasEmpty) return 0;

    if (depth == 0) return evaluate(b);

    // Get candidates (adjacent cells only) - KEY OPTIMIZATION
    getCandidates(b);
    if (candidateCount == 0) return 0;

    if (isMax) {
        int best = -20000;
        for (int k = 0; k < candidateCount; k++) {
            int i = candidates[k] / 5;
            int j = candidates[k] % 5;
            b[i][j] = X;
            int val = minimax(b, depth - 1, false, alpha, beta);
            b[i][j] = EMPTY;
            if (val > best) best = val;
            if (val > alpha) alpha = val;
            if (beta <= alpha) break;
        }
        return best;
    } else {
        int best = 20000;
        for (int k = 0; k < candidateCount; k++) {
            int i = candidates[k] / 5;
            int j = candidates[k] % 5;
            b[i][j] = O;
            int val = minimax(b, depth - 1, true, alpha, beta);
            b[i][j] = EMPTY;
            if (val < best) best = val;
            if (val < beta) beta = val;
            if (beta <= alpha) break;
        }
        return best;
    }
}

// ============ MAIN MOVE FINDER ============

void FindBestMove(char b[5][5]) {
    int r, c;

    // 1. WIN immediately - O(25)
    if (findWinningMove(b, X, r, c)) {
        bestMove[0] = r;
        bestMove[1] = c;
        return;
    }

    // 2. BLOCK opponent win - O(25)
    if (findWinningMove(b, O, r, c)) {
        bestMove[0] = r;
        bestMove[1] = c;
        return;
    }

    // 3. Take CENTER - O(1)
    if (b[2][2] == EMPTY) {
        bestMove[0] = 2;
        bestMove[1] = 2;
        return;
    }

    // 4. Create FORK - O(25)
    if (findForkMove(b, X, r, c)) {
        bestMove[0] = r;
        bestMove[1] = c;
        return;
    }

    // 5. Block opponent FORK - O(25)
    if (findForkMove(b, O, r, c)) {
        bestMove[0] = r;
        bestMove[1] = c;
        return;
    }

    // 6. MINIMAX with adjacent pruning
    getCandidates(b);

    int best = -20000;
    bestMove[0] = -1;
    bestMove[1] = -1;

    // Fixed depth 3 - optimal for ESP32 speed
    const int depth = 3;

    for (int k = 0; k < candidateCount; k++) {
        int i = candidates[k] / 5;
        int j = candidates[k] % 5;

        b[i][j] = X;
        int score = minimax(b, depth, false, -20000, 20000);
        b[i][j] = EMPTY;

        if (score > best) {
            best = score;
            bestMove[0] = i;
            bestMove[1] = j;
        }
    }

    // Fallback
    if (bestMove[0] < 0) {
        for (int i = 0; i < 5 && bestMove[0] < 0; i++) {
            for (int j = 0; j < 5 && bestMove[0] < 0; j++) {
                if (b[i][j] == EMPTY) {
                    bestMove[0] = i;
                    bestMove[1] = j;
                }
            }
        }
    }
}

// Swap bytes for RGB565 format
static inline uint16_t swapbytes(uint16_t val) {
    return (val >> 8) | (val << 8);
}

// Function to convert RGB565 to RGB888
static inline void convert_rgb565_to_rgb888(uint16_t data, uint8_t *r, uint8_t *g, uint8_t *b) {
    data = swapbytes(data);
    *r = (uint8_t)((data & 0xF800) >> 8);
    *g = (uint8_t)((data & 0x07E0) >> 3);
    *b = (uint8_t)((data & 0x001F) << 3);
}

// Process frame buffer directly - OPTIMIZED version (no SPIFFS, no malloc)
// Uses pixel skipping (every 2nd pixel) for 4x speed improvement
void process_image_direct(uint16_t* data, int width, int height) {
    uint16_t pixel;
    uint8_t r, g, b;

    // Process each ROI (25 cells on 5x5 board)
    for (int b_i = 0; b_i < 25; b_i++) {
        const int roi_x = rois[b_i][0];
        const int roi_y = rois[b_i][1];
        const int roi_w = rois[b_i][2];
        const int roi_h = rois[b_i][3];

        // Count pixels that are clearly RED, BLUE, or GREEN
        int red_count = 0;
        int blue_count = 0;
        int green_count = 0;
        int total_pixels = 0;

        // OPTIMIZATION: Skip every other pixel (2x speed, sufficient accuracy)
        const int roi_end_y = roi_y + roi_h;
        const int roi_end_x = roi_x + roi_w;

        for (int i = roi_y; i < roi_end_y && i < height; i += 2) {
            if (i < 0) continue;
            const int row_offset = i * width;

            for (int j = roi_x; j < roi_end_x && j < width; j += 2) {
                if (j < 0) continue;

                pixel = data[row_offset + j];
                // Swap bytes for correct RGB565 interpretation
                pixel = (pixel >> 8) | (pixel << 8);

                // Extract RGB components (inline for speed)
                r = (uint8_t)((pixel & 0xF800) >> 8);
                g = (uint8_t)((pixel & 0x07E0) >> 3);
                b = (uint8_t)((pixel & 0x001F) << 3);

                total_pixels++;

                // Detect RED pawn (Human/O) using defined thresholds
                if (r > COLOR_RED_MIN && r > g + COLOR_DIFF_RED && r > b + COLOR_DIFF_RED) {
                    red_count++;
                }
                // Detect BLUE pawn (Robot/X)
                else if (b > COLOR_BLUE_MIN && b > r + COLOR_DIFF_BLUE && b > g) {
                    blue_count++;
                }
                // Detect GREEN dot (Empty)
                else if (g > COLOR_GREEN_MIN && g > r + COLOR_DIFF_GREEN && g > b + COLOR_DIFF_GREEN) {
                    green_count++;
                }
            }
        }

        // Determine cell state based on pixel counts
        const int threshold = total_pixels / DETECTION_THRESHOLD_DIV;

        const int row = b_i / 5;
        const int col = b_i % 5;

        if (red_count > threshold && red_count > blue_count && red_count > green_count) {
            cameraBoard[row][col] = O;  // RED = Human
        }
        else if (blue_count > threshold && blue_count > red_count && blue_count > green_count) {
            cameraBoard[row][col] = X;  // BLUE = Robot
        }
        else {
            cameraBoard[row][col] = EMPTY;  // GREEN or no dominant color
        }
    }
}

int calc_angle_beta(int i, int j, int isBoard) 
{
    float x, y;
    if (isBoard == 0) 
	{
        x = dis * (i-2);
        y = dis * (4-j);
    } 
	else 
	{
        x = dis * (i+1);
        y = dis * (2-j);
    }
    float distance = sqrt(pow(x, 2) + pow(y, 2));
	int beta = 2 * asin((distance / 2) / length);
    return beta;
}


int calc_angle_alpha(int i, int j, int isBoard) 
{
    float x, y;
    if (isBoard == 0) 
	{
        x = dis * (i-2);
        y = dis * (4-j);
    } 
	else 
	{
        x = dis * (i+1);
        y = dis * (2-j);
    }
    float bigangle = atan2(y, x);
    float distance = sqrt(pow(x, 2) + pow(y, 2));
    float beta = 2 * asin((distance / 2) / length);
    int alpha = bigangle + ((PI - beta) / 2); // Adjust for counterclockwise alpha
    return alpha;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void hang(void)
{
  // Disable PCA Output (PCA is a Active Low device)
  digitalWrite(PCA_ENABLE_PIN_NO, HIGH); // When this pin is low all pins are enabled
  ss_puts("\r\n HANG");

  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextSize(2);
  tft.drawString("    H A N G    ", 0, 0);

  while(1)
  {
    yield();  // Feed watchdog to prevent reset
    ss_puts(" HANG ");
    digitalWrite(LED_RED_INBUILD_PIN_NO, LED_INBUILD_ON);
    delay(500);

    yield();  // Feed watchdog
    ss_puts(" . ");
    digitalWrite(LED_RED_INBUILD_PIN_NO, LED_INBUILD_OFF);
    delay(500);
  }
}
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
void display_camera_fb_t(camera_fb_t *fb) {
    // //////ss_printf("buf: %p\n", fb->buf);
    ss_printf("\r\n len: %zu byte  ", fb->len);
    ss_printf("\r\n width: %zu pixel ", fb->width);
    ss_printf("\r\n height: %zu pixel  ", fb->height);
    //ss_printf("\r\n format: %d  ", fb->format);
    //ss_printf("\r\n timestamp: %lld  ", fb->timestamp);

    // setenv("TZ", "Asia/Kolkata", 1);
    // tzset();

    // // time_t timestamp = (time_t) (fb->timestamp / 1000000ULL);
    // struct tm *tm_info = localtime(&timestamp);
    // char time_str[20];
    // strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    // ss_printf("timestamp: %s\n", time_str);
}
  //////////////////////////////////////////////////////////////////////////////
  void print_SPIFFS_dir_to_serial_port(){ 
  if (SPIFFS_present) { 
    File root = SPIFFS.open("/");
    if (root) {
      root.rewindDirectory();
      ss_puts("\r\nSPIFFS Contents:  ");
      ss_puts("\r\n Type\t NAME\t\t    File/Dir   Size  ");
      printDirectoryToSerialPort("/",0);
      root.close();
    }
    else 
    {
      ss_puts("\r\nNo Files Found  ");
    }
   }
  ss_puts("\r\n");
  uint32_t totalBytes = SPIFFS.totalBytes();
  uint32_t usedBytes = SPIFFS.usedBytes();
  uint32_t freeBytes = totalBytes - usedBytes;
 
  //ss_printf("\r\n Sum of  above :  %-10s \t [%-10u]  ", fileSizeHumanReadable(sum), sum);  
  ss_puts("\r\n");
  ss_printf("\r\n FS Used  space:  %12s \t [%12u]  ", fileSizeHumanReadable(usedBytes), usedBytes);
  ss_printf("\r\n FS Free  space:  %12s \t [%12u]  ", fileSizeHumanReadable(freeBytes), freeBytes);
  ss_puts("\r\n-------------------------------------------------");
  ss_printf("\r\n FS Total space:  %12s \t [%12u]  ", fileSizeHumanReadable(totalBytes), totalBytes);
  ss_puts("\r\n");
   ss_puts("\r\n=================");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void printDirectoryToSerialPort(const char * dirname, uint8_t levels){
  File root = SPIFFS.open(dirname);
  if(!root){
    return;
  }
  if(!root.isDirectory()){
    return;
  }
  File file = root.openNextFile();
  
  while(file){
    
    if(file.isDirectory()){
      ss_printf("\r\n %4s \t %-20s", String(file.isDirectory()?"Dir":"File"), String(file.name() ) );
      printDirectoryToSerialPort(file.name(), levels-1);
    }
    else
    {
      ss_printf("\r\n File \t %-20s", String(file.name()) );
      ss_printf("  %4s", String(file.isDirectory()?"Dir":"File")) ;

      int bytes = file.size();
      ss_printf(" %10s [%u byte]  ", fileSizeHumanReadable(bytes), bytes);
    }
    file = root.openNextFile();
  }
  file.close();
}
///////////////////////////////////////////////////////
String fileSizeHumanReadable(uint32_t bytes)
{
    String fsize = "";
    if (bytes < 1024)                     fsize = String(bytes)+" B";
    else if(bytes < (1024 * 1024))        fsize = String(bytes/1024.0,3)+" KB";
    else if(bytes < (1024 * 1024 * 1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
    else                                  fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
    return fsize;    
}
  //////////////////////////////////////////////////////////////////////////////
  
// void displayPartitionSchemeSPIFFS(void)
// {
//   //ss_printf("\r\n Fash Size:  ");
//   //ss_printf("\r\n %s  ", SPIFFS.getFlashChipSize());
  
//   ss_printf("\r\n Partition Scheme:  ");
//   //ss_printf("\r\n %s  ", SPIFFS.getPartitionScheme());



// //   ss_printf("\r\n Partitions:  ");
// //   File root = SPIFFS.open("/");
// //   File file = root.openNextFile();
// //  // while(file)
// //   {
// //     ss_printf("\r\n %s  ", file.name());
// //     ss_printf("\t\t");
// //     ss_printf("\r\n %s  ", file.size());
// //     file = root.openNextFile();
// //   }

//   uint32_t totalBytes = SPIFFS.totalBytes();
//   uint32_t usedBytes = SPIFFS.usedBytes();
//   uint32_t freeBytes = totalBytes - usedBytes;

//   ss_printf("\r\n Total space: %u  ", totalBytes);
//   ss_printf("\r\n Used space: %u  ", usedBytes);
//   ss_printf("\r\n Free space: %u  ", freeBytes);
//   }
//////////////////////////////////////////////////////////////////////
void renameFile(fs::FS &fs, const char * path1, const char * path2){
    ss_printf("Renaming file %s to %s  : ", path1, path2);
    if (fs.rename(path1, path2)) {
        ss_puts(" Done  ");
    } else {
        ss_puts(" Failed  ");
    }
}
//////////////////////////////////////////////////////////////////////////////
void deleteFile(fs::FS &fs, const char * path){
    ss_printf("\r\n Deleting file: %s  :  ", path);
    if(fs.remove(path)) {
        ss_puts(" Done  ");
    } else {
        ss_puts(" Failed  ");
    }
}
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
void writeImage(const char* path, uint8_t* data, size_t size) {
  File file = SPIFFS.open(path, FILE_WRITE);
  if (!file) {
    ss_puts("\r\n Failed to open file for writing  ");
    return;
  }
  int bytesWritten;
  size_t b;
  for(b=0; b< size; b++){
    bytesWritten = file.write(*data);
    //delayMicroseconds(100);
    if(bytesWritten != 1){ 
      ss_printf("\r\n ERROR : File Write Failed at byte= %d  ", b);
      break;
    }
    data++;
  }
  //delayMicroseconds(100);
  file.close();
  ss_printf("\r\n Written= %u byte  Expected: %u", b, size);
  if(b==size){
    ss_printf("\r\n Written %u bytes sucessfully", b);
  } else {
    ss_printf("\r\nERROR: Saving file: Written= %u byte(s)  Expected to write : %u byte(s)  ", b, size);
    ss_printf("\r\n Deleting in-complete file: %s  ", path);
    deleteFile(SPIFFS, path);

  }


  

}
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// ============================================================
// MODERN WEB INTERFACE - Web Pages
// ============================================================

// Modern Home Page with Dashboard
void HomePage(){
  server.sendHeader("Connection", "close");

  String html;
  html.reserve(6000);
  html += F("<!DOCTYPE html><html><head><meta charset='UTF-8'>");
  html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>TicTacToe Robot</title><style>");
  html += FPSTR(SHARED_CSS);
  html += F(".tool-card{text-align:center;padding:30px 20px}.tool-card svg{width:48px;height:48px;margin-bottom:15px;color:#00d9ff}");
  html += F(".tool-card h3{color:#fff;margin-bottom:8px}.tool-card p{color:#888;font-size:13px;margin-bottom:15px}");
  html += F("</style></head><body><div class='container'>");

  // Header
  html += F("<h1>TicTacToe Robot</h1>");
  html += F("<p class='subtitle'>ESP32-CAM Control Dashboard</p>");

  // Quick Status Card
  html += F("<div class='card'><div class='card-title'>");
  html += F("<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'><circle cx='12' cy='12' r='10'/><path d='M12 6v6l4 2'/></svg>");
  html += F("System Status</div>");
  html += F("<div class='grid grid-2'>");
  html += "<div>WiFi: <span class='tag tag-on'>" + String(wifi_ssid) + "</span></div>";
  html += F("<div>Camera: <span class='tag tag-on'>Ready</span></div>");
  html += F("<div>Servos: <span id='servoSt' class='tag tag-off'>Off</span></div>");
  html += F("<div>SPIFFS: <span class='tag tag-on'>OK</span></div>");
  html += F("</div></div>");

  // Tools Grid
  html += F("<h2>Tools</h2>");
  html += F("<div class='grid grid-2'>");

  // Servo Calibration
  html += F("<div class='card tool-card'>");
  html += F("<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'><circle cx='12' cy='12' r='3'/><path d='M12 1v6m0 6v6m11-7h-6m-6 0H1m15.5-6.5l-4.2 4.2m-2.6 2.6l-4.2 4.2m0-11l4.2 4.2m2.6 2.6l4.2 4.2'/></svg>");
  html += F("<h3>Servo Calibration</h3>");
  html += F("<p>Adjust robot arm positions, heights, and save poses</p>");
  html += F("<a href='/cal' class='btn btn-primary'>Open</a>");
  html += F("</div>");

  // Grid Calibration
  html += F("<div class='card tool-card'>");
  html += F("<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'><rect x='3' y='3' width='7' height='7'/><rect x='14' y='3' width='7' height='7'/><rect x='3' y='14' width='7' height='7'/><rect x='14' y='14' width='7' height='7'/></svg>");
  html += F("<h3>Grid Calibration</h3>");
  html += F("<p>Align camera detection boxes with board positions</p>");
  html += F("<a href='/grid' class='btn btn-primary'>Open</a>");
  html += F("</div>");

  // OTA Update
  html += F("<div class='card tool-card'>");
  html += F("<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'><path d='M21 15v4a2 2 0 01-2 2H5a2 2 0 01-2-2v-4'/><polyline points='17 8 12 3 7 8'/><line x1='12' y1='3' x2='12' y2='15'/></svg>");
  html += F("<h3>OTA Update</h3>");
  html += F("<p>Upload new firmware wirelessly</p>");
  html += F("<a href='/ota' class='btn btn-warning'>Open</a>");
  html += F("</div>");

  // File Manager
  html += F("<div class='card tool-card'>");
  html += F("<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'><path d='M22 19a2 2 0 01-2 2H4a2 2 0 01-2-2V5a2 2 0 012-2h5l2 3h9a2 2 0 012 2z'/></svg>");
  html += F("<h3>File Manager</h3>");
  html += F("<p>Upload, download, and manage SPIFFS files</p>");
  html += F("<a href='/files' class='btn btn-secondary'>Open</a>");
  html += F("</div>");

  // WiFi Settings
  html += F("<div class='card tool-card'>");
  html += F("<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'><path d='M5 12.55a11 11 0 0114 0'/><path d='M1.42 9a16 16 0 0121.16 0'/><path d='M8.53 16.11a6 6 0 016.95 0'/><circle cx='12' cy='20' r='1'/></svg>");
  html += F("<h3>WiFi Settings</h3>");
  html += F("<p>Change WiFi network or view connection status</p>");
  html += F("<a href='/wifi' class='btn btn-secondary'>Open</a>");
  html += F("</div>");

  html += F("</div>"); // End tools grid

  // Quick Actions
  html += F("<h2>Quick Actions</h2>");
  html += F("<div class='card'>");
  html += F("<div class='grid grid-4'>");
  html += F("<button class='btn btn-success btn-sm' onclick='api(\"pca?enable=1\")'>Enable Servos</button>");
  html += F("<button class='btn btn-danger btn-sm' onclick='api(\"pca?enable=0\")'>Disable Servos</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='api(\"goto?name=home\")'>Go Home</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='api(\"goto?name=board\")'>Board View</button>");
  html += F("</div></div>");

  // Footer
  html += F("<div class='divider'></div>");
  html += F("<p style='text-align:center;color:#666;font-size:12px'>TicTacToe Robot v2.0 | ESP32-CAM</p>");

  html += F("</div><script>");
  html += F("async function api(e){await fetch('/api/'+e);refresh()}");
  html += F("async function refresh(){try{const r=await fetch('/api/status');const d=await r.json();");
  html += F("document.getElementById('servoSt').className='tag '+(d.pcaOutputsEnabled?'tag-on':'tag-off');");
  html += F("document.getElementById('servoSt').textContent=d.pcaOutputsEnabled?'On':'Off';}catch(e){}}");
  html += F("refresh();setInterval(refresh,3000);");
  html += F("</script></body></html>");

  server.send(200, "text/html", html);
}

// ============ SERVO CALIBRATION PAGE ============
static void CalibrationPage()
{
  calibrationActive = true;
  calibrationPreviewEnabled = true;

  init_camera_if_needed();
  init_pca_if_needed();
  set_pca_outputs(true);

  move_servo_us(0, current_servo_us[0]);
  move_servo_us(1, current_servo_us[1]);
  move_servo_us(2, current_servo_us[2]);

  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  lcd_row = 240;
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("CALIBRATION MODE", 0, lcd_row - 20);

  server.sendHeader("Connection", "close");

  String html;
  html.reserve(10000);
  html += F("<!DOCTYPE html><html><head><meta charset='UTF-8'>");
  html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Servo Calibration</title><style>");
  html += FPSTR(SHARED_CSS);
  html += F(".servo-row{display:flex;align-items:center;gap:8px;margin:10px 0}");
  html += F(".servo-label{min-width:70px;font-weight:bold;color:#00d9ff}");
  html += F(".servo-val{width:80px;text-align:center}");
  html += F(".board-grid{display:grid;grid-template-columns:repeat(5,1fr);gap:5px;max-width:250px}");
  html += F(".board-grid .btn{padding:10px 5px;font-size:11px}");
  html += F(".rack-grid{display:flex;flex-wrap:wrap;gap:5px}");
  html += F(".rack-grid .btn{min-width:40px}");
  html += F(".toggle-btn{min-width:100px}");
  html += F(".toggle-btn.active{background:linear-gradient(135deg,#00ff88,#00cc6a);color:#000}");
  html += F("</style></head><body><div class='container'>");

  // Header with back button
  html += F("<div class='nav'><a href='/' class='btn btn-secondary btn-sm'>← Home</a>");
  html += F("<a href='/grid' class='btn btn-secondary btn-sm'>Grid Cal</a></div>");
  html += F("<h1>Servo Calibration</h1>");
  html += F("<p class='subtitle'>Adjust robot arm positions and save to flash</p>");

  // Quick Controls Card
  html += F("<div class='card'><div class='card-title'>Quick Controls</div>");
  html += F("<div class='row'>");
  html += F("<button class='btn btn-success btn-sm' onclick='pca(1)'>Enable Servos</button>");
  html += F("<button class='btn btn-danger btn-sm' onclick='pca(0)'>Disable Servos</button>");
  html += F("<button id='emBtn' class='btn btn-secondary btn-sm toggle-btn' onclick='toggleEM()'>Magnet OFF</button>");
  html += F("<button id='laserBtn' class='btn btn-secondary btn-sm toggle-btn' onclick='toggleLaser()'>Laser OFF</button>");
  html += F("</div></div>");

  // Servo Control Card
  html += F("<div class='card'><div class='card-title'>Servo Control</div>");
  html += F("<div class='row'><span class='label'>Step:</span><input type='number' id='step' value='20' class='input servo-val'></div>");

  // Alpha
  html += F("<div class='servo-row'><span class='servo-label'>Alpha:</span>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(0,-10)'>--</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(0,-1)'>-</button>");
  html += "<input type='number' id='s0' value='" + String(current_servo_us[0]) + "' class='input servo-val'>";
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(0,1)'>+</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(0,10)'>++</button></div>");

  // Beta
  html += F("<div class='servo-row'><span class='servo-label'>Beta:</span>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(1,-10)'>--</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(1,-1)'>-</button>");
  html += "<input type='number' id='s1' value='" + String(current_servo_us[1]) + "' class='input servo-val'>";
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(1,1)'>+</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(1,10)'>++</button></div>");

  // Gamma
  html += F("<div class='servo-row'><span class='servo-label'>Gamma:</span>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(2,-10)'>--</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(2,-1)'>-</button>");
  html += "<input type='number' id='s2' value='" + String(current_servo_us[2]) + "' class='input servo-val'>";
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(2,1)'>+</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(2,10)'>++</button></div>");
  html += F("</div>");

  // Poses Card
  html += F("<div class='card'><div class='card-title'>Saved Poses</div>");
  html += F("<div class='grid grid-2'>");
  html += F("<div><h3>Home Position</h3>");
  html += F("<button class='btn btn-primary btn-sm' onclick=\"goTo('home')\">Go to Home</button> ");
  html += F("<button class='btn btn-success btn-sm' onclick=\"savePose('home')\">Save Current</button></div>");
  html += F("<div><h3>Board View</h3>");
  html += F("<button class='btn btn-primary btn-sm' onclick=\"goTo('board')\">Go to Board</button> ");
  html += F("<button class='btn btn-success btn-sm' onclick=\"savePose('board')\">Save Current</button></div>");
  html += F("</div></div>");

  // Heights Card
  html += F("<div class='card'><div class='card-title'>Heights (Gamma)</div>");
  html += F("<div class='servo-row'><span class='servo-label'>Hover:</span>");
  html += "<input type='number' id='hHover' value='" + String(gamma_hover) + "' class='input servo-val'>";
  html += F("<button class='btn btn-secondary btn-sm' onclick=\"testH('hover')\">Test</button>");
  html += F("<button class='btn btn-success btn-sm' onclick=\"saveH('hover')\">Save</button></div>");

  html += F("<div class='servo-row'><span class='servo-label'>Pickup:</span>");
  html += "<input type='number' id='hPickup' value='" + String(gamma_pickup) + "' class='input servo-val'>";
  html += F("<button class='btn btn-secondary btn-sm' onclick=\"testH('pickup')\">Test</button>");
  html += F("<button class='btn btn-success btn-sm' onclick=\"saveH('pickup')\">Save</button></div>");

  html += F("<div class='servo-row'><span class='servo-label'>Place:</span>");
  html += "<input type='number' id='hPlace' value='" + String(gamma_place) + "' class='input servo-val'>";
  html += F("<button class='btn btn-secondary btn-sm' onclick=\"testH('place')\">Test</button>");
  html += F("<button class='btn btn-success btn-sm' onclick=\"saveH('place')\">Save</button></div>");
  html += F("</div>");

  // Board Calibration Card
  html += F("<div class='card'><div class='card-title'>Board Positions (5x5)</div>");
  html += F("<p style='color:#888;font-size:13px;margin-bottom:10px'>Click cell → adjust servos → Save</p>");
  html += F("<div class='board-grid'>");
  for (int r = 0; r < 5; r++) {
    for (int c = 0; c < 5; c++) {
      html += "<button class='btn btn-secondary' onclick='goCell(" + String(r) + "," + String(c) + ")'>" + String(r) + "," + String(c) + "</button>";
    }
  }
  html += F("</div>");
  html += F("<div class='row' style='margin-top:10px'>Cell: <span id='cellInfo' style='color:#00d9ff;font-weight:bold'>-</span>");
  html += F("<button class='btn btn-success btn-sm' onclick='saveCell()'>Save Cell</button></div>");
  html += F("</div>");

  // Rack Calibration Card
  html += F("<div class='card'><div class='card-title'>Rack Slots (12)</div>");
  html += F("<div class='rack-grid'>");
  for (int i = 0; i < 12; i++) {
    html += "<button class='btn btn-secondary btn-sm' onclick='goRack(" + String(i) + ")'>" + String(i) + "</button>";
  }
  html += F("</div>");
  html += F("<div class='row' style='margin-top:10px'>Slot: <span id='slotInfo' style='color:#00d9ff;font-weight:bold'>-</span>");
  html += F("<button class='btn btn-success btn-sm' onclick='saveRack()'>Save Slot</button></div>");
  html += F("</div>");

  // Save & Export Card
  html += F("<div class='card'><div class='card-title'>Save & Export</div>");
  html += F("<div class='row'>");
  html += F("<button class='btn btn-warning' onclick='saveAll()'>Save All to Flash</button>");
  html += F("<button class='btn btn-secondary' onclick='exportCode()'>Export as Code</button>");
  html += F("</div></div>");

  // Status Display
  html += F("<div class='status' id='st'>Loading...</div>");

  html += F("</div><script>");
  html += F("var curCell=[-1,-1],curSlot=-1;");
  html += F("async function api(e){const r=await fetch('/api/'+e);return await r.json();}");

  html += F("async function refresh(){try{const s=await api('status');");
  html += F("document.getElementById('st').innerHTML='<b>Status:</b> Alpha:'+s.current[0]+' | Beta:'+s.current[1]+' | Gamma:'+s.current[2]+' | PCA:'+(s.pcaOutputsEnabled?'<span style=\"color:#0f8\">ON</span>':'OFF')+' | EM:'+(s.em?'ON':'OFF')+' | Laser:'+(s.laser?'ON':'OFF');");
  html += F("document.getElementById('s0').value=s.current[0];document.getElementById('s1').value=s.current[1];document.getElementById('s2').value=s.current[2];");
  html += F("var emBtn=document.getElementById('emBtn');emBtn.textContent='Magnet '+(s.em?'ON':'OFF');emBtn.classList.toggle('active',s.em);");
  html += F("var laserBtn=document.getElementById('laserBtn');laserBtn.textContent='Laser '+(s.laser?'ON':'OFF');laserBtn.classList.toggle('active',s.laser);");
  html += F("if(s.heights){document.getElementById('hHover').value=s.heights[0];document.getElementById('hPickup').value=s.heights[1];document.getElementById('hPlace').value=s.heights[2];}}catch(e){}}");

  html += F("function getStep(){return parseInt(document.getElementById('step').value)||20;}");
  html += F("async function adj(ch,dir){var el=document.getElementById('s'+ch);var v=parseInt(el.value)+dir*getStep();el.value=v;await api('servo?ch='+ch+'&us='+v);refresh();}");
  html += F("async function pca(en){await api('pca?enable='+en);refresh();}");
  html += F("async function toggleEM(){await api('em?toggle=1');refresh();}");
  html += F("async function toggleLaser(){await api('laser?toggle=1');refresh();}");
  html += F("async function goTo(name){await api('goto?name='+name);refresh();}");
  html += F("async function savePose(name){await api('pose/save?name='+name);alert('Saved '+name+'!');refresh();}");
  html += F("async function testH(t){var id='h'+t.charAt(0).toUpperCase()+t.slice(1);var v=document.getElementById(id).value;await api('servo?ch=2&us='+v);refresh();}");
  html += F("async function saveH(t){var id='h'+t.charAt(0).toUpperCase()+t.slice(1);var v=document.getElementById(id).value;await api('height?type='+t+'&value='+v);alert('Saved '+t+'!');refresh();}");
  html += F("async function goCell(r,c){curCell=[r,c];document.getElementById('cellInfo').textContent=r+','+c;await api('gotoBoard?row='+r+'&col='+c);refresh();}");
  html += F("async function saveCell(){if(curCell[0]<0)return alert('Select a cell first');await api('saveBoard?row='+curCell[0]+'&col='+curCell[1]);alert('Saved cell '+curCell[0]+','+curCell[1]);}");
  html += F("async function goRack(s){curSlot=s;document.getElementById('slotInfo').textContent=s;await api('gotoRack?slot='+s);refresh();}");
  html += F("async function saveRack(){if(curSlot<0)return alert('Select a slot first');await api('saveRack?slot='+curSlot);alert('Saved slot '+curSlot);}");
  html += F("async function saveAll(){await api('saveAll');alert('All settings saved to flash!');}");
  html += F("async function exportCode(){const r=await fetch('/api/export');const t=await r.text();navigator.clipboard.writeText(t).then(()=>alert('Code copied to clipboard!')).catch(()=>prompt('Copy this code:',t));}");
  html += F("refresh();setInterval(refresh,2000);");
  html += F("</script></body></html>");

  server.send(200, "text/html", html);
}

// ============ GRID CALIBRATION PAGE ============
static void GridCalibrationPage()
{
  gridCalibrationMode = true;

  init_camera_if_needed();
  init_pca_if_needed();
  set_pca_outputs(true);

  // Move to board view
  move_servo_us(2, board_view_gamma);
  delay(200);
  move_servo_us(0, board_view_alpha);
  delay(200);
  move_servo_us(1, board_view_beta);

  server.sendHeader("Connection", "close");

  String html;
  html.reserve(5000);
  html += F("<!DOCTYPE html><html><head><meta charset='UTF-8'>");
  html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Grid Calibration</title><style>");
  html += FPSTR(SHARED_CSS);
  html += F(".param-row{display:flex;align-items:center;gap:8px;margin:12px 0;flex-wrap:wrap}");
  html += F(".param-label{min-width:100px;font-weight:600;color:#00d9ff}");
  html += F(".param-val{width:70px;text-align:center}");
  html += F("</style></head><body><div class='container'>");

  // Header with navigation
  html += F("<div class='nav'><a href='/' class='btn btn-secondary btn-sm'>← Home</a>");
  html += F("<a href='/cal' class='btn btn-secondary btn-sm'>Servo Cal</a></div>");
  html += F("<h1>Grid Calibration</h1>");
  html += F("<p class='subtitle'>Align detection boxes with board positions (watch TFT display)</p>");

  // Instructions Card
  html += F("<div class='card'><div class='card-title'>Instructions</div>");
  html += F("<ol style='color:#aaa;font-size:14px;padding-left:20px;line-height:1.8'>");
  html += F("<li>Look at the TFT display - you'll see green rectangles over the camera feed</li>");
  html += F("<li>Adjust <b>Start X/Y</b> to move all boxes together</li>");
  html += F("<li>Adjust <b>Spacing</b> to match distance between dots</li>");
  html += F("<li>Adjust <b>Box Size</b> to cover each dot properly</li>");
  html += F("<li>Click <b>Save to Flash</b> when aligned</li></ol></div>");

  // Parameters Card
  html += F("<div class='card'><div class='card-title'>Grid Parameters</div>");

  // Start X
  html += F("<div class='param-row'><span class='param-label'>Start X:</span>");
  html += "<input type='number' id='sx' value='" + String(roi_start_x) + "' class='input param-val'>";
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"sx\",-5)'>-5</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"sx\",-1)'>-1</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"sx\",1)'>+1</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"sx\",5)'>+5</button></div>");

  // Start Y
  html += F("<div class='param-row'><span class='param-label'>Start Y:</span>");
  html += "<input type='number' id='sy' value='" + String(roi_start_y) + "' class='input param-val'>";
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"sy\",-5)'>-5</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"sy\",-1)'>-1</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"sy\",1)'>+1</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"sy\",5)'>+5</button></div>");

  // Spacing X
  html += F("<div class='param-row'><span class='param-label'>Spacing X:</span>");
  html += "<input type='number' id='spx' value='" + String(roi_spacing_x) + "' class='input param-val'>";
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"spx\",-2)'>-2</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"spx\",-1)'>-1</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"spx\",1)'>+1</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"spx\",2)'>+2</button></div>");

  // Spacing Y
  html += F("<div class='param-row'><span class='param-label'>Spacing Y:</span>");
  html += "<input type='number' id='spy' value='" + String(roi_spacing_y) + "' class='input param-val'>";
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"spy\",-2)'>-2</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"spy\",-1)'>-1</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"spy\",1)'>+1</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"spy\",2)'>+2</button></div>");

  // Box Size
  html += F("<div class='param-row'><span class='param-label'>Box Size:</span>");
  html += "<input type='number' id='sz' value='" + String(roi_size) + "' class='input param-val'>";
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"sz\",-2)'>-2</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"sz\",-1)'>-1</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"sz\",1)'>+1</button>");
  html += F("<button class='btn btn-secondary btn-sm' onclick='adj(\"sz\",2)'>+2</button></div>");
  html += F("</div>");

  // Actions Card
  html += F("<div class='card'><div class='card-title'>Actions</div>");
  html += F("<div class='row'>");
  html += F("<button class='btn btn-primary' onclick='apply()'>Apply Changes</button>");
  html += F("<button class='btn btn-success' onclick='save()'>Save to Flash</button>");
  html += F("<button class='btn btn-secondary' onclick='location.reload()'>Reset Page</button>");
  html += F("</div></div>");

  // Status Display
  html += "<div class='status' id='st'>Current: StartX=" + String(roi_start_x);
  html += " | StartY=" + String(roi_start_y);
  html += " | SpacingX=" + String(roi_spacing_x);
  html += " | SpacingY=" + String(roi_spacing_y);
  html += " | Size=" + String(roi_size) + "</div>";

  html += F("</div><script>");
  html += F("function adj(id,d){document.getElementById(id).value=parseInt(document.getElementById(id).value)+d;apply();}");
  html += F("async function apply(){");
  html += F("const p={sx:document.getElementById('sx').value,sy:document.getElementById('sy').value,");
  html += F("spx:document.getElementById('spx').value,spy:document.getElementById('spy').value,sz:document.getElementById('sz').value};");
  html += F("const r=await fetch('/api/grid?sx='+p.sx+'&sy='+p.sy+'&spx='+p.spx+'&spy='+p.spy+'&sz='+p.sz);");
  html += F("const j=await r.json();");
  html += F("document.getElementById('st').innerHTML='<b>Applied:</b> StartX='+j.sx+' | StartY='+j.sy+' | SpacingX='+j.spx+' | SpacingY='+j.spy+' | Size='+j.sz;}");
  html += F("async function save(){await fetch('/api/grid/save');alert('Grid calibration saved to flash!');}");
  html += F("</script></body></html>");

  server.send(200, "text/html", html);
}

// ============ OTA UPDATE PAGE ============
static void OTAPage()
{
  server.sendHeader("Connection", "close");

  String html;
  html.reserve(4000);
  html += F("<!DOCTYPE html><html><head><meta charset='UTF-8'>");
  html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>OTA Update</title><style>");
  html += FPSTR(SHARED_CSS);
  html += F(".upload-zone{border:2px dashed rgba(255,255,255,0.3);border-radius:15px;padding:40px;text-align:center;margin:20px 0;transition:all 0.3s}");
  html += F(".upload-zone:hover{border-color:#00d9ff;background:rgba(0,217,255,0.1)}");
  html += F(".upload-icon{width:64px;height:64px;margin:0 auto 15px;color:#00d9ff}");
  html += F(".progress-wrap{background:rgba(0,0,0,0.4);border-radius:10px;height:30px;overflow:hidden;margin:20px 0}");
  html += F(".progress-bar{height:100%;background:linear-gradient(90deg,#00d9ff,#00ff88);width:0%;transition:width 0.3s;display:flex;align-items:center;justify-content:center;font-weight:bold;color:#000}");
  html += F("</style></head><body><div class='container'>");

  // Header with nav
  html += F("<div class='nav'><a href='/' class='btn btn-secondary btn-sm'>← Home</a></div>");
  html += F("<h1>OTA Firmware Update</h1>");
  html += F("<p class='subtitle'>Upload new firmware wirelessly</p>");

  // Warning Card
  html += F("<div class='card' style='border-color:#ffd93d'>");
  html += F("<div class='card-title' style='color:#ffd93d'>");
  html += F("<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' style='width:24px;height:24px'><path d='M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z'/></svg>");
  html += F("Important</div>");
  html += F("<p style='color:#aaa;font-size:14px'>Do not power off or disconnect during update. The device will automatically restart after upload completes.</p>");
  html += F("</div>");

  // Upload Card
  html += F("<div class='card'><div class='card-title'>Upload Firmware</div>");
  html += F("<form id='uploadForm' enctype='multipart/form-data'>");
  html += F("<div class='upload-zone' onclick='document.getElementById(\"fileInput\").click()'>");
  html += F("<svg class='upload-icon' viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2'><path d='M21 15v4a2 2 0 01-2 2H5a2 2 0 01-2-2v-4'/><polyline points='17 8 12 3 7 8'/><line x1='12' y1='3' x2='12' y2='15'/></svg>");
  html += F("<p style='color:#888;margin-bottom:10px'>Click to select or drag .bin file here</p>");
  html += F("<p id='fileName' style='color:#00d9ff;font-weight:bold'>No file selected</p>");
  html += F("</div>");
  html += F("<input type='file' id='fileInput' name='update' accept='.bin' style='display:none' onchange='fileSelected(this)'>");
  html += F("<div class='progress-wrap'><div class='progress-bar' id='progressBar'>0%</div></div>");
  html += F("<p id='status' class='status'>Select a .bin firmware file and click Upload</p>");
  html += F("<div class='row' style='justify-content:center'>");
  html += F("<button type='button' class='btn btn-warning btn-lg' onclick='startUpload()'>Upload Firmware</button>");
  html += F("</div></form></div>");

  // Info Card
  html += F("<div class='card'><div class='card-title'>Info</div>");
  html += F("<ul style='color:#aaa;font-size:14px;padding-left:20px;line-height:1.8'>");
  html += F("<li>Compile your sketch as <b>.bin</b> in Arduino IDE (Sketch → Export Compiled Binary)</li>");
  html += F("<li>File size limit: ~1.5MB (depends on partition scheme)</li>");
  html += F("<li>After successful upload, page will automatically redirect to home</li></ul></div>");

  html += F("</div><script>");
  html += F("function fileSelected(input){");
  html += F("if(input.files.length>0){");
  html += F("document.getElementById('fileName').textContent=input.files[0].name+' ('+Math.round(input.files[0].size/1024)+' KB)';");
  html += F("document.getElementById('status').textContent='File ready. Click Upload to start.';}}");
  html += F("function startUpload(){");
  html += F("var fileInput=document.getElementById('fileInput');");
  html += F("if(!fileInput.files.length){alert('Please select a file first!');return;}");
  html += F("var formData=new FormData();formData.append('update',fileInput.files[0]);");
  html += F("var xhr=new XMLHttpRequest();xhr.open('POST','/update',true);");
  html += F("xhr.upload.onprogress=function(e){if(e.lengthComputable){");
  html += F("var pct=Math.round(e.loaded/e.total*100);");
  html += F("document.getElementById('progressBar').style.width=pct+'%';");
  html += F("document.getElementById('progressBar').textContent=pct+'%';");
  html += F("document.getElementById('status').textContent='Uploading: '+pct+'%';}};");
  html += F("xhr.onload=function(){if(xhr.status==200){");
  html += F("document.getElementById('status').textContent='Upload complete! Rebooting...';");
  html += F("document.getElementById('progressBar').style.background='linear-gradient(90deg,#00ff88,#00cc6a)';");
  html += F("setTimeout(function(){window.location.href='/';},5000);");
  html += F("}else{document.getElementById('status').textContent='Upload failed! Error: '+xhr.status;}};");
  html += F("xhr.onerror=function(){document.getElementById('status').textContent='Upload failed! Connection error.';};");
  html += F("document.getElementById('status').textContent='Starting upload...';xhr.send(formData);}");
  html += F("</script></body></html>");

  server.send(200, "text/html", html);
}

// ============ FILE MANAGER PAGE ============
static void FileManagerPage()
{
  server.sendHeader("Connection", "close");

  String html;
  html.reserve(5000);
  html += F("<!DOCTYPE html><html><head><meta charset='UTF-8'>");
  html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>File Manager</title><style>");
  html += FPSTR(SHARED_CSS);
  html += F(".file-list{max-height:400px;overflow-y:auto}");
  html += F(".file-item{display:flex;align-items:center;justify-content:space-between;padding:12px;border-bottom:1px solid rgba(255,255,255,0.1);transition:background 0.2s}");
  html += F(".file-item:hover{background:rgba(255,255,255,0.05)}");
  html += F(".file-name{color:#00d9ff;font-weight:500;display:flex;align-items:center;gap:10px}");
  html += F(".file-size{color:#888;font-size:13px}");
  html += F(".file-actions{display:flex;gap:8px}");
  html += F(".upload-section{border:2px dashed rgba(255,255,255,0.2);border-radius:10px;padding:25px;text-align:center;margin:15px 0}");
  html += F("</style></head><body><div class='container'>");

  // Header with nav
  html += F("<div class='nav'><a href='/' class='btn btn-secondary btn-sm'>← Home</a></div>");
  html += F("<h1>File Manager</h1>");
  html += F("<p class='subtitle'>Manage SPIFFS storage files</p>");

  // Storage Info Card
  html += F("<div class='card'><div class='card-title'>");
  html += F("<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' style='width:24px;height:24px'><circle cx='12' cy='12' r='10'/><path d='M12 6v6l4 2'/></svg>");
  html += F("Storage</div>");
  html += F("<div id='storageInfo' class='row' style='color:#888'>Loading...</div></div>");

  // Files List Card
  html += F("<div class='card'><div class='card-title'>");
  html += F("<svg viewBox='0 0 24 24' fill='none' stroke='currentColor' stroke-width='2' style='width:24px;height:24px'><path d='M22 19a2 2 0 01-2 2H4a2 2 0 01-2-2V5a2 2 0 012-2h5l2 3h9a2 2 0 012 2z'/></svg>");
  html += F("Files</div>");
  html += F("<div class='file-list' id='fileList'>Loading files...</div></div>");

  // Upload Card
  html += F("<div class='card'><div class='card-title'>Upload File</div>");
  html += F("<form id='uploadForm' enctype='multipart/form-data' class='upload-section'>");
  html += F("<input type='file' id='fileInput' name='file' style='margin-bottom:15px'><br>");
  html += F("<button type='submit' class='btn btn-success'>Upload File</button>");
  html += F("</form></div>");

  // Status
  html += F("<div class='status' id='status'>Ready</div>");

  html += F("</div><script>");
  // Load files function
  html += F("async function loadFiles(){");
  html += F("try{const r=await fetch('/dir');const t=await r.text();");
  html += F("const lines=t.trim().split('\\n').filter(l=>l.length>0);");
  html += F("let html='';let total=0;let used=0;");
  html += F("for(let line of lines){");
  html += F("if(line.startsWith('SPIFFS')){const m=line.match(/Used:(\\d+).*Total:(\\d+)/);");
  html += F("if(m){used=parseInt(m[1]);total=parseInt(m[2]);}}");
  html += F("else if(line.includes(':')){");
  html += F("const parts=line.split(':');const name=parts[0].trim();const size=parts[1]?parts[1].trim():'';");
  html += F("html+='<div class=\"file-item\"><div class=\"file-name\">';");
  html += F("html+='<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" style=\"width:18px;height:18px\"><path d=\"M13 2H6a2 2 0 00-2 2v16a2 2 0 002 2h12a2 2 0 002-2V9z\"/><polyline points=\"13 2 13 9 20 9\"/></svg>';");
  html += F("html+=name+'</div><div class=\"file-size\">'+size+'</div><div class=\"file-actions\">';");
  html += F("html+='<button class=\"btn btn-primary btn-sm\" onclick=\"download(\\''+name+'\\')\">");
  html += F("<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" style=\"width:14px;height:14px\"><path d=\"M21 15v4a2 2 0 01-2 2H5a2 2 0 01-2-2v-4\"/><polyline points=\"7 10 12 15 17 10\"/><line x1=\"12\" y1=\"15\" x2=\"12\" y2=\"3\"/></svg></button>';");
  html += F("html+='<button class=\"btn btn-danger btn-sm\" onclick=\"del(\\''+name+'\\')\">");
  html += F("<svg viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" style=\"width:14px;height:14px\"><polyline points=\"3 6 5 6 21 6\"/><path d=\"M19 6v14a2 2 0 01-2 2H7a2 2 0 01-2-2V6m3 0V4a2 2 0 012-2h4a2 2 0 012 2v2\"/></svg></button>';");
  html += F("html+='</div></div>';}}}");
  html += F("document.getElementById('fileList').innerHTML=html||'<p style=\"color:#888;padding:20px\">No files found</p>';");
  html += F("if(total>0){const pct=Math.round(used/total*100);");
  html += F("document.getElementById('storageInfo').innerHTML='Used: '+Math.round(used/1024)+' KB / '+Math.round(total/1024)+' KB ('+pct+'%)';}");
  html += F("}catch(e){document.getElementById('fileList').innerHTML='<p style=\"color:#ff6b6b\">Error loading files</p>';}}");
  // Download function
  html += F("function download(name){window.location.href='/download?file='+encodeURIComponent(name);}");
  // Delete function
  html += F("async function del(name){if(!confirm('Delete '+name+'?'))return;");
  html += F("document.getElementById('status').textContent='Deleting...';");
  html += F("await fetch('/delete?file='+encodeURIComponent(name));");
  html += F("document.getElementById('status').textContent='Deleted: '+name;loadFiles();}");
  // Upload form handler
  html += F("document.getElementById('uploadForm').onsubmit=async function(e){");
  html += F("e.preventDefault();const f=document.getElementById('fileInput').files[0];");
  html += F("if(!f){alert('Select a file first');return;}");
  html += F("document.getElementById('status').textContent='Uploading '+f.name+'...';");
  html += F("const fd=new FormData();fd.append('file',f);");
  html += F("await fetch('/fupload',{method:'POST',body:fd});");
  html += F("document.getElementById('status').textContent='Uploaded: '+f.name;");
  html += F("document.getElementById('fileInput').value='';loadFiles();};");
  html += F("loadFiles();");
  html += F("</script></body></html>");

  server.send(200, "text/html", html);
}

static void ApiGridUpdate()
{
  if (server.hasArg("sx")) roi_start_x = server.arg("sx").toInt();
  if (server.hasArg("sy")) roi_start_y = server.arg("sy").toInt();
  if (server.hasArg("spx")) roi_spacing_x = server.arg("spx").toInt();
  if (server.hasArg("spy")) roi_spacing_y = server.arg("spy").toInt();
  if (server.hasArg("sz")) roi_size = server.arg("sz").toInt();

  // Recalculate ROIs with new values
  recalculateROIs();

  String json = "{";
  json += "\"sx\":" + String(roi_start_x) + ",";
  json += "\"sy\":" + String(roi_start_y) + ",";
  json += "\"spx\":" + String(roi_spacing_x) + ",";
  json += "\"spy\":" + String(roi_spacing_y) + ",";
  json += "\"sz\":" + String(roi_size);
  json += "}";
  server.send(200, "application/json", json);
}

static void ApiGridSave()
{
  saveGridCalibration();
  server.send(200, "application/json", "{\"ok\":1}");
}

// EM Control API
static void ApiEM()
{
  init_pca_if_needed();
  if (server.hasArg("toggle")) {
    setEM(!emOn);
  } else if (server.hasArg("on")) {
    setEM(server.arg("on").toInt() != 0);
  }
  server.send(200, "application/json", "{\"em\":" + String(emOn ? 1 : 0) + "}");
}

// Laser Control API
static void ApiLaser()
{
  init_pca_if_needed();
  if (server.hasArg("toggle")) {
    setLaser(!laserOn);
  } else if (server.hasArg("on")) {
    setLaser(server.arg("on").toInt() != 0);
  }
  server.send(200, "application/json", "{\"laser\":" + String(laserOn ? 1 : 0) + "}");
}

// Height Save API
static void ApiHeight()
{
  String type = server.hasArg("type") ? server.arg("type") : "";
  int value = server.hasArg("value") ? server.arg("value").toInt() : 0;

  if (type == "hover") gamma_hover = value;
  else if (type == "pickup") gamma_pickup = value;
  else if (type == "place") gamma_place = value;

  server.send(200, "application/json", "{\"ok\":1}");
}

// Go to board cell API
static void ApiGotoBoard()
{
  int row = server.hasArg("row") ? server.arg("row").toInt() : -1;
  int col = server.hasArg("col") ? server.arg("col").toInt() : -1;

  if (row >= 0 && row < 5 && col >= 0 && col < 5) {
    calCellRow = row;
    calCellCol = col;

    init_pca_if_needed();
    set_pca_outputs(true);

    // Move to hover first
    move_servo_us(2, gamma_hover);
    delay(300);
    // Move to board position
    move_servo_us(0, board_alpha_freq[row][col]);
    delay(300);
    move_servo_us(1, board_beta_freq[row][col]);
    delay(300);
    // Lower to place height
    move_servo_us(2, gamma_place);
  }
  server.send(200, "application/json", "{\"row\":" + String(row) + ",\"col\":" + String(col) + "}");
}

// Save board cell API
static void ApiSaveBoard()
{
  int row = server.hasArg("row") ? server.arg("row").toInt() : -1;
  int col = server.hasArg("col") ? server.arg("col").toInt() : -1;

  if (row >= 0 && row < 5 && col >= 0 && col < 5) {
    // Note: board_alpha_freq and board_beta_freq are const, so we can't modify them directly
    // For now, just acknowledge - user will need to export and paste the values
  }
  server.send(200, "application/json", "{\"ok\":1,\"alpha\":" + String(current_servo_us[0]) + ",\"beta\":" + String(current_servo_us[1]) + "}");
}

// Go to rack slot API
static void ApiGotoRack()
{
  int slot = server.hasArg("slot") ? server.arg("slot").toInt() : -1;

  if (slot >= 0 && slot < 12) {
    calRackSlot = slot;

    init_pca_if_needed();
    set_pca_outputs(true);

    // Move to hover first
    move_servo_us(2, gamma_hover);
    delay(300);
    // Move to rack position
    move_servo_us(0, rack_freq[slot][0]);
    delay(300);
    move_servo_us(1, rack_freq[slot][1]);
    delay(300);
    // Lower to pickup height
    move_servo_us(2, gamma_pickup);
  }
  server.send(200, "application/json", "{\"slot\":" + String(slot) + "}");
}

// Save rack slot API
static void ApiSaveRack()
{
  int slot = server.hasArg("slot") ? server.arg("slot").toInt() : -1;

  if (slot >= 0 && slot < 12) {
    // Note: rack_freq is const, so we can't modify directly
    // For now, just acknowledge - user will need to export and paste the values
  }
  server.send(200, "application/json", "{\"ok\":1,\"alpha\":" + String(current_servo_us[0]) + ",\"beta\":" + String(current_servo_us[1]) + "}");
}

// Save all calibration to flash
static void ApiSaveAll()
{
  savePosesToFS();
  saveGridCalibration();
  server.send(200, "application/json", "{\"ok\":1}");
}

// Export calibration as code
static void ApiExport()
{
  String code = "// Calibration Export\\n";
  code += "home_alpha=" + String(home_alpha) + ";\\n";
  code += "home_beta=" + String(home_beta) + ";\\n";
  code += "home_gamma=" + String(home_gamma) + ";\\n";
  code += "board_view_alpha=" + String(board_view_alpha) + ";\\n";
  code += "board_view_beta=" + String(board_view_beta) + ";\\n";
  code += "board_view_gamma=" + String(board_view_gamma) + ";\\n";
  code += "gamma_hover=" + String(gamma_hover) + ";\\n";
  code += "gamma_pickup=" + String(gamma_pickup) + ";\\n";
  code += "gamma_place=" + String(gamma_place) + ";\\n";
  server.send(200, "text/plain", code);
}

static void ApiStatus()
{
  String json;
  json.reserve(400);
  json += "{";
  json += "\"pcaOutputsEnabled\":" + String(pcaOutputsEnabled ? 1 : 0) + ",";
  json += "\"previewEnabled\":" + String(calibrationPreviewEnabled ? 1 : 0) + ",";
  json += "\"em\":" + String(emOn ? 1 : 0) + ",";
  json += "\"laser\":" + String(laserOn ? 1 : 0) + ",";
  json += "\"current\":[" + String(current_servo_us[0]) + "," + String(current_servo_us[1]) + "," + String(current_servo_us[2]) + "],";
  json += "\"home\":[" + String(home_alpha) + "," + String(home_beta) + "," + String(home_gamma) + "],";
  json += "\"board\":[" + String(board_view_alpha) + "," + String(board_view_beta) + "," + String(board_view_gamma) + "],";
  json += "\"heights\":[" + String(gamma_hover) + "," + String(gamma_pickup) + "," + String(gamma_place) + "]";
  json += "}";
  server.send(200, "application/json", json);
}

static void ApiServo()
{
  if (!init_pca_if_needed()) {
    server.send(500, "application/json", "{\"ok\":0,\"err\":\"pca_init\"}");
    return;
  }

  int ch = server.hasArg("ch") ? server.arg("ch").toInt() : -1;
  int us = server.hasArg("us") ? server.arg("us").toInt() : 0;
  if (ch < 0 || ch > 2) {
    server.send(400, "application/json", "{\"ok\":0,\"err\":\"bad_ch\"}");
    return;
  }
  if (us <= 0) {
    server.send(400, "application/json", "{\"ok\":0,\"err\":\"bad_us\"}");
    return;
  }
  move_servo_us((uint8_t)ch, us);
  ApiStatus();
}

static void ApiPcaEnable()
{
  if (!init_pca_if_needed()) {
    server.send(500, "application/json", "{\"ok\":0,\"err\":\"pca_init\"}");
    return;
  }
  int en = server.hasArg("enable") ? server.arg("enable").toInt() : 0;
  set_pca_outputs(en != 0);
  ApiStatus();
}

static void ApiSavePose()
{
  String name = server.hasArg("name") ? server.arg("name") : "";
  name.toLowerCase();
  if (name == "home")
  {
    home_alpha = current_servo_us[0];
    home_beta  = current_servo_us[1];
    home_gamma = current_servo_us[2];
  }
  else if (name == "board")
  {
    board_view_alpha = current_servo_us[0];
    board_view_beta  = current_servo_us[1];
    board_view_gamma = current_servo_us[2];
  }
  else
  {
    server.send(400, "application/json", "{\"ok\":0,\"err\":\"bad_name\"}");
    return;
  }

  savePosesToFS();
  ApiStatus();
}

static void ApiLoadPoses()
{
  loadPosesFromFS();
  // Reset current to HOME after loading
  current_servo_us[0] = constrain(home_alpha, USMIN, USMAX);
  current_servo_us[1] = constrain(home_beta, USMIN, USMAX);
  current_servo_us[2] = constrain(home_gamma, USMIN, USMAX);
  ApiStatus();
}

static void ApiGoto()
{
  String name = server.hasArg("name") ? server.arg("name") : "";
  name.toLowerCase();

  if (!init_pca_if_needed()) {
    server.send(500, "application/json", "{\"ok\":0,\"err\":\"pca_init\"}");
    return;
  }
  set_pca_outputs(true);

  if (name == "home")
  {
    move_servo_us(0, home_alpha);
    delay(250);
    move_servo_us(1, home_beta);
    delay(250);
    move_servo_us(2, home_gamma);
  }
  else if (name == "board")
  {
    move_servo_us(2, board_view_gamma);
    delay(250);
    move_servo_us(0, board_view_alpha);
    delay(250);
    move_servo_us(1, board_view_beta);
  }
  else
  {
    server.send(400, "application/json", "{\"ok\":0,\"err\":\"bad_name\"}");
    return;
  }

  ApiStatus();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Download(){ // This gets called twice, the first pass selects the input, the second pass then processes the command line arguments
  if (server.args() > 0 ) { // Arguments were received
    if (server.hasArg("download")) DownloadFile(server.arg(0));
  }
  else SelectInput("Enter filename to download","download","download");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void DownloadFile(String filename){
  if (SPIFFS_present) { 
    File download = SPIFFS.open("/"+filename,  "r");
    if (download) {
      server.sendHeader("Content-Type", "text/text");
      server.sendHeader("Content-Disposition", "attachment; filename="+filename);
      server.sendHeader("Connection", "close");
      server.streamFile(download, "application/octet-stream");
      download.close();
    } else ReportFileNotPresent("download"); 
  } else ReportSPIFFSNotPresent();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Upload(){
  append_page_header();
  webpage += F("<h3>Select File to Upload</h3>"); 
  webpage += F("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");
  webpage += F("<input class='buttons' style='width:40%' type='file' name='fupload' id = 'fupload' value=''><br>");
  webpage += F("<br><button class='buttons' style='width:10%' type='submit'>Upload File</button><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  server.send(200, "text/html",webpage);
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
File UploadFile; 
void handleFileUpload(){ // upload a new file to the Filing system
  HTTPUpload& uploadfile = server.upload(); // See https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer/srcv
                                            // For further information on 'status' structure, there are other reasons such as a failed transfer that could be used
  if(uploadfile.status == UPLOAD_FILE_START)
  {
    String filename = uploadfile.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    ss_printf("\r\n Upload File Name: %s", filename);
    SPIFFS.remove(filename);                  // Remove a previous version, otherwise data is appended the file again
    UploadFile = SPIFFS.open(filename, "w");  // Open the file for writing in SPIFFS (create it, if doesn't exist)
  }
  else if (uploadfile.status == UPLOAD_FILE_WRITE)
  {
    if(UploadFile) UploadFile.write(uploadfile.buf, uploadfile.currentSize); // Write the received bytes to the file
  } 
  else if (uploadfile.status == UPLOAD_FILE_END)
  {
    if(UploadFile)          // If the file was successfully created
    {                                    
      UploadFile.close();   // Close the file again
      ss_printf("\r\n Upload Size: %d", uploadfile.totalSize);
      webpage = "";
      append_page_header();
      webpage += F("<h3>File was successfully uploaded</h3>"); 
      webpage += F("<h2>Uploaded File Name: "); webpage += uploadfile.filename+"</h2>";
      webpage += F("<h2>File Size: "); webpage += file_size(uploadfile.totalSize) + "</h2><br>"; 
      append_page_footer();
      server.send(200,"text/html",webpage);
    } 
    else
    {
      ReportCouldNotCreateFile("upload");
    }
  }
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
///// #ifdef ESP32
void SPIFFS_dir(){ 
  if (SPIFFS_present) { 
    File root = SPIFFS.open("/");
    if (root) {
      root.rewindDirectory();
      SendHTML_Header();
      webpage += F("<h3 class='rcorners_m'>SPIFFS Contents</h3><br>");
      webpage += F("<table align='center'>");
      webpage += F("<tr><th>Name/Type</th><th style='width:20%'>Type File/Dir</th><th>File Size</th></tr>");
      printDirectory("/",0);
      uint32_t usedBytes = SPIFFS.usedBytes();
      uint32_t totalBytes = SPIFFS.totalBytes();
      uint32_t freeBytes = totalBytes - usedBytes;
webpage += F("<tr><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td></tr>");
webpage += "<tr><th>Used Bytes </th><th style='width:20%'>"  + fileSizeHumanReadable(usedBytes)  + "</th><th>" + String(usedBytes)  + "</th></tr>";
webpage += "<tr><th>Free Bytes </th><th style='width:20%'>"  + fileSizeHumanReadable(freeBytes)  + "</th><th>" + String(freeBytes)  + "</th></tr>";
webpage += "<tr><th>Total Bytes</th><th style='width:20%'>"  + fileSizeHumanReadable(totalBytes) + "</th><th>" + String(totalBytes) + "</th></tr>";

      webpage += F("</table>");

      SendHTML_Content();
      root.close();
    }
    else 
    {
      SendHTML_Header();
      webpage += F("<h3>No Files Found</h3>");
    }
    append_page_footer();
    SendHTML_Content();
    SendHTML_Stop();   // Stop is needed because no content length was sent
  } else ReportSPIFFSNotPresent();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void printDirectory(const char * dirname, uint8_t levels){
  File root = SPIFFS.open(dirname);
  if(!root){
    return;
  }
  if(!root.isDirectory()){
    return;
  }
  File file = root.openNextFile();
  while(file){
    if (webpage.length() > 1000) {
      SendHTML_Content();
    }
    if(file.isDirectory()){
      webpage += "<tr><td>"+String(file.isDirectory()?"Dir":"File")+"</td><td>"+String(file.name())+"</td><td></td></tr>";
      printDirectory(file.name(), levels-1);
    }
    else
    {
      webpage += "<tr><td>"+String(file.name())+"</td>";
      webpage += "<td>"+String(file.isDirectory()?"Dir":"File")+"</td>";
      webpage += "<td>"+fileSizeHumanReadable(file.size())+"</td></tr>";
    }
    file = root.openNextFile();
  }
  file.close();
}
////#endif
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Stream(){
  if (server.args() > 0 ) { // Arguments were received
    if (server.hasArg("stream")) SPIFFS_file_stream(server.arg(0));
  }
  else SelectInput("Enter a File to Stream","stream","stream");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SPIFFS_file_stream(String filename) { 
  if (SPIFFS_present) { 
    File dataFile = SPIFFS.open("/"+filename,  "r"); // Now read data from SPIFFS Card 
    if (dataFile) { 
      if (dataFile.available()) { // If data is available and present 
        String dataType = "application/octet-stream"; 
        if (server.streamFile(dataFile, dataType) != dataFile.size()) {Serial.print(F("Sent less data than expected!")); } 
      }
      dataFile.close(); // close the file: 
    } else ReportFileNotPresent("Cstream");
  } else ReportSPIFFSNotPresent(); 
}   
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void File_Delete(){
  if (server.args() > 0 ) { // Arguments were received
    if (server.hasArg("delete")) SPIFFS_file_delete(server.arg(0));
  }
  else SelectInput("Select a File to Delete","delete","delete");
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SPIFFS_file_delete(String filename) { // Delete the file 
  if (SPIFFS_present) { 
    SendHTML_Header();
    File dataFile = SPIFFS.open("/"+filename, "r"); // Now read data from SPIFFS Card 
    if (dataFile)
    {
      if (SPIFFS.remove("/"+filename)) {
        ss_puts("\r\n File deleted successfully");
        webpage += "<h3>File '"+filename+"' has been erased</h3>"; 
        webpage += F("<a href='/delete'>[Back]</a><br><br>");
      }
      else
      { 
        webpage += F("<h3>File was not deleted - error</h3>");
        webpage += F("<a href='delete'>[Back]</a><br><br>");
      }
    } else ReportFileNotPresent("delete");
    append_page_footer(); 
    SendHTML_Content();
    SendHTML_Stop();
  } else ReportSPIFFSNotPresent();
} 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Header(){
  server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate"); 
  server.sendHeader("Pragma", "no-cache"); 
  server.sendHeader("Expires", "-1"); 
  server.setContentLength(CONTENT_LENGTH_UNKNOWN); 
  server.send(200, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves. 
  append_page_header();
  server.sendContent(webpage);
  webpage = "";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Content(){
  server.sendContent(webpage);
  webpage = "";
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SendHTML_Stop(){
  server.sendContent("");
  server.client().stop(); // Stop is needed because no content length was sent
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void SelectInput(String heading1, String command, String arg_calling_name){
  SendHTML_Header();
  webpage += F("<h3>"); webpage += heading1 + "</h3>"; 
  webpage += F("<FORM action='/"); webpage += command + "' method='post'>"; // Must match the calling argument e.g. '/chart' calls '/chart' after selection but with arguments!
  webpage += F("<input type='text' name='"); webpage += arg_calling_name; webpage += F("' value=''><br>");
  webpage += F("<type='submit' name='"); webpage += arg_calling_name; webpage += F("' value=''><br><br>");
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReportSPIFFSNotPresent(){
  SendHTML_Header();
  webpage += F("<h3>No SPIFFS Card present</h3>"); 
  webpage += F("<a href='/'>[Back]</a><br><br>");
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReportFileNotPresent(String target){
  SendHTML_Header();
  webpage += F("<h3>File does not exist</h3>"); 
  webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ReportCouldNotCreateFile(String target){
  SendHTML_Header();
  webpage += F("<h3>Could Not Create Uploaded File (write-protected?)</h3>"); 
  webpage += F("<a href='/"); webpage += target + "'>[Back]</a><br><br>";
  append_page_footer();
  SendHTML_Content();
  SendHTML_Stop();
}
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
String file_size(int bytes){
  String fsize = "";
  if (bytes < 1024)                 fsize = String(bytes)+" B";
  else if(bytes < (1024*1024))      fsize = String(bytes/1024.0,3)+" KB";
  else if(bytes < (1024*1024*1024)) fsize = String(bytes/1024.0/1024.0,3)+" MB";
  else                              fsize = String(bytes/1024.0/1024.0/1024.0,3)+" GB";
  return fsize;
}
