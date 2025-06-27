#include <WiFi.h>
#include <Wire.h>
#include <Motoron.h>
#include <Servo.h>

// Network Connection
const char* SSID = "Mars_Rover";
const char* PASS = "Marshgazers9";

WiFiServer server(80); 
MotoronI2C mc(16);
Servo myservo;

inline void writeServoFlipped(int logicalAngle)
{
  logicalAngle = constrain(logicalAngle, 0, 180);   // safety first
  myservo.write(180 - logicalAngle);                // invert direction
}

int servoPos = 0; // Initial Servo Position

void setup()
{
  // Motoron
  Wire1.begin();               
  mc.setBus(&Wire1);

  mc.reinitialize();
  mc.disableCrc();
  mc.clearResetFlag();

  mc.setMaxAcceleration(1, 600);
  mc.setMaxDeceleration(1, 600);
  mc.setMaxAcceleration(2, 600);
  mc.setMaxDeceleration(2, 600);

  // Servo
  myservo.attach(9);           // Connected To Pin 9
  writeServoFlipped(servoPos); // Calibrate Start Position

  // WiFi
  pinMode(LED_BUILTIN, OUTPUT);
  WiFi.config(IPAddress(10,0,0,1));
  WiFi.beginAP(SSID, PASS);
  server.begin();
  digitalWrite(LED_BUILTIN, HIGH);
 
}

// Track Control
void drive(char cmd)
{
  const int spd = 700;
  switch (cmd)
  {
    case 'F': mc.setSpeed(1,  spd); mc.setSpeed(2, -spd); break; // Forwards  
    case 'B': mc.setSpeed(1, -spd); mc.setSpeed(2,  spd); break; // Backwards
    case 'L': mc.setSpeed(1,  spd); mc.setSpeed(2,  spd); break; // Left
    case 'R': mc.setSpeed(1, -spd); mc.setSpeed(2, -spd); break; // Right
    default : mc.setSpeed(1,   0);  mc.setSpeed(2,   0);  break; // Stop
  }
}

// Link Control
void moveServo(int delta)
{
  servoPos = constrain(servoPos + delta, 0, 180);
  writeServoFlipped(servoPos);  // always go through the converter
}

// HTML Website Creator
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <!-- Mobile viewport to ensure proper scaling on phones/tablets -->
    <meta name="viewport" content="width=device-width,initial-scale=1">

    <!-- Inline CSS to style buttons and layout -->
    <style>
      /* Base font and centering for body content */
      body {
        font-family: sans-serif;
        text-align: center;
        margin-top: 40px;
      }

      /* Control Station */
      .grid {
        display: inline-grid;
        grid-template-areas:
          ". w ."
          "a stop d"
          "m s n";
        gap: 12px;
      }

      /* Styling for each control button */
      .btn {
        width: 90px;
        height: 90px;
        font-size: 32px;
        border: 1px solid #444;
        border-radius: 4px;
        cursor: pointer;
        user-select: none;
      }
    </style>
  </head>
  <body>
    <!-- Page title -->
    <h2>Mars Rover Control Station</h2>
    <!-- Brief instruction -->
    <p>Hold a key (or click) to move rover. Release to stop</p>

    <!-- Button grid: W at top, A/D on sides, S bottom, Stop center -->
    <div class="grid">
      <!-- Forward -->
      <button class="btn" style="grid-area:w"
              onmousedown="start('F')" ontouchstart="start('F')">
        W
      </button>
      <!-- Left turn -->
      <button class="btn" style="grid-area:a"
              onmousedown="start('L')" ontouchstart="start('L')">
        A
      </button>
      <!-- Right turn -->
      <button class="btn" style="grid-area:d"
              onmousedown="start('R')" ontouchstart="start('R')">
        D
      </button>
      <!-- Backward -->
      <button class="btn" style="grid-area:s"
              onmousedown="start('B')" ontouchstart="start('B')">
        S
      </button>
      <!-- Stop all motion -->
      <button class="btn" style="grid-area:stop"
              onmousedown="start('S')" ontouchstart="start('S')">
        Stop
      </button>
      <!-- Servo Up -->
      <button class="btn" style="grid-area:m"
              onmousedown="startServo('U')" ontouchstart="startServo('U')">
        M
      </button>
      <!-- Servo Down -->
      <button class="btn" style="grid-area:n"
              onmousedown="startServo('D')" ontouchstart="startServo('D')">
        N
      </button>
    </div>

     <script>
      // --- State Flag ---
      // Tracks which motor command is active (W/A/S/D)
      let motorActive = null;
      // Tracks which servo direction is active (U or D).  null = idle
      let servoActive = null;

      // Interval ID used for repeated /U or /D fetches
      let servoTimer = null;

      // --- Network Helpers ---
      // Fire a GET request:  /F  /B  /L  /R  /S  /U  /D
      function go(cmd) { fetch('/' + cmd); }

      // --- Track Control ---
      function start(cmd) {
        if (motorActive === cmd) return;   // ignore repeat
        motorActive = cmd;
        go(cmd);
      }
      function stop() {                    // called on key-up / blur / etc.
        if (motorActive !== null) {
          go('S');                         // tell GIGA to stop motors
          motorActive = null;
        }
        stopServo();                       // also make sure servo stops
      }

      // --- Servo Control ---
      function startServo(dir) {
        if (servoActive === dir) return;   // already moving that way
        stopServo();                       // cancel any previous direction
        servoActive = dir;                 // remember new direction
        servoTimer = setInterval(() => {   // repeat every 30 ms
          fetch('/' + dir);
        }, 50);
      }

      function stopServo() {
        if (servoTimer) {                  // clear the interval
          clearInterval(servoTimer);
          servoTimer = null;
        }
        servoActive = null;                // mark servo as idle
      }

      // --- Keyboard Bindings ---
      document.addEventListener('keydown', e => {
        const k = e.key.toLowerCase();
        if ('wasd'.includes(k) && motorActive !== k) {
          const m = { w:'F', a:'L', s:'B', d:'R' };
          start(m[k]);                     // tracks
        } else if (k === 'm') {
          startServo('U');                 // servo up
        } else if (k === 'n') {
          startServo('D');                 // servo down
        }
      });

      document.addEventListener('keyup', e => {
        const k = e.key.toLowerCase();
        if ('wasd'.includes(k)) stop();    // stop tracks
        if (k === 'm' || k === 'n') stopServo();
      });

      // Mouse / touch release safety nets
      ['mouseup','touchend','touchcancel','mouseleave'].forEach(evt =>
        document.addEventListener(evt, stop)
      );

      // Lose-focus safety net
      window.addEventListener('blur', stop);
    </script>
  </body>
</html>)rawliteral";

void loop()
{
  WiFiClient c = server.accept();
  if (!c) return;

  String line;
  while (c.connected())
  {
    if (c.available())
    {
      char ch = c.read();
      line += ch;
      if (ch == '\n')
      {
        if      (line.indexOf("GET /F") >= 0) drive('F');
        else if (line.indexOf("GET /B") >= 0) drive('B');
        else if (line.indexOf("GET /L") >= 0) drive('L');
        else if (line.indexOf("GET /R") >= 0) drive('R');
        else if (line.indexOf("GET /S") >= 0) drive('S');
        else if (line.indexOf("GET /U") >= 0) moveServo(+1);
        else if (line.indexOf("GET /D") >= 0) moveServo(-1);

        if (line.startsWith("GET / ")) {
          c.println("HTTP/1.1 200 OK\r\nContent-Type:text/html\r\n\r\n");
          c.println(PAGE);
        } else {
          c.println("HTTP/1.1 204 No Content\r\n\r\n");
        }
        break;
      }
    }
  }
  c.stop();
}