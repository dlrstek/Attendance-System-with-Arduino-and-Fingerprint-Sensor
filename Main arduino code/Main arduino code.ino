#include <Adafruit_Fingerprint.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#if (defined(AVR) || defined(ESP8266)) && !defined(AVR_ATmega2560)
#include <SoftwareSerial.h>
SoftwareSerial mySerial(2, 3);
#else
#define mySerial Serial1
#endif

const int PIN_RED = 5;
const int PIN_GREEN = 6;

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
LiquidCrystal_I2C lcd(0x27, 16, 2);
char received;
bool stopProcessing = false; 
bool enrollSuccess = false; 
bool fingerPrintActive = false; 

void setup() {
    Serial.begin(57600);
    mySerial.begin(57600);
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Yoklama sistemi");
    lcd.setCursor(0, 1);
    lcd.print("Giris yapiniz");

    finger.begin(57600);
    if (finger.verifyPassword()) {
        Serial.println("Found fingerprint sensor!");
    } else {
        Serial.println("Did not find fingerprint sensor :(");
        while (1) { delay(1); }
    }

    pinMode(PIN_RED, OUTPUT);
    pinMode(PIN_GREEN, OUTPUT);
}

void loop() {
    while (!stopProcessing) {
        if (Serial.available() > 0) {
            received = Serial.read();
            
            if (received == '1') {
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Giris Basarili");
                delay(2000);
                lcd.clear();
            } else if (received == '0') {
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Hatali giris");
                lcd.setCursor(0, 1);
                lcd.print("Tekrar deneyin");
                delay(2000);
                lcd.clear();
            } else if (received == 'E') {
                enrollSuccess = enrollFingerprint();
            } else if (received == 'D') {
                int id = readIdFromSerial();
                deleteFingerprint(id);
            } else if (received == 'A') {
                fingerPrintActive = true;
                while (fingerPrintActive) {
                    checkAndDisplayFingerprint();
                    if (stopProcessing) {
                        break;
                    }
                }
            } else if (received == 's') {
                stopProcessing = true;
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Yoklama");
                lcd.setCursor(0, 1);
                lcd.print("Durduruldu");
                delay(2000);
                lcd.clear();
            }
        }
    }

    stopProcessing = false;
}

void setColor(int R, int G) {
    analogWrite(PIN_RED, R);
    analogWrite(PIN_GREEN, G);
}

bool enrollFingerprint() {
    int id = findAvailableID();
    if (id == -1) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("No available IDs");
        delay(2000);
        lcd.clear();
        return false;
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ID: ");
    lcd.print(id);
    lcd.setCursor(0, 1);
    lcd.print("Parmak okut");
    delay(2000);

    if (getFingerprintEnrollStep(1) != FINGERPRINT_OK) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Hata olustu");
        delay(2000);
        lcd.clear();
        return false;
    }

    if (checkFingerprintAlreadyEnrolled()) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Parmak kayitli");
        delay(2000);
        lcd.clear();
        return false;
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Parmaginizi");
    lcd.setCursor(0, 1);
    lcd.print("cekiniz");
    delay(2000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Tekrar okutun");
    delay(2000);

    int enrollStatus = getFingerprintEnrollStep(2);
    if (enrollStatus == FINGERPRINT_OK) {
        lcd.setCursor(0, 1);
        lcd.print("Eslestirme...");
        enrollStatus = finger.createModel();
        if (enrollStatus == FINGERPRINT_OK) {
            finger.storeModel(id);
            digitalWrite(PIN_RED, LOW);
            digitalWrite(PIN_GREEN, HIGH);
            delay(2000);
            digitalWrite(PIN_GREEN, LOW);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Kayit basarili!");
            lcd.setCursor(0, 1);
            Serial.println(id);
            lcd.print("ID: ");
            lcd.print(id);
            delay(2000);
            lcd.clear();
            return true;
        } else if (enrollStatus == FINGERPRINT_ENROLLMISMATCH) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Eslestirme yok");
            delay(2000);
            lcd.clear();
        } else {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Kayit basarisiz");
            delay(2000);
            lcd.clear();
        }
    } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Hata olustu");
        delay(2000);
        lcd.clear();
    }
    return false;
}

int findAvailableID() {
    for (int id = 1; id < 128; id++) {
        if (finger.loadModel(id) != FINGERPRINT_OK) {
            return id;
        }
    }
    return -1; 
}

bool checkFingerprintAlreadyEnrolled() {
    int p = finger.fingerFastSearch();
    return (p == FINGERPRINT_OK);
}

uint8_t getFingerprintEnrollStep(int step) {
    int p = -1;

    // Read fingerprint
    p = getImageWithRetry();
    if (p != FINGERPRINT_OK) {
        return p;
    }

    p = finger.image2Tz(step);
    if (p != FINGERPRINT_OK) {
        return p;
    }

    return FINGERPRINT_OK;
}

int getImageWithRetry() {
    int p;
    while (true) {
        p = finger.getImage();
        if (p == FINGERPRINT_OK) {
            break;
        }
        if (p == FINGERPRINT_NOFINGER) {
            lcd.setCursor(0, 1);
            lcd.print("Parmak bekleniyor...");
        } else {
            lcd.setCursor(0, 1);
            lcd.print("Hata: ");
            lcd.print(p);
        }
        delay(100);
    }
    return p;
}

int readIdFromSerial() {
    while (!Serial.available()) {
        delay(10);
    }
    return Serial.parseInt();
}

void deleteFingerprint(int id) {
    int p = finger.deleteModel(id);
    if (p == FINGERPRINT_OK) {
        Serial.println("Fingerprint deleted successfully.");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("iz silindi");
        delay(2000);
        lcd.clear();
    }
}

void checkAndDisplayFingerprint() {
    while (fingerPrintActive) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Parmak okutun");
        delay(1000);
        if (Serial.available() > 0) {
            char command = Serial.read();
            if (command == 's') {
                fingerPrintActive = false;
                lcd.clear();
                return;
            }
        }

        int imageStatus = getImageWithRetry();
        if (imageStatus != FINGERPRINT_OK) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Okunamadı");
            lcd.setCursor(0, 1);
            lcd.print("Bastan Deneyin");
            delay(2000); 
            lcd.clear();
            continue;
        }

        int imageToTemplateStatus = finger.image2Tz();
        if (imageToTemplateStatus != FINGERPRINT_OK) {
            lcd.clear();
            Serial.print("Image to Template Error: ");
            Serial.println(imageToTemplateStatus);
            continue;
        }

        int searchStatus = finger.fingerFastSearch();
        if (searchStatus == FINGERPRINT_OK) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Yoklama basarili");
            lcd.setCursor(0, 1);
            lcd.print("ID: ");
            lcd.print(finger.fingerID);
            Serial.print("Fingerprint ID: ");
            Serial.println(finger.fingerID);
            digitalWrite(PIN_RED, LOW);    
            digitalWrite(PIN_GREEN, HIGH); 
            delay(2000); 
            digitalWrite(PIN_GREEN, LOW);
            lcd.clear();
            break;
        } else {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Parmak kayitli");
            lcd.setCursor(0, 1);
            lcd.print("degil");
            digitalWrite(PIN_GREEN, LOW); 
            digitalWrite(PIN_RED, HIGH);  
            delay(2000); 
            digitalWrite(PIN_RED, LOW);   
            Serial.print("Fingerprint not found, status: ");
            Serial.println(searchStatus);
            lcd.clear(); 
            continue;
        }
    }
}
