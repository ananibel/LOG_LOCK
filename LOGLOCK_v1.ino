#include <Wire.h>
#include "OPT3001.h"
#include <SPI.h>
#include <Screen_HX8353E.h>
#include <LCD_utilities.h>
#include <Terminal12e.h>
#include "change_icon.h" 

#include "utils.h"   // aquí asumo que defines NOTE_* y los arrays capibara_img y hamsterImage

// ==== PANTALLA ====
Screen_HX8353E myScreen;
uint16_t verdeOscuro = myScreen.calculateColour(0, 150, 0);

bool sonidoEnProgreso = false;

// ==== SENSORES ====
opt3001 opt3001;
const int xpin = 23, ypin = 24, zpin = 25;

// ==== BOTONES ====
const int pinB1 = 33;
const int pinB2 = 32;
const int pinB3 = 5;

// ==== BUZZER ====
const int buzzerPin = 40;

// ==== LED RGB ====
const int RED_PIN = 39;
const int GREEN_PIN = 38;
const int BLUE_PIN = 37;

// === Joystick ===
enum JoyDir { JOY_NONE, JOY_LEFT, JOY_RIGHT, JOY_UP, JOY_DOWN };

void dibujarNumpad(int sel);
JoyDir leerDireccionJoystickDiscreta();
void moverSeleccion(JoyDir d);

#define JOY_X_PIN  2
#define JOY_Y_PIN  26
const int DEADZONE = 300;   // ajusta a tu gusto
int joyCX = 512, joyCY = 512;
bool joyNeutral = true;

// --- Numpad ---
int seleccionado = 1;       // foco inicial
int cantidadIngresada = 0;  // cuántos dígitos lleva el usuario (0..len_ent)

// ==== SISTEMA ====
int restX, restY, restZ;         // baseline acelerómetro

int PWR[3]    = {1, 1, 3};       // contraseña de usuario
int PWRADM[3] = {1, 2, 1};       // contraseña de administrador

int entradaUsuario[3];
int nuevaEntradaUsuario[3];
int index_pwr = 0;

int intentos = 0;
int intmax = 4;
int len_ent = 3;
bool sistemaBloqueado = false;

int D1 = 0;  // Puerta (0 cerrada, 1 abierta)
int lux_flag = 1;

String historialHora[5];
String historialResultado[5];
int historialIndex = 0;
int historialCantidad = 0;

const int maxtime = 100; // ms "máximos" antes de verificar (como lo tenías)
int timeCont = 0;

// ==== ESTADOS ====
enum Estado {
  ESTADO_INICIO,
  ESTADO_INGRESO,
  ESTADO_VERIFICAR,
  ESTADO_BLOQUEO,
  ESTADO_VERIFICAR_ADMIN,
  ESTADO_CAMBIAR_PASS
};
Estado estadoActual = ESTADO_INICIO;

// ==== UMBRALES ====
const int UMBRAL_RUIDO = 100;  // para LED movimiento

// -----------------------------------------------------------------------------
// PROTOTIPOS
// -----------------------------------------------------------------------------
void pantallaInicio();
void mostrarIngresoClave(int cantidadIngresada, int totalDigitos, int intentosRestantes);
void mostrarIngresoBloqueo(int cantidadIngresada, int totalDigitos);
void contrIncorrecta();
void contrCorrecta();
void dibujarCapibara();
void mostrarEventos(String horas[5], String estado[5]);
void drawImage();

int  leerBoton();
void Antirrebote(int pin);
void verificarContrasena();
void verificarContrasena_ADMIN();

void setLedColor(int r,int g,int b);
void calibrateAccelerometer();
void gestionarLedDeEstado();
void Lux_check();
void Puerta_Abierta();

void playSuccessSound();
void playErrorSound();
void playFatalErrorSound();
void playSirenSound();

bool botonPresionado(int pin) { 
  if (digitalRead(pin) == LOW) { 
    while (digitalRead(pin) == LOW) delay(10); 
    delay(80); 
    return true; 
  } 
  return false; 
}

int valorSeleccionado() {
  // mapea seleccionado (1..9, 0, 10[K]) -> dígito (0..9) o -1 si “K”
  if (seleccionado == 10) return -1;  // K
  if (seleccionado == 0)  return 0;
  return seleccionado;                // 1..9
}


// -----------------------------------------------------------------------------
// SETUP
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(9600);
  delay(1000);

  opt3001.begin();
  Serial.println("OPT3001 Inicializado.");

  pinMode(pinB1, INPUT_PULLUP);
  pinMode(pinB2, INPUT_PULLUP);
  pinMode(pinB3, INPUT_PULLUP);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  
  calibrateAccelerometer();
  calibrarJoystick();

  myScreen.begin();
  myScreen.clear(blackColour);
  myScreen.setOrientation(0);

}

// -----------------------------------------------------------------------------
// MAIN LOOP
// -----------------------------------------------------------------------------
void loop() {
  Lux_check();
  gestionarLedDeEstado();

  switch (estadoActual) {
    case ESTADO_INICIO:
      pantallaInicio();
      while (leerBoton() == 0) {
        Lux_check();
        gestionarLedDeEstado();
        delay(50);
      }
      index_pwr = 0;
      cantidadIngresada = 0;
      seleccionado = 1;            // foco inicial
      estadoActual = ESTADO_INGRESO;
      myScreen.clear(blackColour);
      dibujarNumpad(seleccionado, cantidadIngresada, len_ent);
      break;

    case ESTADO_INGRESO: {
      //leer joystick
      JoyDir d = leerDireccionJoystickDiscreta();
      if (d != JOY_NONE) {
        moverSeleccion(d);
        dibujarNumpad(seleccionado, cantidadIngresada, len_ent);
      }
    
      //botón borrar (pinB2)
      if (botonPresionado(pinB2) && cantidadIngresada > 0) {
        cantidadIngresada--;
        dibujarNumpad(seleccionado, cantidadIngresada, len_ent);
      }
    
      //botón aceptar/agregar (pinB1)
      if (botonPresionado(pinB1)) {
        int v = valorSeleccionado();
    
        // si está en K -> confirmar
        if (v < 0) {
          if (cantidadIngresada == len_ent) {
            // vamos a verificar
            index_pwr = len_ent;     // marca entrada completa
            estadoActual = ESTADO_VERIFICAR;
          } else {
            playErrorSound();
          }
          break;
        }
    
        // si es un dígito y aún hay espacio
        if (cantidadIngresada < len_ent) {
          entradaUsuario[cantidadIngresada] = v; // 0..9
          cantidadIngresada++;
          dibujarNumpad(seleccionado, cantidadIngresada, len_ent);
    
          // auto-verificar al completar longitud
          if (cantidadIngresada == len_ent) {
            estadoActual = ESTADO_VERIFICAR;
          }
        }
      }
    
      // (opcional) timeout
      if (timeCont++ > maxtime) {
        timeCont = 0;
        estadoActual = ESTADO_VERIFICAR;
      }
      break;
    }

    case ESTADO_VERIFICAR:
      verificarContrasena(); // admin cambiar contraseña
      if (estadoActual == ESTADO_VERIFICAR) { // Si no se cambió el estado a cambiar pass
          if (sistemaBloqueado) {
              estadoActual = ESTADO_BLOQUEO;
              setLedColor(0, 0, 0);
              playFatalErrorSound();
              cantidadIngresada = 0;
              seleccionado = 1;
              myScreen.clear(blackColour);
              mostrarIngresoBloqueo(0, len_ent);
          } else {
              estadoActual = ESTADO_INICIO;
          }
      }
      delay(1500);
      break;
    
    case ESTADO_BLOQUEO: {
      // joystick para mover foco
      JoyDir d = leerDireccionJoystickDiscreta();
      if (d != JOY_NONE) {
        moverSeleccion(d);
        dibujarNumpad(seleccionado, cantidadIngresada, len_ent);
      }
    
      // borrar (B2)
      if (botonPresionado(pinB2) && cantidadIngresada > 0) {
        cantidadIngresada--;
        dibujarNumpad(seleccionado, cantidadIngresada, len_ent);
      }
    
      // agregar / OK (B1)
      if (botonPresionado(pinB1)) {
        int v = valorSeleccionado();   // 0..9 o -1 si “K”
        if (v < 0) { // K: confirmar
          if (cantidadIngresada == len_ent) {
            estadoActual = ESTADO_VERIFICAR_ADMIN;
          } else {
            playErrorSound(); // clave incompleta
          }
        } else {
          if (cantidadIngresada < len_ent) {
            entradaUsuario[cantidadIngresada] = v;
            cantidadIngresada++;
            dibujarNumpad(seleccionado, cantidadIngresada, len_ent);
            if (cantidadIngresada == len_ent) {
              estadoActual = ESTADO_VERIFICAR_ADMIN;
            }
          }
        }
      }
    } break;


    case ESTADO_VERIFICAR_ADMIN:
      verificarContrasena_ADMIN();
      delay(800);
      if (sistemaBloqueado) {
        estadoActual = ESTADO_BLOQUEO;
        cantidadIngresada = 0;
        seleccionado = 1;
        myScreen.clear(blackColour);
        dibujarNumpad(seleccionado, cantidadIngresada, len_ent);
      } else {
        intentos = 0;
        estadoActual = ESTADO_INICIO;
      }
      break;
      
      case ESTADO_CAMBIAR_PASS: {
  // La lógica para leer el joystick y el botón de borrar no cambia
  JoyDir d = leerDireccionJoystickDiscreta();
  if (d != JOY_NONE) {
    moverSeleccion(d);
    dibujarNumpad(seleccionado, cantidadIngresada, len_ent);
  }

  if (botonPresionado(pinB2) && cantidadIngresada > 0) {
    cantidadIngresada--;
    dibujarNumpad(seleccionado, cantidadIngresada, len_ent);
  }

  // --- LÓGICA DE DOBLE CONFIRMACIÓN (AUTOMÁTICA Y MANUAL) ---
  if (botonPresionado(pinB1)) {
    int v = valorSeleccionado();

    // 1. INTENTAR AÑADIR UN DÍGITO
    // Solo se puede añadir si hay espacio en el buffer.
    if (v >= 0 && cantidadIngresada < len_ent) {
      nuevaEntradaUsuario[cantidadIngresada] = v;
      cantidadIngresada++;
      dibujarNumpad(seleccionado, cantidadIngresada, len_ent);
    }

    // 2. COMPROBAR SI LA CONTRASEÑA ESTÁ LISTA PARA SER GUARDADA
    // Esto se cumple si (A) se acaban de meter 3 dígitos o (B) ya hay 3 y se presiona 'K'.
    if (cantidadIngresada == len_ent) {
      int valorActual = valorSeleccionado();
      // Si la última acción fue meter el 3er dígito O si se presionó 'K' con 3 dígitos ya metidos
      if (v >= 0 || valorActual < 0) { 
        
        // ¡ÉXITO! Guardar la nueva contraseña.
        for (int i = 0; i < len_ent; i++) {
          PWR[i] = nuevaEntradaUsuario[i];
        }
        
       // myScreen.clear(blackColour);
        myScreen.clear(blackColour);
        myScreen.gText(20, 50, "Clave cambiada", whiteColour);
        myScreen.gText(28, 66, "con exito!", whiteColour);
        setLedColor(0, 0, 0);
        playSuccessSound();
        delay(2000);
        
        estadoActual = ESTADO_INICIO;
      }
    }
  }
  break;
}
  }

  delay(100);
}

// -----------------------------------------------------------------------------
// BOTONES
// -----------------------------------------------------------------------------
int leerBoton() {
  if (digitalRead(pinB1) == LOW) { Antirrebote(pinB1); return 1; }
  if (digitalRead(pinB2) == LOW) { Antirrebote(pinB2); return 2; }
  if (digitalRead(pinB3) == LOW) { Antirrebote(pinB3); return 3; }
  return 0;
}

void Antirrebote(int pin) {
  while (digitalRead(pin) == LOW) {
    delay(10);
  }
  delay(100);
}

// -----------------------------------------------------------------------------
// VERIFICACIONES
// -----------------------------------------------------------------------------
void verificarContrasena() {
  bool correcta = true;
  bool admin = true;

  // Comprueba si es la contraseña de usuario
  for (int i = 0; i < len_ent; i++) {
    if (entradaUsuario[i] != PWR[i]) { correcta = false; }
  }

  // Comprueba si es la contraseña de administrador
  for (int i = 0; i < len_ent; i++) {
    if (entradaUsuario[i] != PWRADM[i]) { admin = false; }
  }
  // Hora simple mm:ss desde millis
  unsigned long segundos = millis() / 1000;
  unsigned long minutos  = segundos / 60;
  segundos %= 60;
  String hora = String(minutos) + ":" + (segundos < 10 ? "0" : "") + String(segundos);

  // Historial
  historialHora[historialIndex] = hora;
  historialResultado[historialIndex] = correcta ? "OK" : "ERR";
  historialIndex = (historialIndex + 1) % 5;
  if (historialCantidad < 5) historialCantidad++;

  // Resultado UI + efectos
if (correcta) {
    // ACCESO CONCEDIDO (USUARIO NORMAL)
    D1 = 1;
    contrCorrecta();
    intentos = 0;
    setLedColor(0, 0, 0);
    playSuccessSound();
    Puerta_Abierta();
    mostrarEventos(historialHora, historialResultado);
  } else if (admin && !sistemaBloqueado) {
    // CONTRASEÑA DE ADMIN INGRESADA (SISTEMA NO BLOQUEADO)
    // Inicia el proceso de cambio de contraseña
    estadoActual = ESTADO_CAMBIAR_PASS;
    cantidadIngresada = 0;
    seleccionado = 1;
    drawChangePasswordScreen();
    playSuccessSound();
    delay(2000);
    dibujarNumpad(seleccionado, cantidadIngresada, len_ent);
  } else {
    // ACCESO DENEGADO
    intentos++;
    D1 = 0;
    contrIncorrecta();
    setLedColor(0, 0, 0);
    playErrorSound();
    if (intentos >= intmax) {
      sistemaBloqueado = true;
    }
  }
}

void verificarContrasena_ADMIN() {
  bool correcta = true;
  for (int i = 0; i < 3; i++) {
    if (entradaUsuario[i] != PWRADM[i]) { correcta = false; break; }
  }

  if (correcta) {
    contrCorrecta();
    playSuccessSound();
    sistemaBloqueado = false;
  } else {
    contrIncorrecta();
    playErrorSound();
    sistemaBloqueado = true;
  }
}

// -----------------------------------------------------------------------------
// OLED: Pantallas
// -----------------------------------------------------------------------------
void pantallaInicio() {
  myScreen.clear(blackColour);
  myScreen.setFontSolid(true);
  myScreen.setFontSize(1);
  myScreen.gText(5, 5,  "Acceso a ", whiteColour, blackColour);
  myScreen.gText(5, 20, "Gabinete Seguro", whiteColour, blackColour);
  myScreen.setFontSize(0.9);
  myScreen.gText(10, 100, "Presiona un boton", whiteColour, blackColour);
  myScreen.gText(10, 115, "para entrar ...", whiteColour, blackColour);
  dibujarCapibara();
}

void dibujarNumpad(int sel, int cantidad, int total) {
  myScreen.clear(blackColour);
  myScreen.setFontSize(2);
  myScreen.setFontSolid(true);

  const int btnW=12, btnH=12, margin=6;
  const int startX=40, startY=50;
  int x,y,n=1;
  
  // Título + progreso de clave
  myScreen.setFontSize(1);
  if (estadoActual == ESTADO_BLOQUEO){
    myScreen.gText(36, 5, "SISTEMA ", redColour);
    myScreen.gText(30, 2+16, "BLOQUEADO", redColour);
  }
  else{
    myScreen.gText(5, 10, "Ingresa clave", whiteColour);
  }
  
  int caracterAncho = myScreen.fontSizeX();
  int espacio = 10;
  int totalAncho = total * caracterAncho + (total - 1) * espacio;
  int xInicio = (myScreen.screenSizeX() - totalAncho) / 2;

  for (int i = 0; i < total; i++) {
    int cx = xInicio + i * (caracterAncho + espacio);
    myScreen.gText(cx+1, 36, (i < cantidad) ? "*" : "_", whiteColour);
  }

  // 1..9
  for (int row=0; row<3; row++) {
    for (int col=0; col<3; col++) {
      x = startX + col*(btnW+margin);
      y = startY + row*(btnH+margin);
      if (sel == n) myScreen.gText(x+3, y+3, String(n), blackColour, yellowColour);
      else          myScreen.gText(x+3, y+3, String(n), blackColour, whiteColour);
      n++;
    }
  }

  // 0 (centro abajo)
  x = startX + (btnW+margin);
  y = startY + 3*(btnH+margin);
  if (sel == 0) myScreen.gText(x+3, y+3, "0", blackColour, yellowColour);
  else          myScreen.gText(x+3, y+3, "0", blackColour, whiteColour);

  // OK (derecha abajo, usamos “K”)
  //x = startX + 2*(btnW+margin);
  //if (sel == 10) myScreen.gText(x+3, y+3, "K", blackColour, yellowColour);
  //else           myScreen.gText(x+3, y+3, "K", blackColour, whiteColour);

  // Pie de ayuda
  myScreen.setFontSize(0.8);
  myScreen.gText(5, 120, "B1=OK      B2=Borrar", whiteColour);
}


void mostrarIngresoClave(int cantidadIngresada, int totalDigitos, int intentosRestantes) {
  myScreen.setFontSize(0.9);
  myScreen.setFontSolid(true);
  myScreen.gText(5, 10, "Ingresa una clave ", whiteColour);
  myScreen.gText(5, 26, "de " + String(totalDigitos) + " digitos", whiteColour);

  myScreen.setFontSize(1);
  int caracterAncho = myScreen.fontSizeX();
  int espacio = 10;
  int totalAncho = totalDigitos * caracterAncho + (totalDigitos - 1) * espacio;
  int xInicio = (myScreen.screenSizeX() - totalAncho) / 2;
  int y = 60;

  for (int i = 0; i < totalDigitos; i++) {
    int x = xInicio + i * (caracterAncho + espacio);
    if (i < cantidadIngresada) myScreen.gText(x, y, "*", whiteColour);
    else                       myScreen.gText(x, y, "_", whiteColour);
  }

  myScreen.setFontSize(0.9);
  myScreen.gText(5, 100, "Intent. Restantes: " + String(intentosRestantes), whiteColour);
}

void mostrarIngresoBloqueo(int cantidadIngresada, int totalDigitos) {
  Serial.println("SISTEMA BLOQUEADO: 4 intentos fallidos consecutivos.");
  myScreen.setFontSize(1);
  myScreen.setFontSolid(true);
  myScreen.gText(34, 10,        "SISTEMA ", redColour);
  myScreen.gText(28, 26, "BLOQUEADO", redColour);

  dibujarNumpad(seleccionado, cantidadIngresada, totalDigitos);

  //myScreen.setFontSize(0.8);
  //myScreen.gText(10, 100,      "Ingresar Contr.", whiteColour);
  //myScreen.gText(10, 116,             "Master", whiteColour);
}

void contrIncorrecta() {
  myScreen.clear(redColour);
  myScreen.setFontSize(2);
  myScreen.setFontSolid(true);
  myScreen.gText(23, 50, "Contrasena", whiteColour, redColour);
  myScreen.gText(23, 66, "Incorrecta", whiteColour, redColour);
}

void contrCorrecta() {
  uint16_t verdeOsc = myScreen.calculateColour(0, 100, 0);
  myScreen.clear(verdeOsc);
  myScreen.setFontSize(1);
  myScreen.setFontSolid(true);
  myScreen.gText(40, 50, "Acceso",    whiteColour, verdeOsc);
  myScreen.gText(28, 66, "Concedido", whiteColour, verdeOsc);
}

void dibujarCapibara() {
  int w = 46, h = 46;
  int index = 0;
  // Posición centrada aproximada (ajusta si quieres)
  int px = 42, py = 42;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      myScreen.point(px + x, py + y, capibara_img[index++]);
    }
  }
}

void mostrarEventos(String horas[5], String estado[5]) {
  myScreen.clear(blackColour);
  myScreen.setFontSize(0.9);
  myScreen.setFontSolid(true);
  myScreen.gText(16, 10, "Ultimos Intentos", whiteColour);

 myScreen.setFontSize(1);
  for (int i = 0; i < 5; i++) {
    uint16_t y = 32 + i * 16; // Espaciado vertical por línea
    uint16_t fondo;
    
    if (estado[i] == "OK") {
      fondo = verdeOscuro;
    } else if (estado[i] == "ERR") {
      fondo = redColour;
    } else {
      fondo = blackColour;  // Por si hay un estado inválido
    }

    // Escribe la línea
    myScreen.setFontSize(1);
    myScreen.gText(28, y, horas[i] + "  " + estado[i],whiteColour, fondo);
  }
}

void drawAccesoNoAutorizado() {

  myScreen.clear(redColour);
  myScreen.setFontSolid(true);

  // Coordenadas para el triángulo de advertencia
  int cx = myScreen.screenSizeX() / 2; // centro X
  int cy = 40;                         // centro Y del triángulo
  int size = 40;                       // tamaño del triángulo

  // Vértices del triángulo
  int x1 = cx;
  int y1 = cy - size / 2;
  int x2 = cx - size / 2;
  int y2 = cy + size / 2;
  int x3 = cx + size / 2;
  int y3 = cy + size / 2;

  // Triángulo de advertencia amarillo
  myScreen.setPenSolid(true);
  myScreen.triangle(x1, y1, x2, y2, x3, y3, yellowColour);

  // Texto “/!\” centrado dentro del triángulo
  myScreen.setFontSize(5);
  myScreen.gText(cx - 4 , cy, "!", blackColour, yellowColour);

  // Texto de advertencia abajo
  myScreen.setFontSize(1);
  myScreen.gText(42, 75, "Acceso", whiteColour, redColour);
  myScreen.gText(15, 75 + 16, "No Autorizado", whiteColour, redColour);

  delay(1000);
  
  // Dibuja hamsterImage de 64x64 escalado x2 en (0,0)
  const int source_width = 64;
  const int source_height = 64;
  const int scale = 2;
  int index = 0;

  for (int y = 0; y < source_height; y++) {
    for (int x = 0; x < source_width; x++) {
      uint16_t color = hamsterImage[index++];
      int sx = x * scale;
      int sy = y * scale;
      myScreen.dRectangle(sx, sy, scale, scale, color);
    }
  }
}


// --- FUNCIÓN PARA DIBUJAR LA PANTALLA DE CAMBIO DE CONTRASEÑA (CON ESCALADO 1.5X) ---
void drawChangePasswordScreen() {
  myScreen.clear(blackColour);

  // Dibujar texto al doble de tamaño
  myScreen.gText(10, 15, "CAMBIO DE", whiteColour, blackColour, 2, 2);
  myScreen.gText(4, 35, "CONTRASENA", whiteColour, blackColour, 2, 2);

  // --- LÓGICA DE ESCALADO DE IMAGEN ---
  const int source_width = 40;
  const int source_height = 40;
  const float scale_factor = 1.5; // Factor de escala

  // Calcular las nuevas dimensiones de la imagen en pantalla
  const int display_width = source_width * scale_factor; // 40 * 1.5 = 60
  const int display_height = source_height * scale_factor; // 40 * 1.5 = 60
  
  // Calcular el punto de inicio para centrar la nueva imagen de 60x60
  const int offsetX = (128 - display_width) / 2; // (128 - 60) / 2 = 34
  const int offsetY = 60; // Posición vertical ajustada

  // Bucle que recorre el ESPACIO DE DESTINO (60x60)
  for (int y = 0; y < display_height; y++) {
    for (int x = 0; x < display_width; x++) {
      // Calcular a qué píxel de la imagen ORIGINAL (40x40) corresponde
      int sourceX = x / scale_factor;
      int sourceY = y / scale_factor;

      // Calcular el índice en el array original
      int index = (sourceY * source_width) + sourceX;
      
      uint16_t color = changeIcon[index];

      // Dibujar el píxel si no es parte del fondo negro de la imagen
      if (color != 0x0000) {
        myScreen.point(offsetX + x, offsetY + y, color);
      }
    }
  }
}

// -----------------------------------------------------------------------------
// SONIDO
// -----------------------------------------------------------------------------
void playSuccessSound() {
  sonidoEnProgreso = true; //
  int melody[] = { NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5 };
  int dur[]    = { 8,       8,       8,       4       };
  for (int i = 0; i < 4; i++) {
    int d = 1000 / dur[i];
    tone(buzzerPin, melody[i], d);
    delay(d + 50);
  }
  noTone(buzzerPin);
  sonidoEnProgreso = false; //
}

void playErrorSound() {
  sonidoEnProgreso = true; //
  int melody[] = { NOTE_G3, NOTE_E3 };
  int dur[]    = { 4,       2       };
  for (int i = 0; i < 2; i++) {
    int d = 1000 / dur[i];
    tone(buzzerPin, melody[i], d);
    delay(d + 50);
  }
  noTone(buzzerPin);
  sonidoEnProgreso = false; //
}

void playFatalErrorSound() {
  sonidoEnProgreso = true; //
  int melody[] = { NOTE_G3, NOTE_F3, NOTE_E3, NOTE_D3, NOTE_C2 };
  int dur[]    = { 16,      16,      16,      16,      1       };
  for (int i = 0; i < 5; i++) {
    int d = 1000 / dur[i];
    tone(buzzerPin, melody[i], d);
    delay(d + (dur[i] > 4 ? 20 : 50));
  }
  noTone(buzzerPin);
  sonidoEnProgreso = false; //
}

void playSirenSound() {
  sonidoEnProgreso = true; //
  for (int i = 0; i < 6; i++) {
    tone(buzzerPin, NOTE_A5, 250); delay(250);
    tone(buzzerPin, NOTE_F5, 250); delay(250);
  }
  noTone(buzzerPin);
  sonidoEnProgreso = false; //
}

// -----------------------------------------------------------------------------
// LED y ACELERÓMETRO
// -----------------------------------------------------------------------------
void setLedColor(int r,int g,int b){
  analogWrite(RED_PIN,   r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN,  b);
}

void calibrateAccelerometer() {
  long sx=0, sy=0, sz=0;
  setLedColor(0,0,0);
  for (int i=0;i<128;i++){
    sx += analogRead(xpin);
    sy += analogRead(ypin);
    sz += analogRead(zpin);
    delay(10);
  }
  restX = sx/128;
  restY = sy/128;
  restZ = sz/128;
}

void gestionarLedDeEstado() {
  // Si un sonido está activo, no hagas nada para evitar conflictos.
  if (sonidoEnProgreso) {
    return;
  }
  
  // Si la puerta está abierta, el LED es verde.
  if (D1 == 1) { 
    setLedColor(0, 255, 0); // Verde
    return;
  }

  // Si la puerta está cerrada, lee el movimiento.
  int cx = analogRead(xpin);
  int cy = analogRead(ypin);
  int cz = analogRead(zpin);
  int totalDelta = abs(cx - restX) + abs(cy - restY) + abs(cz - restZ);

  // Compara el movimiento con el umbral.
  if (totalDelta > UMBRAL_RUIDO) {
    // Con movimiento: LED azul fijo.
    setLedColor(0, 0, 255); 
  } else {
    // Sin movimiento: LED rojo fijo.
    setLedColor(255, 0, 0); 
  }
}

// -----------------------------------------------------------------------------
// LUZ y PUERTA
// -----------------------------------------------------------------------------
void Lux_check() {
  unsigned long lux = opt3001.readResult();
  if (lux > 100 && D1 == 0) {
    Serial.println("ALERTA: Luz detectada con puerta cerrada (posible intruso)");
    drawAccesoNoAutorizado();
    playSirenSound();
    // mostrar ~1s sin "congelar" del todo
    uint32_t t0 = millis();
    while (millis() - t0 < 1000) {
    gestionarLedDeEstado();
      delay(10);
    }
    lux_flag = 1;
  }
  // Parche para evitar bug.
  if ((estadoActual == ESTADO_INICIO) && (lux_flag == 1) && (lux < 100)){
    lux_flag = 0;
    pantallaInicio();
    }
}

void Puerta_Abierta() {
  Serial.println("Puerta Abierta");
  // mantener 1.5 s “abierta” sin congelar sensores/LED
  uint32_t t0 = millis();
  while (millis() - t0 < 1500) {
    gestionarLedDeEstado();
    delay(10);
  }
  Serial.println("Puerta Cerrada");
}

// -----------------------------------------------------------------------------
// Joystick
// -----------------------------------------------------------------------------

void calibrarJoystick() {
  long sx=0, sy=0;
  for (int i=0; i<16; i++) { 
    sx += analogRead(JOY_X_PIN); 
    sy += analogRead(JOY_Y_PIN); 
    delay(2); 
  }
  joyCX = sx/16; 
  joyCY = sy/16;
}

JoyDir leerDireccionJoystickDiscreta() {
  int x = analogRead(JOY_X_PIN);
  int y = analogRead(JOY_Y_PIN);

  bool left  = (x < joyCX - DEADZONE);
  bool right = (x > joyCX + DEADZONE);
  bool down  = (y < joyCY - DEADZONE);
  bool up    = (y > joyCY + DEADZONE);

  bool ahoraNeutral = !(left || right || up || down);

  if (joyNeutral && !ahoraNeutral) {
    joyNeutral = false;
    if (left)  return JOY_LEFT;
    if (right) return JOY_RIGHT;
    if (up)    return JOY_UP;
    if (down)  return JOY_DOWN;
  }
  if (ahoraNeutral) joyNeutral = true;

  return JOY_NONE;
}

void moverSeleccion(JoyDir d) {
  int n = seleccionado;
  if (n >= 1 && n <= 9) {
    int col = (n - 1) % 3;
    int row = (n - 1) / 3;
    if (d == JOY_LEFT  && col > 0) col--;
    if (d == JOY_RIGHT && col < 2) col++;
    if (d == JOY_UP    && row > 0) row--;
    if (d == JOY_DOWN) {
      if (row < 2) row++;
      else { // bajar desde 7/8/9
        if (col == 1) { seleccionado = 0;  return; }  // 8 -> 0
        if (col == 2) { seleccionado = 10; return; }  // 9 -> OK
      }
    }
    seleccionado = row*3 + col + 1;
  } else if (n == 0) {
    if (d == JOY_RIGHT) seleccionado = 10;
    if (d == JOY_UP)    seleccionado = 8;
  } else if (n == 10) {
    if (d == JOY_LEFT)  seleccionado = 0;
    if (d == JOY_UP)    seleccionado = 9;
  }
}

