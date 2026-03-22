#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// ─── WiFi AP Settings ───────────────────────────────────────
const char *AP_SSID = "RobotArm";
const char *AP_PASSWORD = "12345678";

// ─── Servo Pins (ESP32-S3 safe GPIOs) ───────────────────────
const int BASE_PIN = 4;
const int ARM_PIN = 10;
const int GRIP_PIN = 16;

// ─── LEDC Channels (0, 2, 4 → each on its own timer) ────────
const int BASE_CH = 0;
const int ARM_CH = 2;
const int GRIP_CH = 4;

const int SERVO_FREQ = 50; // 50 Hz = 20 ms period
const int SERVO_RES = 16;  // 16-bit resolution (0–65535)
const int DUTY_MIN = 1638; // 0.5 ms pulse → 0°
const int DUTY_MAX = 8191; // 2.5 ms pulse → 180°

// ─── Current Angles ─────────────────────────────────────────
int baseAngle = 90;
int armAngle = 90;
int gripAngle = 90;

// ─── Write angle to a servo via LEDC ─────────────────────────
void writeServo(int pin, int angle)
{
  angle = constrain(angle, 0, 180);
  int duty = map(angle, 0, 180, DUTY_MIN, DUTY_MAX);
  ledcWrite(pin, duty);
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
  *{margin:0;padding:0;box-sizing:border-box}
  @font-face{font-family:'Inter';font-display:swap;src:local('Inter')}
  :root{
    --cyan:#00d2ff;--purple:#7a5cff;--pink:#ff6ec7;
    --bg1:#0a0a1a;--bg2:#12122a;--bg3:#1a1a3e;
    --glass:rgba(255,255,255,0.04);--glass-border:rgba(255,255,255,0.08);
    --text:rgba(255,255,255,0.9);--text-dim:rgba(255,255,255,0.4);
    --radius:20px;
  }
  body{
    font-family:'Inter',-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;
    background:var(--bg1);min-height:100vh;min-height:100dvh;
    display:flex;flex-direction:column;align-items:center;
    color:var(--text);overflow-x:hidden;padding:16px 16px 32px;
    position:relative;
  }
  body::before{
    content:'';position:fixed;top:-50%;left:-50%;width:200%;height:200%;
    background:radial-gradient(ellipse at 30% 20%,rgba(0,210,255,0.06),transparent 50%),
               radial-gradient(ellipse at 70% 80%,rgba(122,92,255,0.06),transparent 50%),
               radial-gradient(ellipse at 50% 50%,rgba(255,110,199,0.04),transparent 60%);
    animation:bgShift 20s ease-in-out infinite;z-index:0;pointer-events:none;
  }
  @keyframes bgShift{0%,100%{transform:translate(0,0)}50%{transform:translate(-3%,2%)}}
  .app{position:relative;z-index:1;width:100%;max-width:420px;display:flex;flex-direction:column;align-items:center}

  /* Header */
  .header{text-align:center;margin-bottom:20px;animation:fadeIn .6s ease-out}
  .header h1{font-size:1.6rem;font-weight:700;
    background:linear-gradient(135deg,var(--cyan),var(--purple),var(--pink));
    -webkit-background-clip:text;-webkit-text-fill-color:transparent;
    background-clip:text;letter-spacing:-.5px}
  .header .sub{font-size:.75rem;color:var(--text-dim);margin-top:4px;font-weight:300}

  /* Status Bar */
  .status-bar{
    display:flex;align-items:center;justify-content:center;gap:16px;
    width:100%;padding:10px 16px;margin-bottom:20px;
    background:var(--glass);border:1px solid var(--glass-border);
    border-radius:12px;font-size:.72rem;color:var(--text-dim);
    animation:fadeIn .6s ease-out .1s both;
  }
  .status-bar .conn{display:flex;align-items:center;gap:6px}
  .dot{width:8px;height:8px;border-radius:50%;background:#ff4444;
    transition:all .3s;box-shadow:0 0 6px rgba(255,68,68,.5)}
  .dot.ok{background:#00e676;box-shadow:0 0 8px rgba(0,230,118,.6)}
  .latency{font-variant-numeric:tabular-nums;opacity:.7}

  /* Servo Cards */
  .cards{display:flex;flex-direction:column;gap:14px;width:100%}
  .servo-card{
    background:var(--glass);backdrop-filter:blur(24px);-webkit-backdrop-filter:blur(24px);
    border:1px solid var(--glass-border);border-radius:var(--radius);
    padding:20px;animation:slideUp .5s ease-out both;
    transition:transform .15s,border-color .3s;
  }
  .servo-card:nth-child(1){animation-delay:.1s}
  .servo-card:nth-child(2){animation-delay:.2s}
  .servo-card:nth-child(3){animation-delay:.3s}
  .servo-card:active{transform:scale(.985)}
  .servo-card.base{--accent:var(--cyan);--glow:rgba(0,210,255,.15)}
  .servo-card.arm{--accent:var(--purple);--glow:rgba(122,92,255,.15)}
  .servo-card.grip{--accent:var(--pink);--glow:rgba(255,110,199,.15)}
  .servo-card:hover{border-color:var(--accent,.15)}

  /* Card Top Row: gauge + label + angle */
  .card-top{display:flex;align-items:center;gap:14px;margin-bottom:14px}
  .gauge-wrap{position:relative;width:56px;height:56px;flex-shrink:0}
  .gauge-bg,.gauge-fg{fill:none;stroke-width:5;stroke-linecap:round}
  .gauge-bg{stroke:rgba(255,255,255,.06)}
  .gauge-fg{stroke:var(--accent);transition:stroke-dashoffset .15s ease-out;
    filter:drop-shadow(0 0 4px var(--accent))}
  .gauge-text{
    position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);
    font-size:.65rem;font-weight:600;color:var(--accent);
    font-variant-numeric:tabular-nums;
  }
  .card-info{flex:1}
  .card-label{font-size:.82rem;font-weight:500;display:flex;align-items:center;gap:6px}
  .card-label .ico{font-size:1rem}
  .angle-big{font-size:1.8rem;font-weight:700;color:var(--accent);
    font-variant-numeric:tabular-nums;line-height:1;margin-top:2px;
    cursor:pointer;transition:opacity .2s}
  .angle-big:hover{opacity:.8}
  .angle-input{
    width:70px;font-size:1.4rem;font-weight:700;color:var(--accent);
    background:rgba(255,255,255,.08);border:1px solid var(--accent,.3);
    border-radius:8px;padding:2px 8px;font-family:inherit;
    outline:none;display:none;
  }

  /* Slider Row */
  .slider-row{display:flex;align-items:center;gap:8px}
  .slider{
    -webkit-appearance:none;appearance:none;flex:1;height:6px;
    border-radius:3px;background:rgba(255,255,255,.08);outline:none;
  }
  .slider::-webkit-slider-thumb{
    -webkit-appearance:none;width:28px;height:28px;border-radius:50%;
    background:var(--accent);cursor:pointer;
    border:3px solid rgba(255,255,255,.25);
    box-shadow:0 0 12px var(--glow);transition:transform .12s}
  .slider::-webkit-slider-thumb:active{transform:scale(1.15)}
  .slider::-moz-range-thumb{
    width:28px;height:28px;border-radius:50%;background:var(--accent);
    cursor:pointer;border:3px solid rgba(255,255,255,.25);
    box-shadow:0 0 12px var(--glow)}

  /* Fine-tune Buttons */
  .fine-row{display:flex;justify-content:center;gap:6px;margin-top:10px}
  .fine-btn{
    padding:6px 10px;border:1px solid var(--glass-border);
    border-radius:10px;background:var(--glass);color:var(--text-dim);
    font-family:inherit;font-size:.7rem;font-weight:500;
    cursor:pointer;transition:all .15s;user-select:none;
    min-width:36px;text-align:center;
  }
  .fine-btn:active{transform:scale(.92);background:rgba(255,255,255,.1);color:var(--accent)}
  .fine-btn:hover{border-color:var(--accent,.3);color:var(--text)}

  /* Preset Buttons */
  .presets{
    display:grid;grid-template-columns:repeat(3,1fr);gap:10px;
    width:100%;margin-top:16px;animation:slideUp .5s ease-out .4s both;
  }
  .preset-btn{
    padding:14px 8px;border:1px solid var(--glass-border);
    border-radius:14px;background:var(--glass);
    backdrop-filter:blur(10px);-webkit-backdrop-filter:blur(10px);
    color:#fff;font-family:inherit;font-size:.8rem;font-weight:500;
    cursor:pointer;transition:all .2s;text-align:center;
    display:flex;flex-direction:column;align-items:center;gap:4px;
  }
  .preset-btn .p-icon{font-size:1.1rem}
  .preset-btn:active{transform:scale(.94);background:rgba(255,255,255,.1)}
  .preset-btn:nth-child(1){border-color:rgba(0,210,255,.2)}
  .preset-btn:nth-child(2){border-color:rgba(122,92,255,.2)}
  .preset-btn:nth-child(3){border-color:rgba(255,110,199,.2)}
  .preset-btn:hover{border-color:rgba(255,255,255,.2);box-shadow:0 4px 20px rgba(0,0,0,.3)}

  /* Animations */
  @keyframes fadeIn{from{opacity:0;transform:translateY(-12px)}to{opacity:1;transform:translateY(0)}}
  @keyframes slideUp{from{opacity:0;transform:translateY(24px)}to{opacity:1;transform:translateY(0)}}
  @keyframes pulse{0%,100%{opacity:1}50%{opacity:.5}}
</style>
</head>
<body>
<div class="app">
  <div class="header">
    <h1>&#x1F9BE; Robot Arm</h1>
    <div class="sub">ESP32-S3 Servo Control</div>
  </div>

  <div class="status-bar">
    <div class="conn"><div class="dot" id="dot"></div><span id="stTxt">Connecting...</span></div>
    <div class="latency" id="lat"></div>
  </div>

  <div class="cards">
    <!-- Base -->
    <div class="servo-card base" data-id="b">
      <div class="card-top">
        <div class="gauge-wrap">
          <svg viewBox="0 0 56 56" width="56" height="56">
            <circle class="gauge-bg" cx="28" cy="28" r="23" transform="rotate(-90 28 28)"
              stroke-dasharray="144.51" stroke-dashoffset="0"/>
            <circle class="gauge-fg" id="gB" cx="28" cy="28" r="23" transform="rotate(-90 28 28)"
              stroke-dasharray="144.51" stroke-dashoffset="72.26"/>
          </svg>
          <div class="gauge-text" id="gBt">50%</div>
        </div>
        <div class="card-info">
          <div class="card-label"><span class="ico">&#x1F504;</span> Base</div>
          <div class="angle-big" id="baseVal" onclick="editAngle('b')">90&#176;</div>
          <input class="angle-input" id="baseInp" type="number" min="0" max="180"
            onblur="commitAngle('b')" onkeydown="if(event.key==='Enter')this.blur()">
        </div>
      </div>
      <div class="slider-row">
        <input type="range" class="slider" id="baseSlider" min="0" max="180" value="90">
      </div>
      <div class="fine-row">
        <button class="fine-btn" onclick="nudge('b',-5)">-5</button>
        <button class="fine-btn" onclick="nudge('b',-1)">-1</button>
        <button class="fine-btn" onclick="nudge('b',1)">+1</button>
        <button class="fine-btn" onclick="nudge('b',5)">+5</button>
      </div>
    </div>

    <!-- Arm -->
    <div class="servo-card arm" data-id="a">
      <div class="card-top">
        <div class="gauge-wrap">
          <svg viewBox="0 0 56 56" width="56" height="56">
            <circle class="gauge-bg" cx="28" cy="28" r="23" transform="rotate(-90 28 28)"
              stroke-dasharray="144.51" stroke-dashoffset="0"/>
            <circle class="gauge-fg" id="gA" cx="28" cy="28" r="23" transform="rotate(-90 28 28)"
              stroke-dasharray="144.51" stroke-dashoffset="72.26"/>
          </svg>
          <div class="gauge-text" id="gAt">50%</div>
        </div>
        <div class="card-info">
          <div class="card-label"><span class="ico">&#x1F4AA;</span> Arm</div>
          <div class="angle-big" id="armVal" onclick="editAngle('a')">90&#176;</div>
          <input class="angle-input" id="armInp" type="number" min="0" max="180"
            onblur="commitAngle('a')" onkeydown="if(event.key==='Enter')this.blur()">
        </div>
      </div>
      <div class="slider-row">
        <input type="range" class="slider" id="armSlider" min="0" max="180" value="90">
      </div>
      <div class="fine-row">
        <button class="fine-btn" onclick="nudge('a',-5)">-5</button>
        <button class="fine-btn" onclick="nudge('a',-1)">-1</button>
        <button class="fine-btn" onclick="nudge('a',1)">+1</button>
        <button class="fine-btn" onclick="nudge('a',5)">+5</button>
      </div>
    </div>

    <!-- Gripper -->
    <div class="servo-card grip" data-id="g">
      <div class="card-top">
        <div class="gauge-wrap">
          <svg viewBox="0 0 56 56" width="56" height="56">
            <circle class="gauge-bg" cx="28" cy="28" r="23" transform="rotate(-90 28 28)"
              stroke-dasharray="144.51" stroke-dashoffset="0"/>
            <circle class="gauge-fg" id="gG" cx="28" cy="28" r="23" transform="rotate(-90 28 28)"
              stroke-dasharray="144.51" stroke-dashoffset="72.26"/>
          </svg>
          <div class="gauge-text" id="gGt">50%</div>
        </div>
        <div class="card-info">
          <div class="card-label"><span class="ico">&#x1F90F;</span> Gripper</div>
          <div class="angle-big" id="gripVal" onclick="editAngle('g')">90&#176;</div>
          <input class="angle-input" id="gripInp" type="number" min="0" max="180"
            onblur="commitAngle('g')" onkeydown="if(event.key==='Enter')this.blur()">
        </div>
      </div>
      <div class="slider-row">
        <input type="range" class="slider" id="gripSlider" min="0" max="180" value="90">
      </div>
      <div class="fine-row">
        <button class="fine-btn" onclick="nudge('g',-5)">-5</button>
        <button class="fine-btn" onclick="nudge('g',-1)">-1</button>
        <button class="fine-btn" onclick="nudge('g',1)">+1</button>
        <button class="fine-btn" onclick="nudge('g',5)">+5</button>
      </div>
    </div>
  </div>

  <!-- Presets -->
  <div class="presets">
    <button class="preset-btn" onclick="preset('center')">
      <span class="p-icon">&#x27F2;</span>Center
    </button>
    <button class="preset-btn" onclick="preset('home')">
      <span class="p-icon">&#x1F3E0;</span>Home
    </button>
    <button class="preset-btn" onclick="preset('park')">
      <span class="p-icon">&#x1F17F;</span>Park
    </button>
  </div>
</div>

<script>
(function(){
  /* ── Element refs ── */
  const $ = id => document.getElementById(id);
  const dot=$('dot'), stTxt=$('stTxt'), latEl=$('lat');
  const S={
    b:{sl:$('baseSlider'),val:$('baseVal'),inp:$('baseInp'),gfg:$('gB'),gtx:$('gBt')},
    a:{sl:$('armSlider'), val:$('armVal'), inp:$('armInp'), gfg:$('gA'),gtx:$('gAt')},
    g:{sl:$('gripSlider'),val:$('gripVal'),inp:$('gripInp'),gfg:$('gG'),gtx:$('gGt')}
  };
  const CIRC=144.513;
  let ws, pingT=0;

  /* ── WebSocket ── */
  function connect(){
    ws=new WebSocket('ws://'+location.host+'/ws');
    ws.onopen=()=>{dot.classList.add('ok');stTxt.textContent='Connected';pingLoop()};
    ws.onclose=()=>{dot.classList.remove('ok');stTxt.textContent='Reconnecting...';
      latEl.textContent='';setTimeout(connect,1500)};
    ws.onerror=()=>ws.close();
    ws.onmessage=e=>{
      const p=e.data.split(':');
      if(p.length===2){
        const id=p[0],v=parseInt(p[1]);
        if(S[id]){setUI(id,v,false)}
        if(id==='pong'){latEl.textContent=((Date.now()-pingT))+'ms'}
      }
    };
  }
  connect();

  function pingLoop(){
    setInterval(()=>{
      if(ws&&ws.readyState===1){pingT=Date.now();ws.send('ping')}
    },3000);
  }

  /* ── Send (throttled) ── */
  let sendQ={},sendTimer=null;
  function qSend(id,v){
    sendQ[id]=v;
    if(!sendTimer) sendTimer=setTimeout(()=>{
      for(let k in sendQ){if(ws&&ws.readyState===1)ws.send(k+':'+sendQ[k])}
      sendQ={};sendTimer=null;
    },20);
  }

  /* ── UI update ── */
  function setUI(id,v,doSend){
    v=Math.max(0,Math.min(180,v));
    const s=S[id];
    s.sl.value=v;
    s.val.textContent=v+'\u00B0';
    const pct=v/180;
    s.gfg.style.strokeDashoffset=CIRC*(1-pct);
    s.gtx.textContent=Math.round(pct*100)+'%';
    if(doSend!==false) qSend(id,v);
  }

  /* ── Slider events ── */
  ['b','a','g'].forEach(id=>{
    const sl=S[id].sl;
    sl.addEventListener('input',()=>{setUI(id,parseInt(sl.value));haptic()});
  });

  /* ── Fine-tune ── */
  window.nudge=(id,d)=>{
    const cur=parseInt(S[id].sl.value);
    setUI(id,cur+d);haptic();
  };

  /* ── Direct angle input ── */
  window.editAngle=id=>{
    const s=S[id];
    s.val.style.display='none';
    s.inp.style.display='inline-block';
    s.inp.value=parseInt(s.sl.value);
    s.inp.focus();s.inp.select();
  };
  window.commitAngle=id=>{
    const s=S[id];
    let v=parseInt(s.inp.value)||0;
    v=Math.max(0,Math.min(180,v));
    s.inp.style.display='none';
    s.val.style.display='block';
    setUI(id,v);
  };

  /* ── Presets ── */
  window.preset=name=>{
    haptic();
    const presets={
      center:{b:90,a:90,g:90},
      home:{b:90,a:90,g:0},
      park:{b:90,a:0,g:0}
    };
    const p=presets[name];if(!p)return;
    Object.keys(p).forEach(id=>setUI(id,p[id]));
  };

  /* ── Haptic ── */
  function haptic(){if(navigator.vibrate)navigator.vibrate(8)}

  /* ── Init gauges ── */
  ['b','a','g'].forEach(id=>setUI(id,90,false));
})();
</script>
</body>
</html>
)rawliteral";

// ─── WebSocket Event Handler ────────────────────────────────
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len)
{
  if (type == WS_EVT_CONNECT)
  {
    Serial.printf("WS client #%u connected\n", client->id());
    client->printf("b:%d", baseAngle);
    client->printf("a:%d", armAngle);
    client->printf("g:%d", gripAngle);
  }
  else if (type == WS_EVT_DISCONNECT)
  {
    Serial.printf("WS client #%u disconnected\n", client->id());
  }
  else if (type == WS_EVT_DATA)
  {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
      char msg_buf[len + 1];
      memcpy(msg_buf, data, len);
      msg_buf[len] = '\0';
      String msg = msg_buf;

      // Handle ping for latency measurement
      if (msg == "ping")
      {
        client->text("pong:0");
        return;
      }

      int colonIdx = msg.indexOf(':');
      if (colonIdx > 0)
      {
        String id = msg.substring(0, colonIdx);
        int angle = msg.substring(colonIdx + 1).toInt();
        angle = constrain(angle, 0, 180);

        if (id == "b")
        {
          baseAngle = angle;
          writeServo(BASE_PIN, baseAngle);
          Serial.printf("Base → %d°\n", baseAngle);
        }
        else if (id == "a")
        {
          armAngle = angle;
          writeServo(ARM_PIN, armAngle);
          Serial.printf("Arm  → %d°\n", armAngle);
        }
        else if (id == "g")
        {
          gripAngle = angle;
          writeServo(GRIP_PIN, gripAngle);
          Serial.printf("Grip → %d°\n", gripAngle);
        }
      }
    }
  }
}

// ─── Setup ──────────────────────────────────────────────────
void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Robot Arm Controller ===");

  // LEDC servo setup — channels 0, 2, 4 each on their own timer
  ledcSetup(BASE_CH, SERVO_FREQ, SERVO_RES);
  ledcAttachPin(BASE_PIN, BASE_CH);

  ledcSetup(ARM_CH, SERVO_FREQ, SERVO_RES);
  ledcAttachPin(ARM_PIN, ARM_CH);

  ledcSetup(GRIP_CH, SERVO_FREQ, SERVO_RES);
  ledcAttachPin(GRIP_PIN, GRIP_CH);

  writeServo(BASE_PIN, baseAngle);
  writeServo(ARM_PIN, armAngle);
  writeServo(GRIP_PIN, gripAngle);

  Serial.printf("Servos: Base(GPIO%d/CH%d) Arm(GPIO%d/CH%d) Grip(GPIO%d/CH%d)\n",
                BASE_PIN, BASE_CH, ARM_PIN, ARM_CH, GRIP_PIN, GRIP_CH);

  // ── WiFi Access Point ──
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  delay(200);
  Serial.print("WiFi AP started: ");
  Serial.println(AP_SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.softAPIP());

  // ── WebSocket & HTTP ──
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", index_html); });

  server.begin();
  Serial.println("Web server started on port 80");
  Serial.println("Open http://192.168.4.1 on your phone");
}

// ─── Loop ───────────────────────────────────────────────────
void loop()
{
  ws.cleanupClients();
}
