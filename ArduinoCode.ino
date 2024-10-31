#include <hd44780.h> // Use the hd44780 library compatible with Renesas Uno
#include <Wire.h>
#include <hd44780ioClass/hd44780_I2Cexp.h> // I2C display class
hd44780_I2Cexp lcd;

#define PIN_BUTTON 2
#define PIN_AUTOPLAY 1
#define PIN_READWRITE 10
#define PIN_CONTRAST 7

#define SPRITE_RUN1 1
#define SPRITE_RUN2 2
#define SPRITE_JUMP 3
#define SPRITE_JUMP_LOWER 4
#define SPRITE_TERRAIN_EMPTY ' '
#define SPRITE_TERRAIN_SOLID 5
#define SPRITE_TERRAIN_SOLID_RIGHT 6
#define SPRITE_TERRAIN_SOLID_LEFT 7

#define HERO_HORIZONTAL_POSITION 1
#define TERRAIN_WIDTH 16
#define TERRAIN_EMPTY 0
#define TERRAIN_LOWER_BLOCK 1
#define TERRAIN_UPPER_BLOCK 2

#define HERO_POSITION_OFF 0
#define HERO_POSITION_RUN_LOWER_1 1
#define HERO_POSITION_RUN_LOWER_2 2
#define HERO_POSITION_JUMP 3 // Only one jump position now
#define HERO_POSITION_FALL 4

static char terrainUpper[TERRAIN_WIDTH + 1];
static char terrainLower[TERRAIN_WIDTH + 1];
static bool buttonPushed = false;

void initializeGraphics() {
    static byte graphics[] = {
        // Run position 1
        B00000, B00110, B01110, B01101, B11110, B11110, B01001, B00000,
        // Run position 2
        B00000, B00110, B01110, B01101, B11110, B11110, B00110, B00000,
        // Jump
        B00110, B01110, B01101, B11110, B01110, B11010, B00110, B00000,
        // Jump lower
        B00110, B01110, B01101, B11110, B11110, B00101, B00000, B00000,
        // Ground
        B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111,
        // Ground right
        B00011, B00011, B00011, B00011, B00011, B00011, B00011, B00011,
        // Ground left
        B11000, B11000, B11000, B11000, B11000, B11000, B11000, B11000
    };
    for (int i = 0; i < 7; ++i) {
        lcd.createChar(i + 1, &graphics[i * 8]);
    }
    for (int i = 0; i < TERRAIN_WIDTH; ++i) {
        terrainUpper[i] = SPRITE_TERRAIN_EMPTY;
        terrainLower[i] = SPRITE_TERRAIN_EMPTY;
    }
    terrainUpper[TERRAIN_WIDTH] = '\0';
    terrainLower[TERRAIN_WIDTH] = '\0';
}

void advanceTerrain(char* terrain, byte newTerrain) {
    for (int i = 0; i < TERRAIN_WIDTH; ++i) {
        char current = terrain[i];
        char next = (i == TERRAIN_WIDTH - 1) ? newTerrain : terrain[i + 1];

        switch (current) {
            case SPRITE_TERRAIN_EMPTY:
                terrain[i] = (next == SPRITE_TERRAIN_SOLID) ? SPRITE_TERRAIN_SOLID_RIGHT : SPRITE_TERRAIN_EMPTY;
                break;
            case SPRITE_TERRAIN_SOLID:
                terrain[i] = (next == SPRITE_TERRAIN_EMPTY) ? SPRITE_TERRAIN_SOLID_LEFT : SPRITE_TERRAIN_SOLID;
                break;
            case SPRITE_TERRAIN_SOLID_RIGHT:
                terrain[i] = SPRITE_TERRAIN_SOLID;
                break;
            case SPRITE_TERRAIN_SOLID_LEFT:
                terrain[i] = SPRITE_TERRAIN_EMPTY;
                break;
        }
    }
}

bool drawHero(byte position, char* terrainUpper, char* terrainLower, unsigned int score) {
    bool collide = false;
    char upperSave = terrainUpper[HERO_HORIZONTAL_POSITION];
    char lowerSave = terrainLower[HERO_HORIZONTAL_POSITION];
    byte upper, lower;

    switch (position) {
        case HERO_POSITION_OFF: upper = lower = SPRITE_TERRAIN_EMPTY; break;
        case HERO_POSITION_RUN_LOWER_1: upper = SPRITE_TERRAIN_EMPTY; lower = SPRITE_RUN1; break;
        case HERO_POSITION_RUN_LOWER_2: upper = SPRITE_TERRAIN_EMPTY; lower = SPRITE_RUN2; break;
        case HERO_POSITION_JUMP: upper = SPRITE_JUMP; lower = SPRITE_TERRAIN_EMPTY; break;
        case HERO_POSITION_FALL: upper = SPRITE_TERRAIN_EMPTY; lower = SPRITE_JUMP_LOWER; break;
    }
    
    if (upper != SPRITE_TERRAIN_EMPTY) {
        terrainUpper[HERO_HORIZONTAL_POSITION] = upper;
        collide = (upperSave == SPRITE_TERRAIN_EMPTY) ? false : true;
    }
    if (lower != SPRITE_TERRAIN_EMPTY) {
        terrainLower[HERO_HORIZONTAL_POSITION] = lower;
        collide |= (lowerSave == SPRITE_TERRAIN_EMPTY) ? false : true;
    }

    byte digits = (score > 9999) ? 5 : (score > 999) ? 4 : (score > 99) ? 3 : (score > 9) ? 2 : 1;
    terrainUpper[TERRAIN_WIDTH] = '\0';
    terrainLower[TERRAIN_WIDTH] = '\0';
    char temp = terrainUpper[16 - digits];
    terrainUpper[16 - digits] = '\0';
    
    lcd.setCursor(0, 0);
    lcd.print(terrainUpper);
    terrainUpper[16 - digits] = temp;
    lcd.setCursor(0, 1);
    lcd.print(terrainLower);
    lcd.setCursor(16 - digits, 0);
    lcd.print(score);
    terrainUpper[HERO_HORIZONTAL_POSITION] = upperSave;
    terrainLower[HERO_HORIZONTAL_POSITION] = lowerSave;
    return collide;
}

unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned long jumpStartTime = 0; // To track when jump starts
const unsigned long jumpDuration = 1500; // Duration of jump

void buttonPush() {
    if (millis() - lastDebounceTime > debounceDelay) {
        buttonPushed = true;
        lastDebounceTime = millis();
    }
}

void setup() {
    lcd.begin(16, 2);
    lcd.backlight();
    pinMode(PIN_READWRITE, OUTPUT);
    digitalWrite(PIN_READWRITE, LOW);
    pinMode(PIN_CONTRAST, OUTPUT);
    digitalWrite(PIN_CONTRAST, LOW);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    pinMode(PIN_AUTOPLAY, OUTPUT);
    digitalWrite(PIN_AUTOPLAY, HIGH);
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), buttonPush, FALLING);
    initializeGraphics();
    randomSeed(analogRead(A0));
    Serial.begin(9600); // Initialize Serial for debugging
}

void loop() {
    static byte heroPos = HERO_POSITION_RUN_LOWER_1;
    static byte newTerrainType = TERRAIN_EMPTY;
    static byte newTerrainDuration = 1;
    static bool playing = false;
    static unsigned int distance = 0;
    static unsigned long animationTimer = 0; // Timer for animation
    static const unsigned long animationDelay = 250; // Delay for animation frames

    if (!playing) {
        drawHero(HERO_POSITION_OFF, terrainUpper, terrainLower, distance >> 3);
        lcd.setCursor(0, 0);
        lcd.print("Press Start");
        delay(250);
        // Check if button is pushed to start the game
        if (buttonPushed) {
            initializeGraphics();
            heroPos = HERO_POSITION_RUN_LOWER_1; // Reset hero position
            playing = true;
            buttonPushed = false; // Reset button state
            distance = 0; // Reset distance
            animationTimer = millis(); // Initialize animation timer
        }
        return;
    }

    // Game logic when playing
    advanceTerrain(terrainLower, newTerrainType == TERRAIN_LOWER_BLOCK ? SPRITE_TERRAIN_SOLID : SPRITE_TERRAIN_EMPTY);
    advanceTerrain(terrainUpper, newTerrainType == TERRAIN_UPPER_BLOCK ? SPRITE_TERRAIN_SOLID : SPRITE_TERRAIN_EMPTY);
    if (--newTerrainDuration == 0) {
        newTerrainType = (newTerrainType == TERRAIN_EMPTY) ? ((random(3) == 0) ? TERRAIN_UPPER_BLOCK : TERRAIN_LOWER_BLOCK) : TERRAIN_EMPTY;
        newTerrainDuration = (newTerrainType == TERRAIN_EMPTY) ? 5 + random(10) : 2 + random(6);
    }

    // Check if jump button is pressed
    if (buttonPushed && (heroPos == HERO_POSITION_RUN_LOWER_1 || heroPos == HERO_POSITION_RUN_LOWER_2)) {
        // Start jump
        heroPos = HERO_POSITION_JUMP; // Set to jump position
        jumpStartTime = millis(); // Record when jump starts
        buttonPushed = false; // Reset button state
    }

    // Handle jump and fall states
    if (heroPos == HERO_POSITION_JUMP) {
        if (millis() - jumpStartTime >= jumpDuration) {
            heroPos = HERO_POSITION_FALL;
        }
    } else if (heroPos == HERO_POSITION_FALL) {
        if (terrainLower[HERO_HORIZONTAL_POSITION] == SPRITE_TERRAIN_EMPTY) {
            heroPos = HERO_POSITION_RUN_LOWER_1; // Reset to run position when falling
        }
    } else {
        // Update hero position for run animation when on the floor
        if (millis() - animationTimer >= animationDelay) {
            heroPos = (heroPos == HERO_POSITION_RUN_LOWER_1) ? HERO_POSITION_RUN_LOWER_2 : HERO_POSITION_RUN_LOWER_1;
            animationTimer = millis(); // Reset timer
        }
    }

    // Check for collision
    bool collide = drawHero(heroPos, terrainUpper, terrainLower, distance >> 3);
    if (collide) {
        playing = false; // Reset playing state if there's a collision
        return;
    }

    distance++;
    delay(150); // Control game speed
}
