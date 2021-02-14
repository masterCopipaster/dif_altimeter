#include <TimerOne.h>
#include <EEPROM.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>
#include <SD.h>
#include <RTC.h>
#include <GyverPower.h>
#include <stdlib.h>

#define DATA_FILE_NAME F("datalog.csv")
#define SETTINGS_FILE_NAME F("settings.txt")
char DATA_DELIMITER = ';';
char DECIMAL_DELIMITER = '.';
#define DATA_FORMAT_STR "%02d-%02d-%04d%c%02d:%02d:%02d%c%s%c%s%c%s\n"

#define SIGNAL_LED 5
#define SD_SS 4
#define DATA_DEST dataFile
#define LOG_DEST Serial 
#define ARGN 5
#define DATA_STR_LEN 100
#define SARG_LEN 10
#define CMDSTR_LEN 30
#define WAKEUP_PERIOD_MS 300

uint32_t period = 5000;
bool timing_mode_rtc = 0;
int arg[ARGN];
char sarg[ARGN][SARG_LEN];
char cmd_str[CMDSTR_LEN];
int argn = 0;
int sargn = 0;
char cmd[SARG_LEN] = {};
bool power_saving = 0;
char data_str[DATA_STR_LEN];

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
    if(!RTC.isRunning())
    {
      RTC.setDate(1, 1, 21);
      RTC.setTime(0, 0, 0);
      LOG_DEST.println(F("RTC was found off. So was reset to default!"));
    }
    
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
      //power.autoCalibrate();
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
  create_data_str(data_str);
  DATA_DEST.print(data_str);
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
  argn = sscanf(cmd_str, "%s%d%d%d%d%d", cmd, &arg[0], &arg[1], &arg[2], &arg[3], &arg[4]);
  sargn = sscanf(cmd_str, "%s%s%s%s%s%s", cmd, &sarg[0], &sarg[1], &sarg[2], &sarg[3], &sarg[4]);
}

void select_cmd()
{
  if(!strcmp(cmd, "st")) exec_st();
  else if(!strcmp(cmd, "sdp")) exec_sdp();
  else if(!strcmp(cmd, "sd")) exec_sd();
  else if(!strcmp(cmd, "sps")) exec_sps();
  else if(!strcmp(cmd, "stm")) exec_stm();
  else if(!strcmp(cmd, "sfd")) exec_sfd();
  else if(!strcmp(cmd, "sdd")) exec_sdd();
  else if(!strcmp(cmd, "pd")) exec_pd();
  else if(!strcmp(cmd, "ping")) exec_ping();
  else LOG_DEST.println("command not found");
}

void exec_pd()
{
  create_data_str(data_str);
  LOG_DEST.print(data_str);
}

void char_replace(char* str, char what, char with)
{
  for (int i = 0; i < strlen(str); i++)
  {
    if(str[i] == what) 
      str[i] = with;
  }
}

void create_data_str(char* str)
{
  //"%02d-%02d-%04d%c%02d:%02d:%02d%c%s%c%s%c%s\n"
  char tempstr[10];
  char presspstr[10];
  char altstr[10];
  dtostrf(bmp.readTemperature(), 3, 2, tempstr);
  dtostrf(bmp.readPressure(), 6, 2, presspstr);
  dtostrf(bmp.readAltitude(), 4, 2, altstr);
  char_replace(tempstr, '.', DECIMAL_DELIMITER);
  char_replace(presspstr, '.', DECIMAL_DELIMITER);
  char_replace(altstr, '.', DECIMAL_DELIMITER);
  sprintf(str, DATA_FORMAT_STR, RTC.getDay(), RTC.getMonth(), RTC.getYear(), DATA_DELIMITER,
                           RTC.getHours(), RTC.getMinutes(), RTC.getSeconds(), DATA_DELIMITER,
                           tempstr, DATA_DELIMITER,
                           presspstr, DATA_DELIMITER,
                           altstr);
}


void exec_sdp()
{
  if(argn == 2) 
    period = constrain(arg[0], 1, 36000000);
}

void exec_stm()
{
  if(argn == 2) 
    timing_mode_rtc = arg[0];
}

void exec_sdd()
{
  if(sargn == 2) 
    DECIMAL_DELIMITER = sarg[0][0];
}

void exec_sfd()
{
  if(sargn == 2) 
    DATA_DELIMITER = sarg[0][0];
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

int cycle_time;

#define GET_SECONDS (RTC.getHours() * 3600 + RTC.getMinutes() * 60 + RTC.getSeconds())
void delay_handler()
{
  uint32_t seconds_start = GET_SECONDS;
  while(1)
  {
    //digitalWrite(SIGNAL_LED, HIGH);
    int delay_time = timing_mode_rtc ? WAKEUP_PERIOD_MS : (period - cycle_time);
    if(power_saving)
    {
      power.sleepDelay(delay_time);
    }
    else
    {
      delay(delay_time);
    }
    if(!timing_mode_rtc) break;
    else
    {
      uint32_t seconds = GET_SECONDS;
      //Serial.println(seconds);
      int period_seconds = period / 1000;
      if(seconds != seconds_start && seconds % period_seconds == 0) break; 
    }
    //digitalWrite(SIGNAL_LED, LOW);
  }
  //digitalWrite(SIGNAL_LED, LOW);
}
void loop(void)
{
  cycle_time = millis();
  if(read_cmd_serial())
  {
    parse_cmd();
    select_cmd();
  }
  handle_data();
  cycle_time = millis() - cycle_time;
  //Serial.println(cycle_time);
  delay_handler();
}
