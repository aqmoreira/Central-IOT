/**
 * Central Semafórica IOT
 * Controla cruzamento secundário, dando prioridade ao VLT
 * Se estiver sem conexão atua de forma offline com tempos pré definidos
 * 
 * */

#include <FS.h>          // Isso precisa estar em primeiro para evitar erros no sistema de arquivos
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager V 2.0.3-alpha
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson V5.13.5
#include <SPI.h>
#include <Wire.h>               // Only needed for Arduino 1.6.5 and earlier
#include<ESP8266WiFi.h>       //Esp8266 Boards 2.7.4
#include <PubSubClient.h>    //https://github.com/knolleary/pubsubclient/releases/tag/v2.8
#include "src/ssd1306/SSD1306Wire.h"        //  #include "SSD1306.h" // https://github.com/ThingPulse/esp8266-oled-ssd1306
#include "centralsemaforica.h"
#include "imagens.h"



// Initialize the OLED display using Arduino Wire:
SSD1306Wire display(0x3c, SDA, SCL);

const int CSpino = D8; // Pino CS 74HC595



//T2 T1 VM VD VM VD VM VD - Byte Superior
//VM VD VM AM VD VM AM VD - Byte Inferior
uint16_t semaforo = 0x0000;         //Status dos Leds cada bit em 1, acende
 


//Tempos de monitoração
unsigned long tempoMaxCruzamentoVia = TEMPO_MAXIMO_CRUZAMENTO_VIA;//Tempo para VLT cruzar via Vermelho Rua
unsigned long tempoMaxVerde = TEMPO_MAXIMO_VERDE;        //Tempo máximo verde - Mesmo Online 
unsigned long tempoAlerta = TEMPO_ALERTA;          //Tempo alerta de mundaça de sinal via
unsigned long tempoAmarelo = TEMPO_AMARELO;         //Tempo Amarelo 
unsigned long tempoSeguranca = TEMPO_SEGURANCA;       //Tempo de Segurança qdo fica sem conexão e mudar fase
unsigned long tempoVerde = TEMPO_VERDE;           //Tempo Verde 

unsigned long tempoMaxEsperaVLT = TEMPO_MAXIMO_ESPERA_VLT;    //Tempo máximo VLT fica esperando
unsigned long tempoMinVerde = TEMPO_MINIMO_VERDE;        //Tempo mínimo verde de fluxo rua
unsigned long tempoAlertaPiscante = TEMPO_ALERTA_PISCANTE;  //tempo que fica piscando pedestre e via na mudança de fase


unsigned long tempoAtual = 0;            //variável para usar de rascunho do tempo atual
unsigned long tempoTrocadoManutencao = 0; //armazena o tempo em milis que foi feita ultima troca
unsigned long tempoTrocadoPedestre = 0; //armazena o tempo em milis que foi feita ultima troca
unsigned long tempoTrocadoVia = 0; //armazena o tempo em milis que foi feita ultima troca
unsigned long tempoRefreshRelogio = 0; //de qto tempo relógio deve ser acertado

//Inicializa com valores defaul, se for diferente do valores gravados em config serão alterados
char mqttServer[40] =  MQTTServer;
char mqttPort[6]  = MQTTPort;
char mqttUser[20]  = MQTTUser;
char mqttPass[20]  = MQTTPass;
char idCruzamento[5] = IDCruzamento;

WiFiClient espClient; // Cria o objeto espClient
PubSubClient mqtt(espClient); // Instancia o Cliente MQTT passando o objeto espClient


String topicoCruzamento ;    //tópico de publocação status central/cruzamento/<idCruzamento>
const char* statusCentral = "Ativo e operando";

WiFiEventHandler gotIpEventHandler,disconnectedEventHandler;

//flag para informa se deve salvar propriedades
bool salvarPropriedades = false;


WiFiManager wm; // WiFiManager

unsigned long contadorEspera = 0;                 


// Status das conexões WiFi e MQTT:
//
// status |   WiFi   |    MQTT
// -------+----------+------------
//      0 |   OFF    |    OFF
//      1 | Iniciando|    OFF
//      2 |    ON    |    OFF
//      3 |    ON    |  Iniciando
//      4 |    ON    | Registrando
//      5 |    ON    |     ON
uint8_t statusConexao = 0;                   

unsigned long tempoUltimaAtualizacao = 0;                // counter in example code for statusConexao == 5
unsigned long lastTask = 0;                  // counter in example code for statusConexao <> 5

unsigned long  tempoUltimaMudancaFase = 0; //Tempo da Ultima Mudança Fase  
byte faseAtualSemaforo = 0;   //Fase atual do Semaforo
byte faseAnteriorSemaforo = -1;   //Fase atual do Semaforo
 




void setup() {
    Serial.begin(115200);
  
   
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

  lerPropriedades();
  
  tempoUltimaMudancaFase = millis();    
  faseAtualSemaforo = 0;   
  faseAnteriorSemaforo = -1;
  

}


void loop() {

    conectar();
    wm.process();
    
  controlarSemaforo();
  //piscarManutencao();



  if (digitalRead(SW1) == LOW) {
    Serial.println("drawLogo");
    drawLogo();

  }
  if (digitalRead(SW2) == LOW) {
    drawNome();
  }

  if (digitalRead(SW3) == LOW) {
    tone(BUZZER, 440, 100);
    Serial.print("Id:");
    Serial.println( String(ESP.getChipId()));
     mostrarDisplay("Conectado\nIP: " + WiFi.localIP().toString() + "\nSSID: " + WiFi.SSID() + "\nMQTT State: "+ String(mqtt.state()) + "\nCruzamento:" + String(idCruzamento)  );   

  }

 
  delay(10);

}



// Incompleto ainda.. falta fase offline   
void controlarSemaforo(){

  if ((WiFi.status() == WL_CONNECTED) && mqtt.connected() && (faseAtualSemaforo == FASE_INICIANDO)) { faseAtualSemaforo = FASE_ON_VERMELHO;}
  if (( (WiFi.status() != WL_CONNECTED) || !mqtt.connected())  && (faseAtualSemaforo == FASE_INICIANDO)) { faseAtualSemaforo = FASE_OFF_SEGURO; }
  
  Serial.println("Fase:" + String(faseAtualSemaforo));
  
  switch (faseAtualSemaforo) {
    
    case FASE_INICIO:
      Serial.println("FASE_INICIO");
      if (millis() - tempoUltimaMudancaFase > 10000){
         faseAtualSemaforo = FASE_INICIANDO;
      }else{
        piscarManutencao();
      }
    break;
    
    case FASE_INICIANDO:                                               
      Serial.println("FASE_INICIANDO");
      piscarManutencao();
      
      break;

     case FASE_ON_VERMELHO:                                               
      Serial.println("FASE_ON_VERMELHO");
      if(faseAtualSemaforo != faseAnteriorSemaforo){
        tempoUltimaMudancaFase = millis();
        faseAnteriorSemaforo = faseAtualSemaforo;
        mostrarLed(LED_FASE_VERMELHO);
      }else{
        if (millis() - tempoUltimaMudancaFase > tempoMaxCruzamentoVia){
          faseAtualSemaforo = FASE_ON_ALERTA;
          tempoUltimaMudancaFase = millis();
        }
      }
      break; 

     case FASE_ON_ALERTA:                                               
      Serial.println("FASE_ON_ALERTA");
      if(faseAtualSemaforo != faseAnteriorSemaforo){
        tempoUltimaMudancaFase = millis();
        faseAnteriorSemaforo = faseAtualSemaforo;
        mostrarLed(LED_FASE_ALERTA);
        tempoTrocadoVia =  millis();
      }else{
        if (millis() - tempoUltimaMudancaFase > tempoAlerta){
          faseAtualSemaforo = FASE_ON_VERDE;
          tempoUltimaMudancaFase = millis();
        }else{
          piscarVia();
        }
      }
      
      break; 
     case FASE_ON_VERDE:                                               
      Serial.println("FASE_ON_VERDE");
      if(faseAtualSemaforo != faseAnteriorSemaforo){
         tempoUltimaMudancaFase = millis();
        faseAnteriorSemaforo = faseAtualSemaforo;
        mostrarLed(LED_FASE_VERDE);
      }else if(( !isConectado() && (millis() - tempoUltimaMudancaFase > tempoMinVerde)) 
          || (millis() - tempoUltimaMudancaFase > tempoMaxVerde) ){
          faseAtualSemaforo = FASE_ON_AMARELO;
      }
      
      break; 
    case FASE_ON_AMARELO:                                               
      Serial.println("FASE_ON_AMARELO");
      if(faseAtualSemaforo != faseAnteriorSemaforo){
         tempoUltimaMudancaFase = millis();
        faseAnteriorSemaforo = faseAtualSemaforo;
        mostrarLed(LED_FASE_AMARELO);
        tempoTrocadoPedestre = millis();
      }else{
        if (millis() - tempoUltimaMudancaFase > tempoAmarelo){
          if(isConectado){
            faseAtualSemaforo = FASE_ON_VERMELHO;
          }else{
            faseAtualSemaforo = FASE_OFF_SEGURO;
          }
         
        }else{
          piscarPedestre();
        }
      }
      break;   
    case FASE_OFF_SEGURO:                                               
      Serial.println("FASE_OFF_SEGURO");
      if(faseAtualSemaforo != faseAnteriorSemaforo){
         tempoUltimaMudancaFase = millis();
        faseAnteriorSemaforo = faseAtualSemaforo;
        mostrarLed(LED_FASE_SEGURO);
      }else{
        if (millis() - tempoUltimaMudancaFase > tempoSeguranca){
          
            faseAtualSemaforo = FASE_OFF_VERMELHO;
        }
         
        
      }
      
      
      break;   
   case FASE_OFF_VERMELHO:                                              
      Serial.println("FASE_OFF_VERMELHO");
       if(faseAtualSemaforo != faseAnteriorSemaforo){
         tempoUltimaMudancaFase = millis();
         faseAnteriorSemaforo = faseAtualSemaforo;
         mostrarLed(LED_FASE_VERMELHO);
       }else{
        if (millis() - tempoUltimaMudancaFase > tempoMaxCruzamentoVia){
          faseAtualSemaforo = FASE_OFF_ALERTA;
        }
      }
      
      break; 
   case FASE_OFF_ALERTA:                                               
      Serial.println("FASE_OFF_ALERTA");
      if(faseAtualSemaforo != faseAnteriorSemaforo){
        tempoUltimaMudancaFase = millis();
        faseAnteriorSemaforo = faseAtualSemaforo;
        mostrarLed(LED_FASE_ALERTA);
        tempoTrocadoVia =  millis();
      }else{
        if (millis() - tempoUltimaMudancaFase > tempoAlerta){
          faseAtualSemaforo = FASE_OFF_VERDE;
          tempoUltimaMudancaFase = millis();
        }else{
          piscarVia();
        }
      }
      break; 
   case FASE_OFF_VERDE:                                               
      Serial.println("FASE_OFF_VERDE");
      if(faseAtualSemaforo != faseAnteriorSemaforo){
         tempoUltimaMudancaFase = millis();
        faseAnteriorSemaforo = faseAtualSemaforo;
        mostrarLed(LED_FASE_VERDE);
      }else{
        if ((millis() - tempoUltimaMudancaFase > tempoVerde) ){
          faseAtualSemaforo = FASE_ON_AMARELO;
          
        }
        
      }
      break; 

   case FASE_OFF_AMARELO:                                               
      Serial.println("FASE_OFF_AMARELO");
      if(faseAtualSemaforo != faseAnteriorSemaforo){
         tempoUltimaMudancaFase = millis();
        faseAnteriorSemaforo = faseAtualSemaforo;
        mostrarLed(LED_FASE_AMARELO);
        tempoTrocadoPedestre = millis();
      }else{
        if (millis() - tempoUltimaMudancaFase > tempoAmarelo){
          if(isConectado){
            faseAtualSemaforo = FASE_ON_VERMELHO;
          }else{
            faseAtualSemaforo = FASE_OFF_VERMELHO;
          }
         
        }else{
          piscarPedestre();
        }
      }
      break; 
  }
}


bool isConectado(){
  return ((WiFi.status() == WL_CONNECTED) && mqtt.connected()); 
}




//callback de noticação que propriedades precisa ser salva
void saveConfigCallback () {
  Serial.println("Deve salvar as propriedades");
  salvarPropriedades = true;
}


void gravarPropriedades(){
    Serial.println("Salvando config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqttServer"] = mqttServer;
    json["mqttPort"]   = mqttPort;
    json["mqttUser"]   = mqttUser;
    json["mqttPass"]   = mqttPass;
    json["idCruzamento"]   = idCruzamento;

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("Falha para abrir o arquivo de config para gravação");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //altera flag informando que esse dados já estão salvos
    salvarPropriedades = false;

  
}


void lerPropriedades(){
  //read configuration from FS json
  Serial.println("Montando sistema de arquivos FS...");

  if (SPIFFS.begin()) {
    Serial.println("Montado file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("lendo arquivo config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("Acessando config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nLeitura das propriedades efetuada");

          strcpy(mqttServer, json["mqttServer"]);
          strcpy(mqttPort, json["mqttPort"]);
          strcpy(mqttUser, json["mqttUser"]);
          strcpy(mqttPass, json["mqttPass"]);
          strcpy(idCruzamento, json["idCruzamento"]);
          topicoCruzamento =  TOPICO_CENTRAL_CRUZAMENTO;
          topicoCruzamento += String(idCruzamento);


        } else {
          Serial.println("Falha ao carregar arquivo config json");
        }
      }
    }
  } else {
    Serial.println("Falha na montagem do sistema de arquivos FS");
  }
  //end read
  
}

void eventosMQTT(){
     Serial.print("Publish a conexão reativada ao MQTT ...");
     // Conectado no MQTT, avisar a central
    mqtt.publish(topicoCruzamento.c_str() ,statusCentral);
    // Inscrição para escuta dos eventos para o cruzamento
    mqtt.subscribe((TOPICO_VIA1_CRUZAMENTO + String(idCruzamento)).c_str());
    mqtt.subscribe((TOPICO_VIA2_CRUZAMENTO + String(idCruzamento)).c_str());
 
      
  
}

void iniciarMQTT(){
    unsigned short port = (unsigned short) strtoul(mqttPort, NULL, 0);  //COnvert para numero o *char
    mqtt.setServer(mqttServer, port);   //informa qual broker e porta deve ser conectado
    mqtt.setCallback(callbackMQTT);            //atribui função de callback (função chamada quando qualquer informação de um dos tópicos subescritos chega)
    String centraId = "CentralIOTID-";
    centraId += String(idCruzamento);
    
    
    mqtt.connect(centraId.c_str());  //validar depois a opção sem usuário ou com conforme as propriedades
    //mqtt.connect(centraId.c_str(), mqttUser.c_str(), mqttPass.c_str());
}


void callbackMQTT(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message recebida [");
  Serial.print(topic);
  Serial.print("] ");
  String msgPayload;
  for (int i = 0; i < length; i++) {
    msgPayload +=(char)payload[i];
    
  }
  Serial.println("msg:" + msgPayload);
   mostrarDisplay("MQTT recebida \nTópico:\n" + String(topic) + "\nMensagem:\n" +   msgPayload);



  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}


void iniciarWIFI(){

   WiFi.mode(WIFI_STA); // ajustar o modo de operação, esp defaults to STA+AP
  // gotIpEventHandler = WiFi.onStationModeGotIP(wifiObteveIPHandler);
  // disconnectedEventHandler = WiFi.onStationModeDisconnected(wifiDisconectadoHandler);


   //set config save notify callback
        wm.setSaveConfigCallback(saveConfigCallback);
        
        wm.setConfigPortalBlocking(false);
         
       


    if ((digitalRead(SW1) == LOW) && (digitalRead(SW3) == LOW) ){  //se SW1 e SW3 OFF entra no WifiManager

        wm.setConfigPortalBlocking(true);
       //Se desejar ressetar as configurações via código
        //wm.resetSettings();
        //wm.erase();
        WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqttServer, 40);
        WiFiManagerParameter custom_mqtt_port("port", "mqtt port", MQTTPort, 6);
        WiFiManagerParameter custom_mqtt_user("user", "mqtt user", MQTTUser, 20);
        WiFiManagerParameter custom_mqtt_pass("pass", "mqtt pass", MQTTPass, 20);
        WiFiManagerParameter custom_idcruzamento("idcruzamento", "id cruzamento", IDCruzamento, 4);
       
        
        
        wm.addParameter(&custom_mqtt_server);
        wm.addParameter(&custom_mqtt_port);
        wm.addParameter(&custom_mqtt_user);
        wm.addParameter(&custom_mqtt_pass);
        wm.addParameter(&custom_idcruzamento);
     
        
        bool res;
        // res = wm.autoConnect(); // auto generated AP name from chipid
        // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
        res = wm.autoConnect("CentralIOT","univesp2020"); // password protected ap
    
        if(!res) {
            Serial.println("Falha para conectar e tempo excedido de espera");
             delay(3000);
            // Se ainda não tiver conectado podemos fazer o reset iniciar tudo novamente
            //ESP.restart();
            //delay(5000);
        } 
        else {
            //if you get here you have connected to the WiFi    
            Serial.println("Conexão OK WIFI... :)");
    
              //read updated parameters
            strcpy(mqttServer, custom_mqtt_server.getValue());
            strcpy(mqttPort, custom_mqtt_port.getValue());
            strcpy(mqttUser, custom_mqtt_user.getValue());
            strcpy(mqttPass, custom_mqtt_pass.getValue());
            strcpy(idCruzamento, custom_idcruzamento.getValue());
            topicoCruzamento =  TOPICO_CENTRAL_CRUZAMENTO;
            topicoCruzamento += String(idCruzamento);
    
            //Salvar as propriedades no FS
            if (salvarPropriedades) {
              gravarPropriedades();
            }
        }

      
      
    }
    


    
  
}



void wifiDisconectadoHandler(const WiFiEventStationModeDisconnected& event){
   Serial.printf("T=%d Disconectado (reason=%d)\n",millis(),event.reason);
}

void wifiObteveIPHandler(const WiFiEventStationModeGotIP& event){
  delay(1);
  Serial.printf("T=%d Conectado na rede %s (%s) com IP: %s (ch: %d) hostname=%s\n",millis(),CSTR(WiFi.SSID()),TXTIP(WiFi.gatewayIP()),TXTIP(WiFi.localIP()),WiFi.channel(),CSTR(WiFi.hostname()));
  delay(1);
}



void conectar(){
  if ((WiFi.status() != WL_CONNECTED) && (statusConexao != 1)) { statusConexao = 0; }
  if ((WiFi.status() == WL_CONNECTED) && !mqtt.connected() && (statusConexao != 3))  { statusConexao = 2; }
  if ((WiFi.status() == WL_CONNECTED) && mqtt.connected() && (statusConexao != 5)) { statusConexao = 4;}
  switch (statusConexao) {
    case 0:                                               // MQTT e WiFi OFF: inicia WiFi
      Serial.println("MQTT e WiFi OFF: Iniciando WiFi");
      mostrarDisplay("Atenção \nWIFI OFF \nMQTT OFF \nIniciando WiFi");   
      iniciarWIFI();
      statusConexao = 1;
      break;
    case 1:                                                       // WiFi starting, do nothing here
      Serial.println("WIFI iniciando, aguardando : "+ String(contadorEspera));
      if ((digitalRead(SW1) == LOW) && (digitalRead(SW3) == LOW) ){
        iniciarWIFI();
      }
      contadorEspera++;
      break;
    case 2:                                                       // WiFi ON, MQTT OFF: Iniciar MQTT
      Serial.println("WIFI ON, MQTT OFF: iniciar MQTT");
      Serial.println("SSID: " + (String)wm.getWiFiSSID());
      
      mostrarDisplay("Conectado   WIFI ON\nIP: " + WiFi.localIP().toString() + "\nSSID: " + WiFi.SSID());   
      iniciarMQTT();
      statusConexao = 3;
      contadorEspera = 0;
      break;
    case 3:                                                       // WiFi ON, MQTT iniciando, do nothing here
      Serial.println("WIFI ON, MQTT iniciando, esperando : "+ String(contadorEspera));
      contadorEspera++;
      break;
    case 4:                                                       // WiFi up, MQTT up: finish MQTT configuration
      Serial.println("WIFI ON, MQTT ON: publish subscribe MQTT");
       mostrarDisplay("Conectado\nIP: " + WiFi.localIP().toString() + "\nSSID: " + WiFi.SSID() + "\nMQTT State: "+ String(mqtt.state()) + "\nCruzamento:" + String(idCruzamento)  );   
      eventosMQTT();
      statusConexao = 5;
      contadorEspera = 0;                    
      break;
  }


  if (statusConexao == 5) {  //WIFI e MQTT Operando
    if (millis() - tempoUltimaAtualizacao > 30000) {                            //A cada 30 seg
      Serial.println(statusCentral);
       mqtt.publish(topicoCruzamento.c_str() ,statusCentral);                   // Envia status a central
      mqtt.loop();                                                              //Aciona loop mqtt
      tempoUltimaAtualizacao = millis();                                        //atualiza a ultima atualização status
    }
    
    mqtt.loop();                                              // loop MQTT
  } 


// Executando tarefas que não depende de WIFI E MQTT
  if (millis() - lastTask > 5000) {                                 // A cada 5seg print na serial
    Serial.println("Operando. ID:" + String(idCruzamento));
    lastTask = millis();
  }
  delay(100);

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


void piscarVia() {
   tempoAtual = millis();
   if (tempoAtual - tempoTrocadoVia >= TEMPO_PISCA) {
    tempoTrocadoVia = tempoAtual;
    if (bitRead(semaforo, S5_VM)) {
      bitClear(semaforo, S5_VM);
      bitClear(semaforo, S6_VM);
    } else {
      bitSet(semaforo, S5_VM);
      bitSet(semaforo, S6_VM);
    }
    mostrarLed(semaforo);
   }
}

void piscarPedestre() {
   tempoAtual = millis();
   if (tempoAtual - tempoTrocadoPedestre >= TEMPO_PISCA) {
    tempoTrocadoPedestre = tempoAtual;
    if (bitRead(semaforo, S3_VM)) {
      bitClear(semaforo, S3_VM);
      bitClear(semaforo, S4_VM);
    } else {
      bitSet(semaforo, S3_VM);
      bitSet(semaforo, S4_VM);
    }
     mostrarLed(semaforo);
   }
}


void piscarManutencao() {
  tempoAtual = millis();
  if (tempoAtual - tempoTrocadoManutencao >= TEMPO_PISCA) {
    tempoTrocadoManutencao = tempoAtual;
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


void mostrarDisplay(String str){
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.drawString(0,0,str);
  display.display();
}
