#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESP32Servo.h> 

/* ================= CONFIGURATION MATERIELLE ================= */

// --- Servomoteurs ---
Servo servo1;
Servo servo2;

const int pinSERVO1 = 32;
const int pinSERVO2 = 33;

// --- Moteurs DC ---
// Moteur 1
const int pinENA1 = 15; const int pinIN11 = 2;  const int pinIN12 = 4;
// Moteur 2
const int pinENA2 = 5;  const int pinIN21 = 16; const int pinIN22 = 17;
// Moteur 3
const int pinENA3 = 25; const int pinIN31 = 26; const int pinIN32 = 27;
// Moteur 4
const int pinENA4 = 13; const int pinIN41 = 12; const int pinIN42 = 14;

/* ================= CONFIGURATION WIFI ================= */
const char* ssid = "MPW_controller"; 
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);

WebServer server(80);
DNSServer dnsServer;

/* ================= FONCTIONS PILOTAGE ================= */

// Fonction pour contrôler un moteur spécifique (évite la répétition)
void setMotor(int pinENA, int pinIN1, int pinIN2, int pwmVal, bool forward) {
  // Gestion de la direction
  if (forward) {
    digitalWrite(pinIN1, LOW);
    digitalWrite(pinIN2, HIGH);
  } else {
    digitalWrite(pinIN1, HIGH);
    digitalWrite(pinIN2, LOW);
  }

  // Écriture PWM (0-255)
  analogWrite(pinENA, pwmVal);
}

// Fonction pour passer des infos renvoyés par l'interface web en commandes pour le système réel
void piloterSysteme(int angleJoy, int forceJoy) {
  
  // Gestion des MCC (en vitesse)
  // L'info arrive entre -100 (Arrière) et 100 (Avant)
  // On mappe 0-100 vers 0-255 (PWM)
  int pwm = map(abs(forceJoy), 0, 100, 0, 255);
  bool forward = (forceJoy >= 0);

  // Zone morte
  if (abs(forceJoy) < 5) pwm = 0;

  // Application aux 4 moteurs en même temps
  // (Mettre -forward si l'un des moteur tourne à l'envers)
  setMotor(pinENA1, pinIN11, pinIN12, pwm, forward);
  setMotor(pinENA2, pinIN21, pinIN22, pwm, forward); 
  setMotor(pinENA3, pinIN31, pinIN32, pwm, forward);
  setMotor(pinENA4, pinIN41, pinIN42, pwm, forward);

  // Gestion des servos
  // L'angle Joystick arrive entre -90 (Gauche) et 90 (Droite). 0 = Centre.
  // On doit le mapper vers l'angle Servo (disons 45° à 135°, avec 90° au centre)
  int angleServo = map(angleJoy, -90, 90, 45, 135);
  
  // Contraintes de sécurité (limitation)
  angleServo = constrain(angleServo, 45, 135);

  // Application
  servo1.write(angleServo);
  servo2.write(180 - angleServo); // Miroir pour le 2ème servo
  
  // Affichage
  Serial.printf("Joy: %d° %d%% | Moteurs: %d | Servo: %d°\n", angleJoy, forceJoy, pwm, angleServo);
}

/* ================= PAGE WEB (HTML/JS) ================= */
void handleRoot() {
  String page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<title>Robot Controller</title>
<style>
  body { margin: 0; padding: 0; height: 100vh; background: radial-gradient(circle at center, #1a2a33, #000); overflow: hidden; display: flex; flex-direction: column; align-items: center; justify-content: center; font-family: Arial, sans-serif; user-select: none; touch-action: none; }
  h2 { color: #00ffe1; letter-spacing: 2px; margin-bottom: 20px; }
  #joystick-container { width: 220px; height: 220px; background: rgba(255, 255, 255, 0.05); border: 2px solid rgba(0, 255, 225, 0.4); border-radius: 50%; position: relative; box-shadow: 0 0 30px rgba(0, 255, 225, 0.1); display: flex; align-items: center; justify-content: center; }
  #joystick-knob { width: 80px; height: 80px; background: linear-gradient(145deg, #0ff, #00b3a4); border-radius: 50%; position: absolute; box-shadow: 0 4px 15px rgba(0,0,0,0.6); cursor: pointer; }
  #data-display { margin-top: 30px; color: #00ffe1; font-family: monospace; font-size: 16px; background: rgba(0,0,0,0.5); padding: 10px 20px; border-radius: 10px; border: 1px solid #00ffe1; min-width: 200px; text-align: center;}
</style>
</head>
<body>
  <h2>CONTROL</h2>
  <div id="joystick-container"><div id="joystick-knob"></div></div>
  <div id="data-display">
    DIR: <span id="val-angle">0</span>&deg;<br>
    VIT: <span id="val-speed">0</span>%
  </div>
<script>
  const container = document.getElementById('joystick-container');
  const knob = document.getElementById('joystick-knob');
  const dispAngle = document.getElementById('val-angle');
  const dispSpeed = document.getElementById('val-speed');
  
  const maxRadius = 70; 
  const deadZone = 10;
  let isDragging = false;
  let startX, startY;
  let lastSentTime = 0;
  const sendInterval = 100; // Envoi toutes les 100ms max

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
    updateDisplay(0, 0); 
    sendData(0, 0, true); // Reset moteur à 0
  };

  function handleMove(e) {
    const clientX = e.touches ? e.touches[0].clientX : e.clientX;
    const clientY = e.touches ? e.touches[0].clientY : e.clientY;

    let deltaX = clientX - startX;
    let deltaY = clientY - startY;
    const distance = Math.min(maxRadius, Math.hypot(deltaX, deltaY));
    
    // --- LOGIQUE ANGLE CORRIGÉE ---
    // atan2(x, -y) permet d'avoir 0° en haut (Nord), 90° à droite, -90° à gauche
    let angleRad = Math.atan2(deltaX, -deltaY);
    let degree = Math.round(angleRad * (180 / Math.PI));

    // Limiter l'angle envoyé pour rester cohérent (optionnel, ici on clamp à -90/90 pour l'affichage si on veut juste gauche/droite)
    // Mais pour le pilotage complet, on garde le 360° ou on adapte.
    // Si on recule (bas), l'angle va passer à 180/-180.
    // Pour simplifier le servo : on ne regarde que la composante X si on veut juste tourner
    // VOICI L'ASTUCE POUR "Juste Gauche Droite" peu importe si on avance ou recule :
    
    // On mappe directement la position X (-maxRadius à +maxRadius) vers (-90 à +90)
    // C'est beaucoup plus intuitif pour une voiture télécommandée
    let steering = Math.round((deltaX / maxRadius) * 90);
    if (steering > 90) steering = 90;
    if (steering < -90) steering = -90;

    // --- LOGIQUE VITESSE ---
    let strength = Math.round((distance / maxRadius) * 100);
    if (distance < deadZone) strength = 0;
    if (deltaY > 0) strength = -strength; // Reculer si on tire vers le bas

    // Mise à jour graphique
    const moveRad = Math.atan2(deltaY, deltaX);
    const moveX = Math.cos(moveRad) * distance;
    const moveY = Math.sin(moveRad) * distance;
    knob.style.transform = `translate(${moveX}px, ${moveY}px)`;

    // On envoie "steering" (guidage -90/90) au lieu de l'angle polaire
    updateDisplay(steering, strength);
    sendData(steering, strength, false);
  }

  function updateDisplay(a, s) {
    dispAngle.innerText = a;
    dispSpeed.innerText = s;
  }

  function sendData(angle, speed, forceSend) {
    const now = Date.now();
    if (forceSend || (now - lastSentTime > sendInterval)) {
      lastSentTime = now;
      fetch(`/action?a=${angle}&s=${speed}`).catch(console.log);
    }
  }

  knob.addEventListener('mousedown', startDrag); knob.addEventListener('touchstart', startDrag);
  window.addEventListener('mousemove', moveDrag); window.addEventListener('touchmove', moveDrag, { passive: false });
  window.addEventListener('mouseup', endDrag); window.addEventListener('touchend', endDrag);

</script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", page);
}

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
  dnsServer.start(DNS_PORT, "*", apIP);
  
  // Routes Serveur
  server.on("/", handleRoot);
  server.on("/action", [](){
    if (server.hasArg("a") && server.hasArg("s")) {
      // Réception des données du Joystick
      int valAngle = server.arg("a").toInt();
      int valSpeed = server.arg("s").toInt();
      
      // Appel de la fonction physique
      piloterSysteme(valAngle, valSpeed);
      
      server.send(200, "text/plain", "OK");
    }
  });
  server.onNotFound(handleRoot);
  server.begin();
  
  Serial.println("Systeme pret. Connectez-vous au WiFi !");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  // Pas de delay ici pour garder le serveur réactif
}