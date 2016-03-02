/* Justin Rice
 * CPSC 305-Section 1, Spring 2016  
 * pong, with a nod to Ian's square4.c
 */

/* the width and height of the screen */
#define WIDTH 240
#define HEIGHT 160

/* these identifiers define different bit positions of the display control */
#define MODE4 0x0004
#define BG2 0x0400

/* this bit indicates whether to display the front or the back buffer
 * this allows us to refer to bit 4 of the display_control register */
#define SHOW_BACK 0x10;

/* the screen is simply a pointer into memory at a specific address this
 *  * pointer points to 16-bit colors of which there are 240x160 */
volatile unsigned short* screen = (volatile unsigned short*) 0x6000000;

/* the display control pointer points to the gba graphics register */
volatile unsigned long* display_control = (volatile unsigned long*) 0x4000000;

/* the address of the color palette used in graphics mode 4 */
volatile unsigned short* palette = (volatile unsigned short*) 0x5000000;

/* pointers to the front and back buffers - the front buffer is the start
 * of the screen array and the back buffer is a pointer to the second half */
volatile unsigned short* front_buffer = (volatile unsigned short*) 0x6000000;
volatile unsigned short* back_buffer = (volatile unsigned short*)  0x600A000;

/* the button register holds the bits which indicate whether each button has
 * been pressed - this has got to be volatile as well
 */
volatile unsigned short* buttons = (volatile unsigned short*) 0x04000130;

/* the bit positions indicate each button - the first bit is for A, second for
 * B, and so on, each constant below can be ANDED into the register to get the
 * status of any one button */
#define BUTTON_A (1 << 0)
#define BUTTON_B (1 << 1)
#define BUTTON_SELECT (1 << 2)
#define BUTTON_START (1 << 3)
#define BUTTON_RIGHT (1 << 4)
#define BUTTON_LEFT (1 << 5)
#define BUTTON_UP (1 << 6)
#define BUTTON_DOWN (1 << 7)
#define BUTTON_R (1 << 8)
#define BUTTON_L (1 << 9)

/* the scanline counter is a memory cell which is updated to indicate how
 * much of the screen has been drawn */
volatile unsigned short* scanline_counter = (volatile unsigned short*) 0x4000006;

/* wait for the screen to be fully drawn so we can do something during vblank */
void wait_vblank( ) {
    /* wait until all 160 lines have been updated */
    while (*scanline_counter < 160) { }
}

/* this function checks whether a particular button has been pressed */
unsigned char button_pressed(unsigned short button) {
    /* and the button register with the button constant we want */
    unsigned short pressed = *buttons & button;
    /* if this value is zero, then it's not pressed */
    if (pressed == 0) {
        return 1;
    } else {
        return 0;
    }
}

/* keep track of the next palette index */
int next_palette_index = 0;

/*
 * function which adds a color to the palette and returns the
 * index to it
 */
unsigned char add_color(unsigned char r, unsigned char g, unsigned char b) {
    unsigned short color = b << 10;
    color += g << 5;
    color += r;

    /* add the color to the palette */
    palette[next_palette_index] = color;

    /* increment the index */
    next_palette_index++;

    /* return index of color just added */
    return next_palette_index - 1;
}

/* Struct for the paddle
 * x postion, y position, its width, height, colision border and bottom.
 */
struct paddle {
    unsigned short x , y, width, height, colBorder, colBottom;
    unsigned char color;
};

/* Struct for the ball
 * x position, y position, x velocity, y velocity
 */
struct ball {
    unsigned short x, y, dx, dy;
    unsigned char color;

};

/* put a pixel on the screen in mode 4 */
void put_pixel(volatile unsigned short* buffer, int row, int col, unsigned char color) {
    /* find the offset which is the regular offset divided by two */
    unsigned short offset = (row * WIDTH + col) >> 1;

    /* read the existing pixel which is there */
    unsigned short pixel = buffer[offset];

    /* if it's an odd column */
    if (col & 1) {
        /* put it in the left half of the short */
        buffer[offset] = (color << 8) | (pixel & 0x00ff);
    } else {
        /* it's even, put it in the left half */
        buffer[offset] = (pixel & 0xff00) | color;
    }
}
/* Draw a paddle struct on the screen */
void draw_paddle(volatile unsigned short* buffer, struct paddle* pad){
    unsigned short row, col;
    for (row = pad->y; row < (pad->y + pad->height); row++){
        for (col = pad->x; col < (pad->x + pad->width); col++){
            put_pixel(buffer, row, col, pad->color);
        }
    }
}

/* Draw the ball on the screen. */
void draw_ball(volatile unsigned short* buffer, struct ball* theBall){
    unsigned short row, col;
    for (row = theBall->y; row < (theBall->y + 4); row++){
        for (col = theBall->x; col < (theBall->x + 4); col++){
            put_pixel(buffer, row, col, theBall->color);
        }
    }
}

/* Update the screen, clearing both paddles and the ball. */
void update_screen(volatile unsigned short* buffer, unsigned short color, struct paddle* pad, struct paddle* aiPad, struct ball* theBall){
    unsigned short row, col;
    for (row = pad->y - 6; row < (pad->y + pad->height + 6); row++){
        for (col = pad->x -3; col < (pad->x + pad->width + 3); col++){
            put_pixel(buffer, row, col, color);
        }
    }
    for (row = aiPad->y - 6; row <(aiPad->y + aiPad->height +6); row++){
        for (col = aiPad->x -3; col <(aiPad->x + aiPad->width +3); col++){
            put_pixel(buffer, row, col, color);
        }
    }
    for (row = theBall->y - 2; row <(theBall->y + 6); row ++){
        for (col = theBall->x -2; col<(theBall->x + 6); col ++){
            if (col >0){
                put_pixel(buffer, row, col, color);
            }
        }
    }
}

/* Function to erase the ball from both buffers when it scores off the side. */
void clear_ball(volatile unsigned short* buffer, unsigned short color, struct ball* theBall){
    unsigned short row, col;
    for (row = theBall->y -2; row <(theBall->y+6); row++){
        for (col = theBall->x-2; col<(theBall->x +6); col++){
            if (col >0){
                put_pixel(buffer, row, col, color);
            }
        }
    }
}

/* Move the ball based on its current velocity */
void moveBall(volatile unsigned short* front_buffer, volatile unsigned short* back_buffer, unsigned short color, struct ball* theBall){
    unsigned short newX;
    if(theBall->dx > 50){
        newX=(theBall->x) -1;
    }
    else{
        newX = (theBall->x) + 1;
    }

    /* if statement in case the ball has scored a goal*/
    if ((newX <= 1)||(newX >=238)){
        clear_ball(front_buffer, color, theBall);
        clear_ball(back_buffer, color, theBall);
        theBall->dy = 50;
        theBall->x = 100;
        theBall->y = 100;
    }
    else{
        theBall->x = newX;
    }
    if(theBall->dy > 50){
        theBall->y -= 1;
    }
    if(theBall->dy < 50){
        theBall->y += 1;
    }
}

/* this function takes a video buffer and returns to you the other one */
volatile unsigned short* flip_buffers(volatile unsigned short* buffer) {
    /* if the back buffer is up, return that */
    if(buffer == front_buffer) {
        /* clear back buffer bit and return back buffer pointer */
        *display_control &= ~SHOW_BACK;
        return back_buffer;
    } else {
        /* set back buffer bit and return front buffer */
        *display_control |= SHOW_BACK;
        return front_buffer;
    }
}

/* handle the buttons which are pressed down */
void handle_buttons(struct paddle* pad) {
    /* move the square with the arrow keys */
    if (button_pressed(BUTTON_DOWN)) {
        if(pad->y < 118){
            pad->y += 2;
            pad->colBottom +=2;
        }
    }
    if (button_pressed(BUTTON_UP)) {
        if(pad->y >= 33){
            pad->y -= 2;
            pad->colBottom -=2;
        }
    }
}

/* The AI only moves when the ball is coming at him. Tries to match the ball's x */
void aiMove(struct paddle* pad, struct ball* theBall){
    if (theBall->dx == 49){
        if(((theBall->y -3) > (pad->y + 2)) && (pad->y < 118)){
            pad->y += 2;
            pad->colBottom += 2;
        }
        else if((theBall->y < (pad->y -2)) && (pad->y >= 33)){
            pad->y -= 2;
            pad->colBottom -= 2;
        }
    }
}

/* clear the screen to black */
void clear_screen(volatile unsigned short* buffer, unsigned short color) {
    unsigned short row, col;
    /* set each pixel black */
    for (row = 0; row < HEIGHT; row++) {
        for (col = 0; col < WIDTH; col++) {
            put_pixel(buffer, row, col, color);
        }
    }
}

/* drawing the game borders at the top and bottom */
void drawBorders(volatile unsigned short* frontBuffer, volatile unsigned short* backBuffer, unsigned short color){
    unsigned short column;
    for (column = 0; column < WIDTH; column ++){
        put_pixel(frontBuffer, 23, column, color);
        put_pixel(backBuffer, 23, column, color);
    }
    for (column = 0; column < WIDTH; column ++){
        put_pixel(frontBuffer, 140, column, color);
        put_pixel(backBuffer, 140, column, color);
    }
}

/* Check for collision with a paddle. */
void hitPaddle(struct paddle* pad, struct ball* theBall){
    unsigned short row, col;
    if (theBall->x == pad->x){
        if(((theBall->y+2) >= pad->y) && (theBall->y <= pad->colBottom)){ 
            if (theBall->y >= (pad->y + 11)){
                    theBall->dy = 49;
            }
            else if (theBall->y >= (pad->y +5)){
                theBall->dy = 50;
            }
            else{
                theBall->dy = 51;
            }
            if (theBall->dx == 51){
                theBall->dx = 49;
            }
            else{
                theBall->dx = 51;
            }
        }
    }
}

/* Check for collison with the walls. If so, changes ball's dy velocity. */
void hitWall(struct ball* theBall){
    unsigned short row;
    if (theBall->y > 130){
        theBall->dy = 51;
    }
    if (theBall->y < 30){
        theBall->dy = 49;
    }
}



/* the main function */
int main( ) {
    /* we set the mode to mode 4 with bg2 on */
    *display_control = MODE4 | BG2;

    struct paddle playerPad = {15, 80, 5, 15, 20, 95,  add_color(20,20,20)};
    struct paddle aiPad = {225, 80, 5, 15, 225, 95, add_color(20,20,20)};
    struct ball theBall = {120, 100, 49, 50, add_color(20,20,20)};
    
    /* add black to the palette */
    unsigned char black = add_color(0, 0, 0);

    /* the buffer we start with */
    volatile unsigned short* buffer = front_buffer;

    /* clear whole screen first */
    clear_screen(front_buffer, black);
    clear_screen(back_buffer, black);
    drawBorders(front_buffer, back_buffer, add_color(20, 20, 20));
    /* loop forever */
    while (1) {
        /* clear the screen - only the areas around the square! */
//        update_screen(buffer, black, &s);
        update_screen(buffer, black, &playerPad, &aiPad, &theBall);
        /* draw the square */
        /*draw_square(buffer, &s);*/

        draw_paddle(buffer, &playerPad);
        draw_paddle(buffer, &aiPad);
        draw_ball(buffer, &theBall);

        /* handle button input */
        handle_buttons(&playerPad);
        aiMove(&aiPad, &theBall);
        moveBall(back_buffer, front_buffer, black, &theBall);
        hitPaddle(&playerPad, &theBall);
        hitPaddle(&aiPad, &theBall);
        hitWall(&theBall);
        /* wiat for vblank before switching buffers */
        wait_vblank();

        /* swap the buffers */
        buffer = flip_buffers(buffer);
    }
}

/* the game boy advance uses "interrupts" to handle certain situations
 * for now we will ignore these */
void interrupt_ignore( ) {
    /* do nothing */
}

/* this table specifies which interrupts we handle which way
 * for now, we ignore all of them */
typedef void (*intrp)( );
const intrp IntrTable[13] = {
    interrupt_ignore,   /* V Blank interrupt */
    interrupt_ignore,   /* H Blank interrupt */
    interrupt_ignore,   /* V Counter interrupt */
    interrupt_ignore,   /* Timer 0 interrupt */
    interrupt_ignore,   /* Timer 1 interrupt */
    interrupt_ignore,   /* Timer 2 interrupt */
    interrupt_ignore,   /* Timer 3 interrupt */
    interrupt_ignore,   /* Serial communication interrupt */
    interrupt_ignore,   /* DMA 0 interrupt */
    interrupt_ignore,   /* DMA 1 interrupt */
    interrupt_ignore,   /* DMA 2 interrupt */
    interrupt_ignore,   /* DMA 3 interrupt */
    interrupt_ignore,   /* Key interrupt */
};

