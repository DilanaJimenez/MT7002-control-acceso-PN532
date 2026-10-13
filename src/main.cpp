// =============================================================
// Pruebas de caracterizacion del PN532 (Elechouse NFC Module V3)
// con ESP32-WROOM-32 por UART (modo HSU del PN532)
// MT7002 Sensores y Actuadores - Proyecto final, Entrega 1
// =============================================================
//
// Se usa UART (HSU) en vez de I2C porque con I2C el ESP32 daba
// "i2cRead returned Error 263" en cada lectura: el PN532 estira
// el reloj I2C mientras procesa y el ESP32 se queda esperando
// ~1 s, lo que arruinaba la prueba de tiempo de respuesta.
//
// CONEXION (DIP switch del PN532: 1 = OFF, 2 = OFF -> modo HSU)
//   PN532 GND -> ESP32 GND
//   PN532 VCC -> ESP32 3V3   (NO usar 5V)
//   PN532 TXD (rotulado SDA) -> ESP32 GPIO21  (RX del ESP32)
//   PN532 RXD (rotulado SCL) -> ESP32 GPIO22  (TX del ESP32)
//   Son los mismos cables que se usaban en I2C, solo cambia el DIP.
//   El UART2 del ESP32 se puede asignar a cualquier pin, por eso
//   se reutilizan el 21 y el 22.
//
// MONITOR SERIE: 115200 baud, terminador "Nueva linea"
//
// COMANDOS (se escriben en el monitor serie y se da Enter):
//   f        verifica que el ESP32 si ve al PN532
//   l        lectura continua: muestra el UID y si lee o no
//            (sirve para ver el UID del sticker y para la
//             prueba de distancia limite). Se detiene con 's'
//   e        prueba de exactitud: N_EXACTITUD lecturas seguidas
//   r 2.5    prueba de rango en 2.5 cm: N_RANGO intentos
//   t        prueba de tiempo de respuesta: N_TIEMPO mediciones
//   p <uid>  cambia el patron, ej: p D3:FA:AE:FE
//
// Los resultados salen separados por comas (CSV) para copiarlos
// a Excel o MATLAB.
// =============================================================

#include <Arduino.h>     // necesario en PlatformIO (en Arduino IDE no estorba)
#include <PN532_HSU.h>   // libreria Elechouse: capa UART (HSU)
#include <PN532.h>       // libreria Elechouse: comandos del PN532

// pines del UART2 que se conectan al PN532
#define PIN_RX 21   // aqui llega lo que transmite el PN532 (TXD)
#define PIN_TX 22   // de aqui sale lo que recibe el PN532 (RXD)

PN532_HSU pn532hsu(Serial2);   // el PN532 va por el UART2, 115200 baud
PN532 nfc(pn532hsu);

// ---------------- PARAMETROS DE LAS PRUEBAS ------------------

// UID del sticker patron. Primero se lee con NFC Tools en el
// celular (instrumento de referencia) y se copia aqui igualito,
// en mayusculas y separado por dos puntos.
// Los stickers S50 (Mifare Classic) tienen UID de 4 bytes
String uidPatron = "B3:8B:3A:4F";   // <-- CONFIRMAR con el celular

const int N_EXACTITUD = 100;  // lecturas en la prueba de exactitud
const int N_RANGO     = 20;   // intentos por cada distancia
const int N_TIEMPO    = 50;   // mediciones de tiempo de respuesta

// tiempo maximo (ms) que el ESP32 espera a que el PN532 encuentre
// un tag. Si no hay tag la lectura cuenta como fallida.
const int TIMEOUT_MS = 200;

// cuantas veces reintenta el PN532 por su cuenta antes de decir
// "no hay tag". Con 0xFF esperaria para siempre, por eso se
// limita. Este valor hay que reportarlo en el informe.
const uint8_t REINTENTOS = 0x02;

// pausa entre lecturas para no saturar el bus
const int PAUSA_MS = 100;

// ---------------- VARIABLES GLOBALES -------------------------
uint8_t uid[7];    // aqui queda el UID leido (hasta 7 bytes)
uint8_t uidLen;    // largo del UID (4 o 7 bytes)
bool modoContinuo = false;
String bufferCmd = "";   // aqui se van juntando las letras del comando

// -------------------------------------------------------------
// Convierte el UID (arreglo de bytes) a texto tipo 04:A1:B2:...
// para poder compararlo con uidPatron y mostrarlo bonito
// -------------------------------------------------------------
String uidATexto(uint8_t *u, uint8_t largo) {
  String s = "";
  for (int i = 0; i < largo; i++) {
    if (u[i] < 0x10) s += "0";       // que 0x4 salga como "04"
    s += String(u[i], HEX);
    if (i < largo - 1) s += ":";
  }
  s.toUpperCase();
  return s;
}

// -------------------------------------------------------------
// Intenta leer un tag ISO14443A (NTAG y Mifare son de este tipo)
// Devuelve true si encontro uno; el UID queda en uid[] y uidLen
// -------------------------------------------------------------
bool leerTag() {
  return nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, TIMEOUT_MS);
}

// -------------------------------------------------------------
// Pregunta la version de firmware: si responde, la conexion
// UART esta bien
// -------------------------------------------------------------
bool verificarConexion() {
  uint32_t version = nfc.getFirmwareVersion();
  if (!version) {
    Serial.println("ERROR: no se encontro el PN532");
    Serial.println("Revisar: DIP (1=OFF, 2=OFF), TXD->21, RXD->22, VCC->3V3. Si no, intercambiar 21 y 22");
    return false;
  }
  Serial.print("Chip encontrado: PN5");
  Serial.println((version >> 24) & 0xFF, HEX);
  Serial.print("Firmware: ");
  Serial.print((version >> 16) & 0xFF, DEC);
  Serial.print(".");
  Serial.println((version >> 8) & 0xFF, DEC);
  return true;
}

// -------------------------------------------------------------
// LECTURA CONTINUA
// Imprime cada 100 ms si hay tag o no. Se usa para:
//  1) ver el UID del sticker con el PN532
//  2) prueba de distancia limite: se acerca el sticker poco a
//     poco y se anota en la regla donde pasa de NO LEE a LEE
// -------------------------------------------------------------
void lecturaContinua() {
  if (leerTag()) {
    String leido = uidATexto(uid, uidLen);
    Serial.print("LEE  ");
    Serial.print(leido);
    Serial.print("  (");
    Serial.print(uidLen);
    Serial.print(" bytes)");
    if (leido == uidPatron) Serial.print("  = patron");
    Serial.println();
  } else {
    Serial.println("NO LEE");
  }
  delay(PAUSA_MS);
}

// -------------------------------------------------------------
// PRUEBA DE EXACTITUD
// El sticker queda fijo a una distancia. Se hacen N lecturas y
// cada una se clasifica como:
//   correcta -> leyo y el UID es igual al patron
//   erronea  -> leyo pero el UID es distinto (posible falso +)
//   fallida  -> no leyo nada
// -------------------------------------------------------------
void pruebaExactitud() {
  int correctas = 0, erroneas = 0, fallidas = 0;

  Serial.println("--- PRUEBA DE EXACTITUD ---");
  Serial.println("n,uid_leido,resultado");

  for (int i = 1; i <= N_EXACTITUD; i++) {
    String leido = "";
    String resultado;

    if (leerTag()) {
      leido = uidATexto(uid, uidLen);
      if (leido == uidPatron) {
        resultado = "correcta";
        correctas++;
      } else {
        resultado = "erronea";
        erroneas++;
      }
    } else {
      resultado = "fallida";
      fallidas++;
    }

    Serial.print(i); Serial.print(",");
    Serial.print(leido); Serial.print(",");
    Serial.println(resultado);
    delay(PAUSA_MS);
  }

  // resumen con las ecuaciones del informe
  Serial.println("--- RESUMEN ---");
  Serial.print("Correctas: "); Serial.println(correctas);
  Serial.print("Erroneas: ");  Serial.println(erroneas);
  Serial.print("Fallidas: ");  Serial.println(fallidas);
  Serial.print("E  (%): ");  Serial.println(100.0 * correctas / N_EXACTITUD, 2);
  Serial.print("TE (%): ");  Serial.println(100.0 * erroneas  / N_EXACTITUD, 2);
  Serial.print("TF (%): ");  Serial.println(100.0 * fallidas  / N_EXACTITUD, 2);
}

// -------------------------------------------------------------
// PRUEBA DE RANGO
// Se pone el sticker a una distancia (en cm) y se hacen N_RANGO
// intentos. Sale una linea CSV por distancia:
//   distancia_cm, aciertos, intentos, porcentaje
// Se repite subiendo la distancia hasta que ya no lea
// -------------------------------------------------------------
void pruebaRango(float distancia) {
  int correctas = 0;
  for (int i = 0; i < N_RANGO; i++) {
    if (leerTag() && uidATexto(uid, uidLen) == uidPatron) correctas++;
    delay(PAUSA_MS);
  }
  Serial.print(distancia, 1); Serial.print(",");
  Serial.print(correctas);    Serial.print(",");
  Serial.print(N_RANGO);      Serial.print(",");
  Serial.println(100.0 * correctas / N_RANGO, 1);
}

// -------------------------------------------------------------
// PRUEBA DE TIEMPO DE RESPUESTA
// Mide con micros() cuanto dura una lectura completa con el tag
// ya presente: el PN532 activa el campo, detecta el tag, hace
// la anticolision y devuelve el UID por UART.
// Las mediciones donde no leyo (leyo = 0) NO se promedian.
// -------------------------------------------------------------
void pruebaTiempo() {
  float suma = 0, sumaCuad = 0;
  int validas = 0;

  Serial.println("--- PRUEBA DE TIEMPO DE RESPUESTA ---");
  Serial.println("n,tiempo_ms,leyo");

  for (int i = 1; i <= N_TIEMPO; i++) {
    unsigned long t1 = micros();          // inicio
    bool ok = leerTag();
    unsigned long t2 = micros();          // fin
    float tms = (t2 - t1) / 1000.0;       // microsegundos -> ms

    Serial.print(i); Serial.print(",");
    Serial.print(tms, 3); Serial.print(",");
    Serial.println(ok ? 1 : 0);

    if (ok) {
      suma += tms;
      sumaCuad += tms * tms;
      validas++;
    }
    delay(200);
  }

  // media y desviacion estandar muestral (n-1)
  Serial.println("--- RESUMEN ---");
  Serial.print("Mediciones validas: "); Serial.println(validas);
  if (validas > 1) {
    float media = suma / validas;
    float s = sqrt((sumaCuad - validas * media * media) / (validas - 1));
    Serial.print("Media (ms): "); Serial.println(media, 3);
    Serial.print("Desv. est. (ms): "); Serial.println(s, 3);
  }
}

// =============================================================
void setup() {
  Serial.begin(115200);
  delay(1000);                  // tiempo para abrir el monitor

  Serial.println();
  Serial.println("=== Pruebas PN532 - MT7002 ===");

  // se arranca el UART2 en los pines elegidos (115200 es la
  // velocidad del PN532 en modo HSU) y se despierta al PN532.
  // Esto reemplaza a nfc.begin(), que usaria los pines por defecto
  Serial2.begin(115200, SERIAL_8N1, PIN_RX, PIN_TX);
  pn532hsu.wakeup();
  if (!verificarConexion()) {
    while (true) delay(1000);   // sin sensor no tiene sentido seguir
  }

  nfc.setPassiveActivationRetries(REINTENTOS);
  nfc.SAMConfig();              // pone al PN532 en modo lector

  Serial.print("UID patron configurado: ");
  Serial.println(uidPatron);
  Serial.println("Comandos: f, l, s, e, r <cm>, t, p <uid>");
}

// -------------------------------------------------------------
// Ejecuta el comando escrito en el monitor
// -------------------------------------------------------------
void ejecutarComando(String cmd) {
    if (cmd.length() == 0) return;   // Enter vacio, no hacer nada

    if (cmd == "f")      verificarConexion();
    else if (cmd == "l") { modoContinuo = true;  Serial.println("Lectura continua (s = detener)"); }
    else if (cmd == "s") { modoContinuo = false; Serial.println("Detenido"); }
    else if (cmd == "e") pruebaExactitud();
    else if (cmd.startsWith("r")) pruebaRango(cmd.substring(1).toFloat());
    else if (cmd == "t") pruebaTiempo();
    else if (cmd.startsWith("p ")) {
      // cambia el patron sin volver a subir el codigo
      // ej: p D3:FA:AE:FE   (al reiniciar vuelve al del codigo)
      uidPatron = cmd.substring(2);
      uidPatron.trim();
      uidPatron.toUpperCase();
      Serial.print("Nuevo patron: ");
      Serial.println(uidPatron);
    }
    else Serial.println("Comando no reconocido");
}

void loop() {
  // leer letra por letra hasta que llegue el Enter.
  // Asi funciona igual en el monitor del Arduino IDE (manda la
  // linea completa) y en el de PlatformIO (manda cada tecla)
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      bufferCmd.trim();
      ejecutarComando(bufferCmd);
      bufferCmd = "";
    } else {
      bufferCmd += c;
    }
  }

  if (modoContinuo) lecturaContinua();
}