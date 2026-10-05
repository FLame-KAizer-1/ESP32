#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Preferences.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ============================================================
// ESP32 FLAPPY BIRD
// ============================================================
//
// OLED:
// SDA = GPIO 21
// SCL = GPIO 22
//
// RGB COMMON ANODE:
// Red   = GPIO 18
// Green = GPIO 19
// Blue  = GPIO 23
//
// BUZZER:
// Buzzer = GPIO 4
//
// RGB and buzzer are controlled through NPN transistors.
//
// ============================================================


// ============================================================
// OLED
// ============================================================

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET
);


// ============================================================
// PINS
// ============================================================

#define RED_PIN    18
#define GREEN_PIN  19
#define BLUE_PIN   23

#define BUZZER_PIN 4


// ============================================================
// WIFI
// ============================================================

const char* AP_NAME = "ESP32_FLAPPY";
const char* AP_PASSWORD = "12345678";

WebServer server(80);


// ============================================================
// HIGH SCORE
// ============================================================

Preferences preferences;

int highScore = 0;


// ============================================================
// GAME STATE
// ============================================================

enum GameState
{
  READY,
  PLAYING,
  GAMEOVER
};

GameState state = READY;


// ============================================================
// BIRD
// ============================================================

const int BIRD_X = 25;

float birdY = 32.0;
float birdVelocity = 0.0;

// Slightly smaller hitbox
const int BIRD_HIT_W = 6;
const int BIRD_HIT_H = 3;


// ============================================================
// PHYSICS
// ============================================================

const float GRAVITY = 0.12;

// Gentle flap
const float FLAP_POWER = -1.6;


// ============================================================
// PIPES
// ============================================================

const int PIPE_WIDTH = 12;

const int INITIAL_GAP = 30;
const int MIN_GAP = 23;

float pipeX = 140;

int pipeGapY = 32;

float pipeSpeed = 1.2;


// ============================================================
// SCORE
// ============================================================

int score = 0;


// ============================================================
// TIMING
// ============================================================

unsigned long lastFrame = 0;

const unsigned long FRAME_TIME = 25;


// ============================================================
// RAINBOW TIMING
// ============================================================

unsigned long lastRainbow = 0;

int rainbowStep = 0;

const unsigned long RAINBOW_DELAY = 120;


// ============================================================
// RGB
// ============================================================
//
// Common-anode RGB with NPN low-side transistors:
//
// GPIO HIGH = transistor ON = color ON
//
// ============================================================

void rgbOff()
{
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
}


void rgbRed()
{
  rgbOff();

  digitalWrite(
    RED_PIN,
    HIGH
  );
}


void rgbGreen()
{
  rgbOff();

  digitalWrite(
    GREEN_PIN,
    HIGH
  );
}


void rgbBlue()
{
  rgbOff();

  digitalWrite(
    BLUE_PIN,
    HIGH
  );
}


void rgbYellow()
{
  rgbOff();

  digitalWrite(
    RED_PIN,
    HIGH
  );

  digitalWrite(
    GREEN_PIN,
    HIGH
  );
}


void rgbCyan()
{
  rgbOff();

  digitalWrite(
    GREEN_PIN,
    HIGH
  );

  digitalWrite(
    BLUE_PIN,
    HIGH
  );
}


void rgbMagenta()
{
  rgbOff();

  digitalWrite(
    RED_PIN,
    HIGH
  );

  digitalWrite(
    BLUE_PIN,
    HIGH
  );
}


void rgbWhite()
{
  digitalWrite(
    RED_PIN,
    HIGH
  );

  digitalWrite(
    GREEN_PIN,
    HIGH
  );

  digitalWrite(
    BLUE_PIN,
    HIGH
  );
}


// ============================================================
// RAINBOW EFFECT
// ============================================================

void rainbowEffect()
{
  if (
    millis() - lastRainbow <
    RAINBOW_DELAY
  )
  {
    return;
  }

  lastRainbow = millis();


  switch (rainbowStep)
  {
    case 0:
      rgbRed();
      break;

    case 1:
      rgbYellow();
      break;

    case 2:
      rgbGreen();
      break;

    case 3:
      rgbCyan();
      break;

    case 4:
      rgbBlue();
      break;

    case 5:
      rgbMagenta();
      break;

    case 6:
      rgbWhite();
      break;
  }


  rainbowStep++;


  if (rainbowStep > 6)
  {
    rainbowStep = 0;
  }
}


// ============================================================
// BUZZER
// ============================================================

void buzzerOff()
{
  digitalWrite(
    BUZZER_PIN,
    LOW
  );
}


void flapSound()
{
  digitalWrite(
    BUZZER_PIN,
    HIGH
  );

  delay(25);

  digitalWrite(
    BUZZER_PIN,
    LOW
  );
}


void scoreSound()
{
  // First beep
  digitalWrite(
    BUZZER_PIN,
    HIGH
  );

  delay(60);

  digitalWrite(
    BUZZER_PIN,
    LOW
  );

  delay(45);


  // Second beep
  digitalWrite(
    BUZZER_PIN,
    HIGH
  );

  delay(80);

  digitalWrite(
    BUZZER_PIN,
    LOW
  );
}


void gameOverSound()
{
  digitalWrite(
    BUZZER_PIN,
    HIGH
  );

  delay(300);

  digitalWrite(
    BUZZER_PIN,
    LOW
  );
}


// ============================================================
// HIGH SCORE
// ============================================================

void saveHighScore()
{
  if (score > highScore)
  {
    highScore = score;

    preferences.putInt(
      "highscore",
      highScore
    );
  }
}


// ============================================================
// PIPE GAP
// ============================================================

int currentGap()
{
  int gap =
    INITIAL_GAP -
    (score / 5);


  if (gap < MIN_GAP)
  {
    gap = MIN_GAP;
  }


  return gap;
}


// ============================================================
// START GAME
// ============================================================

void startGame()
{
  birdY = 32.0;

  birdVelocity =
    FLAP_POWER;

  pipeX = 140;

  pipeGapY =
    random(
      20,
      44
    );

  score = 0;

  pipeSpeed = 1.2;

  state = PLAYING;

  rgbGreen();

  flapSound();
}


// ============================================================
// FLAP
// ============================================================

void flap()
{
  // Start
  if (state == READY)
  {
    startGame();

    return;
  }


  // Restart
  if (state == GAMEOVER)
  {
    startGame();

    return;
  }


  // Normal flap
  if (state == PLAYING)
  {
    birdVelocity =
      FLAP_POWER;

    rgbBlue();

    flapSound();

    // Return to green
    rgbGreen();
  }
}


// ============================================================
// COLLISION
// ============================================================

bool collision()
{
  int left =
    BIRD_X -
    BIRD_HIT_W;

  int right =
    BIRD_X +
    BIRD_HIT_W;

  int top =
    (int)birdY -
    BIRD_HIT_H;

  int bottom =
    (int)birdY +
    BIRD_HIT_H;


  // Ceiling
  if (top <= 0)
  {
    return true;
  }


  // Ground
  if (bottom >= 62)
  {
    return true;
  }


  // Pipe horizontal collision
  if (
    right >= pipeX &&
    left <=
      pipeX + PIPE_WIDTH
  )
  {
    int gap =
      currentGap();


    int gapTop =
      pipeGapY -
      gap / 2;


    int gapBottom =
      pipeGapY +
      gap / 2;


    if (top <= gapTop)
    {
      return true;
    }


    if (bottom >= gapBottom)
    {
      return true;
    }
  }


  return false;
}


// ============================================================
// DIFFICULTY
// ============================================================

void updateDifficulty()
{
  pipeSpeed =
    1.2 +
    score * 0.035;


  if (pipeSpeed > 2.1)
  {
    pipeSpeed = 2.1;
  }
}


// ============================================================
// UPDATE GAME
// ============================================================

void updateGame()
{
  if (state != PLAYING)
  {
    return;
  }


  // Gravity
  birdVelocity +=
    GRAVITY;


  birdY +=
    birdVelocity;


  // Move pipe
  pipeX -=
    pipeSpeed;


  // New pipe
  if (
    pipeX +
    PIPE_WIDTH < 0
  )
  {
    pipeX = 140;


    pipeGapY =
      random(
        20,
        44
      );


    score++;


    updateDifficulty();


    // Save record
    if (score > highScore)
    {
      saveHighScore();
    }


    // Score effect
    rgbYellow();

    scoreSound();

    // Back to green
    rgbGreen();
  }


  // Collision
  if (collision())
  {
    saveHighScore();


    state =
      GAMEOVER;


    rgbRed();

    gameOverSound();
  }
}


// ============================================================
// DRAW BIRD
// ============================================================
//
// Slightly smaller long bird:
//
//       ___
//    __/  ●>
//   /       \
//   \_______/
//      \
//
// Head = top-right
// Upper wing + lower wing
//
// ============================================================

void drawBird()
{
  int y =
    (int)birdY;


  int wingOffset;


  if (birdVelocity < 0)
  {
    wingOffset = 2;
  }
  else
  {
    wingOffset = 1;
  }


  // Main body
  display.fillRoundRect(
    BIRD_X - 7,
    y - 2,
    13,
    6,
    2,
    WHITE
  );


  // Head
  display.fillCircle(
    BIRD_X + 4,
    y - 3,
    3,
    WHITE
  );


  // Eye
  display.drawPixel(
    BIRD_X + 5,
    y - 4,
    BLACK
  );


  // Beak
  display.fillTriangle(
    BIRD_X + 7,
    y - 2,

    BIRD_X + 11,
    y,

    BIRD_X + 7,
    y + 1,

    WHITE
  );


  // Upper wing
  display.fillTriangle(
    BIRD_X - 2,
    y - 1,

    BIRD_X - 6,
    y - 4 -
      wingOffset,

    BIRD_X + 1,
    y - 2,

    WHITE
  );


  // Lower wing
  display.fillTriangle(
    BIRD_X - 2,
    y + 2,

    BIRD_X - 6,
    y + 4 +
      wingOffset,

    BIRD_X + 1,
    y + 3,

    WHITE
  );
}


// ============================================================
// DRAW PIPES
// ============================================================

void drawPipes()
{
  int x =
    (int)pipeX;


  int gap =
    currentGap();


  int gapTop =
    pipeGapY -
    gap / 2;


  int gapBottom =
    pipeGapY +
    gap / 2;


  // Top pipe
  if (gapTop > 0)
  {
    display.fillRect(
      x,
      0,
      PIPE_WIDTH,
      gapTop,
      WHITE
    );


    display.fillRect(
      x - 2,
      gapTop - 3,
      PIPE_WIDTH + 4,
      3,
      WHITE
    );
  }


  // Bottom pipe
  if (gapBottom < 62)
  {
    display.fillRect(
      x,
      gapBottom,
      PIPE_WIDTH,
      62 - gapBottom,
      WHITE
    );


    display.fillRect(
      x - 2,
      gapBottom,
      PIPE_WIDTH + 4,
      3,
      WHITE
    );
  }
}


// ============================================================
// GROUND
// ============================================================

void drawGround()
{
  display.drawFastHLine(
    0,
    62,
    128,
    WHITE
  );
}


// ============================================================
// SCORE
// ============================================================

void drawScore()
{
  display.setTextSize(1);

  display.setTextColor(
    WHITE
  );


  display.setCursor(
    2,
    2
  );

  display.print("S:");
  display.print(score);


  display.setCursor(
    91,
    2
  );

  display.print("HI:");
  display.print(highScore);
}


// ============================================================
// READY SCREEN
// ============================================================

void drawReady()
{
  display.setTextSize(1);

  display.setTextColor(
    WHITE
  );


  display.setCursor(
    43,
    9
  );

  display.println(
    "FLAPPY"
  );


  display.setCursor(
    25,
    21
  );

  display.println(
    "SPACE / TAP"
  );


  display.setCursor(
    37,
    33
  );

  display.println(
    "TO FLY"
  );


  display.setCursor(
    38,
    46
  );

  display.print(
    "BEST:"
  );

  display.print(
    highScore
  );


  drawBird();
}


// ============================================================
// GAME OVER SCREEN
// ============================================================

void drawGameOver()
{
  display.setTextSize(1);

  display.setTextColor(
    WHITE
  );


  display.setCursor(
    33,
    9
  );

  display.println(
    "GAME OVER"
  );


  display.setCursor(
    40,
    21
  );

  display.print(
    "SCORE:"
  );

  display.print(
    score
  );


  display.setCursor(
    40,
    32
  );

  display.print(
    "BEST:"
  );

  display.print(
    highScore
  );


  display.setCursor(
    19,
    46
  );

  display.println(
    "SPACE / TAP"
  );
}


// ============================================================
// DRAW GAME
// ============================================================

void drawGame()
{
  display.clearDisplay();


  if (state == READY)
  {
    drawReady();
  }
  else
  {
    drawPipes();

    drawBird();

    drawScore();


    if (state == GAMEOVER)
    {
      drawGameOver();
    }
  }


  drawGround();


  display.display();
}


// ============================================================
// WEB PAGE
// ============================================================

void handleRoot()
{
  const char html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
content="width=device-width,
initial-scale=1,
maximum-scale=1,
user-scalable=no">

<title>ESP32 Flappy Bird</title>

<style>

html,body
{
  margin:0;
  padding:0;

  width:100%;
  height:100%;

  overflow:hidden;

  text-align:center;

  font-family:Arial;

  background:
  linear-gradient(
    #4cc9ff,
    #bdeeff
  );

  touch-action:
  manipulation;
}

h1
{
  color:white;

  margin-top:25px;

  text-shadow:
  2px 2px #4389a5;
}

p
{
  color:white;
}

button
{
  position:absolute;

  left:10%;

  bottom:35px;

  width:80%;

  height:100px;

  border:0;

  border-radius:25px;

  background:#ffd21c;

  color:#604300;

  font-size:32px;

  font-weight:bold;

  box-shadow:
  0 7px 0 #a78a00;
}

button:active
{
  transform:
  translateY(7px);

  box-shadow:none;
}

</style>

</head>

<body>

<h1>🐦 FLAPPY BIRD</h1>

<p>ESP32 OLED</p>

<p>Tap or press SPACE</p>

<button id="flap">
FLAP!
</button>

<script>

function flap()
{
  fetch(
    "/flap",
    {
      cache:"no-store"
    }
  ).catch(
    function(){}
  );
}


document
.getElementById("flap")
.addEventListener(
  "click",
  flap
);


document.addEventListener(
  "keydown",
  function(e)
  {
    if(e.code === "Space")
    {
      e.preventDefault();

      flap();
    }
  }
);


document.addEventListener(
  "touchstart",
  function(e)
  {
    if(
      e.target.id !==
      "flap"
    )
    {
      flap();
    }
  },
  {
    passive:false
  }
);

</script>

</body>

</html>
)rawliteral";


  server.send(
    200,
    "text/html",
    html
  );
}


// ============================================================
// WEB FLAP
// ============================================================

void handleFlap()
{
  flap();


  server.send(
    200,
    "text/plain",
    "OK"
  );
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
  Serial.begin(
    115200
  );


  delay(300);


  // ----------------------------------------------------------
  // RGB
  // ----------------------------------------------------------

  pinMode(
    RED_PIN,
    OUTPUT
  );

  pinMode(
    GREEN_PIN,
    OUTPUT
  );

  pinMode(
    BLUE_PIN,
    OUTPUT
  );


  // ----------------------------------------------------------
  // Buzzer
  // ----------------------------------------------------------

  pinMode(
    BUZZER_PIN,
    OUTPUT
  );


  rgbOff();

  buzzerOff();


  // ----------------------------------------------------------
  // OLED
  // ----------------------------------------------------------

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );


  if (
    !display.begin(
      SSD1306_SWITCHCAPVCC,
      OLED_ADDRESS
    )
  )
  {
    Serial.println(
      "OLED ERROR!"
    );


    while (true)
    {
      delay(1000);
    }
  }


  // ----------------------------------------------------------
  // High score
  // ----------------------------------------------------------

  preferences.begin(
    "flappy",
    false
  );


  highScore =
    preferences.getInt(
      "highscore",
      0
    );


  // ----------------------------------------------------------
  // Random
  // ----------------------------------------------------------

  randomSeed(
    micros()
  );


  // ----------------------------------------------------------
  // Wi-Fi access point
  // ----------------------------------------------------------

  WiFi.mode(
    WIFI_AP
  );


  WiFi.softAP(
    AP_NAME,
    AP_PASSWORD
  );


  // ----------------------------------------------------------
  // Web server
  // ----------------------------------------------------------

  server.on(
    "/",
    HTTP_GET,
    handleRoot
  );


  server.on(
    "/flap",
    HTTP_GET,
    handleFlap
  );


  server.begin();


  // ----------------------------------------------------------
  // Ready
  // ----------------------------------------------------------

  state = READY;

  rainbowStep = 0;

  lastRainbow = millis();


  // ----------------------------------------------------------
  // OLED startup
  // ----------------------------------------------------------

  display.clearDisplay();


  display.setTextColor(
    WHITE
  );

  display.setTextSize(1);


  display.setCursor(
    31,
    10
  );

  display.println(
    "FLAPPY BIRD"
  );


  display.setCursor(
    20,
    25
  );

  display.println(
    "CONNECT WIFI"
  );


  display.setCursor(
    22,
    40
  );

  display.println(
    "192.168.4.1"
  );


  display.display();


  // ----------------------------------------------------------
  // Serial
  // ----------------------------------------------------------

  Serial.println();

  Serial.println(
    "=============================="
  );

  Serial.println(
    "       ESP32 FLAPPY BIRD"
  );

  Serial.println(
    "=============================="
  );

  Serial.print(
    "WiFi: "
  );

  Serial.println(
    AP_NAME
  );

  Serial.print(
    "Password: "
  );

  Serial.println(
    AP_PASSWORD
  );

  Serial.println(
    "Open: http://192.168.4.1"
  );

  Serial.print(
    "High Score: "
  );

  Serial.println(
    highScore
  );

  Serial.println(
    "=============================="
  );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
  // Handle web requests
  server.handleClient();


  // Rainbow while waiting
  if (state == READY)
  {
    rainbowEffect();
  }


  // Game frame
  unsigned long now =
    millis();


  if (
    now - lastFrame >=
    FRAME_TIME
  )
  {
    lastFrame = now;


    updateGame();

    drawGame();
  }
}
