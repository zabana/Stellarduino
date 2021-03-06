/**
 * Stellarduino.ino
 * The base Arduino sketch that makes up the heart of Stellarduino
 *
 * This software is pretty dodgy, but accomplishes PushTo so long as you
 * preselect alignment stars below.
 *
 * Version: 0.3 Meade Autostar
 * Author: Casey Fulton, casey AT caseyfulton DOT com
 * License: MIT, http://opensource.org/licenses/MIT
 */


#include <Encoder.h>
#include <LiquidCrystal.h>
#include <math.h>
//#include <Wire.h>
//#include "RTClib.h"
//#include <SoftwareSerial.h>

// some types
typedef struct {
  float time;
  float ra;
  float dec;
  float alt;
  float az;
  String name;
} Star;

//RTC_DS1307 RTC;

void fillVectorWithT(float* v, float e, float az);
void fillVectorWithC(float* v, Star star, float initialTime);
void fillStarWithCVector(float* star, float* v);

// solar day (24h00m00s) / sidereal day (23h56m04.0916s)
const float siderealFraction = 1.002737908;

// initial time as radians
float initialTime;

// alignment stars
//Star aAnd = {5.619669, 0.034470, 0.506809, 1.732239, 1.463808, "α And"};
//Star aUmi = {5.659376, 0.618501, 1.557218, 5.427625, 0.611563, "α Umi"};

Star rigelK = {0.0, 3.837912142664153, -1.0618086768405413, 0.0, 0.0, "Rigel K"};
Star arcturus = {0.0, 3.733528341608887, 0.33479293562700113, 0.0, 0.0, "Arcturus"};

// calculation vectors
float firstTVector[3];
float secondTVector[3];
float thirdTVector[3];

float firstCVector[3];
float secondCVector[3];
float thirdCVector[3];

float obsTVector[3];
float obsCVector[3];

float obs[2];

// matricies
float telescopeMatrix[9];
float celestialMatrix[9];
float inverseMatrix[9];
float transformMatrix[9];
float inverseTransformMatrix[9];

// encoders
Encoder altEncoder(2, 4);
Encoder azEncoder(3, 5);

// display
LiquidCrystal lcd(6, 7, 8, 9, 10, 11);

// OMFG Software Serial, need to ditch this in favour of USB port!
//SoftwareSerial mySerial(13, 12); // RX, TX

// encoder steps per revolution of scope (typically 4 * CPR * gearing)
const int altSPR = 10000;
const int azSPR = 10000;

// handy modifiers to convert encoder ticks to radians
float altMultiplier, azMultiplier, altT, azT;

const float rad2deg = 57.29577951308232;

// buttons
const int OK_BTN = A0;

void setup()
{
  //mySerial.begin(9600);
  Serial.begin(9600);
  lcd.begin(16, 2);
  lcd.clear();
  pinMode(OK_BTN, INPUT);

  // setup encoders
  altMultiplier = 2.0 * M_PI / ((float)altSPR);
  azMultiplier = -2.0 * M_PI / ((float)azSPR);
  
  doAlignment();
  
  calculateTransforms();
  
/*  Serial.println("Telescope matrix:");
  printMatrix(telescopeMatrix);

  Serial.println("Celestial matrix:");
  printMatrix(celestialMatrix);

  Serial.println("Inverse Celestial matrix:");
  printMatrix(inverseMatrix);
  
  Serial.println("Transform matrix:");
  printMatrix(transformMatrix); 

  Serial.println("Inverse Transform matrix:");
  printMatrix(inverseTransformMatrix); */
  
  lcd.clear();
  lcd.print("RA: ");
  lcd.setCursor(0, 1);
  lcd.print("Dec:  ");
}

void loop()
{
  altT = altMultiplier * altEncoder.read();
  azT = azMultiplier * azEncoder.read();
  
  fillVectorWithT(obsTVector, altT, azT);
  fillMatrixWithProduct(obsCVector, inverseTransformMatrix, obsTVector, 3, 3, 1);
  fillStarWithCVector(obs, obsCVector); // OMFG THIS IS PROBABLY INCORRECT


/*
  Serial.println("Observed vector:");
  printVector(obsTVector);
  Serial.println("Transformed celestial vector:");
  printVector(obsCVector);
  Serial.println("Celestial coordinates:");
  Serial.print(obs[0]);
  Serial.print(",");
  Serial.println(obs[1]);

  while(Serial.available() == 0)
  {
    // do nothing
  }
  Serial.read();
*/

  lcd.setCursor(5,0);
//  lcd.print(obs[0] * rad2deg, 3);
  lcd.print(rad2hm(obs[0]));
  lcd.print(" ");
  lcd.setCursor(5,1);
//  lcd.print(obs[1] * rad2deg, 3);
  lcd.print(rad2dm(obs[1]));
  lcd.print(" ");
  
  // if there's a serial request waiting, process it
  if (Serial.available()) {
    processSerialMessage(obs);
  }
}

void fillVectorWithT(float* v, float e, float az) {
  v[0] = cos(e) * cos(az);
  v[1] = cos(e) * sin(az);
  v[2] = sin(e);
}

void fillVectorWithC(float* v, Star star, float initialTime) {
  v[0] = cos(star.dec) * cos(star.ra - siderealFraction * (star.time - initialTime));
  v[1] = cos(star.dec) * sin(star.ra - siderealFraction * (star.time - initialTime));
  v[2] = sin(star.dec);
}

void fillStarWithCVector(float* star, float* v)
{
  // OMFG THIS IS PROBABLY INCORRECT
  star[0] = atan(v[1] / v[0]) + siderealFraction * ((float)millis() / 86400000.0f * 2.0 * M_PI - initialTime);
  if(v[0] < 0) star[0] = star[0] + M_PI;
  star[1] = asin(v[2]);
}

void fillVectorWithProduct(float* v, float* a, float* b) {
  float multiplier = 1 / sqrt(
    pow(a[1] * b[2] - a[2] * b[1], 2) +
    pow(a[2] * b[0] - a[0] * b[2], 2) +
    pow(a[0] * b[1] - a[1] * b[0], 2)
  );
  v[0] = multiplier * (a[1] * b[2] - a[2] * b[1]);
  v[1] = multiplier * (a[2] * b[0] - a[0] * b[2]);
  v[2] = multiplier * (a[0] * b[1] - a[1] * b[0]);
}

void fillMatrixWithVectors(float* m, float* a, float* b, float* c)
{
  m[0] = a[0];
  m[1] = b[0];
  m[2] = c[0];
  m[3] = a[1];
  m[4] = b[1];
  m[5] = c[1];
  m[6] = a[2];
  m[7] = b[2];
  m[8] = c[2];
}

void invertMatrix(float* m) {
  float temp;
  int pivrow;
  int pivrows[9];
  int i,j,k;
  
  for(k = 0; k < 3; k++) {
    temp = 0;
    for(i = k; i < 3; i++) {
      if(abs(m[i * 3 + k]) >= temp) {
        temp = abs(m[i * 3 + k]);
        pivrow = i;
      }
    }
    // should do something here... if(m[pivrow * 3 + k] == 0.0) "singular matrix"
    if(pivrow != k) {
      for(j = 0; j < 3; j++) {
        temp = m[k * 3 + j];
        m[k * 3 + j] = m[pivrow * 3 + j];
        m[pivrow * 3 + j] = temp;
      }
    }

    //record pivot row swap
    pivrows[k] = pivrow;

    temp = 1.0 / m[k * 3 + k];
    m[k * 3 + k] = 1.0;

    // row reduction
    for(j = 0; j < 3; j++) {
      m[k * 3 + j] = m[k * 3 + j] * temp;
    }
    
    for(i = 0; i < 3; i++) {
      if(i != k) {
        temp = m[i* 3 + k];
        m[i * 3 + k] = 0.0;
        for(j = 0; j < 3; j++) {
          m[i * 3 + j] = m[i * 3 + j] - m[k * 3 + j] * temp;
        }
      }
    }
  }
  
  for(k = 2; k >= 0; k--) {
    if(pivrows[k] != k) {
      for(i = 0; i < 3; i++) {
        temp = m[i * 3 + k];
        m[i * 3 + k] = m[i * 3 + pivrows[k]];
        m[i * 3 + pivrows[k]] = temp;
      }
    }
  }
}

void fillMatrixWithProduct(float* m, float* a, float* b, int aRows, int aCols, int bCols)
{
  for(int i = 0; i < aRows; i++) {
    for(int j = 0; j < bCols; j++) {
      m[bCols * i + j] = 0;
      for(int k = 0; k < aCols; k++) {
        m[bCols * i + j] = m[bCols * i + j] + a[aCols * i + k] * b[bCols * k + j];
      }
    }
  }
}

void copyMatrix(float* recipient, float* donor)
{
  for(int i = 0; i < 9; i++) {
    recipient[i] = donor[i];
  }
}

void doAlignment() {
  // set initial time - actual time not necessary, just the difference!
  initialTime = (float)millis() / 86400000.0f * 2.0 * M_PI;
  
  // ask user to point scope at first star
  lcd.print("Point: ");
  lcd.print(rigelK.name);
  lcd.setCursor(0,1);
  lcd.print("Then press OK");

  // wait for button press
  while(digitalRead(OK_BTN) == LOW);
  rigelK.time = (float)millis() / 86400000.0f * 2.0 * M_PI;
  rigelK.alt = altMultiplier * altEncoder.read();
  rigelK.az = azMultiplier * azEncoder.read();

  lcd.clear();
  lcd.print("Alt set: ");
  lcd.print(rigelK.alt * rad2deg, 3);
  lcd.setCursor(0,1);
  lcd.print("Az set: ");
  lcd.print(rigelK.az * rad2deg, 3);

  delay(2000);

  // ask user to point scope at second star
  lcd.clear();
  lcd.print("Point: ");
  lcd.print(arcturus.name);
  lcd.setCursor(0,1);
  lcd.print("Then press OK");
  
  // wait for button press
  while(digitalRead(OK_BTN) == LOW);
  arcturus.time = (float)millis() / 86400000.0f * 2.0 * M_PI;
  arcturus.az = azMultiplier * azEncoder.read();
  arcturus.alt = altMultiplier * altEncoder.read();

  lcd.clear();
  lcd.print("Alt set: ");
  lcd.print(arcturus.alt * rad2deg, 3);
  lcd.setCursor(0,1);
  lcd.print("Az set: ");
  lcd.print(arcturus.az * rad2deg, 3);

  delay(2000);
}

void calculateTransforms() {
  // calculate vectors for aAnd and aUmi
  fillVectorWithT(firstTVector, rigelK.alt, rigelK.az);
  fillVectorWithT(secondTVector, arcturus.alt, arcturus.az);
  
  // calculate third's vectors
  fillVectorWithProduct(thirdTVector, firstTVector, secondTVector);

  // calculate celestial vectors for aAnd and aUmi
  fillVectorWithC(firstCVector, rigelK, initialTime);
  fillVectorWithC(secondCVector, arcturus, initialTime);
  
  // calculate third's vector
  fillVectorWithProduct(thirdCVector, firstCVector, secondCVector);
  
  fillMatrixWithVectors(telescopeMatrix, firstTVector, secondTVector, thirdTVector);
  fillMatrixWithVectors(celestialMatrix, firstCVector, secondCVector, thirdCVector);  
  
  copyMatrix(inverseMatrix, celestialMatrix);
  invertMatrix(inverseMatrix);
  
  fillMatrixWithProduct(transformMatrix, telescopeMatrix, inverseMatrix, 3, 3, 3);
  copyMatrix(inverseTransformMatrix, transformMatrix);
  invertMatrix(inverseTransformMatrix);
}

void processSerialMessage(float* star) {
  String request;
  String response;
  delay(10); // chill for a bit to wait for the buffer to fill

  while(Serial.available()) {
    request += (char)Serial.read();
  }
  
  request.trim();
  
  /*
  Serial.print("Received request: '");
  Serial.print(request);
  Serial.println("'");

  Serial.print("Request length: '");
  Serial.print(sizeof(request));
  Serial.println("'");
  */
  
  if (request == "#:GR#") response = rad2hm(star[0]);
  if (request == ":GR#") response = rad2hm(star[0]);
  if (request == "#:U##:GR#") response = rad2hm(star[0]);
  if (request == "#:GD#") response = rad2dm(star[1]);
  if (request == ":GD#") response = rad2dm(star[1]);
  if (request == "#:U##:GD#") response = rad2dm(star[1]);
  
  Serial.print(response + "#");
  
  /*
  Serial.print("Responded with: '");
  Serial.print(response);
  Serial.println("'");
  */
}

String rad2hm(float rad) {
  if (rad < 0) rad = rad + 2.0 * M_PI;
  float hours = rad * 24.0 / (2.0 * M_PI);
  float minutes = (hours - floor(hours)) * 60.0;
  float minfrac = (minutes - floor(minutes)) * 10.0;
  return padding((String)int(floor(hours)), 2) + ":" + padding((String)int(floor(minutes)), 2) + "." + (String)int(floor(minfrac));
}

String rad2dm(float rad) {
  float degs = abs(rad) * 360.0 / (2.0 * M_PI);
  float minutes = (degs - floor(degs)) * 60.0;
  String sign = "+";
  if (rad < 0) sign = "-";
  return sign + padding((String)int(floor(degs)), 2) + "*" + padding((String)int(floor(minutes)), 2);
}

String padding(String str, int length) {
  while(str.length() < length) {
    str = "0" + str;
  }
  return str;
}
    
