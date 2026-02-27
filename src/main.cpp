#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESP32Servo.h> 

/* ================= CONFIGURATION MATERIELLE ================= */

<<<<<<< HEAD
// --- Servomoteurs Direction (Voiture) ---
=======
// --- Servomoteurs --- je modifie ce commentaire car je suuis macéo
>>>>>>> 49f401feb9baed97937faa2853d6093b03b01b34
Servo servo1;
Servo servo2;
const int pinSERVO1 = 32;
const int pinSERVO2 = 33;

// --- Servomoteurs Tourelle ---
Servo servoTourellePan;  // Rotation Gauche/Droite
Servo servoTourelleTilt; // Inclinaison Haut/Bas
const int pinTourellePan = 18;
const int pinTourelleTilt = 19;

// --- Moteurs DC ---
const int pinENA1 = 15; const int pinIN11 = 2;  const int pinIN12 = 4;
const int pinENA2 = 5;  const int pinIN21 = 16; const int pinIN22 = 17;
const int pinENA3 = 25; const int pinIN31 = 26; const int pinIN32 = 27;
const int pinENA4 = 13; const int pinIN41 = 12; const int pinIN42 = 14;

/* ================= VARIABLES GLOBALES ================= */
// Tourelle
float currentPan = 90.0;
float currentTilt = 90.0;
int speedPan = 0;  
int speedTilt = 0; 
unsigned long lastTurretUpdate = 0;

// G-Turn
bool isTankTurning = false;

// --- NOUVEAU : GESTION DE L'ACCELERATION (ANTI PIC DE COURANT) ---
int targetSpeed[4]  = {0, 0, 0, 0}; // Vitesse voulue par l'utilisateur (-255 à 255)
int currentSpeed[4] = {0, 0, 0, 0}; // Vitesse réelle envoyée aux moteurs (-255 à 255)
unsigned long lastMotorUpdate = 0;

// REGLAGE DE L'ACCELERATION : 
// De combien de "PWM" la vitesse peut augmenter/diminuer toutes les 20ms.
// 10 = ~0.5 seconde pour passer de 0 à la vitesse max. Plus le chiffre est petit, plus c'est doux.
const int ACCEL_STEP = 10; 

/* ================= CONFIGURATION WIFI ================= */
const char* ssid = "MPW_controller"; 
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

WebServer server(80);
DNSServer dnsServer;

/* ================= FONCTIONS PILOTAGE ================= */

// Nouvelle version : Accepte directement une vitesse de -255 à 255
void applyMotorSpeed(int pinENA, int pinIN1, int pinIN2, int speed) {
  bool forward = (speed >= 0);
  int pwm = abs(speed);
  
  if (forward) {
    digitalWrite(pinIN1, LOW);
    digitalWrite(pinIN2, HIGH);
  } else {
    digitalWrite(pinIN1, HIGH);
    digitalWrite(pinIN2, LOW);
  }
  analogWrite(pinENA, pwm);
}

void piloterSysteme(int angleJoy, int forceJoy) {
  if (isTankTurning) return; // Sécurité : On bloque le joystick normal

  float ratio = abs(forceJoy) / 100.0;
  int pwm = (int)(ratio * ratio * 255);
  if (pwm < 10) pwm = 0;
  
<<<<<<< HEAD
  // Si marche arrière, on rend le pwm négatif
  if (forceJoy < 0) pwm = -pwm; 

  // On definit les CIBLES (avec leurs limites respectives de sécurité)
  targetSpeed[0] = constrain(pwm, -255, 255);
  targetSpeed[1] = constrain(pwm, -255, 255);
  targetSpeed[2] = constrain(pwm, -100, 100);
  targetSpeed[3] = constrain(pwm, -120, 120);

  // La direction réagit instantanément
  int angleServo = map(angleJoy, -90, 90, -10, 10);
  servo1.write(70 - angleServo);
  servo2.write(40 - angleServo); 
=======
  // Normalisation de la force entre 0.0 et 1.0
  float forceJoy_norm = abs(forceJoy) / 100.0;

  // Fonction quadratique pour la PWM (accélération plus progressive)
  int pwm = (int)(forceJoy_norm*forceJoy_norm*255);
  
  // Gestion du sens (avant / arrière)
  bool forward = 0;
  if (forceJoy >= 0)
  {
    forward = 1;
  }

  // Zone morte
  if (pwm < 10) pwm = 0;

  // Application aux 4 moteurs en même temps
  // (Mettre -forward si l'un des moteur tourne à l'envers)
  int intensite1 = 0, intensite2 = 0, intensite3 = 0, intensite4 = 0;
  intensite1 = constrain(pwm, 0, 255);
  intensite2 = constrain(pwm, 0, 255);
  intensite3 = constrain(pwm, 0, 100);
  intensite4 = constrain(pwm, 0, 120);

  setMotor(pinENA1, pinIN11, pinIN12, intensite1, forward);
  setMotor(pinENA2, pinIN21, pinIN22, intensite2, -forward); 
  setMotor(pinENA3, pinIN31, pinIN32, intensite3, forward);
  setMotor(pinENA4, pinIN41, pinIN42, intensite4, -forward);

  // Gestion des servos
  // L'angle Joystick arrive entre -90 (Gauche) et 90 (Droite). 0 = Centre.
  // On doit le mapper vers l'angle Servo (disons 45° à 135°, avec 90° au centre)
  int angleServo = map(angleJoy, -90, 90, 45, 135);
  
  // Contraintes de sécurité (limitation)
  angleServo = constrain(angleServo, 45, 135);

  // Application
  servo1.write(angleServo);
  servo2.write(180 - angleServo); // Miroir pour le 2ème servo
  
  // Affichage consol (debug)
  // Serial.printf("Joy: %d° %d%% | Moteurs: %d | Servo: %d°\n", angleJoy, forceJoy, pwm, angleServo);
>>>>>>> 49f401feb9baed97937faa2853d6093b03b01b34
}

/* ================= PAGE WEB (HTML/JS) ================= */
void handleRoot() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<title>Robot & Tourelle Controller</title>
<style>
  body { margin: 0; padding: 0; height: 100vh; background: radial-gradient(circle at center, #1a2a33, #000); overflow: hidden; display: flex; flex-direction: column; align-items: center; justify-content: center; font-family: Arial, sans-serif; user-select: none; touch-action: none; }
  h2 { color: #00ffe1; letter-spacing: 2px; margin-bottom: 10px; font-size: 18px; text-align: center;}
  
  #joysticks-wrapper { display: flex; width: 100%; max-width: 800px; justify-content: space-evenly; align-items: center; gap: 10px;}
  .joy-box { display: flex; flex-direction: column; align-items: center; }
  
  .j-container { width: 150px; height: 150px; background: rgba(255, 255, 255, 0.05); border: 2px solid rgba(0, 255, 225, 0.4); border-radius: 50%; position: relative; box-shadow: 0 0 30px rgba(0, 255, 225, 0.1); display: flex; align-items: center; justify-content: center; touch-action: none;}
  .j-knob { width: 60px; height: 60px; background: linear-gradient(145deg, #0ff, #00b3a4); border-radius: 50%; position: absolute; box-shadow: 0 4px 15px rgba(0,0,0,0.6); cursor: pointer; touch-action: none;}
  .j-knob.turret { background: linear-gradient(145deg, #ff00ff, #a300a3); } 

  .data-display { margin-top: 15px; color: #00ffe1; font-family: monospace; font-size: 14px; background: rgba(0,0,0,0.5); padding: 8px; border-radius: 8px; border: 1px solid #00ffe1; min-width: 140px; text-align: center;}
  
  #btn-tank { background: linear-gradient(145deg, #ff9900, #cc7a00); color: white; padding: 20px 20px; border-radius: 50%; font-weight: bold; font-size: 14px; text-align: center; cursor: pointer; user-select: none; box-shadow: 0 4px 15px rgba(255, 153, 0, 0.4); border: 3px solid #ffcc00; display: flex; align-items: center; justify-content: center; width: 80px; height: 80px; transition: transform 0.1s; touch-action: none;}
  #btn-tank:active { transform: scale(0.90); background: #cc7a00; }

  #rotate-msg { display: none; color: #ff00ff; font-size: 22px; text-align: center; font-weight: bold; padding: 20px;}
  
  @media screen and (orientation: portrait) {
    #joysticks-wrapper, h2 { display: none !important; }
    #rotate-msg { display: block; }
  }
</style>
</head>
<body>

  <h2>CONTROL SYSTEM</h2>
  <div id="rotate-msg">Veuillez tourner votre téléphone en mode paysage</div>

  <div id="joysticks-wrapper">
    <div class="joy-box">
      <div id="joystick-drive" class="j-container"><div id="knob-drive" class="j-knob"></div></div>
      <div class="data-display">
        DIR: <span id="val-angle">0</span>&deg; | VIT: <span id="val-speed">0</span>%
      </div>
    </div>

    <div id="btn-tank">G-TURN</div>

    <div class="joy-box">
      <div id="joystick-turret" class="j-container"><div id="knob-turret" class="j-knob turret"></div></div>
      <div class="data-display">
        PAN (Vit): <span id="val-pan">0</span>% | TILT (Vit): <span id="val-tilt">0</span>%
      </div>
    </div>
  </div>

<script>
  function setupJoystick(containerId, knobId, onMove, onRelease) {
    const container = document.getElementById(containerId);
    const knob = document.getElementById(knobId);
    let isDragging = false;
    let startX, startY;
    const maxRadius = 45; 

    const startDrag = (e) => {
      isDragging = true; knob.style.transition = 'none';
      const rect = container.getBoundingClientRect();
      startX = rect.left + rect.width / 2; startY = rect.top + rect.height / 2;
      handleMove(e);
    };

    const moveDrag = (e) => { if (isDragging) { e.preventDefault(); handleMove(e); } };

    const endDrag = () => {
      isDragging = false;
      knob.style.transition = 'transform 0.2s ease-out';
      knob.style.transform = `translate(0px, 0px)`;
      onRelease();
    };

    const handleMove = (e) => {
      const clientX = e.touches ? e.touches[0].clientX : e.clientX;
      const clientY = e.touches ? e.touches[0].clientY : e.clientY;
      let deltaX = clientX - startX;
      let deltaY = clientY - startY;
      const distance = Math.min(maxRadius, Math.hypot(deltaX, deltaY));
      const moveRad = Math.atan2(deltaY, deltaX);
      const moveX = Math.cos(moveRad) * distance;
      const moveY = Math.sin(moveRad) * distance;
      
      knob.style.transform = `translate(${moveX}px, ${moveY}px)`;
      onMove(deltaX / maxRadius, deltaY / maxRadius, distance / maxRadius);
    };

<<<<<<< HEAD
    knob.addEventListener('mousedown', startDrag); knob.addEventListener('touchstart', startDrag);
    window.addEventListener('mousemove', moveDrag); window.addEventListener('touchmove', moveDrag, { passive: false });
    window.addEventListener('mouseup', endDrag); window.addEventListener('touchend', endDrag);
  }
=======
    let deltaX = clientX - startX;
    let deltaY = clientY - startY;

    // Zone morte
    const steeringDeadZone = 15;
    if (Math.abs(deltaX) < steeringDeadZone)
    {
      deltaX = 0;
    }

    const distance = Math.min(maxRadius, Math.hypot(deltaX, deltaY));
    
    // --- LOGIQUE ANGLE CORRIGÉE ---
    // atan2(x, -y) permet d'avoir 0° en haut (Nord), 90° à droite, -90° à gauche
    let angleRad = Math.atan2(deltaX, -deltaY);
    let degree = Math.round(angleRad * (180 / Math.PI));
>>>>>>> 49f401feb9baed97937faa2853d6093b03b01b34

  // Joystick Deplacement
  let lastDriveTime = 0;
  setupJoystick('joystick-drive', 'knob-drive', (nx, ny, nd) => {
    let steering = Math.round(nx * 90);
    if (steering > 90) steering = 90;
    if (steering < -90) steering = -90;
    if (Math.abs(nx) < 0.3) steering = 0; 

    let strength = Math.round(nd * 100);
    if (nd < 0.2) strength = 0; 
    if (ny > 0) strength = -strength; 

    document.getElementById('val-angle').innerText = steering;
    document.getElementById('val-speed').innerText = strength;

    const now = Date.now();
    if (now - lastDriveTime > 100) {
      lastDriveTime = now;
      fetch(`/action?a=${steering}&s=${strength}`).catch(()=>{});
    }
  }, () => {
    document.getElementById('val-angle').innerText = 0;
    document.getElementById('val-speed').innerText = 0;
    fetch(`/action?a=0&s=0`).catch(()=>{});
  });

  // Joystick Tourelle
  let lastTurretTime = 0;
  setupJoystick('joystick-turret', 'knob-turret', (nx, ny, nd) => {
    let pSpeed = Math.abs(nx) > 0.2 ? Math.round(nx * 100) : 0;
    let tSpeed = Math.abs(ny) > 0.2 ? Math.round(ny * 100) : 0;

    document.getElementById('val-pan').innerText = pSpeed;
    document.getElementById('val-tilt').innerText = -tSpeed; 

    const now = Date.now();
    if (now - lastTurretTime > 100) {
      lastTurretTime = now;
      fetch(`/turret?p=${pSpeed}&t=${tSpeed}`).catch(()=>{});
    }
  }, () => {
    document.getElementById('val-pan').innerText = 0;
    document.getElementById('val-tilt').innerText = 0;
    fetch(`/turret?p=0&t=0`).catch(()=>{});
  });

  // Bouton G-TURN
  const btnTank = document.getElementById('btn-tank');
  let tankActive = false;

  const startTankTurn = (e) => {
    e.preventDefault();
    if (tankActive) return;
    tankActive = true;
    fetch('/tankturn?state=1').catch(()=>{});
  };

  const stopTankTurn = (e) => {
    e.preventDefault();
    if (!tankActive) return;
    tankActive = false;
    fetch('/tankturn?state=0').catch(()=>{});
  };

  btnTank.addEventListener('mousedown', startTankTurn);
  btnTank.addEventListener('touchstart', startTankTurn);
  btnTank.addEventListener('mouseup', stopTankTurn);
  btnTank.addEventListener('mouseleave', stopTankTurn);
  btnTank.addEventListener('touchend', stopTankTurn);
  btnTank.addEventListener('touchcancel', stopTankTurn);

</script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", page);
}

<<<<<<< HEAD
/* ================= SERVEUR / SETUP ================= */
void setupServer() {
  server.on("/", handleRoot);
  server.on("/generate_204", handleRoot); 
  server.on("/gen_204", handleRoot);
  server.on("/fwlink", handleRoot);
  server.on("/hotspot-detect.html", handleRoot);
=======
/* ================= SETUP & LOOP ================= */
void setup() {
  Serial.begin(115200);

  // Initialisation des MCCs
  pinMode(pinENA1, OUTPUT); pinMode(pinIN11, OUTPUT); pinMode(pinIN12, OUTPUT); // Moteur 1
  pinMode(pinENA2, OUTPUT); pinMode(pinIN21, OUTPUT); pinMode(pinIN22, OUTPUT); // Moteur 2
  pinMode(pinENA3, OUTPUT); pinMode(pinIN31, OUTPUT); pinMode(pinIN32, OUTPUT); // Moteur 3
  pinMode(pinENA4, OUTPUT); pinMode(pinIN41, OUTPUT); pinMode(pinIN42, OUTPUT); // Moteur 4

  // Initialisation des Servos
  servo1.setPeriodHertz(50);
  servo1.attach(pinSERVO1, 500, 2400);
  servo2.setPeriodHertz(50);
  servo2.attach(pinSERVO2, 500, 2400);
  
  // Position initiale
  servo1.write(90);
  servo2.write(90);

  // Init WiFi
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(ssid); 
  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);
  
  // Routes Serveur
  server.on("/", handleRoot);

  server.on("/generate_204", handleRoot);   // Android
  server.on("/gen_204", handleRoot);        // Android
  server.on("/chat", handleRoot);           // Android
  server.on("/fwlink", handleRoot);         // Apple
>>>>>>> 49f401feb9baed97937faa2853d6093b03b01b34

  server.on("/action", [](){
    if (server.hasArg("a") && server.hasArg("s")) {
      piloterSysteme(server.arg("a").toInt(), server.arg("s").toInt());
      server.send(200, "text/plain", "OK");
    }
  });

<<<<<<< HEAD
  server.on("/turret", [](){
    if (server.hasArg("p") && server.hasArg("t")) {
      speedPan = server.arg("p").toInt();
      speedTilt = server.arg("t").toInt();
      server.send(200, "text/plain", "OK");
    }
  });

  server.on("/tankturn", [](){
    if (server.hasArg("state")) {
      int state = server.arg("state").toInt();
      isTankTurning = (state == 1);

      if (isTankTurning) {
        servo1.write(70);
        servo2.write(40);

        int pwmSpeed = 200; 
        // On définit la CIBLE au lieu d'envoyer la commande directe
        targetSpeed[0] = pwmSpeed;   // Gauche Avant
        targetSpeed[1] = pwmSpeed;   // Gauche Avant
        targetSpeed[2] = -pwmSpeed;  // Droite Arrière (inversé)
        targetSpeed[3] = -pwmSpeed;  // Droite Arrière
      } else {
        // Retour à zéro doux
        targetSpeed[0] = 0;
        targetSpeed[1] = 0;
        targetSpeed[2] = 0;
        targetSpeed[3] = 0;
      }
      server.send(200, "text/plain", "OK");
    }
  });

  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + apIP.toString(), true); 
    server.send(302, "text/plain", ""); 
  });

  server.begin();
=======
  // On force à aller sur la page de la manette
  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + apIP.toString(), true); 
    server.send(302, "text/plain", ""); // Code 302 = Redirection temporaire
  });

  server.begin();
  
  // Debug
  // Serial.println("Systeme prêt. Connectez-vous au WiFi !");
>>>>>>> 49f401feb9baed97937faa2853d6093b03b01b34
}

void setup() {
  Serial.begin(115200);

  pinMode(pinENA1, OUTPUT); pinMode(pinIN11, OUTPUT); pinMode(pinIN12, OUTPUT);
  pinMode(pinENA2, OUTPUT); pinMode(pinIN21, OUTPUT); pinMode(pinIN22, OUTPUT);
  pinMode(pinENA3, OUTPUT); pinMode(pinIN31, OUTPUT); pinMode(pinIN32, OUTPUT);
  pinMode(pinENA4, OUTPUT); pinMode(pinIN41, OUTPUT); pinMode(pinIN42, OUTPUT);

  servo1.setPeriodHertz(50); servo1.attach(pinSERVO1, 500, 2400);
  servo2.setPeriodHertz(50); servo2.attach(pinSERVO2, 500, 2400);
  servo1.write(70); 
  servo2.write(40);

  servoTourellePan.setPeriodHertz(50); servoTourellePan.attach(pinTourellePan, 500, 2400);
  servoTourelleTilt.setPeriodHertz(50); servoTourelleTilt.attach(pinTourelleTilt, 500, 2400);
  servoTourellePan.write(90); 
  servoTourelleTilt.write(90);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP(ssid); 
  delay(500); 

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT, "*", apIP);
  
  setupServer();
  Serial.println("Systeme pret.");
}

/* ================= BOUCLE PRINCIPALE ================= */
void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  unsigned long now = millis();

  // --- 1. INTEGRATION TOURELLE (50Hz) ---
  if (!isTankTurning) {
    if (now - lastTurretUpdate >= 20) { 
      if (speedPan != 0 || speedTilt != 0) {
        currentPan += (speedPan / 100.0) * 1.5; 
        currentTilt -= (speedTilt / 100.0) * 1.5; 
        
        currentPan = constrain(currentPan, 0, 180);
        currentTilt = constrain(currentTilt, 0, 180);

        servoTourellePan.write((int)currentPan);
        servoTourelleTilt.write((int)currentTilt);
      }
      lastTurretUpdate = now;
    }
  }

  // --- 2. RAMPE D'ACCELERATION MOTEURS (50Hz) ---
  if (now - lastMotorUpdate >= 20) { 
    
    // Pour chaque moteur (0 à 3)
    for (int i = 0; i < 4; i++) {
      if (currentSpeed[i] < targetSpeed[i]) {
        currentSpeed[i] += ACCEL_STEP;
        if (currentSpeed[i] > targetSpeed[i]) currentSpeed[i] = targetSpeed[i]; // On sature si on dépasse
      } 
      else if (currentSpeed[i] > targetSpeed[i]) {
        currentSpeed[i] -= ACCEL_STEP;
        if (currentSpeed[i] < targetSpeed[i]) currentSpeed[i] = targetSpeed[i];
      }
    }

    // Application PHYSIQUE des vitesses adoucies
    applyMotorSpeed(pinENA1, pinIN11, pinIN12, currentSpeed[0]);
    applyMotorSpeed(pinENA2, pinIN21, pinIN22, currentSpeed[1]);
    applyMotorSpeed(pinENA3, pinIN31, pinIN32, currentSpeed[2]);
    applyMotorSpeed(pinENA4, pinIN41, pinIN42, currentSpeed[3]);

    lastMotorUpdate = now;
  }
}