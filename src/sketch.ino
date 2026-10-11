#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_NeoPixel.h>
#include <Servo.h>

// --- Hardware Pin Definitions ---
#define LED_PIN 6
#define NUM_LEDS 8
#define SERVO_PIN 12
#define BUZZER_PIN 8
#define MAGLEV_PIN 22 // <--- NEW: Maglev Trigger Pin

// Port Sensors
#define PORT_CALICUT 2
#define PORT_LOTHAL 9
#define PORT_MUZIRIS 10
#define PORT_TAXILA 11

// Answer Buttons
#define BTN_A 3
#define BTN_B 4
#define BTN_C 5
#define BTN_D 7

// Rotary Encoder
#define ENCODER_CLK 18
#define ENCODER_DT 19
#define ENCODER_SW 14

// --- Objects ---
LiquidCrystal_I2C lcd(0x27, 20, 4);
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
Servo harborGate;

// --- Multiplayer Game Variables ---
const int NUM_PLAYERS = 2;
int playerGold[NUM_PLAYERS] = {100, 100};
int currentPlayer = 0;
const int WIN_TARGET = 300;

int currentWager = 20;
int lastClk = HIGH;
unsigned long triviaStartTime = 0;
const unsigned long TRIVIA_TIME_LIMIT = 15000; // 15 seconds

// State Machine
enum GameState
{
    WAIT_PORT,
    SET_WAGER,
    TRIVIA,
    GAME_OVER
};
GameState state = WAIT_PORT;

uint32_t playerColors[2] = {
    strip.Color(0, 0, 255), // Player 1 = Blue
    strip.Color(255, 0, 0)  // Player 2 = Red
};

// --- Trivia Database (Indian History & Culture) ---
struct Trivia
{
    String question;
    String optAB;
    String optCD;
    int correct; // 1=A, 2=B, 3=C, 4=D
};

const int NUM_QUESTIONS = 30;
Trivia qBank[NUM_QUESTIONS] = {
    {"Oldest Veda?", "A:Rig     B:Sama", "C:Yajur   D:Atharva", 1},
    {"Harappan port city?", "A:Ujjain  B:Lothal", "C:Taxila  D:Magadha", 2},
    {"Built Taj Mahal?", "A:Akbar   B:Babur", "C:ShahjahnD:Tipu", 3},
    {"Capital of Cholas?", "A:Hampi   B:Madurai", "C:Kanchi  D:Thanjavr", 4},
    {"Discovered Zero?", "A:AryabhatB:Raman", "C:Bose    D:Kalam", 1},
    {"Iron Man of India?", "A:Bose    B:Bhagat", "C:Patel   D:Nehru", 3},
    {"1st Indian in Space?", "A:Kalam   B:Rakesh S", "C:Vikram  D:Homi", 2},
    {"Silk Route Empire?", "A:KushanasB:Guptas", "C:Cholas  D:Pandyas", 1},
    {"Ancient University?", "A:Lothal  B:Muziris", "C:Taxila  D:Hampi", 3},
    {"Father of Nation?", "A:Bose    B:Patel", "C:Nehru   D:Gandhi", 4},
    {"Dance of Kerala?", "A:Kathak  B:Bihu", "C:KthakaliD:Odissi", 3},
    {"Festival of Lights?", "A:Holi    B:Diwali", "C:Onam    D:Pongal", 2},
    {"Wrote Mahabharata?", "A:Valmiki B:Tulsi", "C:Vyasa   D:Kalidas", 3},
    {"Golden Temple city?", "A:Delhi   B:Amritsar", "C:Agra    D:Lahore", 2},
    {"Currency of India?", "A:Rupee   B:Riyal", "C:Taka    D:Yen", 1},
    {"Mughal founder?", "A:Akbar   B:Babur", "C:Humayun D:SherShah", 2},
    {"Ashoka's Empire?", "A:Maurya  B:Gupta", "C:Chola   D:Pallava", 1},
    {"Ajanta Caves state?", "A:MP      B:Gujarat", "C:Maha    D:Bihar", 3},
    {"National River?", "A:Yamuna  B:Ganga", "C:GodavariD:Narmada", 2},
    {"Gateway of India?", "A:Delhi   B:Mumbai", "C:Kochi   D:Chennai", 2},
    {"Sun Temple konark?", "A:Odisha  B:Bihar", "C:Bengal  D:Assam", 1},
    {"Wrote Arthashastra?", "A:Ashoka  B:Harsha", "C:KautilyaD:Bana", 3},
    {"National Bird?", "A:Parrot  B:Peacock", "C:Eagle   D:Swan", 2},
    {"Famous Tea state?", "A:Assam   B:Punjab", "C:Gujarat D:Goa", 1},
    {"Sikhism Founder?", "A:Gobind  B:Nanak", "C:Arjun   D:Ranjit", 2},
    {"Classical language?", "A:Hindi   B:Tamil", "C:Bengali D:Marathi", 2},
    {"Yoga origin country", "A:China   B:Nepal", "C:India   D:Japan", 3},
    {"Shivaji's Empire?", "A:Maratha B:Rajput", "C:Mughal  D:Sikh", 1},
    {"Space Agency?", "A:DRDO    B:ISRO", "C:BARC    D:HAL", 2},
    {"Hampi ruins state?", "A:Kerala  B:AP", "C:KarnatakD:TN", 3}};

// --- Shuffle Tracking Variables ---
int questionDeck[NUM_QUESTIONS];
int currentDrawIndex = 0;
int currentQIndex = 0;

// --- Setup ---
void setup()
{
    randomSeed(analogRead(A0));

    pinMode(PORT_CALICUT, INPUT_PULLUP);
    pinMode(PORT_LOTHAL, INPUT_PULLUP);
    pinMode(PORT_MUZIRIS, INPUT_PULLUP);
    pinMode(PORT_TAXILA, INPUT_PULLUP);

    pinMode(BTN_A, INPUT_PULLUP);
    pinMode(BTN_B, INPUT_PULLUP);
    pinMode(BTN_C, INPUT_PULLUP);
    pinMode(BTN_D, INPUT_PULLUP);

    pinMode(ENCODER_CLK, INPUT_PULLUP);
    pinMode(ENCODER_DT, INPUT_PULLUP);
    pinMode(ENCODER_SW, INPUT_PULLUP);

    pinMode(BUZZER_PIN, OUTPUT);

    // <--- NEW: Initialize Maglev Pin and keep it OFF
    pinMode(MAGLEV_PIN, OUTPUT);
    digitalWrite(MAGLEV_PIN, LOW);

    lcd.init();
    lcd.backlight();
    strip.begin();
    strip.show();
    harborGate.attach(SERVO_PIN);

    shuffleDeck(); // Shuffle questions on startup
    resetTurn();
}

// --- Helper Functions ---
void shuffleDeck()
{
    for (int i = 0; i < NUM_QUESTIONS; i++)
    {
        questionDeck[i] = i;
    }
    for (int i = NUM_QUESTIONS - 1; i > 0; i--)
    {
        int j = random(0, i + 1);
        int temp = questionDeck[i];
        questionDeck[i] = questionDeck[j];
        questionDeck[j] = temp;
    }
    currentDrawIndex = 0;
}

void resetTurn()
{
    harborGate.write(0); // Gate locked
    state = WAIT_PORT;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("PLAYER ");
    lcd.print(currentPlayer + 1);
    lcd.print("'s TURN");
    lcd.setCursor(0, 1);
    lcd.print("P1:");
    lcd.print(playerGold[0]);
    lcd.print("G  P2:");
    lcd.print(playerGold[1]);
    lcd.print("G");
    lcd.setCursor(0, 2);
    lcd.print("Target to Win: ");
    lcd.print(WIN_TARGET);
    lcd.setCursor(0, 3);
    lcd.print("Press Port to Trade");

    for (int i = 0; i < NUM_LEDS; i++)
        strip.setPixelColor(i, playerColors[currentPlayer]);
    strip.show();
}

// --- Main Loop ---
void loop()
{
    switch (state)
    {

    // 1. Wait for Player to arrive at a port
    case WAIT_PORT:
        if (digitalRead(PORT_CALICUT) == LOW || digitalRead(PORT_LOTHAL) == LOW ||
            digitalRead(PORT_MUZIRIS) == LOW || digitalRead(PORT_TAXILA) == LOW)
        {

            state = SET_WAGER;
            currentWager = 20;
            harborGate.write(90); // Gate opens for docking

            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Port Docked!");
            lcd.setCursor(0, 1);
            lcd.print("Turn Dial to Wager");
            lcd.setCursor(0, 2);
            lcd.print("Stake: [ ");
            lcd.print(currentWager);
            lcd.print(" G ]");
            lcd.setCursor(0, 3);
            lcd.print("Press 'A' to Confirm");

            tone(BUZZER_PIN, 500, 200);
            delay(500); // Debounce
        }
        break;

    // 2. Select Wager
    case SET_WAGER:
    {
        int newClk = digitalRead(ENCODER_CLK);
        if (newClk != lastClk && newClk == LOW)
        {
            if (digitalRead(ENCODER_DT) == HIGH)
                currentWager += 5;
            else
                currentWager -= 5;

            if (currentWager < 5)
                currentWager = 5;
            if (currentWager > playerGold[currentPlayer])
                currentWager = playerGold[currentPlayer];

            lcd.setCursor(9, 2);
            lcd.print(currentWager);
            lcd.print(" G ]  ");
        }
        lastClk = newClk;

        // Confirm Wager
        if (digitalRead(BTN_A) == LOW || digitalRead(ENCODER_SW) == LOW)
        {
            state = TRIVIA;
            triviaStartTime = millis();

            currentQIndex = questionDeck[currentDrawIndex];
            currentDrawIndex++;

            if (currentDrawIndex >= NUM_QUESTIONS)
            {
                shuffleDeck();
            }

            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print(qBank[currentQIndex].question);
            lcd.setCursor(0, 1);
            lcd.print(qBank[currentQIndex].optAB);
            lcd.setCursor(0, 2);
            lcd.print(qBank[currentQIndex].optCD);
            lcd.setCursor(0, 3);
            lcd.print("15s! Press A/B/C/D");

            tone(BUZZER_PIN, 800, 100);
            delay(300);
        }
    }
    break;

    // 3. Trivia Challenge
    case TRIVIA:
    {
        unsigned long elapsed = millis() - triviaStartTime;
        int answer = 0;

        // LED Hourglass effect
        if (elapsed < TRIVIA_TIME_LIMIT)
        {
            int ledsOn = 8 - (elapsed * 8 / TRIVIA_TIME_LIMIT);
            strip.clear();
            for (int i = 0; i < ledsOn; i++)
                strip.setPixelColor(i, strip.Color(255, 255, 0));
            strip.show();
        }
        else
        {
            answer = 5; // Timeout
        }

        if (digitalRead(BTN_A) == LOW)
            answer = 1;
        else if (digitalRead(BTN_B) == LOW)
            answer = 2;
        else if (digitalRead(BTN_C) == LOW)
            answer = 3;
        else if (digitalRead(BTN_D) == LOW)
            answer = 4;

        if (answer != 0)
        {
            lcd.clear();
            lcd.setCursor(0, 0);

            if (answer == qBank[currentQIndex].correct)
            {
                lcd.print("CORRECT! +");
                lcd.print(currentWager);
                lcd.print(" G");
                playerGold[currentPlayer] += currentWager;
                strip.fill(strip.Color(0, 255, 0));
                harborGate.write(90);
                tone(BUZZER_PIN, 1200, 500);
            }
            else
            {
                if (answer == 5)
                {
                    lcd.print("TIME'S UP! -");
                    lcd.print(currentWager);
                    lcd.print(" G");
                }
                else
                {
                    lcd.print("WRONG! -");
                    lcd.print(currentWager);
                    lcd.print(" G");
                }
                playerGold[currentPlayer] -= currentWager;
                strip.fill(strip.Color(255, 0, 0));
                harborGate.write(0);
                tone(BUZZER_PIN, 200, 800);
            }
            strip.show();
            delay(3000);

            // 4. Check for Winner
            if (playerGold[currentPlayer] >= WIN_TARGET)
            {
                state = GAME_OVER;

                // <--- NEW: TRIGGER THE MAGLEV MODULE ON WIN!
                digitalWrite(MAGLEV_PIN, HIGH);

                lcd.clear();
                lcd.setCursor(0, 1);
                lcd.print("  PLAYER ");
                lcd.print(currentPlayer + 1);
                lcd.print(" WINS!");
                lcd.setCursor(0, 2);
                lcd.print("   FINAL: ");
                lcd.print(playerGold[currentPlayer]);
                lcd.print(" G");
                strip.fill(strip.Color(255, 0, 255));
                strip.show();
                harborGate.write(90);
            }
            else if (playerGold[currentPlayer] <= 0)
            {
                state = GAME_OVER;
                lcd.clear();
                lcd.setCursor(0, 1);
                lcd.print(" PLAYER ");
                lcd.print(currentPlayer + 1);
                lcd.print(" BANKRUPT!");
                strip.fill(strip.Color(255, 0, 0));
                strip.show();
            }
            else
            {
                currentPlayer = (currentPlayer + 1) % NUM_PLAYERS;
                resetTurn();
            }
        }
    }
    break;

    case GAME_OVER:
        break;
    }
}