#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// ─── WiFi AP Settings ───────────────────────────────────────
const char* AP_SSID     = "RobotArm";
const char* AP_PASSWORD = "12345678";

// ─── Servo Pins (ESP32-S3 safe GPIOs) ───────────────────────
const int BASE_PIN = 4;
const int ARM_PIN  = 5;
const int GRIP_PIN = 6;

// ─── LEDC PWM Settings for Servos ───────────────────────────
// Each servo gets its own explicit LEDC channel to prevent conflicts
const int BASE_CH = 0;
const int ARM_CH  = 1;
const int GRIP_CH = 2;
const int SERVO_FREQ = 50;        // 50Hz = 20ms period
const int SERVO_RES  = 16;        // 16-bit resolution (0-65535)
const int DUTY_MIN   = 1638;      // 0.5ms pulse  → 0°
const int DUTY_MAX   = 8192;      // 2.5ms pulse  → 180°

// ─── Current Angles ─────────────────────────────────────────
int baseAngle = 90;
int armAngle  = 90;
int gripAngle = 90;

// ─── Write angle to a servo channel via LEDC ────────────────
void writeServo(int channel, int angle) {
  angle = constrain(angle, 0, 180);
  int duty = map(angle, 0, 180, DUTY_MIN, DUTY_MAX);
  ledcWrite(channel, duty);
}

// ─── Web Server & WebSocket ─────────────────────────────────
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ─── Embedded Web UI ────────────────────────────────────────
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
<title>Robot Arm Control</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;500;700&display=swap');

  * { margin:0; padding:0; box-sizing:border-box; }

  body {
    font-family: 'Inter', -apple-system, BlinkMacSystemFont, sans-serif;
    background: linear-gradient(135deg, #0f0c29, #302b63, #24243e);
    min-height: 100vh;
    display: flex;
    flex-direction: column;
    align-items: center;
    color: #fff;
    overflow-x: hidden;
    padding: 20px;
  }

  /* ── Header ── */
  .header {
    text-align: center;
    margin-bottom: 28px;
    animation: fadeDown 0.6s ease-out;
  }
  .header h1 {
    font-size: 1.7rem;
    font-weight: 700;
    background: linear-gradient(90deg, #00d2ff, #7a5cff, #ff6ec7);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    letter-spacing: -0.5px;
  }
  .header .sub {
    font-size: 0.82rem;
    color: rgba(255,255,255,0.45);
    margin-top: 4px;
    font-weight: 300;
  }

  /* ── Status Indicator ── */
  .status {
    display: flex;
    align-items: center;
    gap: 8px;
    margin-bottom: 24px;
    font-size: 0.78rem;
    color: rgba(255,255,255,0.55);
    animation: fadeDown 0.6s ease-out 0.1s both;
  }
  .status .dot {
    width: 10px; height: 10px;
    border-radius: 50%;
    background: #ff4444;
    transition: background 0.3s;
    box-shadow: 0 0 8px rgba(255,68,68,0.5);
  }
  .status .dot.connected {
    background: #00e676;
    box-shadow: 0 0 8px rgba(0,230,118,0.5);
  }

  /* ── Servo Card ── */
  .card-container {
    display: flex;
    flex-direction: column;
    gap: 16px;
    width: 100%;
    max-width: 400px;
  }

  .servo-card {
    background: rgba(255,255,255,0.06);
    backdrop-filter: blur(20px);
    -webkit-backdrop-filter: blur(20px);
    border: 1px solid rgba(255,255,255,0.08);
    border-radius: 20px;
    padding: 22px 24px;
    animation: fadeUp 0.5s ease-out both;
    transition: transform 0.2s, box-shadow 0.2s;
  }
  .servo-card:nth-child(1) { animation-delay: 0.15s; }
  .servo-card:nth-child(2) { animation-delay: 0.25s; }
  .servo-card:nth-child(3) { animation-delay: 0.35s; }

  .servo-card:active {
    transform: scale(0.985);
  }

  .card-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 16px;
  }
  .card-header .label {
    font-size: 0.9rem;
    font-weight: 500;
    display: flex;
    align-items: center;
    gap: 8px;
  }
  .card-header .icon {
    font-size: 1.2rem;
  }
  .card-header .angle-display {
    font-size: 1.6rem;
    font-weight: 700;
    font-variant-numeric: tabular-nums;
  }

  /* Accent colors per servo */
  .servo-card.base .angle-display { color: #00d2ff; }
  .servo-card.arm  .angle-display { color: #7a5cff; }
  .servo-card.grip .angle-display { color: #ff6ec7; }

  .servo-card.base .slider::-webkit-slider-thumb { background: #00d2ff; box-shadow: 0 0 16px rgba(0,210,255,0.5); }
  .servo-card.arm  .slider::-webkit-slider-thumb { background: #7a5cff; box-shadow: 0 0 16px rgba(122,92,255,0.5); }
  .servo-card.grip .slider::-webkit-slider-thumb { background: #ff6ec7; box-shadow: 0 0 16px rgba(255,110,199,0.5); }

  .servo-card.base .slider::-moz-range-thumb { background: #00d2ff; box-shadow: 0 0 16px rgba(0,210,255,0.5); }
  .servo-card.arm  .slider::-moz-range-thumb { background: #7a5cff; box-shadow: 0 0 16px rgba(122,92,255,0.5); }
  .servo-card.grip .slider::-moz-range-thumb { background: #ff6ec7; box-shadow: 0 0 16px rgba(255,110,199,0.5); }

  /* ── Slider Styling ── */
  .slider {
    -webkit-appearance: none;
    appearance: none;
    width: 100%;
    height: 8px;
    border-radius: 4px;
    background: rgba(255,255,255,0.1);
    outline: none;
    transition: background 0.2s;
  }
  .slider::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 32px;
    height: 32px;
    border-radius: 50%;
    cursor: pointer;
    border: 3px solid rgba(255,255,255,0.3);
    transition: transform 0.15s;
  }
  .slider::-webkit-slider-thumb:active {
    transform: scale(1.2);
  }
  .slider::-moz-range-thumb {
    width: 32px;
    height: 32px;
    border-radius: 50%;
    cursor: pointer;
    border: 3px solid rgba(255,255,255,0.3);
  }

  .range-labels {
    display: flex;
    justify-content: space-between;
    font-size: 0.68rem;
    color: rgba(255,255,255,0.3);
    margin-top: 6px;
  }

  /* ── Buttons ── */
  .btn-row {
    display: flex;
    gap: 10px;
    width: 100%;
    max-width: 400px;
    margin-top: 8px;
    animation: fadeUp 0.5s ease-out 0.45s both;
  }
  .btn {
    flex: 1;
    padding: 14px;
    border: 1px solid rgba(255,255,255,0.1);
    border-radius: 14px;
    background: rgba(255,255,255,0.06);
    backdrop-filter: blur(10px);
    color: #fff;
    font-family: inherit;
    font-size: 0.85rem;
    font-weight: 500;
    cursor: pointer;
    transition: all 0.2s;
    text-align: center;
  }
  .btn:active {
    transform: scale(0.96);
    background: rgba(255,255,255,0.12);
  }
  .btn.reset { border-color: rgba(255,110,199,0.3); }
  .btn.center { border-color: rgba(0,210,255,0.3); }

  /* ── Animations ── */
  @keyframes fadeDown {
    from { opacity:0; transform: translateY(-20px); }
    to   { opacity:1; transform: translateY(0); }
  }
  @keyframes fadeUp {
    from { opacity:0; transform: translateY(20px); }
    to   { opacity:1; transform: translateY(0); }
  }
</style>
</head>
<body>

  <div class="header">
    <h1>🦾 Robot Arm</h1>
    <div class="sub">ESP32-S3 WebSocket Control</div>
  </div>

  <div class="status">
    <div class="dot" id="statusDot"></div>
    <span id="statusText">Connecting...</span>
  </div>

  <div class="card-container">

    <!-- Base Servo -->
    <div class="servo-card base">
      <div class="card-header">
        <div class="label"><span class="icon">🔄</span> Base</div>
        <div class="angle-display" id="baseVal">90°</div>
      </div>
      <input type="range" class="slider" id="baseSlider" min="0" max="180" value="90">
      <div class="range-labels"><span>0°</span><span>90°</span><span>180°</span></div>
    </div>

    <!-- Arm Servo -->
    <div class="servo-card arm">
      <div class="card-header">
        <div class="label"><span class="icon">💪</span> Arm</div>
        <div class="angle-display" id="armVal">90°</div>
      </div>
      <input type="range" class="slider" id="armSlider" min="0" max="180" value="90">
      <div class="range-labels"><span>0°</span><span>90°</span><span>180°</span></div>
    </div>

    <!-- Gripper Servo -->
    <div class="servo-card grip">
      <div class="card-header">
        <div class="label"><span class="icon">🤏</span> Gripper</div>
        <div class="angle-display" id="gripVal">90°</div>
      </div>
      <input type="range" class="slider" id="gripSlider" min="0" max="180" value="90">
      <div class="range-labels"><span>0°</span><span>90°</span><span>180°</span></div>
    </div>

  </div>

  <div class="btn-row">
    <button class="btn center" onclick="centerAll()">⟲ Center All</button>
    <button class="btn reset" onclick="homePosition()">🏠 Home</button>
  </div>

<script>
  // ─── WebSocket ─────────────────────────────────────────
  let ws;
  const dot = document.getElementById('statusDot');
  const statusText = document.getElementById('statusText');

  function connectWS() {
    ws = new WebSocket('ws://' + location.host + '/ws');

    ws.onopen = () => {
      dot.classList.add('connected');
      statusText.textContent = 'Connected';
    };

    ws.onclose = () => {
      dot.classList.remove('connected');
      statusText.textContent = 'Reconnecting...';
      setTimeout(connectWS, 1500);
    };

    ws.onerror = () => {
      ws.close();
    };

    ws.onmessage = (evt) => {
      // Server can push angle updates (e.g., after reset)
      const parts = evt.data.split(':');
      if (parts.length === 2) {
        const id = parts[0];
        const val = parseInt(parts[1]);
        if (id === 'b') { document.getElementById('baseSlider').value = val; document.getElementById('baseVal').textContent = val + '°'; }
        if (id === 'a') { document.getElementById('armSlider').value = val;  document.getElementById('armVal').textContent = val + '°'; }
        if (id === 'g') { document.getElementById('gripSlider').value = val; document.getElementById('gripVal').textContent = val + '°'; }
      }
    };
  }
  connectWS();

  // ─── Slider Handlers ──────────────────────────────────
  function send(cmd) {
    if (ws && ws.readyState === WebSocket.OPEN) ws.send(cmd);
  }

  const baseSlider = document.getElementById('baseSlider');
  const armSlider  = document.getElementById('armSlider');
  const gripSlider = document.getElementById('gripSlider');

  baseSlider.addEventListener('input', () => {
    const v = baseSlider.value;
    document.getElementById('baseVal').textContent = v + '°';
    send('b:' + v);
  });
  armSlider.addEventListener('input', () => {
    const v = armSlider.value;
    document.getElementById('armVal').textContent = v + '°';
    send('a:' + v);
  });
  gripSlider.addEventListener('input', () => {
    const v = gripSlider.value;
    document.getElementById('gripVal').textContent = v + '°';
    send('g:' + v);
  });

  // ─── Buttons ───────────────────────────────────────────
  function centerAll() {
    [baseSlider, armSlider, gripSlider].forEach(s => { s.value = 90; });
    document.getElementById('baseVal').textContent = '90°';
    document.getElementById('armVal').textContent  = '90°';
    document.getElementById('gripVal').textContent = '90°';
    send('b:90');
    send('a:90');
    send('g:90');
  }

  function homePosition() {
    baseSlider.value = 90; armSlider.value = 90; gripSlider.value = 0;
    document.getElementById('baseVal').textContent = '90°';
    document.getElementById('armVal').textContent  = '90°';
    document.getElementById('gripVal').textContent = '0°';
    send('b:90');
    send('a:90');
    send('g:0');
  }
</script>
</body>
</html>
)rawliteral";

// ─── WebSocket Event Handler ────────────────────────────
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WS client #%u connected\n", client->id());
    // Send current positions to new client
    client->printf("b:%d", baseAngle);
    client->printf("a:%d", armAngle);
    client->printf("g:%d", gripAngle);
  }
  else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WS client #%u disconnected\n", client->id());
  }
  else if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = '\0';
      String msg = (char*)data;

      int colonIdx = msg.indexOf(':');
      if (colonIdx > 0) {
        String id  = msg.substring(0, colonIdx);
        int angle  = msg.substring(colonIdx + 1).toInt();
        angle = constrain(angle, 0, 180);

        if (id == "b") {
          baseAngle = angle;
          writeServo(BASE_CH, baseAngle);
          Serial.printf("Base → %d°\n", baseAngle);
        }
        else if (id == "a") {
          armAngle = angle;
          writeServo(ARM_CH, armAngle);
          Serial.printf("Arm  → %d°\n", armAngle);
        }
        else if (id == "g") {
          gripAngle = angle;
          writeServo(GRIP_CH, gripAngle);
          Serial.printf("Grip → %d°\n", gripAngle);
        }
      }
    }
  }
}

// ─── Setup ──────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Robot Arm Controller ===");

  // ── Servos via direct LEDC (no ESP32Servo library) ──
  // Each channel is set up independently, then attached to its pin
  ledcSetup(BASE_CH, SERVO_FREQ, SERVO_RES);
  ledcSetup(ARM_CH,  SERVO_FREQ, SERVO_RES);
  ledcSetup(GRIP_CH, SERVO_FREQ, SERVO_RES);

  ledcAttachPin(BASE_PIN, BASE_CH);
  ledcAttachPin(ARM_PIN,  ARM_CH);
  ledcAttachPin(GRIP_PIN, GRIP_CH);

  writeServo(BASE_CH, baseAngle);
  writeServo(ARM_CH,  armAngle);
  writeServo(GRIP_CH, gripAngle);
  Serial.printf("Servos initialized: Base(GPIO%d/CH%d) Arm(GPIO%d/CH%d) Grip(GPIO%d/CH%d)\n",
                BASE_PIN, BASE_CH, ARM_PIN, ARM_CH, GRIP_PIN, GRIP_CH);

  // ── WiFi Access Point ──
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(200);
  Serial.print("WiFi AP started: ");
  Serial.println(AP_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  // ── WebSocket ──
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // ── HTTP Routes ──
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.begin();
  Serial.println("Web server started on port 80");
  Serial.println("Open http://192.168.4.1 on your phone");
}

// ─── Loop ───────────────────────────────────────────────
void loop() {
  ws.cleanupClients();
}