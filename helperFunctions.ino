void customMemset(void* ptr, int value, size_t numBytes){
  unsigned char* bytePtr = static_cast<unsigned char*>(ptr);
  
  for(size_t i = 0; i < numBytes; i++){
    bytePtr[i] = static_cast<unsigned char>(value); 
  }
}

//Structure for saving data to the EEPROM
struct SensorData {
  float temperature;
  float humidity;
  DateTime timestamp;
};

//set the time on the RTC, only hour and minute
void setTime(){
  DateTime timestamp = Clock.read();
  if(tokenBuffer[2] <=24 && tokenBuffer[3] <= 60){
    timestamp.Hour = tokenBuffer[2];
    timestamp.Minute = tokenBuffer[3];
    timestamp.Second = 0;
    Clock.write(timestamp);
  } else {
    Serial.println("Incorrect Time");
  }
}

//print the time from the RTC module
void printTime(){
  DateTime dis = Clock.read();
  if(!etherCommand){
    Serial.print(dis.Hour); Serial.print(":");
    if(dis.Minute > 10)
      Serial.println(dis.Minute);
    else{
      Serial.print(0); Serial.println(dis.Minute);
    }
  } else {
    Udp.beginPacket(retIP, 48996);
    writeEthPacket("Time:", dis.Hour);
    if(dis.Minute > 9)
      writeEthPacket(":", dis.Minute);
    else{
      writeEthPacket(":", 0);
      writeEthPacket("", dis.Minute);
    }
    Udp.endPacket();
    sentPackets++;
  }
}
//used to find the next avaliable address
int findRecentAddress(){
  int address = 12;
  while(address < EEPROM.length()){
    if(EEPROM.read(address) == 255 && address != 12){
      return address; 
    }
    address++;
  }
  return 12;
}


//used to save the Temperature and Humidity at 5 second and 15 minute intervals
void saveTempAndHumidity(){
  unsigned long currMillis = millis();

  //save every 5 seconds to global var
  if(currMillis - prevMillisSensor >= 5000){
    prevMillisSensor = currMillis;
    dht22.read2(&temperature, &humidity, NULL);
    temperature = celsiusToFahrenheit(temperature);
    if(menu == HOME){
      lcd.setCursor(5,0);
      lcd.print(temperature);
      lcd.setCursor(9, 1);
      lcd.print(humidity);
    }
    return;
  }
  //save every 15 minutes using the RTC module
  DateTime now = Clock.read();
  if((now.Minute == 0 || now.Minute == 15 || now.Minute == 30 || now.Minute == 45) && now.Minute != written){
    prevMillisSensor = currMillis;
    dht22.read2(&temperature, &humidity, NULL);
    temperature = celsiusToFahrenheit(temperature);
    written = now.Minute;
    SensorData data;
    data.temperature = temperature;
    data.humidity = humidity;
    data.timestamp = now;
  
    int address = findRecentAddress();
    EEPROM.put(address, data);
    EEPROM.put(address + sizeof(SensorData), 255);
  }
}

//depending on rgbState either switch RGB off or on
void switchRGB(){
  if(rgbState){
    leds[3].setRGB(LED_RGB[0], LED_RGB[1], LED_RGB[2]);
  } else {
    leds[3].setRGB(0, 0, 0);
  }
  FastLED.show();
}

//chnage the RGB color
void changeRGB(){
  for(int i = 1; i <=3; i++){
    LED_RGB[i-1] = tokenBuffer[i];
  }
}

//convert from celsius to fahrenheit
float celsiusToFahrenheit(float cel){
  return (cel * 9/5.0) + 32;
}

byte tempThreshold[4] = {60, 70, 81, 90};

//function to check the temperature reading and change the state of alarm
void updateAlarm(){
  if(temperature <= tempThreshold[0]){
    setAlarmState(MAJOR_UNDER, "MAJOR_UNDER");
    leds[1].setRGB(128, 0, 128);
  } else if(temperature <= tempThreshold[1]){
    setAlarmState(MINOR_UNDER, "MINOR_UNDER");
    leds[1].setRGB(0, 0, 255);
  } else if(temperature >= tempThreshold[3]){
    setAlarmState(MAJOR_OVER, "MAJOR_OVER");
    leds[1].setRGB(255, 0, 0);
  } else if(temperature >= tempThreshold[2]){
    setAlarmState(MINOR_OVER, "MINOR_OVER");
    leds[1].setRGB(255, 165, 0);
  } else {
    setAlarmState(COMFORTABLE, "COMFORTABLE");
    leds[1].setRGB(0, 255, 0);
  }
  FastLED.show();
}

//changes the alarm state as well as sends a "alarm" packet
void setAlarmState(AlarmState newState, char mess[]){
  if(currentAlarm != newState){
    currentAlarm = newState;
    int dSize = 0;
    for(int i = 0; mess[i] != '\0'; i++)
      dSize++;
    
    Udp.beginPacket(retIP, 8888);
    Udp.write(mess, dSize);
    Udp.endPacket();
    sentPackets++;

  }
}

//checks to see if there is connectivity on the ethernet module, changes led based on that
void setEthStatusLED(){
  if (Ethernet.linkStatus() != LinkOFF)
    leds[0].setRGB(0, 255, 0);
  else 
    leds[0].setRGB(255, 0, 0);

  FastLED.show();
}

//takes any incoming commands from UDP packets and runs them through the processing and executing
void parseUDPCommands(){
  int packetSize = Udp.parsePacket();
  int pBufferIndex = 0;
  if(packetSize){
    recievedPackets++;
    Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
    for(int i = 0; i < UDP_TX_PACKET_MAX_SIZE; i++){
      if(packetBuffer[i] == '\0')
        break;
      Serial.println(packetBuffer[i], HEX);
      pBufferIndex++;
    }
    byte checksum = DCP_genCmndBCH(packetBuffer, pBufferIndex-1);
    if(checksum == packetBuffer[pBufferIndex-1]){

      if(packetBuffer[pBufferIndex - 3] == deviceID){
        if(packetBuffer[pBufferIndex -2] == 0x03){
          Serial.println("Recieved FUDR request");
        } else if(packetBuffer[pBufferIndex -2] == 0x02){
          Serial.println("Recieved UPDR request");
          sendData data;
          data.temperature = temperature;
          data.address = 0x01;
          char p[sizeof(data)];
          memcpy(p, &data, sizeof(data));
          for(int i = 0; i < sizeof(p); i++){
            Serial.println(p[i], HEX);
          }
          byte bch = DCP_genCmndBCH(p, sizeof(p));
          Serial.println(bch, HEX);
          Udp.beginPacket(retIP, 48996);
          Udp.write(p, sizeof(p));
          Udp.write(bch);
          Udp.endPacket();
          sentPackets++;
        }
      }
    } else {
      menu = CHECKSUM_ERR;
      lcd.setCursor(0,0);
      lcd.print("CHECKSUM ERR");
      lcd.setCursor(0,1);
      lcd.print("CALC:");
      lcd.print(checksum, HEX);
      lcd.print(" ");
      lcd.print("EXP:");
      lcd.print(packetBuffer[pBufferIndex-1], HEX);
      
      
    }
  }
  customMemset(packetBuffer, '\0', UDP_TX_PACKET_MAX_SIZE);
}

//simple packet with just string data
void writeEthPacket(char data[]){
  int dataSize = 0;
  for(int i = 0; data[i] != '\0'; i++)
    dataSize++;

  Udp.beginPacket(retIP, 48996);
  Udp.write(data, dataSize);
  Udp.endPacket();
  sentPackets++;
  
}


//sends a UDP packet with a string followed by a float
void writeEthPacket(char sData[], float data){
  char out[8];
  dtostrf(data, 5, 2, out);
  int floatSize = 0;
  int charSize = 0;
  for(int i = 0; sData[i] != '\0'; i++)
    charSize++;
  for(int i = 0; out[i] != '\0'; i++)
    floatSize++;

  Udp.write(sData, charSize);
  Udp.write(out, floatSize);
}

//sends a USP packet with a string followed by an integer
void writeEthPacket(char sData[], int data){
  char out[8];
  itoa(data, out, 10);
  int intSize = 0;
  int charSize = 0;
  for(int i = 0; sData[i] != '\0'; i++)
    charSize++;
  for(int i = 0; out[i] != '\0'; i++)
    intSize++;

  Udp.write(sData, charSize);
  Udp.write(out, intSize);
}

//prints the string according to if the commands originated from UDP or Serial
void printString(char data[], int newLine = 0){
  if(etherCommand)
    writeEthPacket(data);
  else if(newLine)
    Serial.println(data);
  else
    Serial.print(data);
}

//length of the array
int charArrLen(char in[]){
  int charSize = 0;
  for(int i = 0; in[i] != '\0'; i++)
    charSize++;
  return charSize;
}

int histIndex = 12;
int changeIndex = 0;
int changeTempNum = 0;
byte changeIP[] = {(ipAdd[0]/100)%10, (ipAdd[0]/10)%10, ipAdd[0]%10,
                   (ipAdd[1]/100)%10, (ipAdd[1]/10)%10, ipAdd[1]%10,
                   (ipAdd[2]/100)%10, (ipAdd[2]/10)%10, ipAdd[2]%10,
                   (ipAdd[3]/100)%10, (ipAdd[3]/10)%10, ipAdd[3]%10
};
byte changeSub[] = {(subnet[0]/100)%10, (subnet[0]/10)%10, subnet[0]%10,
                    (subnet[1]/100)%10, (subnet[1]/10)%10, subnet[1]%10,
                    (subnet[2]/100)%10, (subnet[2]/10)%10, subnet[2]%10,
                    (subnet[3]/100)%10, (subnet[3]/10)%10, subnet[3]%10
};
byte changeGate[] = {(gateway[0]/100)%10, (gateway[0]/10)%10, gateway[0]%10,
                     (gateway[1]/100)%10, (gateway[1]/10)%10, gateway[1]%10,
                     (gateway[2]/100)%10, (gateway[2]/10)%10, gateway[2]%10,
                     (gateway[3]/100)%10, (gateway[3]/10)%10, gateway[3]%10
};
void switchInput(){
  int analogVal = analogRead(A0);
  int switchReading = 0;
  for(int i = 0; i < NUM_SWITCHES; i++){
    if(switchThreshold[i]+ errThresh > analogVal && analogVal > switchThreshold[i] - errThresh)
      switchReading = i+1;
  }

  if(switchReading != prevSwitchState)
    lastDebounce = millis();

  if((millis() - lastDebounce) > debounceDelay){
    if(switchReading != currSwitchState){
      currSwitchState = switchReading;

      switch(currSwitchState) {
        case 1:
          switch(menu){
            case MAIN:
              selectMenuItem();
              break;
            case HOME:
              exitToMain();
              break;
            case STATS:
              exitToMain();
              break;
            case HISTORY:
              exitToMain();
              break;
            case CHECKSUM_ERR:
              exitToMain();
              break;
            case SETTINGS:
              switch(settings){
                case SETTING_MAIN:
                  selectSetting();
                  break;
                case IP_ADDRESS:
                  saveSettingToEEPROM(changeIP, 0);
                  settings = SETTING_MAIN;
                  updateSettingsMenu();
                  break;
                case SUBNET:
                  saveSettingToEEPROM(changeSub, 4);
                  settings = SETTING_MAIN;
                  updateSettingsMenu();
                  break;
                case GATEWAY:
                  saveSettingToEEPROM(changeGate, 8);
                  settings = SETTING_MAIN;
                  updateSettingsMenu();
                  break;
                case TEMPERATURE_SETTINGS:
                  switch(tempSettings){
                    case TEMP_MAIN:
                      selectTemp();
                      break;
                    default:
                      updateTempMenu();
                      tempSettings = TEMP_MAIN;
                      break;
                  }
              }
              break;
          }
          break;
        case 2:
          switch(menu){
            case SETTINGS:
              switch(settings){
                case IP_ADDRESS:
                  changeIndex++;
                  if(changeIndex > 15)
                    changeIndex = 15;
                  changeAddress(changeIP);
                  break;
                case SUBNET:
                  changeIndex++;
                  if(changeIndex > 15)
                    changeIndex = 15;
                  changeAddress(changeSub);
                  break;
                case GATEWAY:
                  changeIndex++;
                  if(changeIndex > 15)
                    changeIndex = 15;
                  changeAddress(changeGate);
                  break;
              }
          }
          break;
        case 3:
          switch(menu){
            case MAIN:
              moveMenu(1);
              break;
            case HISTORY:
              histIndex -= sizeof(SensorData);
              showHistory();
              break;
            case SETTINGS:
              switch(settings){
                case SETTING_MAIN:
                  moveMenu(1);
                  break;
                case IP_ADDRESS:
                  changeAddressNumber(-1, changeIP);
                  break;
                case SUBNET:
                  changeAddressNumber(-1, changeSub);
                  break;
                case GATEWAY:
                  changeAddressNumber(-1, changeGate);
                  break;
                case TEMPERATURE_SETTINGS:
                  switch(tempSettings){
                    case TEMP_MAIN:
                      moveMenu(1);
                      break;
                    default:
                      changeTemp(-1);
                      break;
                  }
              }
              break;
          }
          break;
        case 4:
          switch(menu){
            case MAIN:
              moveMenu(-1);
              break;
            case HISTORY:
              histIndex += sizeof(SensorData);
              showHistory();
              break;
            case SETTINGS:
              switch(settings){
                case SETTING_MAIN:
                  moveMenu(-1);
                  break;
                case IP_ADDRESS:
                  changeAddressNumber(1, changeIP);
                  break;
                case SUBNET:
                  changeAddressNumber(1, changeSub);
                  break;
                case GATEWAY:
                  changeAddressNumber(1, changeGate);
                  break;
                case TEMPERATURE_SETTINGS:
                  switch(tempSettings){
                    case TEMP_MAIN:
                      moveMenu(-1);
                      break;
                    default:
                      changeTemp(1);
                  }
              }
              break;
          }
          break;
        case 5:
          switch(menu){
            case SETTINGS:
              switch(settings){
                case IP_ADDRESS:
                  changeIndex--;
                  if(changeIndex < 0)
                    changeIndex = 0;
                  changeAddress(changeIP);
                  break;
                case SUBNET:
                  changeIndex--;
                  if(changeIndex > 15)
                    changeIndex = 15;
                  changeAddress(changeSub);
                  break;
                case GATEWAY:
                  changeIndex--;
                  if(changeIndex > 15)
                    changeIndex = 15;
                  changeAddress(changeGate);
                  break;
              }
              break;
          }
          break;
      }
    }
  }
  prevSwitchState = switchReading;
}

void moveMenu(int dir){

  if(menu != SETTINGS){
    selectedMenuItem += dir;
    menuCorrection(&selectedMenuItem, numMenuItemsMain, &menuOffset);
    updateMainMenu();
  } else {
    if(settings != TEMPERATURE_SETTINGS){
      selectedSetting += dir;
      menuCorrection(&selectedSetting, numSettingsItems, &settingOffset);
      updateSettingsMenu();
    } else {
      selectedTemp += dir;
      menuCorrection(&selectedTemp, numTempItems, &tempOffset);
      updateTempMenu();
    }
  }
}

void menuCorrection(int *selected, int numItems, int *offset){
  int tSelected = *selected;
  int tOffset = *offset;
  if(tSelected < 0){
    tSelected = numItems -1;
    tOffset = numItems - 2;
  } else if (tSelected >= numItems){
    tSelected = 0;
    tOffset = 0;
  } else if (tSelected < tOffset){
    tOffset--;
  } else if(tSelected >= tOffset + 2) {
    tOffset++;
  }
  *selected = tSelected;
  *offset = tOffset;
}

void selectMenuItem(){

  switch(selectedMenuItem){
    case 0:
      showHome();
      menu = HOME;
      break;
    case 1:
      showHistory();
      menu = HISTORY;
      break;
    case 2:
      showEthStats();
      menu = STATS;
      break;
    case 3:
      updateSettingsMenu();
      menu = SETTINGS;
      break;
    
  }
}

void selectSetting(){

  switch(selectedSetting){
    case 0:
      changeAddress(changeIP);
      settings = IP_ADDRESS;
      break;
    case 1:
      changeAddress(changeSub);
      settings = SUBNET;
      break;
    case 2:
      changeAddress(changeGate);
      settings = GATEWAY;
      break;
    case 3:
      settings = TEMPERATURE_SETTINGS;
      updateTempMenu();
      break;
    case 4:
      eraseHistory();
      settings = SETTING_MAIN;
      break;
    case 5:
      selectedSetting = 0;
      settingOffset = 0;
      exitToMain();
      menu = MAIN;
      break;
    
  }
}

void selectTemp(){

  switch(selectedTemp){
    case 0:
      changeTempNum = 0;
      changeTemp(0);
      tempSettings = UNDER_MAJOR;
      break;
    case 1:
      changeTempNum = 1;
      changeTemp(0);
      tempSettings = UNDER_MINOR;
      break;
    case 2:
      changeTempNum = 2;
      changeTemp(0);
      tempSettings = OVER_MINOR;
      break;
    case 3:
      changeTempNum = 3;
      changeTemp(0);
      tempSettings = OVER_MAJOR;
      break;
    case 4:
      updateSettingsMenu();
      settings = SETTING_MAIN;
      break;
    
  }
}

void updateMainMenu(){
  lcd.clear();

  for(int i = 0; i < 2; ++i){
    int index = i + menuOffset;
    if(index >= 0 && index < numMenuItemsMain){
      lcd.setCursor(1, i);
      lcd.print(mainMenu[index]);

      if(index == selectedMenuItem) {
        lcd.setCursor(0, i);
        lcd.write(0x7E);
      }
    }
  }
}

void updateSettingsMenu(){
  lcd.clear();

  for(int i = 0; i < 2; ++i){
    int index = i + settingOffset;
    if(index >= 0 && index < numSettingsItems){
      lcd.setCursor(1, i);
      lcd.print(settingsMenu[index]);

      if(index == selectedSetting) {
        lcd.setCursor(0, i);
        lcd.write(0x7E);
      }
    }
  }
}

void updateTempMenu(){
  lcd.clear();

  for(int i = 0; i < 2; ++i){
    int index = i + tempOffset;
    if(index >= 0 && index < numTempItems){
      lcd.setCursor(1, i);
      lcd.print(tempMenu[index]);

      if(index == selectedTemp) {
        lcd.setCursor(0, i);
        lcd.write(0x7E);
      }
    }
  }
}

void showHome(){
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Temp:");
  lcd.print(temperature);

  lcd.setCursor(0,1);
  lcd.print("Humidity:");
  lcd.print(humidity);
}

void showEthStats(){
  lcd.clear();

  lcd.setCursor(0,0);
  lcd.print("Sent:");
  lcd.print(sentPackets);

  lcd.setCursor(0,1);
  lcd.print("Recieved:");
  lcd.print(recievedPackets);
}

void showHistory(){
  SensorData data;
  lcd.clear();
  if(histIndex >= findRecentAddress()){
    histIndex = 12;
    
  } else if(histIndex < 0){
    histIndex = findRecentAddress() - sizeof(SensorData);
  }
  EEPROM.get(histIndex, data);
  lcd.setCursor(0,0);
  lcd.print(data.temperature);
  lcd.setCursor(7,0);
  lcd.print(data.humidity);
  lcd.setCursor(0, 1);
  lcd.print(data.timestamp.Hour);
  lcd.print(":");
  if(data.timestamp.Minute < 10){
    lcd.print("0");
    lcd.print(data.timestamp.Minute);
  }else
    lcd.print(data.timestamp.Minute);
  lcd.setCursor(6,1);
  lcd.print(data.timestamp.Month);
  lcd.print("/");
  lcd.print(data.timestamp.Day);
  lcd.print("/");
  lcd.print(data.timestamp.Year);
}

void changeAddressNumber(int dir, byte nani[]){
  lcd.setCursor(changeIndex, 0);
  int setNumber;
  if(changeIndex < 3){
    nani[changeIndex] = circleNum(nani[changeIndex]+dir);
    lcd.print(nani[changeIndex]);
  } else if(changeIndex > 3 && changeIndex < 7){
    nani[changeIndex-1] = circleNum(nani[changeIndex-1]+dir);
    lcd.print(nani[changeIndex-1]);
  } else if(changeIndex > 7 && changeIndex < 11){
    nani[changeIndex-2] = circleNum(nani[changeIndex-2]+dir);
    lcd.print(nani[changeIndex-2]);
  } else if(changeIndex > 11){
    nani[changeIndex-3] = circleNum(nani[changeIndex-3]+dir);
    lcd.print(nani[changeIndex-3]);
  }
  
}

int circleNum(int input){
  if(input < 0)
    return 9;
  if(input > 9)
    return 0;
  return input;
}

void changeAddress(byte input[]){
  lcd.clear();
  for(int i = 0; i < 12; i++){
    if(i%3 != 0 || i == 0)
      lcd.print(input[i]);
    else{
      lcd.print('.');
      lcd.print(input[i]);
    }
  }
  lcd.setCursor(changeIndex,1);
  lcd.write(0);
}

void changeTemp (int dir){
  lcd.clear();
  lcd.setCursor(0,0);

  tempThreshold[changeTempNum] += dir;
  lcd.print(tempThreshold[changeTempNum]);
}

void saveSettingToEEPROM(byte input[], byte start){
  for(int i = start; i < start + 4; i++){
    int val = 0;
    byte mult = 100;
    for(int j = 0; j < 3; j++){
      val += input[j+((i-start)*3)] * mult;
      mult /= 10;
    }
    EEPROM.write(i, val);
  }
  resetFunc();
}

void exitToMain(){
  menu = MAIN;

  updateMainMenu();
}

void eraseHistory(){
  for(int i = 12; i <= findRecentAddress()+1; i++){
    EEPROM.write(i, 0);
  }
}

byte DCP_genCmndBCH(byte buff[], int count){
  byte i, j, bch, nBCHpoly, fBCHpoly;

  nBCHpoly = 0xB8;
  fBCHpoly = 0xFF;

  bch = 0;
  for(i = 0; i < count; i++){
    bch ^= buff[i];

    for(j = 0; j < 8; j++){
      if((bch & 1) == 1)
        bch = (bch >> 1) ^ nBCHpoly;
      else
        bch >>= 1;
    }
  }
  bch ^= fBCHpoly;
  return(bch);
}
