// ********************************************************************** //
// *** MONITORING DE LA CONSOMMATION ELECTRIQUE D'UNE SALLE DE PROJET *** //
// ***** MAXENCE MONTFORT | FLORIAN PARIS | TIMOTHEE FERRER-LOUBEAU ***** //
// ******************* POLYTECH'PARIS-UPMC | CFA UPMC ******************* //
// ************************* SEMESTRE 8 - 2017 ************************** //
// ********************************************************************** //

// DECLARATION DES LIBRAIRIES 
#include <Akeru.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_BMP085.h>
#include "DS1307.h"

// DEFINITION DES PINCES
#define PINCE1 A0 // PINCE1
#define PINCE2 A1 // PINCE2
#define PINCE3 A2 // PINCE3

// DEFINITION DU PRIX AU KWH
#define PRIX_KWH 0.15

// DEFINITION DES PINS ENVIRONNEMENT
#define PHOTOCELL A3 // Capteur de lumière
#define DETECTEUR 7 // Detecteur de mouvement
#define LED 8 // LED de détection de mouvement
#define DHTPIN 2 // Module Température et humidité
#define DHTTYPE DHT21 // DHT 21
#define BAROMETRE 6 // Lecture de pression
#define Adafruit_BMP085PIN 3; // Lecture de pression
#define Adafruit_BMP085TYPE Adafruit_BMP085; // Lecture de pression

// DECLARATION DES VARIABLES GLOBALES
// Délai entre chaque envoi à travers le réseau Sigfox
unsigned long prevMillis = 0;
//unsigned long interval =  3600000; // 60mn*60s*1000ms envoi un message toutes les heures
unsigned long interval =  60000; // 10mn*60s*1000ms envoi un message toutes les 10 minutes
// Variables de calculs
float courant_efficace = 0;
float puissance_consommee = 0, energie_tmp = 0, energie_consommee = 0;
int energie_consommee_kilowattheure = 0;
int cout_total = 0;  
int etat_lumiere = 0, lumiere = 0, lecture_photocell = 0;
int valeur_detecteur = 0, etat_detecteur = LOW, presence_tmp = 0, presence = 0;
DHT dht(DHTPIN, DHTTYPE);
int humidite = 0, temperature = 0, pression = 0;
Adafruit_BMP085 bmp;
DS1307 clock;

// STRUCTURE QUI STOCKE LES DONNEES A ENVOYER VIA RESEAU SIGFOX
struct DataToSend 
{
  uint16_t energie_kwh;
  uint16_t cout_total;
  uint8_t temperature;
  uint8_t humidite;
  uint8_t presence;
  uint8_t lumiere;
  uint16_t pression;
};

DataToSend d;

// SETUP
void setup()
{
  Serial.begin(9600);
  Serial.println("Demarrage en cours...");
  delay(3000);
  Akeru.begin();
  dht.begin();
  bmp.begin();
  clock.begin();
  clock.fillByYMD(2017, 3, 10);
  clock.fillByHMS(12, 57, 0);
  clock.fillDayOfWeek(MON);
  clock.setTime();
}

// INITIALISATION DES PINS
void Pin_init()
{
    pinMode(PINCE1, INPUT);
    pinMode(PINCE2, INPUT);
    pinMode(PINCE3, INPUT);
    pinMode(PHOTOCELL, INPUT);
    pinMode(DETECTEUR, INPUT);
    pinMode(LED, OUTPUT);
    pinMode(BAROMETRE, INPUT);
}

// LOOP
void loop()
{
  // Reset des valeurs de calculs si passage à minuit
  getReset();
  
  // Affichage de l'heure (RTC)
  affichageRTC();
  
  // Calcul du courant efficace
  courant_efficace = getValeurEfficace(getValeurMaxPince());
  Serial.println("Courant_efficace (A):");
  Serial.println(courant_efficace);  
  
  // Calcul de la puissance consommee, l'energie consommee (joules et kW.h)
  puissance_consommee = 3*230*courant_efficace;
  Serial.println("Puissance consommee (W):");
  Serial.println(puissance_consommee);  
  energie_consommee = energie_tmp + puissance_consommee; // Attention ! Energie en joules !
  Serial.println("Energie consommee (joules):");
  Serial.println(energie_consommee);
  energie_tmp = energie_consommee; // Pour visualiser la somme de l'énergie consommée dans la journée par interval
  energie_consommee_kilowattheure = energie_consommee/3600/1000; // Energie en kW.h (3600/1000)
  Serial.println("Energie consommee (kW.h):");
  Serial.println(energie_consommee_kilowattheure );
  cout_total = getCoutTotal(energie_consommee_kilowattheure);
  Serial.println("Cout en Euros:");
  Serial.println(cout_total);

  // Etat de la lumière
  lumiere = getValeurLumiere();

  // Détection de mouvement
  presence_tmp = getValeurDetecteur();
  if(presence_tmp == 1)
  {
    presence = 1;
  }
  
  // Lecture de la température
  getValeurTemperature();
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" *C");
    
  // Lecture de l'humidité
  getValeurHumidite();
  Serial.print("Humidite: ");
  Serial.print(humidite);
  Serial.println(" %\t");

  // Lecture de la pression
  pression = getValeurPression();
  Serial.print("Pression: ");
  Serial.print(pression);
  Serial.println(" hPa");
  
  // Envoi des données via le réseau sigfox
  if ((millis() - prevMillis) > interval)
  {
    Akeru.begin();
    prevMillis = millis(); // Sauvegarde de l'heure actuelle pour la prochaine boucle
    d.energie_kwh = energie_consommee_kilowattheure;
    d.cout_total = cout_total;
    d.lumiere = lumiere;
    d.presence = presence;
    d.temperature = temperature;
    d.humidite = humidite;
    d.pression = pression;
    Akeru.send(&d, sizeof(d)); // Envoi des données via Sigfox
    presence = 0;
  }
}

// FONCTION : PASSAGE A MINUIT
void getReset()
{
  if(clock.hour == 0 && clock.minute == 0)
  {
    puissance_consommee = 0, energie_tmp = 0, energie_consommee = 0;
    energie_consommee_kilowattheure = 0;
    cout_total = 0;
    Serial.println("Passage a minuit : valeurs remises a 0.");  
  }
}

// FONCTION : AFFICHAGE DE L'HEURE (RTC)
void affichageRTC()
{
   clock.getTime();
   Serial.println(" ");
   Serial.print(clock.hour, DEC);
   Serial.print(":");
   Serial.print(clock.minute, DEC);
   Serial.print(":");
   Serial.print(clock.second, DEC);
   Serial.print("  ");
   Serial.print(clock.month, DEC);
   Serial.print("/");
   Serial.print(clock.dayOfMonth, DEC);
   Serial.print("/");
   Serial.print(clock.year+2000, DEC);
   Serial.print(" ");

   switch (clock.dayOfWeek)// Friendly printout the weekday
   {
       case MON:
       Serial.print("MON");
       break;
       case TUE:
       Serial.print("TUE");
       break;
       case WED:
       Serial.print("WED");
       break;
       case THU:
       Serial.print("THU");
       break;
       case FRI:
       Serial.print("FRI");
       break;
       case SAT:
       Serial.print("SAT");
       break;
       case SUN:
       Serial.print("SUN");
       break;
   }
   Serial.println(" ");
}

// FONCTION : RECUPERATION VALEUR MAX DE LA PINCE
float getValeurMaxPince()
{
  float valeur_pince1 = 0, valeur_pince2 = 0, valeur_pince3 = 0;
  float valeur_pince_max = 0, valeur_pince1_max = 0, valeur_pince2_max = 0, valeur_pince3_max = 0;
  uint32_t heure_depart = millis();
  
  while((millis()-heure_depart < 1000)) // Echantillonnage sur 1 seconde
  {
    valeur_pince1 = analogRead(PINCE1);
    if(valeur_pince1 > valeur_pince1_max)
    {
      valeur_pince1_max = valeur_pince1;
    }
    valeur_pince2 = analogRead(PINCE2);
    if(valeur_pince2 > valeur_pince2_max)
    {
      valeur_pince2_max = valeur_pince2;
    }
    valeur_pince3 = analogRead(PINCE3);
    if(valeur_pince3 > valeur_pince3_max)
    {
      valeur_pince3_max = valeur_pince3;
    }
  }
  valeur_pince_max = valeur_pince1_max + valeur_pince2_max + valeur_pince3_max;
  return valeur_pince_max;
}

// FONCTION : CALCUL DE IEFF REEL
float getValeurEfficace(float valeur_pince_max)
{
  float valeur_efficace_pince = 0;
  float valeur_efficace_reelle = 0;
  
  valeur_efficace_pince = (valeur_pince_max)/1.41; // Ieff = Imax/rac(2)
  valeur_efficace_reelle = valeur_efficace_pince*0.00488*300; // Pour obtenir la valeur réelle efficace de I (différent de son image) | 0.00488 : résolution | 300 = rapport de transformation+résistance
  return valeur_efficace_reelle;
}

// FONCTION : CALCUL COUT TOTAL
float getCoutTotal(int energie_consommee_kilowattheure)
{
  cout_total = energie_consommee_kilowattheure*PRIX_KWH;
  return cout_total;
}

// FONCTION : CAPTEUR DE LUMIERE
int getValeurLumiere()
{
  lecture_photocell = analogRead(PHOTOCELL);
  Serial.println("Luminosite de la piece :");
  if (lecture_photocell < 10) 
  {
    Serial.println("Piece noire");
    return etat_lumiere = 0;
  }
  else if (lecture_photocell >= 10 && lecture_photocell < 200)
  {
    Serial.println("Piece sombre");
    return etat_lumiere = 1;
  }
  else if (lecture_photocell >= 200 && lecture_photocell < 500) 
  {
    Serial.println("Piece eclairee");
    return etat_lumiere = 2;
  }
  else if (lecture_photocell >= 500 && lecture_photocell < 800) 
  {
    Serial.println("Piece lumineuse");
    return etat_lumiere = 3;
  }
  else 
  {
    Serial.println("Piece tres lumineuse");
    return etat_lumiere = 4;
  }
}

// FONCTION : DETECTEUR DE PRESENCE
int getValeurDetecteur()
{
  valeur_detecteur = digitalRead(DETECTEUR);
  if(valeur_detecteur == HIGH)
  {
    digitalWrite(LED, HIGH);
    if(etat_detecteur == LOW)
    {
      Serial.println("Mouvement detecte...");
      etat_detecteur = HIGH;
    }
    return 1;
  }
  else
  {
    digitalWrite(LED, LOW);
    if(etat_detecteur == HIGH)
    {
      Serial.println("Fin de mouvement detecte...");
      etat_detecteur = LOW;
    }
    return 0;
  }
} 

// FONCTION : LECTURE DE TEMPERATURE
int getValeurTemperature()
{
  temperature = dht.readTemperature();
  if (isnan(temperature) || isnan(humidite)) 
  {
    Serial.println("Impossible de lire le module DHT (Temperature et humidite");
  } 
}

// FONCTION : LECTURE D'HUMIDITE
int getValeurHumidite()
{
  humidite = dht.readHumidity();
  if (isnan(temperature) || isnan(humidite)) 
  {
    Serial.println("Impossible de lire le module DHT (Temperature et humidite");
  } 
}

// FONCTION : LECTURE DE PRESSION
int getValeurPression()
{
  pression = bmp.readPressure()/100;
  return pression;
}
