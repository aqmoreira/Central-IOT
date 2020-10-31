#include <SPI.h>
#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
#include "src/ssd1306/SSD1306Wire.h"        //  #include "SSD1306.h" // https://github.com/ThingPulse/esp8266-oled-ssd1306
#include "centralsemaforica.h"
#include "imagens.h"



// Initialize the OLED display using Arduino Wire:
SSD1306Wire display(0x3c, SDA, SCL);

const int CSpino = D8; // Pino CS 74HC595
unsigned long tempoTrocadoAmarelo = 0; //armazena o tempo em milis que foi feita ultima troca
unsigned long tempoAtual;



//T2 T1 VM VD VM VD VM VD - Byte Superior
//VM VD VM AM VD VM AM VD - Byte Inferior
uint16_t semaforo = 0x0000;

void setup() {
  Serial.begin(9600);
  //controle do 595 acionamento dos LEDS
  pinMode(CSpino, OUTPUT);
  pinMode(SW1, INPUT); //botão 1
  pinMode(SW2, INPUT); //botão 2
  pinMode(SW3, INPUT); //botão 3
  pinMode(BUZZER, OUTPUT); //botão 3

  SPI.begin();

  // Inicializa o display
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.clear();
  drawLogo();
  ligarTodosLeds();
  delay(2000);
  apagarTodosLeds();
  drawNome();
  display.display();
  delay(2000);

}

/**
   //T2 T1 VM VD VM VD VM VD - Byte Superior
   //VM VD VM AM VD VM AM VD - Byte Inferior
*/
void mostrarLed(int led) {
  Serial.print("Semaforo : ");
  Serial.println(led, BIN);
  semaforo = led;
  digitalWrite(CSpino, LOW); //Desabilita Transferencia SN74HC595
  SPI.transfer(led >> 8); //Transfere dado para o 74HC595 Parte Superior
  SPI.transfer(led); //Transfere o dado para o 74HC595 Parte Interior
  digitalWrite(CSpino, HIGH); //inicia transferencia
}


void apagarTodosLeds() {
  mostrarLed(0x0000);
}

void ligarTodosLeds() {
  mostrarLed(0xffff);
}


void piscarAmarelo() {
  tempoAtual = millis();
  if (tempoAtual - tempoTrocadoAmarelo >= TEMPO_PISCA) {
    tempoTrocadoAmarelo = tempoAtual;
    if (bitRead(semaforo, S1_AM)) {
      bitClear(semaforo, S1_AM);
    } else {
      bitSet(semaforo, S1_AM);
    }

    if (bitRead(semaforo, S2_AM)) {
      bitClear(semaforo, S2_AM);
    } else {
      bitSet(semaforo, S2_AM);
    }

    if (bitRead(semaforo, S3_VM)) {
      bitClear(semaforo, S3_VM);
    } else {
      bitSet(semaforo, S3_VM);
    }


    if (bitRead(semaforo, S4_VM)) {
      bitClear(semaforo, S4_VM);
    } else {
      bitSet(semaforo, S4_VM);
    }

    if (bitRead(semaforo, S5_VM)) {
      bitClear(semaforo, S5_VM);
    } else {
      bitSet(semaforo, S5_VM);
    }

    if (bitRead(semaforo, S6_VM)) {
      bitClear(semaforo, S6_VM);
    } else {
      bitSet(semaforo, S6_VM);
    }
    mostrarLed(semaforo);
  }

}


void drawNome() {
  display.clear();
  // Font Demo1
  // create more fonts at http://oleddisplay.squix.ch/
  display.setTextAlignment(TEXT_ALIGN_CENTER);

  //display.setFont(ArialMT_Plain_16);
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 0, "CENTRAL");
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 26, "SEMAFÓRICA");
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 40, "IOT");
  display.display();
}


void drawLogo() {
  // see http://blog.squix.org/2015/05/esp8266-nodemcu-how-to-create-xbm.html
  // on how to create xbm files
  display.clear();
  display.drawXbm(20, 20, UNIVESP_Logo_width, UNIVESP_Logo_height, UNIVESP_Logo_bits);
  display.display();
}


void loop() {
  piscarAmarelo();



  if (digitalRead(SW1) == LOW) {
    Serial.println("drawLogo");
    drawLogo();

  }
  if (digitalRead(SW2) == LOW) {
    drawNome();
  }

  if (digitalRead(SW3) == LOW) {
    tone(BUZZER, 440, 100);

  }



}
