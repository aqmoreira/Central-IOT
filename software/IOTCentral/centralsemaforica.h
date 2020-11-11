
#define MQTTServer       "broker.hivemq.com"
#define MQTTPort         "1883"
#define MQTTUser         "mqtt_user"
#define MQTTPass         "mqtt_pass"
#define IDCruzamento    "9999"


//Tempos das Fases
#define TEMPO_MAXIMO_CRUZAMENTO_VIA   9000
#define TEMPO_MAXIMO_VERDE           25000
#define TEMPO_ALERTA                 10000
#define TEMPO_AMARELO                 6000
#define TEMPO_SEGURANCA               3000
#define TEMPO_VERDE                  18000
#define TEMPO_MAXIMO_ESPERA_VLT      25000
#define TEMPO_MINIMO_VERDE           12000
#define TEMPO_ALERTA_PISCANTE          300



//Tópicos MQTT
#define TOPICO_CENTRAL_CRUZAMENTO   "central/cruzamento/"   ; 
#define TOPICO_VIA1_CRUZAMENTO      "via01/+/"
#define TOPICO_VIA2_CRUZAMENTO      "via02/+/"


#define TEMPO_PISCA 500
#define TEMPO_INTERVALO_ENTRE_RECONECTAR 5000

//Chaves
#define SW1 D0
#define SW2 D3
#define SW3 D4
#define BUZZER D6

#define FASE_INICIO 0
#define FASE_INICIANDO 1
#define FASE_ON_VERMELHO 2
#define FASE_ON_VERDE 3
#define FASE_ON_AMARELO 4
#define FASE_ON_ALERTA 5

#define FASE_OFF_SEGURO 6
#define FASE_OFF_VERMELHO 7
#define FASE_OFF_VERDE 8
#define FASE_OFF_AMARELO 9
#define FASE_OFF_ALERTA 10

#define LED_FASE_VERMELHO 0x16A4
#define LED_FASE_AMARELO 0x2A92
#define LED_FASE_VERDE 0x2949
#define LED_FASE_ALERTA 0x2AA4
#define LED_FASE_MANUTENCAO 0x2A92
#define LED_FASE_SEGURO 0x2AA4





//Semaforos
//T2 T1 VM VD VM VD VM VD - Byte Superior
//VM VD VM AM VD VM AM VD - Byte Inferior

#define S1_VM 2
#define S1_AM 1
#define S1_VD 0

#define S2_VM 5
#define S2_AM 4
#define S2_VD 3

#define S3_VM 7
#define S3_VD 6

#define S4_VM 9
#define S4_VD 8

#define S5_VM 11
#define S5_VD 10

#define S6_VM 13
#define S6_VD 12

#define VLT1 14
#define VLT2 15


#define CSTR(x) x.c_str()   
#define TXTIP(x) CSTR(x.toString())



void getTime() {
  int tz           = -5;
  int dst          = 0;
  time_t now       = time(nullptr);
  unsigned timeout = 5000;
  unsigned start   = millis();  
  configTime(tz * 3600, dst * 3600, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync: ");
  while (now < 8 * 3600 * 2 ) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
    if((millis() - start) > timeout){
      Serial.println("\n[ERROR] Failed to get NTP time.");
      return;
    }
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
}
