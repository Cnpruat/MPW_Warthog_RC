#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESP32Servo.h> 

/* ================= CONFIGURATION MATERIELLE ================= */

// --- Servomoteurs Direction ---
Servo servo1;
Servo servo2;
const int pinSERVO1 = 32;
const int pinSERVO2 = 33;
int currentAngleS1 = 82; 
int currentAngleS2 = 81; 

// --- Servomoteurs Tourelle ---
Servo servoTourellePan; 
Servo servoTourelleTilt;
const int pinTourellePan = 18;
const int pinTourelleTilt = 19;

// --- Moteurs DC (Tableaux : Moteur 1, 2, 3, 4) ---
const int pinENA[4] = {15, 5, 25, 13};
const int pinIN1[4] = {2, 16, 26, 12};
const int pinIN2[4] = {4, 17, 27, 14};

// --- Capteurs de Vitesse ---
const int pinCapteur1 = 21;
const int pinCapteur2 = 3; 
const float TICKS_PER_REV = 10.0;

// Variables pour le calcul de vitesse sans interruption
long countCapteur1 = 0;
long countCapteur2 = 0;
unsigned long lastTimeSpeedMeasured = 0;
float rpmCapteur1 = 0.0, rpmCapteur2 = 0.0;

// Anti-rebond (Debounce)
bool lastStateCapteur1 = HIGH;
bool lastStateCapteur2 = HIGH;
unsigned long lastDebounceTime1 = 0;
unsigned long lastDebounceTime2 = 0;
const unsigned long DEBOUNCE_DELAY = 2; 

/* ================= VARIABLES GLOBALES ================= */
// Tourelle
float currentPan = 84.0, currentTilt = 50.0; 
int speedPan = 0, speedTilt = 0; 
unsigned long lastTurretUpdate = 0;

// Véhicule
bool isTankTurning = false;
int targetSpeed[4]  = {0, 0, 0, 0}; 
int currentSpeed[4] = {0, 0, 0, 0}; 
unsigned long lastMotorUpdate = 0;
const int ACCEL_STEP = 10; 

// --- NOUVEAU : GESTION DE L'ENERGIE ---
unsigned long lastMessageTime = 0; // Heure du dernier ordre reçu
bool servosAttached = true;        // État actuel des servos
const unsigned long SERVO_TIMEOUT = 2000; // Coupure après 2 secondes d'inactivité

/* ================= CONFIGURATION WIFI ================= */
const char* ssid = "Warthog_Controller";
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

WebServer server(80);
DNSServer dnsServer;


/* ================= FONCTIONS GESTION ENERGIE ================= */

// Réveille ou endort les servos pour limiter le courant
void manageServos() {
  if (millis() - lastMessageTime > SERVO_TIMEOUT) {
    // Si on dépasse le temps, on coupe le signal PWM
    if (servosAttached) {
      servo1.detach();
      servo2.detach();
      servoTourellePan.detach();
      servoTourelleTilt.detach();
      servosAttached = false;
      // Optionnel : Serial.println("Servos en veille (Economie d'energie)");
    }
  } else {
    // Si on a reçu un message récent, on réactive le PWM avant d'écrire
    if (!servosAttached) {
      servo1.attach(pinSERVO1, 500, 2400); 
      servo2.attach(pinSERVO2, 500, 2400); 
      servoTourellePan.attach(pinTourellePan, 500, 2400); 
      servoTourelleTilt.attach(pinTourelleTilt, 500, 2400); 
      servosAttached = true;
      // On restaure immédiatement leur dernière position connue
      servo1.write(currentAngleS1);
      servo2.write(currentAngleS2);
      servoTourellePan.write((int)currentPan);
      servoTourelleTilt.write((int)currentTilt);
    }
  }
}

/* ================= FONCTIONS PILOTAGE ================= */
void setMotor(int index, int speed) {
  bool forward = (speed >= 0);
  digitalWrite(pinIN1[index], forward ? LOW : HIGH);
  digitalWrite(pinIN2[index], forward ? HIGH : LOW);
  analogWrite(pinENA[index], abs(speed));
}

void piloterSysteme(int angleJoy, int forceJoy) {
  if (isTankTurning) return; 

  float ratio = abs(forceJoy) / 100.0;
  int pwm = (int)(ratio * ratio * 255);
  if (pwm < 10) pwm = 0;
  if (forceJoy < 0) pwm = -pwm; 

  targetSpeed[0] = targetSpeed[1] = targetSpeed[2] = targetSpeed[3] = constrain(pwm, -255, 255);

  int angleServoOffset = map(angleJoy, -90, 90, 10, -10); 
  
  currentAngleS1 = 82 - angleServoOffset;
  currentAngleS2 = 81 - angleServoOffset;
  
  if (servosAttached) {
    servo1.write(currentAngleS1);
    servo2.write(currentAngleS2); 
  }
}

/* ================= LOGIQUE DE MISE A JOUR (50Hz) ================= */
void updateTurret() {
  if (!isTankTurning && (millis() - lastTurretUpdate >= 20)) { 
    if (speedPan != 0 || speedTilt != 0) {
      
      // --- MODIFICATION : INVERSION PAN --- (Le '-' au lieu du '+')
      currentPan = constrain(currentPan - (speedPan * 0.015), 0, 180);
      currentTilt = constrain(currentTilt + (speedTilt * 0.015), 20, 60); 
      
      if (servosAttached) {
        servoTourellePan.write((int)currentPan);
        servoTourelleTilt.write((int)currentTilt);
      }
    }
    lastTurretUpdate = millis();
  }
}

void updateMotors() {
  if (millis() - lastMotorUpdate >= 20) { 
    for (int i = 0; i < 4; i++) {
      if (currentSpeed[i] < targetSpeed[i]) {
        currentSpeed[i] = min(currentSpeed[i] + ACCEL_STEP, targetSpeed[i]);
      } else if (currentSpeed[i] > targetSpeed[i]) {
        currentSpeed[i] = max(currentSpeed[i] - ACCEL_STEP, targetSpeed[i]);
      }
      setMotor(i, currentSpeed[i]);
    }
    lastMotorUpdate = millis();
  }
}

/* ================= PAGE WEB (HTML/JS) ================= */
void handleRoot() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<title>Warthog Controller</title>
<style>
  body { margin: 0; padding: 0; height: 100vh; background: radial-gradient(circle at center, #1a2a33, #000); overflow: hidden; display: flex; flex-direction: column; align-items: center; justify-content: center; font-family: Arial, sans-serif; user-select: none; touch-action: none; }
  h2 { color: #00ffe1; letter-spacing: 2px; margin-bottom: 2px; font-size: 16px; text-align: center;}
  
  /* Zone Télémétrie Globale */
  #telemetry-container { display: flex; flex-direction: column; align-items: center; gap: 5px; margin-bottom: 10px; }
  
  /* Vitesses */
  .telemetry-row { display: flex; gap: 15px; background: rgba(0,0,0,0.6); padding: 5px 15px; border-radius: 8px; border: 1px solid #ffcc00; box-shadow: 0 0 10px rgba(255,204,0,0.15); }
  .telemetry-item { color: #ffcc00; font-family: monospace; font-size: 14px; font-weight: bold; text-align: center; }
  .telemetry-label { font-size: 9px; color: #aaa; margin-bottom: 2px; letter-spacing: 1px;}
  .telemetry-val { font-size: 18px; text-shadow: 0 0 5px #ffcc00; }
  
  /* Angles */
  .telemetry-row.angles { border-color: #00ffe1; box-shadow: 0 0 10px rgba(0,255,225,0.15); }
  .telemetry-row.angles .telemetry-item { color: #00ffe1; }
  .telemetry-row.angles .telemetry-val { text-shadow: 0 0 5px #00ffe1; font-size: 16px;}

  #joysticks-wrapper { display: flex; width: 100%; max-width: 700px; justify-content: space-evenly; align-items: center; gap: 5px;}
  .joy-box { display: flex; flex-direction: column; align-items: center; }
  .j-container { width: 130px; height: 130px; background: rgba(255, 255, 255, 0.05); border: 2px solid rgba(0, 255, 225, 0.4); border-radius: 50%; position: relative; box-shadow: 0 0 20px rgba(0, 255, 225, 0.1); display: flex; align-items: center; justify-content: center; touch-action: none;}
  .j-knob { width: 50px; height: 50px; background: linear-gradient(145deg, #0ff, #00b3a4); border-radius: 50%; position: absolute; box-shadow: 0 4px 10px rgba(0,0,0,0.6); cursor: pointer; touch-action: none;}
  .j-knob.turret { background: linear-gradient(145deg, #ff00ff, #a300a3); } 
  .data-display { margin-top: 5px; color: #00ffe1; font-family: monospace; font-size: 12px; background: rgba(0,0,0,0.5); padding: 5px; border-radius: 6px; border: 1px solid #00ffe1; min-width: 120px; text-align: center; line-height: 1.2;}
  #btn-tank { background: linear-gradient(145deg, #ff9900, #cc7a00); color: white; padding: 10px; border-radius: 50%; font-weight: bold; font-size: 12px; text-align: center; cursor: pointer; user-select: none; box-shadow: 0 4px 10px rgba(255, 153, 0, 0.4); border: 2px solid #ffcc00; display: flex; align-items: center; justify-content: center; width: 65px; height: 65px; transition: transform 0.1s; touch-action: none;}
  #btn-tank:active { transform: scale(0.90); background: #cc7a00; }
  
  #rotate-msg { display: none; color: #ff00ff; font-size: 20px; text-align: center; font-weight: bold; padding: 20px;}
  @media screen and (orientation: portrait) { #joysticks-wrapper, h2, #telemetry-container { display: none !important; } #rotate-msg { display: block; } }
</style>
</head>
<body>
  <h2>WARTHOG OS</h2>
  
  <div id="telemetry-container">
    <div class="telemetry-row">
      <div class="telemetry-item">
        <div class="telemetry-label">VIT. MOY.</div>
        <div class="telemetry-val"><span id="rpm">0</span> <span style="font-size:10px">km/h</span></div>
      </div>
    </div>
    
    <div class="telemetry-row angles">
      <div class="telemetry-item">
        <div class="telemetry-label">DIR G (S1)</div>
        <div class="telemetry-val"><span id="ang-s1">82</span>&deg;</div>
      </div>
      <div class="telemetry-item">
        <div class="telemetry-label">DIR D (S2)</div>
        <div class="telemetry-val"><span id="ang-s2">81</span>&deg;</div>
      </div>
      <div class="telemetry-item">
        <div class="telemetry-label">T. PAN</div>
        <div class="telemetry-val"><span id="ang-pan">90</span>&deg;</div>
      </div>
      <div class="telemetry-item">
        <div class="telemetry-label">T. TILT</div>
        <div class="telemetry-val"><span id="ang-tilt">50</span>&deg;</div>
      </div>
    </div>
  </div>

  <div id="rotate-msg">Veuillez tourner votre téléphone en mode paysage</div>
  
  <div id="joysticks-wrapper">
    <div class="joy-box">
      <div id="joystick-drive" class="j-container"><div id="knob-drive" class="j-knob"></div></div>
      <div class="data-display">CMD DIR: <span id="val-angle">0</span>&deg;<br>CMD VIT: <span id="val-speed">0</span>%</div>
    </div>
    <div id="btn-tank">G-TURN</div>
    <div class="joy-box">
      <div id="joystick-turret" class="j-container"><div id="knob-turret" class="j-knob turret"></div></div>
      <div class="data-display">PAN: <span id="val-pan">0</span>%<br>TILT: <span id="val-tilt">0</span>%</div>
    </div>
  </div>

<script>
  setInterval(() => {
    fetch('/telemetry').then(r => r.json()).then(data => {
      document.getElementById('rpm').innerText = data.rpm_mes.toFixed(0);
      document.getElementById('ang-s1').innerText = data.s1;
      document.getElementById('ang-s2').innerText = data.s2;
      document.getElementById('ang-pan').innerText = data.pan;
      document.getElementById('ang-tilt').innerText = data.tilt;
    }).catch(console.error);
  }, 500);

  function setupJoystick(containerId, knobId, onMove, onRelease) {
    const container = document.getElementById(containerId), knob = document.getElementById(knobId);
    let isDragging = false, startX, startY;
    const maxRadius = 35; 
    const startDrag = (e) => {
      isDragging = true; knob.style.transition = 'none';
      const rect = container.getBoundingClientRect();
      startX = rect.left + rect.width / 2; startY = rect.top + rect.height / 2;
      handleMove(e);
    };
    const moveDrag = (e) => { if (isDragging) { e.preventDefault(); handleMove(e); } };
    const endDrag = () => { isDragging = false; knob.style.transition = 'transform 0.2s ease-out'; knob.style.transform = `translate(0px, 0px)`; onRelease(); };
    const handleMove = (e) => {
      const clientX = e.touches ? e.touches[0].clientX : e.clientX;
      const clientY = e.touches ? e.touches[0].clientY : e.clientY;
      let deltaX = clientX - startX, deltaY = clientY - startY;
      const distance = Math.min(maxRadius, Math.hypot(deltaX, deltaY));
      const moveRad = Math.atan2(deltaY, deltaX);
      knob.style.transform = `translate(${Math.cos(moveRad) * distance}px, ${Math.sin(moveRad) * distance}px)`;
      onMove(deltaX / maxRadius, deltaY / maxRadius, distance / maxRadius);
    };
    knob.addEventListener('mousedown', startDrag); knob.addEventListener('touchstart', startDrag);
    window.addEventListener('mousemove', moveDrag); window.addEventListener('touchmove', moveDrag, { passive: false });
    window.addEventListener('mouseup', endDrag); window.addEventListener('touchend', endDrag);
  }

  let lastDriveTime = 0;
  setupJoystick('joystick-drive', 'knob-drive', (nx, ny, nd) => {
    let steering = Math.max(-90, Math.min(90, Math.round(nx * 90)));
    if (Math.abs(nx) < 0.3) steering = 0; 
    let strength = nd < 0.2 ? 0 : Math.round(nd * 100);
    if (ny > 0) strength = -strength; 

    document.getElementById('val-angle').innerText = steering;
    document.getElementById('val-speed').innerText = strength;

    if (Date.now() - lastDriveTime > 100) {
      lastDriveTime = Date.now();
      fetch(`/action?a=${steering}&s=${strength}`).catch(()=>{});
    }
  }, () => {
    document.getElementById('val-angle').innerText = 0; document.getElementById('val-speed').innerText = 0;
    fetch(`/action?a=0&s=0`).catch(()=>{});
  });

  let lastTurretTime = 0;
  setupJoystick('joystick-turret', 'knob-turret', (nx, ny, nd) => {
    let pSpeed = Math.abs(nx) > 0.2 ? Math.round(nx * 100) : 0;
    let tSpeed = Math.abs(ny) > 0.2 ? Math.round(ny * 100) : 0;
    document.getElementById('val-pan').innerText = pSpeed; document.getElementById('val-tilt').innerText = -tSpeed; 

    if (Date.now() - lastTurretTime > 100) {
      lastTurretTime = Date.now();
      fetch(`/turret?p=${pSpeed}&t=${tSpeed}`).catch(()=>{});
    }
  }, () => {
    document.getElementById('val-pan').innerText = 0; document.getElementById('val-tilt').innerText = 0;
    fetch(`/turret?p=0&t=0`).catch(()=>{});
  });

  const btnTank = document.getElementById('btn-tank');
  let tankActive = false;
  const setTankMode = (state, e) => {
    if(e) e.preventDefault();
    if(tankActive === state) return;
    tankActive = state;
    fetch(`/tankturn?state=${state ? 1 : 0}`).catch(()=>{});
  };
  btnTank.addEventListener('mousedown', (e) => setTankMode(true, e));
  btnTank.addEventListener('touchstart', (e) => setTankMode(true, e));
  ['mouseup','mouseleave','touchend','touchcancel'].forEach(evt => btnTank.addEventListener(evt, (e) => setTankMode(false, e)));
</script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", page);
}

/* ================= SERVEUR / SETUP ================= */
void setupServer() {
  server.on("/", handleRoot);
  
  server.on("/action", [](){
    lastMessageTime = millis();
    manageServos(); // On réveille les servos IMMÉDIATEMENT avant de leur donner un ordre
    if (server.hasArg("a") && server.hasArg("s")) {
      piloterSysteme(server.arg("a").toInt(), server.arg("s").toInt());
      server.send(200, "text/plain", "OK");
    }
  });
  
  server.on("/turret", [](){
    lastMessageTime = millis();
    manageServos(); // On réveille les servos
    if (server.hasArg("p") && server.hasArg("t")) {
      speedPan = server.arg("p").toInt(); speedTilt = server.arg("t").toInt();
      server.send(200, "text/plain", "OK");
    }
  });
  
  server.on("/tankturn", [](){
    lastMessageTime = millis();
    manageServos(); // On réveille les servos
    if (server.hasArg("state")) {
      isTankTurning = (server.arg("state").toInt() == 1);
      
      if (isTankTurning) {
        // Activation du mode Tank
        currentAngleS1 = 82; currentAngleS2 = 81; 
        if (servosAttached) { servo1.write(82); servo2.write(81); }
        targetSpeed[0] = targetSpeed[3] = 200;   
        targetSpeed[1] = targetSpeed[2] = -200;  
      } else {
        // --- MODIFICATION : RETOUR AU CENTRE ---
        currentAngleS1 = 82; currentAngleS2 = 81; 
        if (servosAttached) { servo1.write(82); servo2.write(81); }
        targetSpeed[0] = targetSpeed[1] = targetSpeed[2] = targetSpeed[3] = 0;
      }
      server.send(200, "text/plain", "OK");
    }
  });
  
  server.on("/telemetry", [](){
    String json = "{";
    json += "\"rpm_mes\":" + String(((rpmCapteur1 + rpmCapteur2) / 2.0)*0.09/60) + ",";
    json += "\"s1\":" + String(currentAngleS1) + ",";
    json += "\"s2\":" + String(currentAngleS2) + ",";
    json += "\"pan\":" + String((int)currentPan) + ",";
    json += "\"tilt\":" + String((int)currentTilt);
    json += "}";
    server.send(200, "application/json", json);
  });
  
  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + apIP.toString(), true); 
    server.send(302, "text/plain", ""); 
  });
  server.begin();
}

void setup() {
  Serial.begin(115200);
  
  lastMessageTime = millis(); // Init du timer

  pinMode(pinCapteur1, INPUT_PULLUP);
  pinMode(pinCapteur2, INPUT_PULLUP);

  for (int i = 0; i < 4; i++) {
    pinMode(pinENA[i], OUTPUT);
    pinMode(pinIN1[i], OUTPUT);
    pinMode(pinIN2[i], OUTPUT);
  }

  servo1.setPeriodHertz(50); servo1.attach(pinSERVO1, 500, 2400); servo1.write(currentAngleS1);
  servo2.setPeriodHertz(50); servo2.attach(pinSERVO2, 500, 2400); servo2.write(currentAngleS2); 
  servoTourellePan.setPeriodHertz(50); servoTourellePan.attach(pinTourellePan, 500, 2400); servoTourellePan.write((int)currentPan); 
  servoTourelleTilt.setPeriodHertz(50); servoTourelleTilt.attach(pinTourelleTilt, 500, 2400); servoTourelleTilt.write((int)currentTilt);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(ssid); 
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);
  setupServer();
}

/* ================= BOUCLE PRINCIPALE ================= */
void loop() {
  unsigned long now = millis();
  
  // Gestion de l'économie d'énergie des servos
  manageServos();
  
  // Lecture des capteurs avec Anti-Rebond (Debounce)
  bool currentState1 = digitalRead(pinCapteur1);
  if (currentState1 != lastStateCapteur1) {
    if (now - lastDebounceTime1 > DEBOUNCE_DELAY) {
      if (currentState1 == LOW) countCapteur1++;
      lastDebounceTime1 = now;
    }
    lastStateCapteur1 = currentState1;
  }

  bool currentState2 = digitalRead(pinCapteur2);
  if (currentState2 != lastStateCapteur2) {
    if (now - lastDebounceTime2 > DEBOUNCE_DELAY) {
      if (currentState2 == LOW) countCapteur2++;
      lastDebounceTime2 = now;
    }
    lastStateCapteur2 = currentState2;
  }

  if (now - lastTimeSpeedMeasured >= 100) {
    rpmCapteur1 = ((float)countCapteur1 / TICKS_PER_REV) * (60000.0 / 100.0);
    rpmCapteur2 = ((float)countCapteur2 / TICKS_PER_REV) * (60000.0 / 100.0);
    countCapteur1 = 0;
    countCapteur2 = 0;
    lastTimeSpeedMeasured = now;
  }

  dnsServer.processNextRequest();
  server.handleClient();
  
  updateTurret();
  updateMotors();
}