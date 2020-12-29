#include <TimerOne.h>
#include <EEPROM.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include <SD.h>
#include <RTC.h>
#include <GyverPower.h>

#define DATA_FILE_NAME F("datalog.csv")
#define SETTINGS_FILE_NAME F("settings.txt")
#define DATA_DELIMITER ';'

#define SIGNAL_LED 5
#define SD_SS 4
#define DATA_DEST dataFile
#define LOG_DEST Serial 

uint32_t period = 5000;
int arg[5];
char cmd_str[30];
int argn;
char cmd[10];
bool power_saving = 0;

Adafruit_BMP280 bmp; // I2C
DS1307 RTC;
void setup(void)
{
  pinMode(SIGNAL_LED, OUTPUT);
  digitalWrite(SIGNAL_LED, HIGH);
  Serial.begin(9600);
  Serial.setTimeout(-1);
  while(1)
  {
    if (!bmp.begin()) {
      LOG_DEST.println(F("Could not find a valid BMP280 sensor, check wiring!"));
      continue;
    }
    
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                    Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                    Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
    
    RTC.begin();
    RTC.setHourMode(CLOCK_H24);
    
    if (!SD.begin(SD_SS)) {
      LOG_DEST.println(F("Card failed, or not present"));
      if(read_cmd_serial())
      {
        parse_cmd();
        select_cmd();
      }
      continue;
    }
    
    File settings = SD.open(SETTINGS_FILE_NAME, FILE_READ);
    if(settings)
    {
      LOG_DEST.print(F("Reading settings from file... "));
      while(read_cmd_file(settings))
      {
        parse_cmd();
        select_cmd();
      }
      LOG_DEST.println(F("Ready."));
    }
    
    if(power_saving)
    {
      power.setSleepMode(STANDBY_SLEEP);
      power.autoCalibrate();
      //power.hardwareEnable(PWR_UART0);
    }

    break;
  }
  digitalWrite(SIGNAL_LED, LOW);
}

uint16_t read_data(byte n)
{
  uint16_t res = 0;
  for(byte i = 0; i < n; i++) res += analogRead(i);
  return res;
}

void handle_data(void)
{
  //Serial.println("isr");
  digitalWrite(SIGNAL_LED, HIGH);
  File dataFile = SD.open(DATA_FILE_NAME, FILE_WRITE);
  if(!dataFile)
  {
    if (!SD.begin(SD_SS))
    {
      LOG_DEST.println(F("Card failed, or not present"));
      return;
    }
    File dataFile = SD.open(DATA_FILE_NAME, FILE_WRITE);
  }
  //print out date
  DATA_DEST.print(RTC.getDay());
  DATA_DEST.print("-");
  DATA_DEST.print(RTC.getMonth());
  DATA_DEST.print("-");
  DATA_DEST.print(RTC.getYear());
  DATA_DEST.print(DATA_DELIMITER);
  //print out time
  DATA_DEST.print(RTC.getHours());
  DATA_DEST.print(":");
  DATA_DEST.print(RTC.getMinutes());
  DATA_DEST.print(":");
  DATA_DEST.print(RTC.getSeconds());
  DATA_DEST.print(DATA_DELIMITER);
  //print out temperature
  DATA_DEST.print(bmp.readTemperature());
  DATA_DEST.print(DATA_DELIMITER);
  //print out pressure
  DATA_DEST.print(bmp.readPressure());
  DATA_DEST.print(DATA_DELIMITER);
  //print out approx altitude
  DATA_DEST.print(bmp.readAltitude(1013.25)); /* Adjusted to local forecast! */
  DATA_DEST.println();
  dataFile.close();
  digitalWrite(SIGNAL_LED, LOW);
}


int read_cmd_serial()
{
  int n = 0;
  if(Serial.available())
  {
    while((cmd_str[n] = Serial.read()) != '\n')
    {
      if(cmd_str[n] > 0) n++;
    }
  }
  cmd_str[n] = 0;
  return n;
}

int read_cmd_file(File f)
{
  int n = 0;
  if(f.available())
  {
    while((cmd_str[n] = f.read()) != '\n')
    {
      if(cmd_str[n] < 0) return 0;
      n++;
    }
  }
  cmd_str[n] = 0;
  return n;
}

void parse_cmd()
{
  cmd[0] = 0;
  argn = sscanf(cmd_str, "%s %d %d %d", cmd, &arg[0], &arg[1], &arg[2]);
  //Serial.println("got parse");
  //Serial.println(argn);
  //Serial.println(arg);
}

void select_cmd()
{
  if(!strcmp(cmd, "st")) exec_st();
  else if(!strcmp(cmd, "sdp")) exec_sdp();
  else if(!strcmp(cmd, "sd")) exec_sd();
  else if(!strcmp(cmd, "sps")) exec_sps();
  else if(!strcmp(cmd, "ld")) exec_ld();
  else if(!strcmp(cmd, "ping")) exec_ping();
  else LOG_DEST.println("command not found");
}

void exec_ld()
{
  //print out date
  LOG_DEST.print(RTC.getDay());
  LOG_DEST.print("-");
  LOG_DEST.print(RTC.getMonth());
  LOG_DEST.print("-");
  LOG_DEST.print(RTC.getYear());
  LOG_DEST.print(DATA_DELIMITER);
  //print out time
  LOG_DEST.print(RTC.getHours());
  LOG_DEST.print(":");
  LOG_DEST.print(RTC.getMinutes());
  LOG_DEST.print(":");
  LOG_DEST.print(RTC.getSeconds());
  LOG_DEST.print(DATA_DELIMITER);
  //print out temperature
  LOG_DEST.print(bmp.readTemperature());
  LOG_DEST.print(DATA_DELIMITER);
  //print out pressure
  LOG_DEST.print(bmp.readPressure());
  LOG_DEST.print(DATA_DELIMITER);
  //print out approx altitude
  LOG_DEST.print(bmp.readAltitude(1013.25)); /* Adjusted to local forecast! */
  LOG_DEST.println();
}


void exec_sdp()
{
  if(argn == 2) 
    period = constrain(arg[0], 1, 36000000);
}

void exec_st()
{
  if(argn == 4) 
  {
      RTC.setTime(arg[0], arg[1], arg[2]);  
  }
}

void exec_sd()
{
  if(argn == 4) 
  {
      RTC.setDate(arg[0], arg[1], arg[2]);  
  }
}

void exec_sps()
{
  if(argn == 2) 
  {
      power_saving = arg[0] != 0;  
  }
}

void exec_ping()
{
  LOG_DEST.println("pong");
}


void loop(void)
{
  if(read_cmd_serial())
  {
    parse_cmd();
    select_cmd();
  }
  handle_data();
  if(power_saving)
  {
    power.sleepDelay(period);
  }
  else
  {
    delay(period);
  }
}
