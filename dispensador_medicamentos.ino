/*
 * TimeRTCSet.pde
 * example code illustrating Time library with Real Time Clock.
 *
 * RTC clock is set in response to serial port time message 
 * A Processing example sketch to set the time is included in the download
 * On Linux, you can use "date +T%s > /dev/ttyACM0" (UTC time zone)
 */

#include <TimeLib.h>
#include <Wire.h>
#include <DS1307RTC.h>  // a basic DS1307 library that returns time as a time_t
#include <TimeAlarms.h> // para fijar las alarmas;

#include <LiquidCrystal.h>

// se inicializa los pines que sirven de interface
LiquidCrystal lcd(6,8,5,4,3,2);

#define UPbutton 9
#define DOWNbutton 10
#define LEFTbutton 11
#define RIGHTbutton 12
#define ENTER 13

int pantalla = 0;   // vista 0 para diferentes vistas en el display
int slotConfigurado;
int cfgCursor = 0;  // indica la posicion del cursor en la pantalla de configuracion
bool parpadeo = false; // flag que activa el parpadeo del cursor de config
unsigned long timeOutpointer = 0;  // determina el cambio en el parapadeo del cursor

// instanciamos los id para cada Alarma corresondiente a los dispensadores
AlarmId id1;
AlarmId id2;
AlarmId id3;
bool sonar = false;
unsigned long timestampAlarma;
unsigned long periodoAlarma;

const int pinAlarma = A0;

// inicializamos los slots donde se guardan los tiempos
char primerSlot[6] = "00:00";
char segundoSlot[6] = "00:00";
char tercerSlot[6] = "00:00";
char *cfg;  // apunta a los slots a configurar

// limita el valor de los digitos para fijar la configuracion hasta 24 hs y 59 min
char limite;

// seteamos los pines del dispensador
const int dispensador1 = A1;
const int dispensador2 = A2;
const int dispensador3 = A3;

void setup()  {
  // seteamos los botones para interacctuar con el sistema
  pinMode(UPbutton, INPUT_PULLUP);
  pinMode(DOWNbutton, INPUT_PULLUP);
  pinMode(LEFTbutton, INPUT_PULLUP);
  pinMode(RIGHTbutton, INPUT_PULLUP);
  pinMode(ENTER, INPUT_PULLUP);
  
  Serial.begin(9600);
  setSyncProvider(RTC.get);   // the function to get the time from the RTC
  lcd.begin(16,2);
}

void loop()
{
  if (Serial.available()) {
    time_t t = processSyncMessage();
    if (t != 0) {
      RTC.set(t);   // set the RTC and the system time to the received value
      setTime(t);          
    }
  }

  if(pantalla != 4 && isPressedButton(UPbutton)){
    ++pantalla; // cambia la vista
    if (pantalla > 3)
      pantalla = 0;
  }

  if(pantalla != 4 && isPressedButton(DOWNbutton)){
    --pantalla;
    if (pantalla < 0)
      pantalla = 3;
  }

  // se muestra la pantalla que desea el usuario
  if(pantalla == 0)
    portada();

  if(pantalla == 1){
    mostrarSlots(primerSlot, "Slot1");
    if(isPressedButton(ENTER)){
        pantalla = 4;
        slotConfigurado = 1;
        cfg = primerSlot;
    }
  }
  if(pantalla == 2){
    mostrarSlots(segundoSlot, "Slot2");
    if(isPressedButton(ENTER)){
        pantalla = 4;
        slotConfigurado = 2;
        cfg = segundoSlot;
    }
  }

  if(pantalla == 3){
    mostrarSlots(tercerSlot, "Slot3");
    if(isPressedButton(ENTER)){
      pantalla = 4;
      slotConfigurado = 3;
      cfg = tercerSlot;
    }
  }

  if(pantalla == 4){
    mostrarConfig(cfg, cfgCursor);

    // disposicion del cursor sobre la pantalla para cambiar los valores
    if(isPressedButton(RIGHTbutton)){
        cfgCursor++;
        if(cfgCursor == 2)  // se saltan los :
            cfgCursor = 3;
        if(cfgCursor > 4)
            cfgCursor = 0; 
    }

    if(isPressedButton(LEFTbutton)){
        cfgCursor--;
        if(cfgCursor == 2)
            cfgCursor = 1;
        if(cfgCursor < 0)
            cfgCursor = 4;
    }
    if(isPressedButton(UPbutton)){

        digLimit();  // calcula el valor de 'limite'
        
        // reseteamos el segundo digito
        // para cuando el primero sea '2' y este supere '4'
        *(cfg + cfgCursor) += 1;
        if(*(cfg) > '1' && *(cfg + 1) > '4')
            *(cfg + 1) = '0';
            
        // reseteamos su supera el limite    
        if(*(cfg + cfgCursor) > limite)
            *(cfg + cfgCursor) = '0';
    }

    if(isPressedButton(DOWNbutton)){

        digLimit();
        
        *(cfg + cfgCursor) -= 1;
        if(*(cfg + cfgCursor) < '0')
            *(cfg + cfgCursor) = limite;
    }
    
    if(isPressedButton(ENTER)){
      // salimos de la configurcion
      pantalla = 0;
      cfgCursor = 0;
      // guardamos la frecuencia e iniciamos la Alarma
      // lo convertimos en segundos
      int hora = (*(cfg) - '0') * 10 + (*(cfg + 1) - '0');
      int minutos = (*(cfg + 3) - '0') * 10 + (*(cfg + 4) - '0');
      int horaEnSeg = hora * 3600;
      int minEnSeg = minutos * 60;
      unsigned long timer = horaEnSeg + minEnSeg;

      // asignamos la Alarma para el slot seleccionado
      switch(slotConfigurado){
        case 1:
            if(!timer){
                Alarm.free(id1);
             }
            else
                id1 = Alarm.timerRepeat(timer, dispensar_slot1);
            break;
        case 2:
            if(!timer){
                Alarm.free(id2);
            }
            else
                id2 = Alarm.timerRepeat(timer, dispensar_slot2);
            break;
        case 3:
            if(!timer){
                Alarm.free(id3);
            }
            else
                id3 = Alarm.timerRepeat(timer, dispensar_slot3);
            break;
      } 
    }
  }

  // Se hace sonar la alarma y se enciende el led
  if(sonar){
    if(millis() - timestampAlarma > 10000){
        sonar = false;  
        analogWrite(pinAlarma, 0);
    }
    if(millis() - periodoAlarma > 1000){
        analogWrite(pinAlarma, 255);
        periodoAlarma = millis();
        Alarm.delay(200);   
    }

    analogWrite(pinAlarma, 0);
  }
}


/********************************************************************
 *                Procedimientos y funciones                        *
 ********************************************************************/
void dispensar_slot1(){
    activarAlarma();

    analogWrite(dispensador1, 255);
    Alarm.delay(200);
    analogWrite(dispensador1, 0);
}

void dispensar_slot2(){
    activarAlarma();

    analogWrite(dispensador2, 255);
    Alarm.delay(200);
    analogWrite(dispensador2, 0);
    
}

void dispensar_slot3(){
    activarAlarma();

    analogWrite(dispensador3, 255);
    Alarm.delay(200);
    analogWrite(dispensador3, 0);
}

void activarAlarma(){
    sonar = true;

    // capturamos el momento en que ocurrio el evento en ms
    timestampAlarma = millis();

    // periodo de intermitencia de la Alarma
    periodoAlarma = millis();
}

bool isPressedButton(const int button){
    bool pressed = false;
    while(!digitalRead(button)){
        pressed = true;
    }
    return pressed;
}

void digLimit(){
    // horario primer digito
    if(cfgCursor == 0){
        limite = '2';
    }
    // horario segundo digito
    if(cfgCursor == 1){
        if(*(cfg) == '2')
            limite = '4';
        else
            limite = '9';
    }

    // minuto primer digito
    if(cfgCursor == 3)
        limite = '5';
    // segundo digito
    if(cfgCursor == 4)
        limite = '9';
}


void portada(){
  /*
   * Muestra el nombre del proyecto
   * con la fecha y la hora
   */
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Medispenser");
  digitalClockDisplay(0);  
  Alarm.delay(100); 
}

void mostrarSlots( char *slot, const char *mensaje){
    /*
     * Guarda en un slot de memoria
     * la frecuencia con que se suministra el
     * medicamento
     * 
     * muestra en pantalla los datos
     * 
     */

     lcd.clear();
     lcd.setCursor(0,0);
     lcd.print(mensaje);

     lcd.setCursor(0,1);
     lcd.print("Frecuecia:");
     lcd.setCursor(11, 1);

     lcd.print(slot);
     Alarm.delay(100);
}

void mostrarConfig(const char *frec, int &cfgCursor){
    /*
     * Muestra la frecuencia
     * con que fue conf el slot
     */
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Configurar Frec: ");
    lcd.setCursor(0,1);
    lcd.print(frec);
    if(millis() - timeOutpointer > 300){
        parpadeo = !parpadeo;
        timeOutpointer = millis();
    }
    if(parpadeo){
        lcd.setCursor(cfgCursor, 1);
        lcd.print(" ");
        Alarm.delay(100);    
    }else{
        lcd.setCursor(0,1);
        lcd.print(frec);
        Alarm.delay(100);
    }
}

void digitalClockDisplay(int cursorPosition){
  // digital clock display of the time
  printDigits(day(), cursorPosition);
  lcd.print("/");
  cursorPosition++;
  printDigits(month(), cursorPosition);
  lcd.print("/");
  lcd.setCursor(++cursorPosition, 1);
  lcd.print(year());
  cursorPosition += 5; 
  printDigits(hour(), cursorPosition);
  lcd.print(":");
  cursorPosition++;
  printDigits(minute(), cursorPosition);
  //cursorPosition++;
  //printDigits(second(), cursorPosition);
}

void printDigits(int digits, int &cursorPosition){
  // utility function for digital clock display: prints preceding colon and leading 0
  lcd.setCursor(cursorPosition, 1);
  if(digits < 10){
    lcd.print('0');
    lcd.setCursor(++cursorPosition, 1);
    lcd.print(digits);
    lcd.setCursor(++cursorPosition, 1);
  } else{
      lcd.print(digits);
      cursorPosition += 2;
      lcd.setCursor(cursorPosition, 1); 
  }
}

/*  code to process time sync messages from the serial port   */
#define TIME_HEADER  "T"   // Header tag for serial time sync message

unsigned long processSyncMessage() {
  unsigned long pctime = 0L;
  const unsigned long DEFAULT_TIME = 1357041600; // Jan 1 2013 

  if(Serial.find(TIME_HEADER)) {
     pctime = Serial.parseInt();
     return pctime;
     if( pctime < DEFAULT_TIME) { // check the value is a valid time (greater than Jan 1 2013)
       pctime = 0L; // return 0 to indicate that the time is not valid
     }
  }
  return pctime;
}
