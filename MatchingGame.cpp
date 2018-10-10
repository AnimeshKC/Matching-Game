#include <Arduino.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>

// TFT display and SD card will share the hardware SPI interface.
// For the Adafruit shield, these are the default.
// The display uses hardware SPI, plus #9 & #10
// Mega2560 Wiring: connect TFT_CLK to 52, TFT_MISO to 50, and TFT_MOSI to 51.
#define TFT_DC 9
#define TFT_CS 10
#define SD_CS 6

// joystick pins
#define JOY_VERT_ANALOG A1
#define JOY_HORIZ_ANALOG A0
#define JOY_SEL 2

// constants for the joystick
#define JOY_DEADZONE 256
#define JOY_CENTRE 512

// width/height of the display when rotated horizontally
#define TFT_WIDTH 320
#define TFT_HEIGHT 240

// layout of display for this assignment,
#define RATING_SIZE 48
#define DISP_WIDTH (TFT_WIDTH - RATING_SIZE)
#define DISP_HEIGHT TFT_HEIGHT

// Use hardware SPI (on Mega2560, #52, #51, and #50) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

int mode; // mode to determine menu/game/score modes

struct gamecard {
	int xpos;
	int ypos;
	int matchVal;
	bool flip = 0; // 0 for face down, 1 for face up
};

struct gamecard cardVal[15];

int cardSel = 7;
int score; // score should be tracked during gameplay, and updated on the right side of the screen
int combo; // combo for consecuitive matches
int sel1 = -1; // for determining matches
int sel2 = -1; // for determining matches
int oldscore; // for determining when to update sidebar's score

void setup() {
	init();

	pinMode(JOY_SEL, INPUT_PULLUP);

	Serial.begin(9600);

	tft.begin();

	tft.setRotation(-1);
	tft.setTextWrap(false);

	randomSeed(analogRead(4));

	// inital struct
	for (int i = 0; i < 15; i++) {
		if (i != 7) {
			cardVal[i].xpos = i%5;
			cardVal[i].ypos = i/5;
		}
	}
}

// beginning of starting menu stuff
int menuSel; // either 0 or 1
void menuMove() { // moving joystick in menuMode
	int moved = 0;
	while (digitalRead(JOY_SEL) == LOW) {} // in case button is still being pressed
	while (moved == 0) {
		if (analogRead(JOY_HORIZ_ANALOG) < JOY_CENTRE - JOY_DEADZONE) { // move right
			menuSel++;
			moved = 1;
		}
		else if (analogRead(JOY_HORIZ_ANALOG) > JOY_CENTRE + JOY_DEADZONE) { // move left
			menuSel--;
			moved = 1;
		}
		else if (digitalRead(JOY_SEL) == LOW) { // button pressed
			if (menuSel == 0) { // play is currently highlighted
				mode = 1; // play was selected
			}
			else if (menuSel == 1) { // score is currently highlighted
				mode = 2; // scores was selected
				score = -1;
			}
			moved = 1;
		}
	}
	menuSel = constrain(menuSel, 0, 1);
}

void menuMode() { // starting screen
	int oldmenuSel;
	menuSel = 0;

	// game data is cleard if there is any
	cardSel = 7;
	score = 0;
	oldscore = 0;
	combo = 0;
	sel1 = -1;
	sel2 = -1;
	for (int i = 0; i < 15; i++) {
		cardVal[i].flip = 0;
	}

	while (digitalRead(JOY_SEL) == LOW) {} // in case button is still being pressed

	// black color for background
	tft.fillScreen(ILI9341_BLACK);
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);

	// title printing
	tft.setTextSize(4);
	tft.setCursor(6, 80);
	tft.print("MATCHING GAME");

	tft.setTextSize(3);
	tft.setCursor(48, 200);
	tft.print("Play");
	tft.setCursor(170, 200);
	tft.print("Scores");

	tft.drawRect(48-2,200-2,73,25,ILI9341_RED); // drawing inital red rectangle around play

	while (mode == 0) {
		oldmenuSel = menuSel;
		menuMove();
		if (menuSel == 0 && oldmenuSel != menuSel) { // menuSel = 0 so play is selected
			tft.drawRect(170-2,200-2,109,25,ILI9341_BLACK); // removing red rectangle around scores
			tft.drawRect(48-2,200-2,73,25,ILI9341_RED); // drawing red rectangle around play
		}
		else if (menuSel == 1 && oldmenuSel != menuSel) { // menuSel = 1 so scores is selected
			tft.drawRect(48-2,200-2,73,25,ILI9341_BLACK); // removing red rectangle around play
			tft.drawRect(170-2,200-2,109,25,ILI9341_RED); // drawing red rectangle around scores
		}
		while (abs(analogRead(JOY_HORIZ_ANALOG) - JOY_CENTRE) > JOY_DEADZONE) {}	// prevents more than one move per joystick movement
	}
}
// end of starting menu stuff


// beginning of game stuff

void cardDraw() {
	// printing all the cards
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLUE);
	for (int i = 0; i < 5; i++) {
		for (int ii = 0; ii < 3; ii++) {
			tft.drawRect(DISP_WIDTH/2-113+50*i,DISP_HEIGHT/2-68+50*ii,27,37,ILI9341_WHITE);
			tft.fillRect(DISP_WIDTH/2-112+50*i,DISP_HEIGHT/2-67+50*ii,25,35,ILI9341_BLUE);
		}
	}

	// printing menu button in the middle of the playing field
	tft.fillRect(DISP_WIDTH/2-13,DISP_HEIGHT/2-18,27,37,ILI9341_WHITE);
	tft.fillRect(DISP_WIDTH/2-12,DISP_HEIGHT/2-17,25,35,ILI9341_RED);
	tft.setCursor(DISP_WIDTH/2-11,DISP_HEIGHT/2-3);
	tft.setTextColor(ILI9341_WHITE, ILI9341_RED);
	tft.print("Menu");
}

int hCardBox; // horizontal position of card selected, value of 0-4
int vCardBox; // vertical position of card selected, value of 0-2

void cardBox() {
	hCardBox = cardSel%5;
	vCardBox = cardSel/5;
	for (int i = 0; i < 5; i++) {
		for (int ii = 0; ii < 3; ii++) {
			tft.drawRect(DISP_WIDTH/2-115+50*i,DISP_HEIGHT/2-70+50*ii,31,41,0x0320);
		}
	}
	tft.drawRect(DISP_WIDTH/2-115+50*hCardBox,DISP_HEIGHT/2-70+50*vCardBox,31,41,ILI9341_RED);
}

// printing game field
void gamePrint() {
	// olive color for background
	tft.fillScreen(0x0320);

	// Black out the right side of the screen for keeping track of the scores
	tft.fillRect(DISP_WIDTH, 0, RATING_SIZE, DISP_HEIGHT, ILI9341_BLACK);

	// printing score top of the right side of the screen
	tft.setTextSize(1);
	tft.setCursor(DISP_WIDTH+1,1);
	tft.print("Score");
	tft.setCursor(DISP_WIDTH+1,9);
	tft.print(score);
	tft.setCursor(DISP_WIDTH+1,17);
	tft.print("Combo");
	tft.setCursor(DISP_WIDTH+1,25);
	tft.print(combo);

	// drawing the cards and the box highlighting the selected
	cardDraw();
	cardBox();
}
int resume; // for going back to game from scoreboard
int gameMenuSel;
int select = 1;

void gameMenuMove() {
	int moved = 0;

	while (moved == 0) {
		if (analogRead(JOY_HORIZ_ANALOG) < JOY_CENTRE - JOY_DEADZONE) { // move right
			gameMenuSel++;
			moved = 1;
		}
		else if (analogRead(JOY_HORIZ_ANALOG) > JOY_CENTRE + JOY_DEADZONE) { // move left
			gameMenuSel--;
			moved = 1;
		}
		gameMenuSel = constrain(gameMenuSel, 0, 2);
		if (digitalRead(JOY_SEL) == LOW) { // button is pressed
			if (gameMenuSel == 0) { // continue button
				select = 1;
			}
			else if (gameMenuSel == 1) { // quit button
				mode = 0;
				select = 1;
			}
			moved = 1;
		}
	}
}

void cardFlipMenu(); // forward declaration

void gameMenuMode() {
	int oldgameMenuSel;

	// printing the pause menu screen
	tft.fillScreen(ILI9341_BLACK);
	tft.setTextSize(3);
	tft.setTextColor(ILI9341_WHITE,ILI9341_BLACK);
	tft.setCursor(107,40);
	tft.print("Paused");

	// printing current score
	tft.setCursor(107,70);
	tft.print("Score:");
	tft.setCursor(125,94);
	tft.print(score);

	// printing the buttons
	tft.setTextSize(2);
	tft.setCursor(73, 200);
	tft.print("Continue");
	tft.setCursor(187, 200);
	tft.print("Quit");

	// initial red rectangle around continue
	tft.drawRect(71,198,98,18,ILI9341_RED);

	// if button is still being pressed, loop until not pressed to prevent excessive output
	while (select == 1) {
		if (digitalRead(JOY_SEL) == HIGH) { // button no longer pressed
			select = 0; // move onto main part of gameMenuMode
		}
	}
	gameMenuSel = 0;
	while (select == 0) {
		oldgameMenuSel = gameMenuSel;
		gameMenuMove();
		if (oldgameMenuSel != gameMenuSel) {
			tft.drawRect(71,198,98,18,ILI9341_BLACK); // removing red rectangle around continue
			tft.drawRect(185,198,50,18,ILI9341_BLACK); // removing red rectangle around quit
			if (gameMenuSel == 0) { // gameMenuSel = 0 so continue is selected
				tft.drawRect(71,198,98,18,ILI9341_RED); // drawing red rectangle around continue
			}
			else if (gameMenuSel == 1) { // gameMenuSel = 1 so quit is selected
				tft.drawRect(185,198,50,18,ILI9341_RED); // drawing red rectangle around quit
			}
		}
		while (abs(analogRead(JOY_HORIZ_ANALOG) - JOY_CENTRE) > JOY_DEADZONE) {
			delay(50);
		}	// prevents more than one move per joystick movement
		if (select == 1 && mode != 0) {
			gamePrint();
			cardFlipMenu();
			gameMenuSel = 0;
		}
	}
}

void cardFlip();

void gameMove() {
	int moved = 0;
	while (moved == 0) {
		if (analogRead(JOY_HORIZ_ANALOG) < JOY_CENTRE - JOY_DEADZONE && cardSel != 4 && cardSel != 9) { // move right
			cardSel++;
			moved = 1;
		}
		else if (analogRead(JOY_HORIZ_ANALOG) > JOY_CENTRE + JOY_DEADZONE && cardSel != 5 && cardSel != 10) { // move left
			cardSel--;
			moved = 1;
		}
		else if (analogRead(JOY_VERT_ANALOG) < JOY_CENTRE - JOY_DEADZONE && cardSel > 4) { // move up
			cardSel -= 5;
			moved = 1;
		}
		else if (analogRead(JOY_VERT_ANALOG) > JOY_CENTRE + JOY_DEADZONE && cardSel < 10) { // move down
			cardSel +=5;
			moved = 1;
		}
		else if (digitalRead(JOY_SEL) == LOW) { // button pressed
			if (cardSel == 7) { // menu button is currently highlighted
				gameMenuMode(); // menu button was selected
			}
			else { // a card is currently highlighted
				cardSel = constrain(cardSel, 0, 14);
				if (cardVal[cardSel].flip == 1) {} // if card is already flipped, do nothing
				else if (sel1 == -1 && sel2 == -1 && cardVal[cardSel].flip == 0) {
					sel1 = cardSel;
					cardFlip();
				}
				else if (sel1 != -1 && sel2 == -1 && cardVal[cardSel].flip == 0){
					sel2 = cardSel;
					cardFlip();
				}
				while (digitalRead(JOY_SEL) == LOW) {}
			}
			moved = 1;
		}
		cardSel = constrain(cardSel, 0, 14);
	}
}

void symboldraw(int sel) {
	if (cardVal[sel].matchVal == 0) {
		tft.fillCircle(DISP_WIDTH/2-100+50*(sel%5),DISP_HEIGHT/2-51+50*(sel/5),10,ILI9341_RED);
	}
	else if (cardVal[sel].matchVal == 1) {
		tft.fillCircle(DISP_WIDTH/2-100+50*(sel%5),DISP_HEIGHT/2-51+50*(sel/5),10,ILI9341_ORANGE);
	}
	else if (cardVal[sel].matchVal == 2) {
		tft.fillCircle(DISP_WIDTH/2-100+50*(sel%5),DISP_HEIGHT/2-51+50*(sel/5),10,ILI9341_YELLOW);
	}
	else if (cardVal[sel].matchVal == 3) {
		tft.fillCircle(DISP_WIDTH/2-100+50*(sel%5),DISP_HEIGHT/2-51+50*(sel/5),10,ILI9341_GREEN);
	}
	else if (cardVal[sel].matchVal == 4) {
		tft.fillCircle(DISP_WIDTH/2-100+50*(sel%5),DISP_HEIGHT/2-51+50*(sel/5),10,ILI9341_BLUE);
	}
	else if (cardVal[sel].matchVal == 5) {
		tft.fillCircle(DISP_WIDTH/2-100+50*(sel%5),DISP_HEIGHT/2-51+50*(sel/5),10,0x4810);
	}
	else if (cardVal[sel].matchVal == 6) {
		tft.fillCircle(DISP_WIDTH/2-100+50*(sel%5),DISP_HEIGHT/2-51+50*(sel/5),10,ILI9341_PINK);
	}
}

void cardReveal() { // initially showing all cards for 4 seconds
	for (int i = 0; i < 15; i++) {
		if (i != 7){
			tft.drawRect(DISP_WIDTH/2-113+50*(i%5),DISP_HEIGHT/2-68+50*(i/5),27,37,ILI9341_BLUE);
			tft.fillRect(DISP_WIDTH/2-112+50*(i%5),DISP_HEIGHT/2-67+50*(i/5),25,35,ILI9341_WHITE);
			symboldraw(i);
		}
	}
	delay(4000);
	cardDraw();
}

void cardFlip() { // revealing a card
	if (sel1 != -1 && sel2 == -1) {
		tft.drawRect(DISP_WIDTH/2-113+50*(sel1%5),DISP_HEIGHT/2-68+50*(sel1/5),27,37,ILI9341_BLUE);
		tft.fillRect(DISP_WIDTH/2-112+50*(sel1%5),DISP_HEIGHT/2-67+50*(sel1/5),25,35,ILI9341_WHITE);
		cardVal[sel1].flip = 1;
		symboldraw(sel1);
	}
	else if (sel1 != -1 && sel2 != -1){
		tft.drawRect(DISP_WIDTH/2-113+50*(sel2%5),DISP_HEIGHT/2-68+50*(sel2/5),27,37,ILI9341_BLUE);
		tft.fillRect(DISP_WIDTH/2-112+50*(sel2%5),DISP_HEIGHT/2-67+50*(sel2/5),25,35,ILI9341_WHITE);
		cardVal[sel2].flip = 1;
		symboldraw(sel2);
	}
}

void cardFlipMenu() { // revealing cards that have been flipped after exiting the game menu
	for (int i = 0; i < 15; i++) {
		if (cardVal[i].flip == 1) {
			tft.drawRect(DISP_WIDTH/2-113+50*(i%5),DISP_HEIGHT/2-68+50*(i/5),27,37,ILI9341_BLUE);
			tft.fillRect(DISP_WIDTH/2-112+50*(i%5),DISP_HEIGHT/2-67+50*(i/5),25,35,ILI9341_WHITE);
			symboldraw(i);
		}
	}
}

void cardUnflip() { // unflipps incorrect matches
	// cards sel1 and sel2 have been selected and flipped up already
	delay(1000); // delay 1 second to show cards before unflipping
	tft.drawRect(DISP_WIDTH/2-113+50*(sel1%5),DISP_HEIGHT/2-68+50*(sel1/5),27,37,ILI9341_WHITE);
	tft.fillRect(DISP_WIDTH/2-112+50*(sel1%5),DISP_HEIGHT/2-67+50*(sel1/5),25,35,ILI9341_BLUE);

	tft.drawRect(DISP_WIDTH/2-113+50*(sel2%5),DISP_HEIGHT/2-68+50*(sel2/5),27,37,ILI9341_WHITE);
	tft.fillRect(DISP_WIDTH/2-112+50*(sel2%5),DISP_HEIGHT/2-67+50*(sel2/5),25,35,ILI9341_BLUE);
	cardVal[sel1].flip = 0;
	cardVal[sel2].flip = 0;
}

//create ordered pairs for the values
void constructArray() {
	int value;
	for (int i = 0; i < 15; ++i){
		if (i < 7) {
			cardVal[i].matchVal = value;
			if (i%2 == 1) {
				// every odd value is the end of the pairs
				// for example 1 would be for indices 0 and 1
				// and then move onto 2 for indices 2 and 3
				value +=1;
			}
		}
		else if (i > 7) {
			cardVal[i].matchVal = value;
			if ((i+1)%2 == 1) {
				// now every even value
				value +=1;
			}
		}
		else if (i == 7) {} // skip this value
	}
}

void randomizePositions(){
	// swap positions random times from 100 to 200 times
	int randomRange = random(100,200);
	for (int i = 0; i < randomRange; ++i) {
		//choose a position
		int randomPosition1 = 7;
		int randomPosition2 = 7;

		while (randomPosition1 == 7) { // as long as menu button is selected (7), take a new position
			randomPosition1 = random(15);
		}

		//loop until a second different position is chosen randomly that isnt the menu position (7)
		while (randomPosition2 == 7 || randomPosition2 == randomPosition1) {
			randomPosition2 = random(15);
		}
		//perform a swap of positions
		int tempPosition = cardVal[randomPosition2].matchVal;
		cardVal[randomPosition2].matchVal = cardVal[randomPosition1].matchVal;
		cardVal[randomPosition1].matchVal = tempPosition;
	}
}

void scoreSetMode(); // forward declaration

void gameMode() {
	int oldcardSel;
	int cardFlipCount; // for determining when all cards are correctly matched

	gamePrint();

	// card shuffling
	constructArray();
	randomizePositions();
	cardReveal(); // reveals all cards, flipping all back face down after 4 seconds

	while (mode == 1) { // actual gameplay happens here
		oldcardSel = cardSel;
		oldscore = score;
		while (digitalRead(JOY_SEL) == LOW) {} // in case button is still being pressed
		gameMove();
		if (cardSel != oldcardSel) {
			cardBox();
			while ((abs(analogRead(JOY_VERT_ANALOG) - JOY_CENTRE) > JOY_DEADZONE)
							|| abs(analogRead(JOY_HORIZ_ANALOG) - JOY_CENTRE) > JOY_DEADZONE) {} // so the box only moves once per joystick movement
			delay(100); // if cursor is released from max position, recoil could cause unwanted input
		}
		if (sel1 != -1 && sel2 != -1) { // 2 cards have been selected, check for matching
			if (cardVal[sel1].matchVal == cardVal[sel2].matchVal) { // match
				score = score+100+combo*50;
				combo++;
				// wont unflip correct matches
				// checking if all cards are flipped over
				cardFlipCount = 0;
				for (int i = 0; i < 15; i++) {
					if (cardVal[i].flip == 1) {
						cardFlipCount++;
					}
				}
				if (cardFlipCount == 14) {
					scoreSetMode();
					break;
				}
			} // base add score 100, combo adds 50, 7 matches, maximum score of 1750
			else if (cardVal[sel1].matchVal != cardVal[sel2].matchVal) { // not match
				combo = 0;
				score = score-50;
				score = constrain(score,0,2000);
				// unflips cards
				cardUnflip();
			} // combo resets, wrong match subtracts 50 score
			sel1 = -1;
			sel2 = -1;
		}

		if (oldscore != score && mode == 1) { // updating score
			tft.fillRect(DISP_WIDTH+1,9,25,7,ILI9341_BLACK);
			tft.setTextColor(ILI9341_WHITE,ILI9341_BLACK);
			tft.setTextSize(1);
			tft.setCursor(DISP_WIDTH+1,9);
			tft.print(score);
			tft.setCursor(DISP_WIDTH+1,25);
			tft.print(combo);
		}
	}
}
// end of game stuff


// beginning of score stuff
// for quicksort
int pivot(int a[], int start, int end) {
	//int pivot = a[pi];

	//set the last value element of the array as the pivot
	int pi = end;
	//Move the pivot index to the end of the array
	//if (pi != n-1){
		//int prevpivotvalue = a[pi];
		//a[pi] = a[n-1];
		//a[n-1]= prevpivotvalue;
	//}

	//define high and low indices
	//low starts as the first index
	//high starts as the index directly left of the pivot (end of the array)
	int lo = start;
	int hi= end-1;

	while (lo <= hi){
		//raise the index of lo by 1 when the current value is not greater than the pivot
		if (a[lo] <= a[end]){
			lo += 1;
		}
		//lower the index of hi by 1 if the current value is higher than the pivot
		else if (a[hi] > a[end]){
			hi -= 1;
		}
		//if the lo value is higher than the pivot and the hi value is lower than the pivot,
		//swap the lo and hi values
		else {
			int tempvaluelo = a[lo];
			a[lo] = a[hi];
			a[hi] = tempvaluelo;

		}
	}
	//once the loop finishes, the lo value will be higher than the pivot,
	//so swap the two values

	int tempvaluelo = a[lo];
	a[lo] = a[end];
	a[end] = tempvaluelo;
	pi = lo;

	return pi;
}

// Sort an array with n elements using Quick Sort
void qsort(int array[], int start, int end) {
	// if n <= 1 do nothing (just return)

	// base case, an array of size <= 1 is already sorted
	if ((end+1-start) <= 1)  {
		return;
	}

	// call pivot with this pivot index, store the returned
	// location of the pivot in a new variable, say new_pi

	int new_pi = pivot(array, start, end);

	//length of the part before index new pi is equal to new pi
	qsort(array,start,new_pi-1);

	// - once with the part after index new_pi
	qsort(array, new_pi+1,end);
}

void scoreMode(); // forward declaration
// array of ten for top 10 highscores
int highscore[10] = {0}; // 10 scores are stored, the 10th is overwritten with newest score, scores are then sorted
void scoreSetMode() {
	// black color for background
	tft.fillScreen(ILI9341_BLACK);
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.setTextSize(5);
	tft.setCursor(27,27);
	tft.print("Game Over");
	tft.setTextSize(3);
	tft.setCursor(116,100);
	tft.print("Score:");
	tft.setCursor(134,124);
	tft.print(score);
	tft.setTextSize(2);
	tft.setCursor(137,220);
	tft.print("Next");
	// printing red box
	tft.drawRect(135,218,50,18,ILI9341_RED);
	// theres only the next button so theres nothing moving

	highscore[0] = score;
	// sorting all the scores
	qsort(highscore,0,9);


	while (digitalRead(JOY_SEL) == LOW) {} // in case button is still being pressed
	while (digitalRead(JOY_SEL) == HIGH) {} // waiting for button press
	scoreMode(); // going to scoreMode after score is set
}

void scoreMode() {
	// black color for background
	tft.fillScreen(ILI9341_BLACK);
	// printing back button
	tft.setTextColor(ILI9341_WHITE, ILI9341_BLACK);
	tft.setTextSize(3);
	tft.setCursor(71,10);
	tft.print("Highscores");
	tft.setTextSize(2);
	tft.setCursor(137,220);
	tft.print("Back");
	// printing red box
	tft.drawRect(135,218,48,18,ILI9341_RED);
	// theres only the back button so theres nothing moving

	int box = 0; // so only one red rectangle is drawn incase of duplicate scores
	for (int i = 0; i < 9; i++) {
		tft.setCursor(71,48+i*18);
		tft.print(i+1);
		tft.setCursor(203,48+i*18);
		tft.print(highscore[9-i]);

		if (score == highscore[9-i] && box == 0) {
			tft.drawRect(69,46+i*18,182,18,ILI9341_RED);
			box = 1;
		}
	}
	while (digitalRead(JOY_SEL) == LOW) {} // in case button is still being pressed
	while (digitalRead(JOY_SEL) == HIGH) { // waiting for button press
		mode = 0;
	}
}

int main() {
	setup();

	while (true) { // infinite loop to keep game always running
		if (mode == 0) {
			menuMode();
		}
		else if (mode == 1) {
			gameMode();
		}
		else if (mode == 2) {
			scoreMode();
		}
	}

	Serial.end();
	return 0;
}
