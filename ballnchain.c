#include <c64/joystick.h>
#include <c64/vic.h>
#include <c64/sprites.h>
#include <c64/memmap.h>
#include <c64/rasterirq.h>
#include <c64/sid.h>
#include <c64/cia.h>
#include <math.h>
#include <fixmath.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <oscar.h>
#include <audio/sidfx.h>

// setup main memory region for code and data
#pragma region( main, 0x0a00, 0x9e00, , , {code, data, bss, heap} )
#pragma region( stack, 0x9e00, 0xa000, , , {stack})

// Extended bss section in unused system screen buffer
#pragma section( xbss, 0, , , bss)
#pragma region( xbss, 0x0400, 0x0800, , , {xbss})

// Music data below the basic rom
#pragma section( music, 0)
#pragma region( music, 0xa000, 0xc000, , , {music} )

// Sprite data
#pragma section( spriteset, 0)
#pragma region( spriteset, 0xc000, 0xd000, , , {spriteset} )

// Math tables
#pragma section( tables, 0, , , bss)
#pragma region( tables, 0xf000, 0xff00, , , {tables})

// Load music into music section
#pragma data(music)

__export char music[] = {
	#embed 0x2000 0x88 "GameScene.sid"
};

// Load sprites into sprite section
#pragma data(spriteset)

char spriteset[4095] = {
	#embed 4095 0 "ballnchain - Sprites.bin"
};

#pragma data(data)

// Compressed charset, tiles and attributes for center range
char charset_center[] = {
	#embed lzo "ballnchain_center - Chars.bin"
};

char tileset_center[] = {
	#embed "ballnchain_center - Tiles.bin"	
};

char charattribs_center[] = {
	#embed "ballnchain_center - CharAttribs_L1.bin"	
};

// Compressed charset, attributes and tioles for bottom range
char charset_bottom[] = {
	#embed lzo "ballnchain_bottom - Chars.bin"
};

char tileset_bottom[] = {
	#embed "ballnchain_bottom - Tiles.bin"	
};


// Shared foreground charset
char charset_front[] = {
	#embed "ballnchain_front - Chars.bin"
};

// Numeric digits
char charset_digits[] = {
	#embed "ballnchain_digits - Chars.bin"
};

// Compressed multicolor hires screen
char titlescreen[] = {
	#embed 8000    0 lzo "titlesketch.bin"
	#embed 1000 8000 lzo "titlesketch.bin"
	#embed 1000 9000 lzo "titlesketch.bin"
};

// Double buffered screend
byte * const Screen0 = (byte *)0xc800;
byte * const Screen1 = (byte *)0xcc00;

// Fonts 
byte * const Font = (byte *)0xe000;
byte * const FontBottom = (byte *)0xe800;

// VIC Color memory
byte * const Color = (byte *)0xd800;

// Sprite data under the IO range
byte * const Sprites = (byte *)0xd000;

// Dynamic sprite data
byte * const DynSprites = (byte *)0xc000;

// Math tables, created at runtime
#pragma bss(tables)

byte	asrtab4[256];	// Signed shift by four bits to the right
byte	lsrtab4[256];	// Unsigned shift by four bits to the right
byte	asltab4[256];	// Shift by four bits to the left
byte	sqrtabl[256];	// Square table low byte
byte	sqrtabh[256];	// Square table high byte

#pragma bss(bss)

// Align all tables on 6502 pages
#pragma align(asrtab4, 256)
#pragma align(lsrtab4, 256)
#pragma align(asltab4, 256)
#pragma align(sqrtabl, 256)
#pragma align(sqrtabh, 256)

// Variables that depend on video standard
bool		ntsc;
char		music56, physics56;	// 5/6 reduction for music and physics in NTSC
char		maxvx, minvx;	// max and min velocity per frame

// Current frame index and pointer to current screen
bool		cframe;
byte	*	cscreen;

// Score status
char		score[9];
char		nstars;
unsigned	scorecnt;

#pragma bss(xbss)

// Raster based interrupt descriptors
RIRQCode		irq_title[7], irq_title_hide, irq_title_show, irq_title_music;

// Variables for title sprites
char irq_title_i;
char irq_title_c;
char irq_title_y[7];
char irq_title_s[7];
char irq_title_x[4][7];
char irq_title_msbx[7];

// Address of title sprite columns for title text
byte * const DynSpriteChars[12] = {
	DynSprites +   0, DynSprites +   1, DynSprites +   2,
	DynSprites + 128, DynSprites + 129, DynSprites + 130,
	DynSprites + 256, DynSprites + 257, DynSprites + 258,
	DynSprites + 384, DynSprites + 385, DynSprites + 386
};

// Zero page variables for current scrolled in column char and color
__zeropage char	scr_column[25], col_column[25];

// Screen row buffer
char		 scr_row[40];

// Zero page variable for scrolling screen
__zeropage char	ctop, cbottom ,csize, bimg, bcnt, cimg, ccnt, cdist, cimgy;

// Title screen animation variables
const char * 	title_tp;
char			title_sy, title_ky, title_ty, title_by[8];
char			title_delay;
int				title_bx, title_cy;

// Title screen sprite animation data
sbyte	title_vx[7];
int		title_px[7];

RIRQCode20		irq_top20, irq_bottom20;
RIRQCode		irq_center, irq_music;

RIRQCode	* const	irq_top = &irq_top20.c;
RIRQCode	* const	irq_bottom = &irq_bottom20.c;


#pragma bss(bss)

// Speed animation for bats
static sbyte batyspeed[32] = {
	-1, -1, -1, -2, -2, -3, -2, -2, -1, -1, -1, 0, 0, 0, 0, 0,
	 1,  1,  1,  2,  2,  3,  2,  2,  1,  1,  1, 0, 0, 0, 0, 0
};

// Color animation for flashing mines
static char mineflash[16] = {
	VCOL_RED, VCOL_RED, VCOL_RED, VCOL_RED,
	VCOL_ORANGE, VCOL_YELLOW, VCOL_WHITE, VCOL_ORANGE,
	VCOL_RED, VCOL_RED, VCOL_RED, VCOL_RED,
	VCOL_RED, VCOL_RED, VCOL_RED, VCOL_RED	
};

// Current highscore status
struct Highscore
{
	char	name[3];
	char	score[6];

}	highscores[5] = {
	{"ABC", "000500"},
	{"DEF", "000400"},
	{"GHI", "000300"},
	{"JKL", "000200"},
	{"MNO", "000100"},
};

char	highscoreName[] = "AAA";

// Scrolling title text
const char title_text[] = 
//   012345678901
	"DEEP DOWN IN"
	"THE DARK AND"
	"   GLOOMY   "
	"DUNGEONS OF "
	" THE SPIKE  "
	" EMPIRE, AN "
	"  INNOCENT  "
	" BOUNCY BALL"
	"  HAS BEEN  "
	"  HELD  IN  "
	"  CAPTIVITY "
	"AND CHAINED "
	"TO A .. A .."
	"    BALL,   "
	"A HEAVY IRON"
	"  BALL THAT "
	"     IS.    "
	"            "
	"SO ONE DAY, "
	"  AFTER A   "
	"  GENEROUS  "
	"  GULP OF   "
	"HELIUM, OUR "
	"HERO DECIDES"
	"TO FLEE FROM"
	" THIS JAIL  "
	" AND RETURN "
	"TO THE PLACE"
	" OF FUN AND "
	"   GAMES.   "
	"            "
	"   COLLECT  "
	"COINS ON THE"
	"WAY FOR MORE"
	"  POINTS.   "
	"            "
	"STARS GRANT "
	"  A HIGHER  "
	" MULTIPLIER "
	"  BUT ALSO  "
	"INCREASE THE"
	"  WEIGHT OF "
	"  THE IRON  "
	"    BALL.   "
	"            "
	"PRESS FIRE  "
	"FOR TIGHTER "
	"CONTROL OF  "
	"YOUR BALLS. "
	"            ";

// Animated credits text
const char credits_text[] = 
	" CODING &   "
	"    DESIGN: "
	" DR. MORTAL "
	"  WOMBAT    "

	"   MUSIC:   "
	"  MASTER(?) "
	"  COMPOSER  "
	"   CRISPS   "

	"TESTERS:    "
	"            "
	"TBD TBD TBD "
	" AND MORE   "

	" CRACKED BY "
	"            "
	"INSERT NAME "
	" -- HERE -- "

	"  VISIT US  "
	"  FOR MORE  "
	"    ON      "
	"  ITCH.IO   "
	;

// Credits animation, start position, color and direction
struct Credits
{
	int		px;
	int		py;
	sbyte	dx, dy;
	char	color;
}	credits[5] = {
	{
		344, 240, -4, -2, VCOL_YELLOW
	},
	{
		-120, 60, 4, 1, VCOL_PURPLE
	},
	{
		200, 8, -1, 2, VCOL_ORANGE
	},
	{
		140, 244, -1, -4, VCOL_LT_GREY
	},
	{
		344, 90, -4, 1, VCOL_CYAN
	},
};

// Initialize a sub tune in the music code
void music_init(char tune)
{
	__asm
	{
		lda		tune
		jsr		$a000
	}
}

// Play next track element in the music code
void music_play(void)
{
	// Decimate to 5/6 for ntsc
	if (ntsc && music56 == 5)
	{
		music56 = 0;
	}
	else
	{
		music56++;
		__asm
		{
			jsr		$a003
		}
	}
}

// Change music volume
void music_patch_volume(char vol)
{
	*(char *)0xa103 = vol;	
}

// Enable or disable the 3rd SID voice in the music code
void music_patch_voice3(bool enable)
{
	*(char *)0xa10c = enable ? 0x20 : 0x4c;
}

// Zero page random seed
__zeropage unsigned zseed;

// Roll the dice
void zrand(void)
{
    zseed ^= zseed << 7;
    zseed ^= zseed >> 9;
    zseed ^= zseed << 8;
}

// Next random number
static inline unsigned frand()
{
	zrand();
	return zseed;
}

// Table lookup for next darker color for each VIC color
char darker[16] = {
	VCOL_BLACK,
	VCOL_LT_GREY,
	VCOL_BROWN,
	VCOL_LT_BLUE,
	
	VCOL_BLUE,
	VCOL_BROWN,
	VCOL_BLACK,
	VCOL_ORANGE,

	VCOL_BROWN,
	VCOL_BLACK,
	VCOL_RED,
	VCOL_BLACK,

	VCOL_DARK_GREY,
	VCOL_GREEN,
	VCOL_BLUE,
	VCOL_MED_GREY
};

// Title screen sprite multiplexer interrupt for rolling text
__interrupt void titlescreen_irq(void)
{
	// Get y pos and sprite index for current row of text

	char y = irq_title_y[irq_title_i];
	char s = irq_title_s[irq_title_i];

	// Update sprite y position

	vic.spr_pos[0].y = y;		
	vic.spr_pos[1].y = y;		
	vic.spr_pos[2].y = y;		
	vic.spr_pos[3].y = y;

	// And for the outline

	vic.spr_pos[4].y = y - 1;
	vic.spr_pos[5].y = y - 1;	
	vic.spr_pos[6].y = y - 1;		
	vic.spr_pos[7].y = y - 1;

	// Update sprite index

	Screen1[0x3f8 + 0] = s + 0;
	Screen1[0x3f8 + 4] = s + 1;
	Screen1[0x3f8 + 1] = s + 2;
	Screen1[0x3f8 + 5] = s + 3;
	Screen1[0x3f8 + 2] = s + 4;
	Screen1[0x3f8 + 6] = s + 5;
	Screen1[0x3f8 + 3] = s + 6;
	Screen1[0x3f8 + 7] = s + 7;
}

// Title screen sprite multiplexer for floating texts
__interrupt void titlescreen_irqx(void)
{
	// Get ypos

	char y = irq_title_y[irq_title_i];

	// Update sprite y position

	vic.spr_pos[0].y = y;		
	vic.spr_pos[1].y = y;
	vic.spr_pos[2].y = y;		
	vic.spr_pos[3].y = y;

	// And for the outline

	vic.spr_pos[4].y = y - 1;
	vic.spr_pos[5].y = y - 1;	
	vic.spr_pos[6].y = y - 1;		
	vic.spr_pos[7].y = y - 1;

	// Update sprite x position LSBs

	vic.spr_pos[4].x = irq_title_x[0][irq_title_i];
	vic.spr_pos[0].x = irq_title_x[0][irq_title_i] + 1;
	vic.spr_pos[5].x = irq_title_x[1][irq_title_i];
	vic.spr_pos[1].x = irq_title_x[1][irq_title_i] + 1;
	vic.spr_pos[6].x = irq_title_x[2][irq_title_i];
	vic.spr_pos[2].x = irq_title_x[2][irq_title_i] + 1;		
	vic.spr_pos[7].x = irq_title_x[3][irq_title_i];
	vic.spr_pos[3].x = irq_title_x[3][irq_title_i] + 1;

	// Update sprite x position combined MSB

	vic.spr_msbx = irq_title_msbx[irq_title_i];

	// And the sprite index

	char s = irq_title_s[irq_title_i];

	Screen1[0x3f8 + 0] = s + 0;
	Screen1[0x3f8 + 4] = s + 1;
	Screen1[0x3f8 + 1] = s + 2;
	Screen1[0x3f8 + 5] = s + 3;
	Screen1[0x3f8 + 2] = s + 4;
	Screen1[0x3f8 + 6] = s + 5;
	Screen1[0x3f8 + 3] = s + 6;
	Screen1[0x3f8 + 7] = s + 7;
}

// Draw one char into a title screen sprite

void titlescreen_char(char x, char y, char c)
{
	// Lookup x target position
	char * dp = DynSpriteChars[x];

	// Adjust for vertical offset (2 lines per sprite)
	if (y & 1)
		dp += 36;

	// Select row of sprites using the upper bits
	dp += (y >> 1) * (8 * 64);

	// Source glyph data
	const char * sp = charset_digits + 8 * (c & 0x3f);

	char a, b, c;

	// Copy the glyph data into the sprite, line by line and
	// form an outline of the glyph in parallel by or-ing three
	// columns and three rows

	a = sp[0];
	dp[ 0] = a;
	a |= a >> 1;
	dp[64 + 0] = a;

	b = sp[1];
	dp[ 3] = b;
	b |= b >> 1;
	dp[64 +  3] = a | b;

	c = sp[2];
	dp[ 6] = c;
	c |= c >> 1;
	dp[64 +  6] = a | b | c;

	a = sp[3];
	dp[ 9] = a;
	a |= a >> 1;
	dp[64 +  9] = a | b | c;

	b = sp[4];
	dp[12] = b;
	b |= b >> 1;
	dp[64 + 12] = a | b | c;

	c = sp[5];
	dp[15] = c;
	c |= c >> 1;
	dp[64 + 15] = a | b | c;

	a = sp[6];
	dp[18] = a;
	a |= a >> 1;
	dp[64 + 18] = a | b | c;

	b = sp[7];
	dp[21] = b;
	b |= b >> 1;
	dp[64 + 21] = a | b | c;
	dp[64 + 24] = a | b;
}

//  Draw one line into a row of title screen sprites
void titlescreen_line(char y, const char * tp)
{
	for(char i=0; i<12; i++)
		titlescreen_char(i, y, tp[i]);
}

//  Draw spaces into a row of title screen sprites
void titlescreen_spaces(char y)
{
	for(char i=0; i<12; i++)
		titlescreen_char(i, y, ' ');
}

//  Draw zero terminated string into a row of title screen sprites
void titlescreen_string(char x, char y, const char * p)
{
	while (*p)
		titlescreen_char(x++, y, *p++);
}

// Initialize the title screen scroll animation
void titlescreen_scroll_init(void)
{
	// Prepare basic sprite attributes

	vic.spr_enable = 0xff;
	vic.spr_multi = 0x00;
	vic.spr_expand_x = 0xff;
	vic.spr_expand_y = 0x00;

	// Prepare the sprites for 4 foreground and 4 background 

	for(char i=0; i<4; i++)
	{
		vic_sprxy(i    , 88 + 48 * i, 110);
		vic_sprxy(i + 4, 87 + 48 * i, 110);
		vic.spr_color[i] = VCOL_WHITE;
		vic.spr_color[i + 4] = VCOL_BLACK;
		Screen1[0x3f8 + i] = 0;
		Screen1[0x3fc + i] = 0;
	}

	irq_title_c = VCOL_BLACK;

	// Interrupt for start of scroll range

	rirq_build(&irq_title_show, 2);
	rirq_write(&irq_title_show, 0, &vic.spr_msbx, 0x00);
	rirq_write(&irq_title_show, 1, &vic.spr_expand_x, 0xff);
	rirq_set(7, 116, &irq_title_show);

	// Interrupt for end of scroll range

	rirq_build(&irq_title_hide, 2);
	rirq_write(&irq_title_hide, 0, &vic.spr_msbx, 0xff);
	rirq_write(&irq_title_hide, 1, &vic.spr_expand_x, 0x00);
	rirq_set(8, 250, &irq_title_hide);

	// Up to seven rows of scrolling sprites

	for(char i=0; i<7; i++)
	{
		rirq_build(irq_title + i, 2);
		rirq_write(irq_title + i, 0, &irq_title_i, i);
		rirq_call(irq_title + i, 1, titlescreen_irq);
		rirq_set(i, 116 + 25 * i, irq_title + i);

		irq_title_y[i] = 120 + 25 * i;
		irq_title_s[i] = 8 * i;
	}

	// sort the raster IRQs
	rirq_sort();

	// Init title variables

	title_tp = title_text;
	title_sy = 0;
	title_ky = 0;
	title_delay = 0;
	title_ty = 0;
}

// Animate one frame of title scroll animation
bool titlescreen_scroll_step(void)
{
	// Slow down scrolling to 1/3 of frame rate
	title_delay++;
	if (title_delay == 3)
	{
		title_delay = 0;

		// Advance one pixel row
		title_sy++;

		// Check for row of sprites left the visible range
		char	ky = title_ky;
		if (title_sy == 25)
		{
			// Rotate rows
			title_ky++;
			if (title_ky == 6)
				title_ky = 0;	
			title_sy = 0;
		}

		// Prepare vertical position of the scrolling sprite rows
		char j = title_ky;
		for(char i=0; i<7; i++)
		{
			// Target position for the ith row
			unsigned	ty =  116 - title_sy + 25 * i;

			// Check if still on screen
			if (ty < 245)
				rirq_set(i, i ? ty : 80, irq_title + i);
			else
				rirq_clear(i);

			// Set position and index
			irq_title_y[i] = ty + 4;
			irq_title_s[i] = 8 * j;
			j++;
			if (j == 6)
				j = 0;
		}

		// Update interrupt list
		rirq_sort();

		// Check for new glyph data
		if (title_sy == 15)
		{
			// Upper line in sprite
			if (title_tp[0])
			{
				titlescreen_line(2 * ky, title_tp);
				title_tp += 12;
			}
			else
			{
				// End of text, fill with spaces
				titlescreen_spaces(2 * ky);
			}
		}
		else if (title_sy == 0)
		{
			// Lower line in sprite
			if (title_tp[0])
			{
				titlescreen_line(2 * ky + 1, title_tp);
				title_tp += 12;
			}
			else
			{
				// End of text, fill with spaces
				titlescreen_spaces(2 * ky + 1);
				title_ty++;
			}


		}
	}

	// Check for end of text and trailing empty lines
	return title_ty < 6;
}

// Cleanup after scrolling title screen
void titlescreen_scroll_clear(void)
{
	vic.spr_enable = 0x00;

	for(char i=0; i<10; i++)
		rirq_clear(i);
	rirq_sort();

	title_delay = 0;	
}

// Init title screen for highscore display

void titlescreen_highscore_init(void)
{
	// Prepare sprites
	vic.spr_multi = 0x00;
	vic.spr_expand_x = 0xff;
	vic.spr_expand_y = 0x00;

	// Prepare the sprites for 4 foreground and 4 background 

	for(char i=0; i<4; i++)
	{
		vic_sprxy(i    , 72 + 48 * i, 110);
		vic_sprxy(i + 4, 71 + 48 * i, 110);
		vic.spr_color[i] = VCOL_WHITE;
		vic.spr_color[i + 4] = VCOL_BLACK;

		Screen1[0x3f8 + i] = 0;
		Screen1[0x3fc + i] = 0;
	}

	irq_title_c = VCOL_BLACK;

	// Six lines of highscore data

	for(char i=0; i<6; i++)
	{
		// Prepare interrupt

		rirq_build(irq_title + i, 2);
		rirq_write(irq_title + i, 0, &irq_title_i, i);
		rirq_call(irq_title + i, 1, titlescreen_irqx);
		rirq_set(i, 105 + 25 * i, irq_title + i);

		// Position outside of screen

		irq_title_y[i] = 110 + 25 * i;
		irq_title_s[i] = 8 * i;
		irq_title_msbx[i] = 0xff;
		irq_title_x[0][i] = 254;
		irq_title_x[1][i] = 254;
		irq_title_x[2][i] = 254;
		irq_title_x[3][i] = 254;

		// Position per line and velocity

		title_px[i] = 4 * (- 24 - 48 * (i + 3));
		title_vx[i] = 64;
	}

	// sort the raster IRQs
	rirq_sort();

	// Titletext of the highscore

	titlescreen_string(2, 0, "HIGHSCORE");

	vic.spr_enable = 0xff;

	title_delay = 0;
	title_ty = 0;
}

// Animate one frame of title screen for highscore display

bool titlescreen_highscore_step(void)
{
	title_delay++;
	if (!title_delay)
		title_ty++;

	// Staggered init of highscore text
	if (title_ty == 0 && title_delay < 6)
	{
		char i = title_delay - 1;

		for(char j=0; j<3; j++)
		{
			titlescreen_char(j     , 2 * i + 2, highscores[i].name[j]);
			titlescreen_char(j  + 3, 2 * i + 2, '.');
		}
		for(char j=0; j<6; j++)
			titlescreen_char(j +  6, 2 * i + 2, highscores[i].score[j]);		
	}

	// Update x position of highscore lines

	for(char i=0; i<6; i++)
	{
		int			x = 2 * (title_px[i] >> 2);
		char		msb = 0;

		// Calculate effective x lsb and msb for
		// all sprites of this row
		for(char j=0; j<4; j++)
		{
			// Check for out of sight, take care for the
			// x expand issue in PAL
			if (x < -24 || x > 344)
			{
				irq_title_x[j][i] = 200;
				msb |= 0x11 << j;				
			}
			else if (!ntsc && x < 0)
			{
				irq_title_x[j][i] = x - 8;
				msb |= 0x11 << j;
			}
			else
			{
				// Sprite is fully visible
				irq_title_x[j][i] = x;
				if (x & 0x100)
					msb |= 0x11 << j;
			}

			// Next sprite
			x += 48;
		}
		irq_title_msbx[i] = msb;

		// Move sprite by current velocity
		title_px[i] += title_vx[i] >> 2;

		// Animate velocity
		if (title_ty == 0)
		{
			// Moving in, decelerating
			if (title_px[i] > 0)
			{
				if (title_vx[i] > 0)
					title_vx[i] -= 3;
				else
					title_vx[i] = 0;
			}
		}
		else if (title_ty == 2)
		{
			// Moving out accelerating
			if (i < (title_delay >> 3))
			{
				if (title_vx[i] < 64)
					title_vx[i]++;
			}
		}
	}

	return title_ty < 3;
}

// Clean up title screen highscore animation

void titlescreen_highscore_clear(void)
{
	vic.spr_enable = 0x00;

	for(char i=0; i<10; i++)
		rirq_clear(i);
	rirq_sort();	

	title_delay = 0;
}

// Init title screen for credits display

void titlescreen_credits_init(void)
{
	vic.spr_multi = 0x00;
	vic.spr_expand_x = 0xff;
	vic.spr_expand_y = 0x00;

	for(char i=0; i<4; i++)
	{
		vic_sprxy(i    , 72 + 48 * i, 110);
		vic_sprxy(i + 4, 71 + 48 * i, 110);
		vic.spr_color[i] = VCOL_WHITE;
		vic.spr_color[i + 4] = VCOL_BLACK;

		Screen1[0x3f8 + i] = 0;
		Screen1[0x3fc + i] = 0;
	}

	irq_title_c = VCOL_BLACK;

	// Only two rows of sprites

	for(char i=0; i<2; i++)
	{
		rirq_build(irq_title + i, 2);
		rirq_write(irq_title + i, 0, &irq_title_i, i);
		rirq_call(irq_title + i, 1, titlescreen_irqx);
		rirq_set(i, 105 + 25 * i, irq_title + i);

		irq_title_y[i] = 110 + 25 * i;
		irq_title_s[i] = 8 * i;
		irq_title_msbx[i] = 0xff;
		irq_title_x[0][i] = 200;
		irq_title_x[1][i] = 200;
		irq_title_x[2][i] = 200;
		irq_title_x[3][i] = 200;
	}

	rirq_build(&irq_title_hide, 1);
	rirq_write(&irq_title_hide, 0, &vic.color_border, VCOL_BLACK);
	rirq_set(2, 250, &irq_title_hide);

	// sort the raster IRQs
	rirq_sort();

	vic.spr_enable = 0xff;

	title_delay = 0;
	title_ty = 0;
	title_sy = 0;
	title_tp = credits_text;
}

// Animate one frame of title screen for credits display

bool titlescreen_credits_step(void)
{
	// Check current display phase

	switch (title_ty)
	{
	case 0:
	case 2:
	case 4:
	case 6:
	case 8:
		// Even phase, preparation

		// Prepare one line
		if (title_sy & 1)
		{
			for(char j=0; j<12; j++)
				titlescreen_char(j, title_sy >> 1, title_tp[j]);
			title_tp += 12;
		}

		title_sy ++;
		if (title_sy == 8)
		{
			// All lines done, now for the display

			title_ty++;
			title_sy = 0;			

			// Setup start position and color

			char	k = title_ty >> 1;
			title_bx = 2 * credits[k].px;
			title_cy = 2 * credits[k].py;

			vic.spr_color[0] = 
			vic.spr_color[1] = 
			vic.spr_color[2] = 
			vic.spr_color[3] = credits[k].color;
		}

		vic_waitTop();
		break;
	case 1:
	case 3:
	case 5:
	case 7:
	case 9:
	{
		// Odd phase, animation

		char	k = title_ty >> 1;
		sbyte	dx = credits[k].dx, dy = credits[k].dy;

		// Move sprite block

		title_bx += dx;
		title_cy += dy;

		// Check if block is at least partially visible

		if (dx < 0 && title_bx < -408 || dx > 0 && title_bx >= 688 ||
			dy < 0 && title_cy < -20 || dy > 0 && title_cy >= 490)
		{
			// If not just advance clock

			title_ty++;
		}
		else
		{
			// For both lines

			for(char i=0; i<2; i++)
			{
				int			x = title_bx >> 1;
				char		msb = 0;

				// Update x position

				for(char j=0; j<4; j++)
				{
					// Special cases for x expanded sprites to the left

					if (x < -24 || x > 344)
					{
						irq_title_x[j][i] = 200;
						msb |= 0x11 << j;				
					}
					else if (!ntsc && x < 0)
					{
						irq_title_x[j][i] = x - 8;
						msb |= 0x11 << j;
					}
					else if (x == 255)
					{
						irq_title_x[j][i] = 255;
						msb |= 0x01 << j;
					}
					else
					{
						irq_title_x[j][i] = x;
						if (x & 0x100)
							msb |= 0x11 << j;
					}
					x += 48;
				}
				irq_title_msbx[i] = msb;

				// Update y position

				unsigned	ty =  (title_cy >> 1) + 25 * i;
				if (ty > 23 && ty < 245)
					rirq_set(i, ty, irq_title + i);
				else
					rirq_clear(i);

				irq_title_y[i] = ty + 6;
			}
		}
	}	break;
	}

	rirq_sort();

	return title_ty < 10;
}

// Clean up after credits display

void titlescreen_credits_clear(void)
{
	vic.spr_enable = 0x00;

	for(char i=0; i<10; i++)
		rirq_clear(i);
	rirq_sort();	

	title_delay = 0;
}

// Vertical bat animation start position

static char batyoffset[8] = {30, 20, 0, 40, 10, 37, 90, 42};

// 7 bit sine table

static sbyte sintab64[64] = {
	0, 6, 12, 19, 24, 30, 36, 41, 45, 49, 53, 56, 59, 61, 63, 64, 
	64, 64, 63, 61, 59, 56, 53, 49, 45, 41, 36, 30, 24, 19, 12, 6, 
	0, -6, -12, -19, -24, -30, -36, -41, -45, -49, -53, -56, -59, -61, -63, -64, 
	-64, -64, -63, -61, -59, -56, -53, -49, -45, -41, -36, -30, -24, -19, -12, -6
};

// Init title screen for bat fly by

void titlescreen_bats_init(void)
{
	spr_init(Screen1);

	vic.spr_mcolor0 = VCOL_BLACK;
	vic.spr_mcolor1 = VCOL_WHITE;

	// Eight bats
	for(char i=0; i<8; i++)
	{
		spr_set(i, true, 400, 0, 112, VCOL_BROWN, true, false, false);		
		title_by[i] = 90 + batyoffset[i];
	}

	title_delay = 0;
	title_bx = 360;
}

// Animate title screen for bat fly by

bool titlescreen_bats_step(void)
{
	// One step to the left
	title_bx--;

	// For each bat
	for(char i=0; i<8; i++)
	{
		char k = title_delay + 4 * i;

		// Vertical bat movement

		title_by[i] += batyspeed[(k & 63) >> 1];

		// Animate bat
		spr_move(i, title_bx + 20 * i, title_by[i]);
		spr_image(i, 112 + ((k & 15) >> 2));
	}

	title_delay++;

	return title_bx > -150;
}

// Cleanup after bat animation

void titlescreen_bats_clear(void)
{
	vic.spr_enable = 0x00;
	title_delay = 0;
}

// One title screen bouncing ball, with position and velocity

struct TitleBall
{
	int	px, py, dx, dy;
}	titleBalls[8];

// Init title screen bouncing balls

void titlescreen_balls_init(void)
{
	spr_init(Screen1);

	vic.spr_mcolor0 = VCOL_BLACK;
	vic.spr_mcolor1 = VCOL_WHITE;

	// Setup 8 initial balls

	for(char i=0; i<8; i++)
	{
		spr_set(i, true, 400, 0, 64 + i, VCOL_ORANGE, true, false, false);

		// Four from the left, and four from the right
		if (i & 1)
		{
			titleBalls[i].px = 0;
			titleBalls[i].dx = (frand() & 31) + 8;
		}
		else
		{
			titleBalls[i].px = 344 * 16;
			titleBalls[i].dx = - 8 - (frand() & 31);
		}

		// Random vertical position
		titleBalls[i].py = (frand() & 1023) + 800;
		titleBalls[i].dy = 0;
	}

	title_delay = 0;
}

// Animate one frame of bouncing balls

bool titlescreen_balls_step(void)
{
	for(char i=0; i<8; i++)
	{
		int	dx = titleBalls[i].dx;
		int dy = titleBalls[i].dy;

		// Let gravity do its thing

		dy += 2;

		// Integrate position by velocity

		int x = titleBalls[i].px + dx;
		int	y = titleBalls[i].py + dy;

		// Check for bottom bounce
		if (dy > 0 && y > 234 * 16)
		{
			// Reflect on ground
			dy = 5 - dy;
			y += dy;
		}

		// Check for leaving the screen
		if (dx < 0 && x < 0)
		{
			x = 344 * 16;
			dx = - 8 - (frand() & 31);
			y = (frand() & 1023) + 800;
			dy = 0;

			if (++title_delay >= 32)
				spr_show(i, false);
		}
		else if (dx > 0 && x > 344 * 16)
		{
			x = 0;
			dx = (frand() & 31) + 8;
			y = (frand() & 1023) + 800;
			dy = 0;						

			if (++title_delay >= 32)
				spr_show(i, false);
		}

		// Update ballse
		titleBalls[i].dx = dx;
		titleBalls[i].dy = dy;

		titleBalls[i].px = x;
		titleBalls[i].py = y;

		// Animate balls
		spr_move(i, x >> 4, y >> 4);
	}

	return vic.spr_enable != 0;
}

// Clean up after bouncing balls

void titlescreen_balls_clear(void)
{
	vic.spr_enable = 0x00;
	title_delay = 0;
}

// Init title screen chase animation

void titlescreen_chase_init(void)
{
	spr_init(Screen1);

	vic.spr_mcolor0 = VCOL_BLACK;
	vic.spr_mcolor1 = VCOL_WHITE;

	// Two sprites for the two balls
	spr_set(0, true, 400, 0, 68, VCOL_ORANGE, true, false, false);		
	spr_set(1, true, 400, 0, 76, VCOL_MED_GREY, true, false, false);

	for(char i=0; i<3; i++)
	{
		// Three sprites for the chain
		spr_set(2 + i, true, 400, 0, 77, VCOL_YELLOW, true, false, false);

		// Three sprites for the ghosts
		spr_set(5 + i, true, 400, 0, 120, VCOL_LT_BLUE, true, false, false);
	}

	title_delay = 0;
	title_bx = 320;
	title_ty = 0;
}

// One frame of title screen chase animation

bool titlescreen_chase_step(void)
{
	title_delay++;

	int	pix, piy, bix, biy, gix, giy;
	int	ix1, ix2, ix3, iy1, iy2, iy3;

	if (title_ty == 0)
	{
		// Right to left, ghosts chasing balls

		title_bx--;

		// Movement of balls
		pix = title_bx;
		piy = 90 + (sintab64[(title_delay * 3) & 63] >> 4);
		bix = title_bx + 50 + (sintab64[title_delay & 63] >> 2);
		biy = 90 + 20 + (sintab64[(title_delay * 4) & 63] >> 5);

		// Movement of ghosts
		gix = title_bx + 80;
		giy = 90 - 10;

		// Position of chain links at 1/4, 1/2 and 3/4 positions between
		// the balls
		ix2 = (pix + bix) >> 1; iy2 = (piy + biy) >> 1;
		ix1 = (pix + ix2) >> 1; iy1 = (piy + iy2) >> 1;
		ix3 = (ix2 + bix) >> 1; iy3 = (iy2 + biy) >> 1;

		if (title_bx < -160)
		{
			// Change direction
			title_ty++;
			spr_image(0, 64);
		}
	}
	else if (title_ty == 1)
	{
		// Left to right, balls chasing ghosts

		title_bx++;

		// Movement of balls
		pix = title_bx;
		piy = 90 + (sintab64[(title_delay * 3) & 63] >> 4);
		bix = pix + (sintab64[title_delay & 63] >> 1);
		biy = piy - (sintab64[(title_delay + 16) & 63] >> 1);

		// Movement of ghosts
		gix = title_bx + 80;
		giy = 90 - 10;

		// Position of chain links at 1/4, 1/2 and 3/4 positions between
		// the balls
		ix2 = (pix + bix) >> 1; iy2 = (piy + biy) >> 1;
		ix1 = (pix + ix2) >> 1; iy1 = (piy + iy2) >> 1;
		ix3 = (ix2 + bix) >> 1; iy3 = (iy2 + biy) >> 1;

		if (title_bx > 330)
		{
			title_ty++;
		}
	}

	// Update sprite positions

	spr_move(0, pix + (24 - 8), piy + (50 - 8));
	spr_move(1, bix + (24 - 12), biy + (50 - 12));
	spr_move(2, ix1 + (24 - 6), iy1 + (50 - 5));
	spr_move(3, ix2 + (24 - 6), iy2 + (50 - 5));
	spr_move(4, ix3 + (24 - 6), iy3 + (50 - 5));

	// Ghost sprite position
	spr_move(5, gix + (24 - 6) +  0, giy + (50 - 5) + 10 + (sintab64[title_delay & 63] >> 4));
	spr_move(6, gix + (24 - 6) + 30, giy + (50 - 5) + 30 + (sintab64[(title_delay + 7) & 63] >> 4));
	spr_move(7, gix + (24 - 6) + 55, giy + (50 - 5) + 20 + (sintab64[(title_delay + 29) & 63] >> 4));

	// Ghost image animation
	spr_image(5, 120 + ((title_delay & 15) >> 2));
	spr_image(6, 120 + ((title_delay & 15) >> 2));
	spr_image(7, 120 + ((title_delay & 15) >> 2));


	return title_ty < 2;

}

// Clean up after chase title screen animation
void titlescreen_chase_clear(void)
{
	vic.spr_enable = 0x00;
	title_delay = 0;
}

// Title screen animations
enum TitleScreenAnim
{
	TSA_NONE,
	TSA_INTRO_TEXT_0,
	TSA_INTRO_TEXT_1,
	TSA_HIGHSCORE_0,
	TSA_HIGHSCORE_1,
	TSA_CREDITS_0,
	TSA_CREDITS_1,
	TSA_BATS,
	TSA_CHASE,
	TSA_BALLS
};

// One step of title screen animation clean up
// requires 12 calls for full cleanup

bool titlescreen_clear_step(void)
{
	if (title_delay < 12)
	{
		memset(DynSprites + 256 * title_delay, 0, 256);
		title_delay++;
		return true;
	}
	else
		return false;
}

// Show and animate the title screen
void titlescreen_show(void)
{
	// Enable third voice and start title music
	music_patch_voice3(true);
	music_init(1);

	// Expand hires image
	const char * sp = titlescreen;

	sp = oscar_expand_lzo(Font, sp);
	sp = oscar_expand_lzo(Screen1, sp);
	sp = oscar_expand_lzo(Color, sp);

	// Hide sprites
	vic.spr_enable = 0x00;

	// Change VIC display to title screen
	vic_setmode(VICM_HIRES_MC, Screen1, Font);
	vic.color_border = VCOL_BLACK;
	vic.color_back = VCOL_BLACK;

	// Initial cleanup of dynamic sprite range
	memset(DynSprites, 0x00, 48 * 64);

	title_delay = 0;
	TitleScreenAnim	anim = TSA_INTRO_TEXT_0;

	// anim = TSA_CREDITS_0;

	rirq_sort();

	// start raster IRQ processing
	rirq_start();

	do {
		frand();

		switch (anim)
		{
		case TSA_NONE:
			vic_waitBottom();
			break;

		case TSA_CREDITS_0:
			vic_waitBottom();
			if (!titlescreen_clear_step())
			{
				titlescreen_credits_init();
				anim = TSA_CREDITS_1;
			}
			break;

		case TSA_INTRO_TEXT_0:
			vic_waitBottom();
			if (!titlescreen_clear_step())
			{
				titlescreen_scroll_init();
				anim = TSA_INTRO_TEXT_1;
			}
			break;

		case TSA_HIGHSCORE_0:
			vic_waitBottom();
			if (!titlescreen_clear_step())
			{
				titlescreen_highscore_init();
				anim = TSA_HIGHSCORE_1;
			}
			break;

		case TSA_CREDITS_1:
			rirq_wait();
			vic_waitBottom();

			if (!titlescreen_credits_step())
			{
				titlescreen_credits_clear();
				titlescreen_chase_init();
				anim = TSA_CHASE;
			}
			break;

		case TSA_INTRO_TEXT_1:
			rirq_wait();
			vic_waitBottom();

			if (!titlescreen_scroll_step())
			{
				titlescreen_scroll_clear();
				titlescreen_bats_init();
				anim = TSA_BATS;
			}
			break;

		case TSA_HIGHSCORE_1:
			rirq_wait();
			vic_waitBottom();

			if (!titlescreen_highscore_step())
			{
				titlescreen_highscore_clear();
				titlescreen_balls_init();
				anim = TSA_BALLS;
			}
			break;

		case TSA_BATS:
			vic_waitBottom();
			if (!titlescreen_bats_step())
			{
				titlescreen_bats_clear();
				anim = TSA_CREDITS_0;
			}
			break;

		case TSA_CHASE:
			vic_waitBottom();
			if (!titlescreen_chase_step())
			{
				titlescreen_chase_clear();
				anim = TSA_HIGHSCORE_0;
			}
			break;

		case TSA_BALLS:
			vic_waitBottom();
			if (!titlescreen_balls_step())
			{
				titlescreen_balls_clear();
				anim = TSA_INTRO_TEXT_0;
			}
			break;

		}

		// Wait for top of screen
		vic_waitTop();

		// Play some music
		music_play();

		// Wait for center screen
		while (vic.raster < 150)
			;

		// Play more music
		music_play();

		// Check joystick button
		joy_poll(0);

	} while (!joyb[0]);

	// Clear screen if needed
	switch (anim)
	{
	case TSA_INTRO_TEXT_1:
		rirq_wait();
		vic_waitBottom();
		titlescreen_scroll_clear();
		break;

	case TSA_HIGHSCORE_1:
		rirq_wait();
		vic_waitBottom();
		titlescreen_highscore_clear();
		break;
	}

	// Prepare fading table
	for(int i=0; i<256; i++)
		Screen0[i] = darker[i & 0x0f] | (darker[(i & 0xf0) >> 4] << 4);

	// Four steps of fading
	for(char i=0; i<4; i++)
	{
		// Fade VIC color portion in even frames
		char j = 0;
		do
		{
			Color  [j + 0x000] = Screen0[Color  [j + 0x000]];
			Color  [j + 0x100] = Screen0[Color  [j + 0x100]];
			Color  [j + 0x200] = Screen0[Color  [j + 0x200]];
			Color  [j + 0x300] = Screen0[Color  [j + 0x300]];
			j++;
		} while (j);

		vic_waitFrame();

		// Fade music
		sid.fmodevol = 14 - 4 * i;

		// Fade screen portion in odd frames
		j = 0;
		do
		{
			Screen1[j + 0x000] = Screen0[Screen1[j + 0x000]];
			Screen1[j + 0x100] = Screen0[Screen1[j + 0x100]];
			Screen1[j + 0x200] = Screen0[Screen1[j + 0x200]];
			Screen1[j + 0x300] = Screen0[Screen1[j + 0x300]];
			j++;
		} while (j);

		vic_waitFrame();

		// Fade music
		sid.fmodevol = 12 - 4 * i;

	}

	// Final cleanup of display
	vic_waitBottom();
	vic.ctrl1 = VIC_CTRL1_RST8;

	sid.voices[0].ctrl = 0;
	sid.voices[1].ctrl = 0;
	sid.voices[2].ctrl = 0;

	// Wait for button up again
	while (joyb[0])
	{
		joy_poll(0);

		// Free run random generator
		frand();
	}	
}

// Move some random columns down for game end animation

void column_down(void)
{
	// Number of columns depends on display mode, lower
	// number in NTSC due to higher frame rate

	for(char i=ntsc ? 3 : 5; i>0; i--)
	{
		// Draw one random column from deck
		char j = (frand() >> 8) & 15;

		// Get actual column index
		char x = scr_row[j];

		// Remove column from deck
		while (j < 39)
		{
			scr_row[j] = scr_row[j + 1];
			j++;
		}

		// Put it at the end of the deck
		scr_row[39] = x;

		// Scroll column down, complete unroll
	#assign yi 23
	#repeat
		Screen1[yi * 40 + 40 + x] = Screen1[yi * 40 + x];
		Color[yi * 40 + 40 + x] = Color[yi * 40 + x];
	#assign yi yi - 1
	#until yi < 0
	#undef yi

		// Fill new char at top with black
		Screen1[x] = 0xc1;
	}
}

// Compare current score with highscore table entry, return true if greater

bool highscore_greater(char n)
{
	// Loop over six digits
	for(char k=0; k<6; k++)
	{
		// Compare digit with highscore entry
		if (score[k] > highscores[n].score[k])
			return true;
		else if (score[k] < highscores[n].score[k])
			return false;
	}

	// All the same
	return false;
}

// List of shades of grey

static const char greys[4] = {
	VCOL_LT_GREY,
	VCOL_MED_GREY,
	VCOL_DARK_GREY,
	VCOL_BLACK
};

// Show highscore screen

bool highscore_show(void)
{
	// Enable third SID voice for music

	music_patch_voice3(true);

	// Make room for highscore in table
	char hi = 5;
	while (hi > 0 && highscore_greater(hi - 1))
	{
		if (hi != 5)
			highscores[hi] = highscores[hi - 1];
		hi--;
	}

	// Did we get into the top 5
	if (hi != 5)
	{
		// Insert current score into table
		for(char i=0; i<6; i++)
			highscores[hi].score[i] = score[i];
		// Insert current name into table
		for(char i=0; i<3; i++)		
			highscores[hi].name[i] = highscoreName[i];
	}

	// Play win or lose music
	if (hi == 5)		
		music_init(frand() & 1 ? 3 : 4);
	else
		music_init(frand() & 1 ? 2 : 5);

	// Copy double buffer into Sceen1 if needed
	if (cframe)
		memcpy(Screen1, Screen0, 1000);

	// Wait for end of screen
	vic_waitBottom();

	// Disable the in game sprites
	vic.spr_enable = 0x00;

	rirq_wait();

	for(char i=0; i<10; i++)
		rirq_clear(i);

	// Raster interrupt for title row show
	rirq_build(&irq_title_show, 1);
	rirq_write(&irq_title_show, 0, &vic.memptr, 0x3a);
	rirq_set(6, 178, &irq_title_show);

	// Raster interrupt for title row hide
	rirq_build(&irq_title_hide, 1);
	rirq_write(&irq_title_hide, 0, &vic.memptr, 0x38);
	rirq_set(7, 250, &irq_title_hide);

	// Sprite attributes
	vic.spr_multi = 0x00;
	vic.spr_expand_x = 0xff;
	vic.spr_expand_y = 0x00;

	// Prepare all sprites for character display

	for(char i=0; i<4; i++)
	{
		vic_sprxy(i    , 88 + 48 * i, 110);
		vic_sprxy(i + 4, 87 + 48 * i, 110);
		vic.spr_color[i] = VCOL_WHITE;
		vic.spr_color[i + 4] = VCOL_BLACK;
	}

	irq_title_c = VCOL_BLACK;

	// Prepare six lines of character display
	for(char i=0; i<6; i++)
	{
		rirq_build(irq_title + i, 2);
		rirq_write(irq_title + i, 0, &irq_title_i, i);
		rirq_call(irq_title + i, 1, titlescreen_irq);
		rirq_set(i, 75 + 25 * i, irq_title + i);

		irq_title_y[i] = 80 + 25 * i;
		irq_title_s[i] = 8 * i;
	}

	irq_title_y[0] = 70;
	rirq_set(0, 65, irq_title);


	// sort the raster IRQs
	rirq_sort();

	rirq_wait();

	// Clear dynamic sprite area
	memset(DynSprites, 0x00, 48 * 64);	

	// Title of highscore screen in top two text rows (first sprite row)
	if (hi == 5)
	{
		titlescreen_string(2, 0, "YOU LOST");
		titlescreen_string(1, 1, "YOUR BALLS");
	}
	else
	{
		titlescreen_string(3, 0, "A NEW");
		titlescreen_string(1, 1, "HIGHSCORE");
	}

	// Highscore names and score into sprites
	for(char i=0; i<5; i++)
	{
		for(char j=0; j<3; j++)
		{
			titlescreen_char(j     , 2 * i + 2, highscores[i].name[j]);
			titlescreen_char(j  + 3, 2 * i + 2, '.');
		}
		for(char j=0; j<6; j++)
			titlescreen_char(j +  6, 2 * i + 2, highscores[i].score[j]);
	}

	vic.spr_enable = 0xff;

	// Build deck of cards for scrolling column animation
	for(char i=0; i<40; i++)
		scr_row[i] = i;

	// Timing parameters for highscore entry watchdog timer
	title_delay = 50;
	title_ty = 20;

	char	hix = 0;
	char	hic = 0;

	bool	down = false;
	bool	restart = false;

	do {			
		// First music slot
		vic_waitTop();
		music_play();

		// Move some columns
		column_down();

		// Second music slot
		while (vic.raster < 150)
			;

		music_play();

		// Move some columns
		column_down();

		// Wait for bottom
		vic_waitBottom();

		// Check for joystick action
		joy_poll(0);
		if (joyx[0] || joyy[0] || joyb[0])
		{
			// Refresh watchdog timer

			title_delay = 50;
			title_ty = 20;
		}

		// We made highscore, so player may enter name
		if (hi < 5)
		{
			// Characters for name, or flashing cursor
			if ((hic & 0x1f) == 0x00)
				titlescreen_char(hix, 2 * hi + 2, 0x1c);
			else if ((hic & 0x1f) == 0x10)
				titlescreen_char(hix, 2 * hi + 2, highscores[hi].name[hix]);

			// Next animation phase
			hic++;
			if (hic == 0x20)
				hic = 0x00;

			// Highscore name entry not yet completed?
			if (hic < 0x80)
			{
				// Check for joystick input
				if (joyy[0] > 0)
				{
					// Previous letter Z -> A
					if (highscores[hi].name[hix] == 'A')
						highscores[hi].name[hix] = 'Z';
					else
						highscores[hi].name[hix]--;

					// Add wait timer for next input
					hic = 0xf0;
				}
				else if (joyy[0] < 0)
				{
					// Next letter A -> Z
					if (highscores[hi].name[hix] == 'Z')
						highscores[hi].name[hix] = 'A';
					else
						highscores[hi].name[hix]++;

					// Add wait timer for next input
					hic = 0xf0;
				}
				else if (hix < 2 && (joyb[0] && !down || joyx[0] > 0))
				{
					// Next char of 3 letter sequence
					titlescreen_char(hix, 2 * hi + 2, highscores[hi].name[hix]);
					hix++;
					hic = 0xe0;
				}
				else if (hix > 0 && joyx[0] < 0)
				{
					// Backspace
					titlescreen_char(hix, 2 * hi + 2, highscores[hi].name[hix]);
					hix--;
					hic = 0xe0;
				}
				else if (joyb[0] && !down)
				{
					// And done
					titlescreen_char(hix, 2 * hi + 2, highscores[hi].name[hix]);

					// Remember for next time
					for(char i=0; i<3; i++)		
						highscoreName[i] = highscores[hi].name[i];

					// No more entry
					hi = 0xff;
				}
			}
			else if (joyx[0] == 0 && joyy[0] == 0)
				hic &= 0x1f;

		}
		else if (joyb[0] && !down)
		{
			// Done, restart next round
			restart = true;
			break;
		}

		// Watchdog timer
		down = joyb[0];
		title_delay--;
		if (title_delay == 0)
		{
			title_delay = 0;
			title_ty--;
		}

	} while (title_ty);

	do {
		vic_waitFrame();
		joy_poll(0);
	} while (joyb[0]);

	// Cleanup highscore, fade out
	for(int i=0; i<4; i++)
	{
		char	color = greys[i];
		spr_color(0, color);
		spr_color(1, color);
		spr_color(2, color);
		spr_color(3, color);

		vic_waitFrame();

		sid.fmodevol = 14 - 4 * i;

		vic_waitFrame();

		sid.fmodevol = 12 - 4 * i;
	}

	sid.voices[0].ctrl = 0;
	sid.voices[1].ctrl = 0;
	sid.voices[2].ctrl = 0;

	for(char i=0; i<10; i++)
		rirq_clear(i);

	rirq_sort();

	// Disable display
	vic_waitBottom();
	vic.ctrl1 = VIC_CTRL1_RST8;

	return restart;
}

// Prepare tile sets for display by flipping vertical and horizontal
// orientation.  Charpad saves them from left to right, but we work
// in columns so we need them to top bottom

void tileset_init(void)
{
	for(char i=0; i<8; i++)
	{
		for(char j=0; j<8; j++)
		{
			for(char k=0; k<j; k++)
			{
				// Swap two chars
				char c = tileset_bottom[64 * i + 8 * j + k];
				tileset_bottom[64 * i + 8 * j + k] = tileset_bottom[64 * i + 8 * k + j];
				tileset_bottom[64 * i + 8 * k + j] = c;
			}
		}
	}
	for(char i=0; i<16; i++)
	{
		for(char j=0; j<8; j++)
		{
			for(char k=0; k<j; k++)
			{
				// Swap two chars
				char c = tileset_center[64 * i + 8 * j + k];
				tileset_center[64 * i + 8 * j + k] = tileset_center[64 * i + 8 * k + j];
				tileset_center[64 * i + 8 * k + j] = c;
			}
		}
	}
}

// Sound effect for enemy explosion
SIDFX	SIDFXExplosion[1] = {{
	1000, 1000, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_1500,
	-20, 0,
	8, 40,
	5
}};

// Sound effect for player explosion
SIDFX	SIDFXPlayerExplosion[4] = {{
	100, 1000, 
	SID_CTRL_GATE | SID_CTRL_SAW,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_1500,
	0, 0,
	2, 0,
	10
},{
	300, 1000, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_1500,
	100, 0,
	8, 0,
	10
},{
	1000, 1000, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_1500,
	200, 0,
	4, 20,
	10
},{
	2000, 1000, 
	SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_1500,
	0, 0,
	0, 40,
	10
}};

// Sound effect for bouncing agains wall or floor
SIDFX	SIDFXBoing[1] = {{
	3200, 2048 + 300, 
	SID_CTRL_GATE | SID_CTRL_RECT,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_750,
	-80, -40,
	4, 60,
	1
}};

// Sound effect for capturing a coin
SIDFX	SIDFXKatching[5] = {{
	NOTE_C(10), 512, 
	SID_CTRL_GATE | SID_CTRL_RECT,
	SID_ATK_2 | SID_DKY_114,
	0x80  | SID_DKY_114,
	0, 0,
	1, 0,
	2
},{
	NOTE_C(9), 512, 
	SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_114,
	0x80  | SID_DKY_114,
	0, 0,
	1, 0,
	2
},{
	NOTE_G(10), 512, 
	SID_CTRL_GATE | SID_CTRL_RECT,
	SID_ATK_2 | SID_DKY_114,
	0x80  | SID_DKY_114,
	0, 0,
	1, 0,
	2
},{
	NOTE_G(9), 512, 
	SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_114,
	0x80  | SID_DKY_114,
	0, 0,
	1, 0,
	2
},{
	NOTE_E(10), 512, 
	SID_CTRL_GATE | SID_CTRL_RECT,
	SID_ATK_2 | SID_DKY_750,
	0xf0  | SID_DKY_750,
	0, 16,
	4, 60,
	2
}};

// Sound effect for catching a star
SIDFX	SIDFXStar[4] = {{
	NOTE_C(8), 3072, 
	SID_CTRL_GATE | SID_CTRL_SAW,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_6,
	0, 0,
	1, 0,
	4
},{
	NOTE_E(8), 3072, 
	SID_CTRL_GATE | SID_CTRL_SAW,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_6,
	0, 0,
	1, 0,
	4
},{
	NOTE_G(8), 3072, 
	SID_CTRL_GATE | SID_CTRL_SAW,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_6,
	0, 0,
	1, 0,
	4
},{
	NOTE_C(9), 3072, 
	SID_CTRL_GATE | SID_CTRL_SAW,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_300,
	0, 0,
	4, 32,
	4
}};

// Sound effect of flying shuriken
SIDFX	SIDFXShuriken[4] = {{
	8000, 3072, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_168,
	-400, 0,
	6, 0,
	7
},{
	8000, 3072, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xc0  | SID_DKY_168,
	-500, 0,
	6, 0,
	7
},{
	8000, 3072, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xa0  | SID_DKY_168,
	-600, 0,
	5, 0,
	3
},{
	8000, 3072, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0x80  | SID_DKY_750,
	-800, 0,
	1, 8,
	3
}};

// Prepare math tables

void math_init(void)
{
	// Loop from 0 to 255 and break, when at 0 again
	char	c = 0;
	do
	{
		asrtab4[c] = (int)c << 8 >> 12;
		lsrtab4[c] = c >> 4;
		asltab4[c] = c << 4;
		unsigned	s = c * c;
		sqrtabl[c] = s & 0xff;
		sqrtabh[c] = s >> 8;
		c++;
	} while (c);
}

// Rigid body physics of player and iron ball.  Position px/py uses
// four fractional bits, velocity vx/vy used eight fractional bits.

__zeropage struct Body
{
	int		px, py, vx, vy;
	char	weight;

}	player, ball;

static const unsigned PBITS = 4;
static const unsigned VBITS = 4;

// Signed shift by four to the right, using table lookup
// compiler would use shift and ror to avoid large tables

static inline int asr4(int v)
{
	byte	hv = (unsigned)v >> 8;
	byte	lv = v & 0xff;

	return (asrtab4[hv] << 8) | (asltab4[hv] | lsrtab4[lv]);
}

// State of the playfield

enum PlayfieldState
{
	PPHASE_IDLE,
	PPHASE_SCROLLED,
	PPHASE_COPIED
};

// The playfield has an x position (px) and an x velocity (vx) with
// four fractional bits.

__zeropage struct Playfield
{
	char			px, vx;
	char			cx;
	PlayfieldState	phase;
	char			wsize, wfreq;

}	playfield;

// Type of power up

enum PowerUp
{
	PWUP_SHIELD,
	PWUP_SLOWDOWN,
	PWUP_MAGNET,
	PWUP_BUBBLE,
};

// Colors of the four powerup types

static const char PowerUpColors[] = {VCOL_LT_GREY, VCOL_GREEN, VCOL_YELLOW, VCOL_LT_BLUE};

// Flags controling enemy behaviour

#define ET_BALL_COLLISION	0x10
#define ET_PLAYER_COLLISION	0x20
#define ET_LETHAL			0x40

// Types of enemies, including behaviour flags

enum EnemyType
{
	ET_NONE = 0,
	ET_EXPLODE = 1,
	ET_RISING_STAR = 2,
	ET_DROPPING_COIN = 3,
	ET_EXPLODE_POWERUP = 4,
	ET_POWERUP_ESCAPE = 5,

	ET_MINE = ET_BALL_COLLISION + ET_PLAYER_COLLISION + ET_LETHAL + 2,
	ET_STAR = ET_BALL_COLLISION + ET_PLAYER_COLLISION + 3,

	ET_LOWER_SPIKE = ET_PLAYER_COLLISION + ET_LETHAL + 4,
	ET_UPPER_SPIKE = ET_PLAYER_COLLISION + ET_LETHAL + 5,
	ET_KNIFE = ET_BALL_COLLISION + ET_PLAYER_COLLISION + ET_LETHAL + 6,
	ET_COIN = ET_PLAYER_COLLISION + 7,

	ET_SHURIKEN_UP = ET_BALL_COLLISION + ET_PLAYER_COLLISION + ET_LETHAL + 8,
	ET_SHURIKEN_DOWN = ET_BALL_COLLISION + ET_PLAYER_COLLISION + ET_LETHAL + 9,

	ET_BAT = ET_BALL_COLLISION + ET_PLAYER_COLLISION + ET_LETHAL + 10,
	ET_SPRING = ET_BALL_COLLISION + ET_PLAYER_COLLISION + ET_LETHAL + 11,
	ET_SPRING_JUMP = ET_BALL_COLLISION + ET_PLAYER_COLLISION + ET_LETHAL + 12,
	ET_GHOST = ET_BALL_COLLISION + ET_PLAYER_COLLISION + ET_LETHAL + 13,

	ET_POWERUP = ET_PLAYER_COLLISION + 14
};

// Enemy entry event

enum EnemyEvent
{
	EE_NONE,

	EE_MINE_1,
	EE_MINE_2,
	EE_MINE_3,

	EE_STAR_MID,
	EE_STAR_TOP,
	EE_STAR_BOTTOM,

	EE_COIN_MID,
	EE_COIN_TOP,
	EE_COIN_BOTTOM,

	EE_SPIKES,
	EE_KNIVE,
	EE_SHURIKEN,
	EE_BAT,
	EE_SPRING,
	EE_GHOST,

	EE_SHRINK,
	EE_FREQUENCY,
};

// Table lookup for enemy events based on random number, gets
// filled with more and more difficult events as game progresses

EnemyEvent	eventMatrix[64];

// Event to add to the event matrix when entering a new level

EnemyEvent	eventLevels[100] = {

	EE_FREQUENCY, EE_MINE_1, EE_NONE,     EE_NONE,   EE_NONE, 
	EE_SHRINK,    EE_MINE_1, EE_NONE,     EE_NONE,   EE_NONE, 
	EE_FREQUENCY, EE_MINE_1, EE_NONE,     EE_NONE,   EE_NONE, 
	EE_SHRINK,    EE_MINE_1, EE_NONE,     EE_NONE,   EE_NONE, 

	EE_FREQUENCY, EE_SPIKES, EE_KNIVE,    EE_NONE,   EE_NONE, 
	EE_SHRINK,    EE_SPIKES, EE_KNIVE,    EE_SPRING, EE_NONE, 
	EE_FREQUENCY, EE_SPIKES, EE_KNIVE,    EE_SPRING, EE_NONE, 
	EE_FREQUENCY, EE_SPIKES, EE_KNIVE,    EE_BAT,    EE_NONE, 

	EE_SHRINK,    EE_MINE_2, EE_SHURIKEN, EE_NONE,   EE_NONE, 
	EE_FREQUENCY, EE_MINE_2, EE_SHURIKEN, EE_SPRING, EE_NONE, 
	EE_FREQUENCY, EE_MINE_2, EE_SHURIKEN, EE_NONE,   EE_NONE, 
	EE_SHRINK,    EE_MINE_2, EE_SHURIKEN, EE_BAT,    EE_NONE, 

	EE_FREQUENCY, EE_SPIKES, EE_KNIVE,    EE_SPRING, EE_NONE, 
	EE_FREQUENCY, EE_SPIKES, EE_KNIVE,    EE_GHOST,  EE_NONE, 
	EE_FREQUENCY, EE_SPIKES, EE_KNIVE,    EE_NONE,   EE_NONE, 
	EE_SHRINK,    EE_SPIKES, EE_KNIVE,    EE_BAT,    EE_NONE, 

	EE_FREQUENCY, EE_MINE_3, EE_SHURIKEN, EE_NONE,   EE_NONE, 
	EE_FREQUENCY, EE_MINE_3, EE_SHURIKEN, EE_GHOST,  EE_GHOST, 
	EE_FREQUENCY, EE_MINE_3, EE_SHURIKEN, EE_GHOST,  EE_GHOST, 
	EE_FREQUENCY, EE_MINE_3, EE_SHURIKEN, EE_BAT,    EE_GHOST, 
};

// Data structure of one enemy on screen, up to three enemies
// visible at the same time.  Enemy position is in sprite pixels
// thus eight bits vertical and 9 bits horizontal

struct Enemy
{
	char		py;
	unsigned	px;
	char		phase, height;
	EnemyType	type;
	sbyte		vy, vx;

}	enemies[3];

// Next enemy struct to activate in round robin mode

char		nenemy;

// Score start address in sprite position.  The score sprites are at the top
// of the screen and multiplexed with the enemies.  The text is at the
// bottom of the sprites so they only cover eight lines at the top, and
// enemies can start early

const char doffset[9] = {39, 40, 41, 103, 104, 105, 167, 168, 169};

// Game state
enum GameState
{
	GS_TITLE,			// Show title screen

	GS_START,			// Waiting for the game to start

	GS_READY,			// Getting ready

	GS_PLAYING,			// Actually playing
	GS_EXPLODING,		// Player ship is exploding

	GS_GAME_OVER		// Game is over
};

// Game data
struct Game
{
	GameState	state;	// Current game state	
	char		count;	// Countdown for states that change after delay
	bool		pulling, throw;
	char		level;
	char		shield, bubble, magnet;

}	game;


// top IRQ: line 58
// set enemy sprites, 3 * (color, ylow, xlow), xhigh, mcolor
// play music
//
// center IRQ: line 220
// play music
//
// bottom IRQ: line 250
// set score sprites, 3 * (color, ylow, xlow), xhigh, mcolor, xexpand

__zeropage	char	xspr_msb;

// Expand font and sprite data

void playfield_init_font(void)
{
	oscar_expand_lzo(Font, charset_center);
	memcpy(Font + 0xc0 * 8, charset_front, 64 * 8);
	oscar_expand_lzo(FontBottom, charset_bottom);
	memcpy(FontBottom + 0xc0 * 8, charset_front, 64 * 8);
	memset(DynSprites, 0, 2048);

	memset(Screen0, 0xc1, 1000);
	memset(Screen1, 0xc1, 1000);
}

// Init interrupts for game play

void playfield_initirqs(void)
{
	// Top interrupt for game sprites and first music call

	rirq_build(irq_top, 19);
	rirq_write(irq_top, 0, &(vic.spr_pos[5].y), 0);
	rirq_write(irq_top, 1, &(vic.spr_pos[6].y), 0);
	rirq_write(irq_top, 2, &(vic.spr_pos[7].y), 0);

	rirq_write(irq_top, 3, &(vic.spr_pos[5].x), 0);
	rirq_write(irq_top, 4, &(vic.spr_pos[6].x), 0);
	rirq_write(irq_top, 5, &(vic.spr_pos[7].x), 0);

	rirq_write(irq_top, 6, &(vic.spr_color[5]), 0);
	rirq_write(irq_top, 7, &(vic.spr_color[6]), 0);
	rirq_write(irq_top, 8, &(vic.spr_color[7]), 0);

	rirq_write(irq_top, 9, &(Screen0[0x3fd]), 0);
	rirq_write(irq_top, 10, &(Screen0[0x3fe]), 0);
	rirq_write(irq_top, 11, &(Screen0[0x3ff]), 0);

	rirq_write(irq_top, 12, &(Screen1[0x3fd]), 0);
	rirq_write(irq_top, 13, &(Screen1[0x3fe]), 0);
	rirq_write(irq_top, 14, &(Screen1[0x3ff]), 0);

	rirq_write(irq_top, 15, &vic.spr_msbx, 0);
	rirq_write(irq_top, 16, &vic.spr_multi, 0xff);
	rirq_write(irq_top, 17, &vic.spr_expand_x, 0x00);

	rirq_call(irq_top, 18, music_play);
	rirq_set(0, 58, irq_top);

	// Center interrupt switching font to bottom set

	rirq_build(&irq_center, 1);
	rirq_write(&irq_center, 0, &vic.memptr, 0x2a);
	rirq_set(1, 178, &irq_center);

	// Second music interrupt

	rirq_build(&irq_music, 1);
	rirq_call(&irq_music, 0, music_play);
	rirq_set(2, 220, &irq_music);

	// Bottom interrupt for score display

	rirq_build(irq_bottom, 19);
	rirq_write(irq_bottom, 0, &(vic.spr_pos[5].y), 37);
	rirq_write(irq_bottom, 1, &(vic.spr_pos[6].y), 37);
	rirq_write(irq_bottom, 2, &(vic.spr_pos[7].y), 37);

	rirq_write(irq_bottom, 3, &(vic.spr_pos[5].x), 180);
	rirq_write(irq_bottom, 4, &(vic.spr_pos[6].x), 228);
	rirq_write(irq_bottom, 5, &(vic.spr_pos[7].x),  20);

	rirq_write(irq_bottom, 6, &(vic.spr_color[5]), VCOL_WHITE);
	rirq_write(irq_bottom, 7, &(vic.spr_color[6]), VCOL_WHITE);
	rirq_write(irq_bottom, 8, &(vic.spr_color[7]), VCOL_WHITE);

	rirq_write(irq_bottom, 9, &(Screen0[0x3fd]), 0);
	rirq_write(irq_bottom, 10, &(Screen0[0x3fe]), 1);
	rirq_write(irq_bottom, 11, &(Screen0[0x3ff]), 2);

	rirq_write(irq_bottom, 12, &(Screen1[0x3fd]), 0);
	rirq_write(irq_bottom, 13, &(Screen1[0x3fe]), 1);
	rirq_write(irq_bottom, 14, &(Screen1[0x3ff]), 2);

	rirq_write(irq_bottom, 15, &vic.spr_msbx, 0);
	rirq_write(irq_bottom, 16, &vic.spr_multi, 0x1f);
	rirq_write(irq_bottom, 17, &vic.spr_expand_x, 0xe0);

	rirq_write(irq_bottom, 18, &vic.memptr, 0x28);

	rirq_set(3, 250, irq_bottom);


	// sort the raster IRQs
	rirq_sort();

	// start raster IRQ processing
	rirq_start();
}

// Sprites 5, 6 and 7 are used for enemies and the score, thus
// require some special setup code

inline void xspr_move(char sp, int xpos, int ypos)
{
	__assume(sp < 8);

	if (sp < 5)
	{
		// Non multiplexed sprites get updated directly

		vic.spr_pos[sp].y = ypos;
		vic.spr_pos[sp].x = xpos;
	}
	else
	{
		// Multiplexed sprites get updated in interrupt

		rirq_data(irq_top, sp - 5 + 0, ypos);
		rirq_data(irq_top, sp - 5 + 3, xpos);		
	}

	// msb needs to be updated in both cases

	if (xpos & 0x100)
		xspr_msb |= 1 << sp;
	else
		xspr_msb &= ~(1 << sp);

	rirq_data(irq_top, 15, xspr_msb);
	rirq_data(irq_bottom, 15, xspr_msb & 0x1f | 0x80);
}

// Prepare movement of sprites, not changing the irq msb

inline void xspr_move_prep(char sp, int xpos, int ypos)
{
	__assume(sp < 8);

	vic.spr_pos[sp].y = ypos;
	vic.spr_pos[sp].x = xpos;

	if (xpos & 0x100)
		xspr_msb |= 1 << sp;
	else
		xspr_msb &= ~(1 << sp);
}

// Now for the irq msb

inline void xspr_move_done(void)
{
	rirq_data(irq_top, 15, xspr_msb);
	rirq_data(irq_bottom, 15, xspr_msb & 0x1f | 0x80);
}

// Change sprite image

inline void xspr_image(char sp, char img)
{
	__assume(sp < 8);

	if (sp < 5)
	{
		// Non multiplexed sprites get updated in both screen buffers

		Screen0[0x3f8 + sp] = img;
		Screen1[0x3f8 + sp] = img;
	}
	else
	{
		// Multiplexed sprites in next irq

		rirq_data(irq_top, sp - 5 +  9, img);
		rirq_data(irq_top, sp - 5 + 12, img);		
	}
}

// Change sprite color

inline void xspr_color(char sp, char color)
{
	__assume(sp < 8);

	if (sp < 5)
		vic.spr_color[sp] = color;
	else
		rirq_data(irq_top, sp - 5 + 6, color);
}

// Init in game sprites

void xspr_init(void)
{
	xspr_msb = 0;
	vic.spr_expand_x = 0x00;
	vic.spr_expand_y = 0x00;
	vic.spr_enable = 0xff;
}

// Draw one score digit into sprite memory

inline void score_draw(char ci)
{
	// Target address in sprite memory
	char * dp = DynSprites + doffset[ci];

	// Source address in font
	const char * sp = charset_digits + 8 * score[ci];

	// Copy 8 lines, sprites have a stride of three
	dp[ 0] = sp[0];
	dp[ 3] = sp[1];
	dp[ 6] = sp[2];
	dp[ 9] = sp[3];
	dp[12] = sp[4];
	dp[15] = sp[5];
	dp[18] = sp[6];
	dp[21] = sp[7];
}

// Init score with 000000*00

void score_init(void)
{
	// Put zeros
	for(char i=0; i<9; i++)
		score[i] = 0x30;

	// Put star
	score[6] = 0x00;
	nstars = 1;
	scorecnt = 0;

	// Fill sprite image
	for(char i=0; i<9; i++)
		score_draw(i);
}

// Add to defered score increment

void score_add(char amount)
{
	scorecnt += amount << 8;
}

// Increment score by one digit at a time

void score_inc(void)
{
	// Add to score based on number of stars captured, 8 bit fraction
	scorecnt += 32 * nstars;

	// New non fractional score to account for
	if (scorecnt & 0xff00)
	{
		// Check for increment of ten column
		char	c = 5;
		if (scorecnt >= 0xa00)
		{
			scorecnt -= 0xa00;
			score[4]++;
			c = 4;
		}
		else
		{
			score[5] += scorecnt >> 8;
			scorecnt &= 0x00ff;
		}
	
		// Take care of carry
		while (score[c] >= 0x3a)
		{
			score[c] -= 10;
			score_draw(c);
			c--;
			score[c]++;		
		}

		score_draw(c);
	}
}

// Increment number of stars captured

void star_inc(void)
{
	nstars++;

	// Update star display
	score[8]++;

	// Check carry
	if (score[8] == 0x3a)
	{
		score[8] = 0x30;
		score[7]++;		
		score_draw(7);
	}

	score_draw(8);

	// Increase weight of ball every four stars

	if (!(nstars & 3))
	{	
		player.weight--;
		ball.weight++;
	}
}

// Init enemy data structure and sprites

void enemies_init(void)
{
	enemies[0].type = ET_NONE;
	enemies[1].type = ET_NONE;
	enemies[2].type = ET_NONE;

	xspr_move(5, 0, 0);
	xspr_move(6, 0, 0);
	xspr_move(7, 0, 0);
}

// Add an enemy

void enemies_add(EnemyType type, char y)
{
	// Is slot free?
	if (enemies[nenemy].type == ET_NONE)
	{
		// Position, type and velocity
		enemies[nenemy].px = 354;
		enemies[nenemy].py = y;
		enemies[nenemy].type = type;
		enemies[nenemy].phase = 0;
		enemies[nenemy].vx = 0;
		enemies[nenemy].vy = 0;

		// Per type initialization
		switch (type)
		{
			case ET_MINE:				
				enemies[nenemy].height = 17;
				xspr_image(5 + nenemy, 80);
				xspr_color(5 + nenemy, VCOL_RED);
				break;
			case ET_STAR:
				enemies[nenemy].height = 14;
				xspr_image(5 + nenemy, 104);
				xspr_color(5 + nenemy, VCOL_LT_BLUE);
				break;
			case ET_COIN:
				enemies[nenemy].height = 13;
				xspr_image(5 + nenemy, 108);
				xspr_color(5 + nenemy, VCOL_YELLOW);
				break;
			case ET_LOWER_SPIKE:
				enemies[nenemy].height = 14;
				xspr_image(5 + nenemy, 81);
				xspr_color(5 + nenemy, VCOL_LT_GREY);
				break;
			case ET_UPPER_SPIKE:
				enemies[nenemy].height = 14;
				xspr_image(5 + nenemy, 82);
				xspr_color(5 + nenemy, VCOL_LT_GREY);
				break;
			case ET_KNIFE:
				enemies[nenemy].height = 10;
				xspr_image(5 + nenemy, 83);
				xspr_color(5 + nenemy, VCOL_LT_GREY);
				break;
			case ET_SHURIKEN_UP:
			case ET_SHURIKEN_DOWN:
				enemies[nenemy].height = 17;
				xspr_image(5 + nenemy, 84);
				xspr_color(5 + nenemy, VCOL_LT_GREY);
				sidfx_play(2, SIDFXShuriken, 4);
				break;
			case ET_BAT:
				enemies[nenemy].height = 9;
				xspr_image(5 + nenemy, 112);
				xspr_color(5 + nenemy, VCOL_BROWN);
				break;
			case ET_SPRING:
				enemies[nenemy].height = 21;
				xspr_image(5 + nenemy, 116);
				xspr_color(5 + nenemy, VCOL_LT_GREY);
				break;
			case ET_GHOST:
				enemies[nenemy].height = 21;
				xspr_image(5 + nenemy, 120);
				xspr_color(5 + nenemy, VCOL_LT_BLUE);
				break;
		}

		// Put enemy in its place
		xspr_move(5 + nenemy, enemies[nenemy].px, enemies[nenemy].py);

		// Advance to next slot
		nenemy++;
		if (nenemy == 3)
			nenemy = 0;
	}
}

// Respond to an enemy event

void enemies_event(EnemyEvent ee)
{
	// ctop and cbottom represent the free area of the newly added column,
	// thus control enemy placement

	switch (ee)
	{
	case EE_NONE:
		break;

	// Adding 1, 2 or 3 mines
	case EE_MINE_1:
		enemies_add(ET_MINE, (ctop + cbottom) * 4 + 40);
		break;
	case EE_MINE_2:
		enemies_add(ET_MINE, (ctop + cbottom) * 4 + 30);
		enemies_add(ET_MINE, (ctop + cbottom) * 4 + 50);
		break;
	case EE_MINE_3:
		enemies_add(ET_MINE, (ctop + cbottom) * 4 + 20);
		enemies_add(ET_MINE, (ctop + cbottom) * 4 + 40);
		enemies_add(ET_MINE, (ctop + cbottom) * 4 + 60);
		break;

	// Add a star
	case EE_STAR_MID:
		enemies_add(ET_STAR, (ctop + cbottom) * 4 + 40);
		break;
	case EE_STAR_TOP:
		enemies_add(ET_STAR, ctop * 8 + 64);
		break;
	case EE_STAR_BOTTOM:
		enemies_add(ET_STAR, cbottom * 8 + 30);
		break;

	// Add a coin
	case EE_COIN_MID:
		enemies_add(ET_COIN, (ctop + cbottom) * 4 + 40);
		break;
	case EE_COIN_TOP:
		enemies_add(ET_COIN, ctop * 8 + 64);
		break;
	case EE_COIN_BOTTOM:
		enemies_add(ET_COIN, cbottom * 8 + 30);
		break;

	case EE_SPIKES:
		if (ctop > 0)
			enemies_add(ET_UPPER_SPIKE, ctop * 8 + 50);
		enemies_add(ET_LOWER_SPIKE, cbottom * 8 + 36);
		break;

	case EE_KNIVE:
		enemies_add(ET_KNIFE, (ctop + cbottom) * 4 + 40);
		break;

	case EE_SHURIKEN:
	{
		// Shuriken target player
		char yp = asr4(player.py);
		if (yp < 100)
			enemies_add(ET_SHURIKEN_UP, yp + 50 + ((320 - asr4(player.px)) >> 2));
		else
			enemies_add(ET_SHURIKEN_DOWN, yp + 50 - ((320 - asr4(player.px)) >> 2));
	}
		break;

	case EE_BAT:
		enemies_add(ET_BAT, 92 + (frand() & 127));
		break;

	case EE_SPRING:
		enemies_add(ET_SPRING, cbottom * 8 + 37);
		break;

	case EE_GHOST:
		enemies_add(ET_GHOST, 90);
		break;
	}
}

// Move enemies by n pixel

void enemies_scroll(char n)
{
	// Loop through list of enemies

	for(char i=0; i<3; i++)
	{
		// Check if slot used

		if (enemies[i].type != ET_NONE)
		{
			// Move enemy, and remove if it leaves screen to the left

			if (enemies[i].px > n)	
				enemies[i].px -= n;
			else
				enemies[i].type = ET_NONE;

			// Per type

			switch (enemies[i].type)
			{
				case ET_MINE:
					// Mines just flash their color
					enemies[i].phase++;
					xspr_color(5 + i, mineflash[enemies[i].phase & 0x0f]);
					break;

				case ET_LOWER_SPIKE:
					// Lower spike moves up and down
					if (enemies[i].phase & 4)
						enemies[i].py--;
					else
						enemies[i].py++;
					enemies[i].phase++;
					break;

				case ET_UPPER_SPIKE:
					// Upper spike moves down and up
					if (enemies[i].phase & 4)
						enemies[i].py++;
					else
						enemies[i].py--;
					enemies[i].phase++;
					break;

				case ET_EXPLODE:
					// Explosions go through animation frames
					// and then vanish
					xspr_image(5 + i, 88 + enemies[i].phase);
					enemies[i].phase++;
					if (enemies[i].phase == 17)
					{
						enemies[i].type = ET_NONE;
						xspr_move(5 + i, 0, 0);
					}
					break;					

				case ET_EXPLODE_POWERUP:
					// Explosions with powerup go through animation frames
					// and then change into a power up
					xspr_image(5 + i, 88 + enemies[i].phase);
					enemies[i].phase++;
					if (enemies[i].phase == 17)
					{
						enemies[i].type = ET_POWERUP_ESCAPE;
						xspr_image(5 + i, 118);						
					}
					break;

				case ET_POWERUP_ESCAPE:
					// First stage of power up tries to get away from player

					enemies[i].px += 4 + n;
					if (enemies[i].px > asr4(player.px) + 32)
					{
						// Far enough, it becomes a real powerup

						enemies[i].type = ET_POWERUP;
						enemies[i].phase = 40;
						enemies[i].vx = frand() & 3;						
						xspr_image(5 + i, 124);
						xspr_color(5 + i, PowerUpColors[(PowerUp)enemies[i].vx]);
					}
					break;

				case ET_POWERUP:
					// Powerup moves to the right and wobbles vertically using
					// the sintab
					enemies[i].px += 1 + n;
					enemies[i].py += (sintab64[enemies[i].phase & 63] + 16) >> 5;
					xspr_image(5 + i, 124 + ((enemies[i].phase & 15) >> 2));
					enemies[i].phase++;

					// Extra check for exiting to the right or top
					if (enemies[i].px >= 344 || enemies[i].py < 20)
					{
						enemies[i].type = ET_NONE;
						xspr_move(5 + i, 0, 0);
					}
					break;

				case ET_RISING_STAR:
					// Rising star moves to the top of screen and then vanishes

					if (enemies[i].py > 50)
					{
						enemies[i].py -= 8;
						xspr_image(5 + i, 104 + ((enemies[i].phase & 15) >> 2));
						enemies[i].phase++;
					}
					else
						enemies[i].type = ET_NONE;
					break;

				case ET_STAR:
					// Star rotates through animation sequence
					xspr_image(5 + i, 104 + ((enemies[i].phase & 15) >> 2));
					enemies[i].phase++;
					break;

				case ET_DROPPING_COIN:

					// Dropping coin accelerates towards the floor
					if (enemies[i].py > 50)
					{
						enemies[i].py += enemies[i].phase;
						xspr_image(5 + i, 108 + ((enemies[i].phase & 15) >> 2));
						enemies[i].phase++;
					}
					else
					{
						enemies[i].type = ET_NONE;
						xspr_move(5 + i, 0, 0);
					}
					break;

				case ET_COIN:
					// Normal coin just rotates
					xspr_image(5 + i, 108 + ((enemies[i].phase & 15) >> 2));
					enemies[i].phase++;
					if (game.magnet)
					{
						// Coin magnet accelerates coin towards the player
						// in x direction
						if (enemies[i].px < asr4(player.px) + 32)
						{
							if (enemies[i].vx < 16)
								enemies[i].vx++;
						}
						else
						{
							if (enemies[i].vx > -16)
								enemies[i].vx--;
						}
						// and y direction
						if (enemies[i].py < asr4(player.py) + 58)
						{
							if (enemies[i].vy < 16)
								enemies[i].vy++;
						}
						else
						{
							if (enemies[i].vy > -16)
								enemies[i].vy--;						
						}

						enemies[i].px += (enemies[i].vx + 2) >> 2;
						enemies[i].py += (enemies[i].vy + 2) >> 2;
					}
					break;

				case ET_KNIFE:
					if (enemies[i].px >= 4)
						enemies[i].px -= 4;
					else
						enemies[i].type = ET_NONE;
					break;


				case ET_SHURIKEN_UP:
					// Shuriken moves up if it starts low
					if (enemies[i].px >= 4 && enemies[i].py > 44)
					{
						enemies[i].px -= 4;
						enemies[i].py --;
						xspr_image(5 + i, 84 + (enemies[i].phase & 3));
						enemies[i].phase++;
					}
					else
						enemies[i].type = ET_NONE;

					break;

				case ET_SHURIKEN_DOWN:
					// Shuriken moves down if it starts high
					if (enemies[i].px >= 4 && enemies[i].py < 250)
					{
						enemies[i].px -= 4;
						enemies[i].py ++;
						xspr_image(5 + i, 84 + (enemies[i].phase & 3));
						enemies[i].phase++;
					}
					else
						enemies[i].type = ET_NONE;

					break;

				case ET_BAT:
					// Bat animates and moves up and down
					if (enemies[i].px >= 1)
					{
						enemies[i].px -= 1;
						enemies[i].py += batyspeed[(enemies[i].phase & 63) >> 1];
						xspr_image(5 + i, 112 + ((enemies[i].phase & 15) >> 2));
						enemies[i].phase++;
					}
					else
						enemies[i].type = ET_NONE;
					break;

				case ET_SPRING:
					// Spring waits for the player to come above it

					if (enemies[i].px < asr4(player.px) + 32)
					{
						// Transforming into a jumping spring

						enemies[i].type = ET_SPRING_JUMP;
						enemies[i].phase = -32;
						xspr_image(5 + i, 117);
					}
					else if (enemies[i].px < asr4(player.px) + 96)
					{
						// Shiver in excitement if player comes closer

						enemies[i].phase++;						
						if (enemies[i].phase & 2)
							enemies[i].px++;
						else
							enemies[i].px--;
					}
					break;

				case ET_SPRING_JUMP:
					// Let gravity control the jump

					if (enemies[i].py > 20 && enemies[i].py < 250)
					{
						enemies[i].py += (sbyte)enemies[i].phase >> 2;
						enemies[i].phase ++;
					}
					else
						enemies[i].type = ET_NONE;
					break;

				case ET_GHOST:
					// Ghost animates

					xspr_image(5 + i, 120 + ((enemies[i].phase & 15) >> 2));
					enemies[i].phase++;

					// And moves towards player
					if (enemies[i].px < asr4(player.px) + 24)
					{
						if (enemies[i].vx < 32)
							enemies[i].vx++;
					}
					else
					{
						if (enemies[i].vx > -32)
							enemies[i].vx--;
					}

					if (enemies[i].py < asr4(player.py) + 50)
					{
						if (enemies[i].vy < 32)
							enemies[i].vy++;
					}
					else
					{
						if (enemies[i].vy > -32)
							enemies[i].vy--;						
					}

					enemies[i].px += (enemies[i].vx + 8) >> 4;
					enemies[i].py += (enemies[i].vy + 8) >> 4;
					break;
			}


			if (enemies[i].type != ET_NONE)
			{
				// Set new enemy position
				xspr_move(5 + i, enemies[i].px, enemies[i].py);
			}
		}
	}
}

// Vertical start position and random mask for tiles in the center range

const char ctileybase[16] = {5, 5, 5, 5, 5, 5, 5, 8, 8, 8, 1, 1, 6, 6, 7, 7};
const char ctileymask[16] = {3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 7, 7, 0, 0, 0, 0};

// Build the next column to scroll into the screen

void playfield_column(void)
{
	// Clean up
	scr_column[0] = 0xc2;
	for(char i=1; i<17; i++)
		scr_column[i] = 0xc0;

	// Add some random sprinkles to the empty column
	unsigned q = frand();
	scr_column[1 + (char)(q & 15)] = 0xc9 + ((char)q >> 5);
	q >>= 8;
	scr_column[4 + (char)(q & 15)] = 0xc9 + ((char)q >> 5);

	// Set base color
	for(char i=0; i<25; i++)
		col_column[i] = 9;

	__assume(bcnt < 8 && bimg < 8);

	// Draw single tile color bottom tile segment
	for(char i=0; i<8; i++)
	{
		char c = tileset_bottom[i + 8 * bcnt + 64 * bimg];
		scr_column[i + 17] = c;
	}

	// Horizontal end of bottom tile?
	bcnt++;
	if (bcnt == 8)
	{
		// Pick a random bottom tile for the next column
		bimg = (bimg + (frand() & 3) + 1) & 7;
		bcnt = 0;
	}

	// Do we have the end of a center tile and no foreground elements?
	if (csize == 0 && ccnt == 8)
	{
		// Check if it s time for the next foreground obstacle
		if (cdist == 0)
		{
			if (frand() & 3)
			{
				// Bottom and top obstacle

				ctop = (frand() & 15) + 2;
				cbottom = ctop + playfield.wsize + (frand() & 3);
				if (cbottom > 23)
				{
					ctop -= cbottom - 23;
					cbottom = 23;
				}

				// And an optional enemy
				enemies_event(eventMatrix[frand() & 63]);
			}
			else
			{
				// Center obstacle

				cbottom = frand() & 7;
				ctop = cbottom + 8 + 2 * (frand() & 3);
			}
			csize = 5;
			cdist = (playfield.wfreq + (frand() & 7)) >> 1;
		}
		else
		{
			// Pick the next center tile

			cimg = (frand() & 255) & 15;
			ccnt = 0;
			cdist--;
			cimgy = ctileybase[cimg] + (frand() & ctileymask[cimg]);

			// And an optional enemy

			ctop = 0;
			cbottom = 25;
			enemies_event(eventMatrix[frand() & 63]);
		}
	}

	// Do we have a foreground wall?
	if (csize > 0)
	{		
		csize--;
		if (csize == 0)
		{
			// Right edge, we do the shadow

			if (ctop < cbottom)
			{
				if (ctop > 0)
				{
					for(char i=0; i<ctop; i++)
						scr_column[i] = 0xc5;
					scr_column[ctop] = 0xc8;
				}

				scr_column[cbottom + 1] = 0xc4;
				for(char i=cbottom+2; i<25; i++)
					scr_column[i] = 0xc5;
			}
			else
			{
				scr_column[cbottom + 1] = 0xc4;
				for(char i=cbottom+2; i<ctop; i++)
					scr_column[i] = 0xc5;
				scr_column[ctop] = 0xc8;
			}
		}
		else if (ctop < cbottom)
		{
			// It is top and bottom obstacle

			char j = 255;
			if (ctop > 0)
			{
				// Draw top wall
				for(char i=ctop-1; i>0; i--)
				{
					scr_column[i] = 0xe0 + (j & 3) * 4 + (4 - csize);
					j--;
				}
				scr_column[0] = 0xf0 + (j & 3) * 4 + (4 - csize);

				// Draw shadow if not left most element
				if (csize == 3)
					scr_column[ctop] = 0xc6;
				else if (csize < 3)
					scr_column[ctop] = 0xc7;
			}

			// Draw bottom wall
			j = 0;
			for(char i=cbottom; i<25; i++)
			{
				scr_column[i] = 0xe0 + (j & 3) * 4 + (4 - csize);
				j++;
			}
		}
		else
		{
			// Center obstacle

			char j = 0;
			for(char i=cbottom; i<ctop; i++)
			{
				scr_column[i] = 0xe0 + (j & 3) * 4 + (4 - csize);
				j++;
			}

			// Draw shadow
			if (csize == 3)
				scr_column[ctop] = 0xc6;
			else if (csize < 3)
				scr_column[ctop] = 0xc7;
		}
	}
	else
	{
		// No obstacle so we have a center image

		__assume(ccnt < 8 && cimg < 16);		
		for(char i=0; i<8; i++)
		{
			char c = tileset_center[i + 8 * ccnt + 64 * cimg];
			// Update glyph and color of glyph
			scr_column[i + cimgy] = c;
			col_column[i + cimgy] = charattribs_center[c];

		}
		ccnt++;
	}
}

// Scroll first screen buffer to the left

void playfield_scroll0(void)
{
	// Unroll all vertical and three times horizontal with 13 loop iterations

	for(sbyte x=12; x>=0; x--) 
	{
	#assign ry 0
	#repeat		
		#assign rx 0
		#repeat
			Screen0[40 * ry + 13 * rx + x] = Screen1[40 * ry + 13 * rx + x + 1];
		#assign rx rx + 1
		#until rx == 3
	#assign ry ry + 1
	#until ry == 25
	}
	#undef ry
	#undef rx
}

// Copy the new column to first screen buffer

void playfield_column0(void)
{
	// Full unroll

	#assign ry 0
	#repeat		
		Screen0[40 * ry + 39] = scr_column[ry];
	#assign ry ry + 1
	#until ry == 25
	#undef ry
}

// Scroll second screen buffer to the left

void playfield_scroll1(void)
{
	for(sbyte x=12; x>=0; x--) 
	{
	#assign ry 0
	#repeat		
		#assign rx 0
		#repeat
			Screen1[40 * ry + 13 * rx + x] = Screen0[40 * ry + 13 * rx + x + 1];
		#assign rx rx + 1
		#until rx == 3
	#assign ry ry + 1
	#until ry == 25
	}
	#undef ry
	#undef rx
}

// Copy the new column to second screen buffer

void playfield_column1(void)
{
	#assign ry 0
	#repeat		
		Screen1[40 * ry + 39] = scr_column[ry];
	#assign ry ry + 1
	#until ry == 25
	#undef ry
}

// Fill the screen with the first 40 columns

void playfield_fill(void)
{
	// For each column

	for(char x=0; x<40; x++)
	{
		// Build a column

		playfield_column();

		// Copy it to screen and color ram
		for(char y=0; y<25; y++)
		{
			Screen0[40 * y + x] = scr_column[y];
			Color[40 * y + x] = col_column[y];
		}
	}
}

// Scroll color memory to the left
void playfield_scrollc(void)
{
	// First section lines 5 to 7
	for(char x=0; x<39; x++)
	{
	#assign ry 5
	#repeat		
		Color[40 * ry + x] = Color[40 * ry + x + 1];
	#assign ry ry + 1
	#until ry == 8
	}

	// Second section lines 8 to 11
	for(char x=0; x<39; x++)
	{
	#repeat		
		Color[40 * ry + x] = Color[40 * ry + x + 1];
	#assign ry ry + 1
	#until ry == 12
	}

	// Third section lines 12 to 15
	for(char x=0; x<39; x++)
	{
	#repeat		
		Color[40 * ry + x] = Color[40 * ry + x + 1];
	#assign ry ry + 1
	#until ry == 16
	}

	// Copy new colour column
	#assign ry 1
	#repeat		
		Color[40 * ry + 39] = col_column[ry];
	#assign ry ry + 1
	#until ry == 16
	#undef ry
}

// Integer square root based on Dijkstras algorithm

unsigned tsqrt(unsigned n)
{
    unsigned p, q, r, h;

    p = 0;
    r = n;

#assign q 0x4000
#repeat
    {
        h = p | q;
        p >>= 1;
        if (r >= h)
        {
            p |= q;
            r -= h;
        } 
    }
#assign q q >> 2
#until q == 0
#undef q

    return p;
}

// Integer square using table lookup

inline unsigned csquare(char c)
{
	return sqrtabl[c] | (sqrtabh[c] << 8);
}

// Signed 9 bit square using table lookup

inline unsigned usquare(int i)
{
	if (i < 0)
		return csquare(- i);
	else
		return csquare(i);
}

// Init per game data

void game_init(void)
{
	// Main game data structure parameters 
	game.level = 0;
	game.pulling = false;
	game.throw = false;

	game.shield = 0;
	game.bubble = 0;
	game.magnet = 0;

	// Init event matrix to start state, mostly empty, stars and coins

	for(char i=0; i<64; i++)
	{
		switch (i & 7)
		{
			case 0:
				eventMatrix[i] = EE_STAR_TOP;
				break;
			case 1:
				eventMatrix[i] = EE_STAR_BOTTOM;
				break;
			case 2:
				eventMatrix[i] = EE_STAR_MID;
				break;

			case 3:
			case 4:
				eventMatrix[i] = EE_COIN_TOP;
				break;
			case 5:
				eventMatrix[i] = EE_COIN_MID;
				break;
			case 6:
			case 7:
				eventMatrix[i] = EE_COIN_BOTTOM;
				break;
			default:
				eventMatrix[i] = EE_NONE;
		}
	}
}

// Check for advance in game level

void game_level(void)
{
	// Advance until we reach level 100

	if (game.level < 100)	
	{
		// Add a new event to the event matrix, based on the level

		EnemyEvent	ee = eventLevels[game.level];

		switch (ee)
		{
			case EE_NONE:
				break;

			case EE_SHRINK:
				// Shring distance between walls
				playfield.wsize--;
				break;

			case EE_FREQUENCY:
				// Increase wall frequency
				playfield.wfreq--;
				break;

			default:
				// New enemy event
				eventMatrix[frand() & 63] = ee;
		}

		// Increase level
		game.level++;

		// Increase speed every second level
		if ((game.level & 1) && playfield.vx < maxvx)
			playfield.vx++;
	}
}

// Init player data structure

void player_init(void)
{
	// Initial position and speed

	player.px = 0;
	player.py = 20 << PBITS;
	player.vx = 100 << PBITS;
	player.vy = 0;
	player.weight = 31;

	ball.px = 0 << PBITS;
	ball.py = 20 << PBITS;
	ball.vx = 0;
	ball.vy = 0;
	ball.weight = 1;

	game.pulling = false;
	game.throw = false;

	// Prepare player sprites

	xspr_image(0, 64); xspr_color(0, VCOL_ORANGE);
	xspr_image(1, 76); xspr_color(1, VCOL_MED_GREY);
	xspr_image(2, 77); xspr_color(2, VCOL_YELLOW);
	xspr_image(3, 77); xspr_color(3, VCOL_YELLOW);
	xspr_image(4, 77); xspr_color(4, VCOL_YELLOW);
}

// Put player sprites on screen

void player_show(void)
{
	// Player position has four fractional bits, so we need to shift
	// to get to pixel position

	int	pix = asr4(player.px), piy = asr4(player.py);
	int bix = asr4(ball.px), biy = asr4(ball.py);


	// Interpolate position of rings at 1/4, 1/2 and 3/4 between the two balls

	int ix2 = (pix + bix) >> 1, iy2 = (piy + biy) >> 1;
	int ix1 = (pix + ix2) >> 1, iy1 = (piy + iy2) >> 1;
	int ix3 = (ix2 + bix) >> 1, iy3 = (iy2 + biy) >> 1;

	// Set the five sprites

	xspr_move_prep(0, pix + (24 - 8), piy + (50 - 8));
	xspr_move_prep(1, bix + (24 - 12), biy + (50 - 12));
	xspr_move_prep(2, ix1 + (24 - 6), iy1 + (50 - 5));
	xspr_move_prep(3, ix2 + (24 - 6), iy2 + (50 - 5));
	xspr_move_prep(4, ix3 + (24 - 6), iy3 + (50 - 5));

	// Atomic update of MSB

	xspr_move_done();
}

// Control the player ball with the joystick

void player_control(void)
{	
	// Read joystick
	joy_poll(0);

	// Set image of ball based on direction
	if (joyx[0] < 0)
		xspr_image(0, 67 + joyy[0]);
	else if (joyx[0] > 0)
		xspr_image(0, 70 + joyy[0]);
	else if (joyy[0] < 0)
		xspr_image(0, 65);
	else if (joyy[0] > 0)
		xspr_image(0, 64);

	// Accelerate in direction of joystick
	player.vx += joyx[0] << (PBITS + 2);
	player.vy += joyy[0] << (PBITS + 3);

	// Check for button and pull in the chain if pressed, push
	// away when released

	if (joyb[0])
		game.pulling = true;
	else if (game.pulling)
	{
		game.pulling = false;
		game.throw = true;
	}
	else
		game.throw = false;
}

// Check player collisions

EnemyType player_collision(void)
{
	// Collision result
	EnemyType	explode = ET_NONE;
	bool		boing = false;

	// Check for left and right screen border, bounce
	// off when colliding

	if (player.px < (8 << PBITS))
	{
		player.px = (8 << PBITS);
		player.vx = - player.vx;
		boing = true;
	}
	else if (player.px > (312 << PBITS))
	{
		player.px = 312 << PBITS;
		player.vx = - player.vx;
		boing = true;
	}

	// Check for top and bottom border, bounce
	// off when colliding

	if (player.py < (8 << PBITS))
	{
		player.py = (8 << PBITS);
		player.vy = - player.vy;
		boing = true;
	}
	else if (player.py > (192 << PBITS))
	{
		player.py = 192 << PBITS;
		player.vy = - player.vy;
		boing = true;
	}

	// Ball position in character grid coordinates

	char	cix = (player.px - (4 << PBITS)) >> (3 + PBITS);
	char	ciy = (player.py - (4 << PBITS)) >> (3 + PBITS);

	// Check for potential character positions for collisions,
	// chars 0xe0 to 0xff are foreground wall

	bool	c00 = cscreen[40 * ciy + cix +  0] >= 0xe0;
	bool	c01 = cscreen[40 * ciy + cix +  1] >= 0xe0;
	bool	c10 = cscreen[40 * ciy + cix + 40] >= 0xe0;
	bool	c11 = cscreen[40 * ciy + cix + 41] >= 0xe0;

	// Check collision from top or bottom, collides with bottom if velocity is positive
	// and one of the bottom two characters is populated but none of the top two characters

	if ((c10 || c11) && !(c00 || c01) && player.vy > 0)
	{
		player.vy = - player.vy;
		boing = true;
	}
	else if ((c00 || c01) && !(c10 || c11) && player.vy < 0)
	{
		player.vy = - player.vy;
		boing = true;		
	}
	else
	{
		// Check for left right collision, adapt ball velocity to scrolling velocity
		// for easy calculation of reflection on moving walls, ball will accumulate
		// speed when colliding from the left

		player.vx += playfield.vx << (PBITS + VBITS - 4);
		if ((c01 || c11) && player.vx > 0)
		{
			// Explode if ball gets too fast, due to being blocked between
			// the wall and the left screen border

			if (player.vx > (16 << (PBITS + VBITS)))
				explode = ET_MINE;
			else
			{
				player.vx = -player.vx;
				boing = true;		
			}
		}
		else if ((c00 || c10) && player.vx < 0)
		{
			player.vx = -player.vx;
			if (player.vx > (16 << (PBITS + VBITS)))
				explode = ET_MINE;
			else
				boing = true;
		}

		// Readjust ball velocity
		player.vx -= playfield.vx << (PBITS + VBITS - 4);
	}

	// Sprite position of ball for collision check with enemies

	char	siy = asr4(player.py) + (50 - 8);
	int		six = asr4(player.px) + (24 - 8);


	for(char i=0; i<3; i++)
	{
		char	py = enemies[i].py;

		// Check if enemy can collect with player using behaviour flag
		// encoded in enemy type

		if (enemies[i].type & ET_PLAYER_COLLISION)
		{
			// Check for vertical overlap

			if ((char)(siy + 16) > py && siy < (char)(py + enemies[i].height))
			{

				// And horizontal overlap

				if (six + 16 > enemies[i].px && six < enemies[i].px + 16)
				{
					// Remember enemy type we collided with

					explode = enemies[i].type;
					if (explode == ET_STAR)
					{
						// Star bling bling

						enemies[i].type = ET_RISING_STAR;
						sidfx_play(2, SIDFXStar, 4);
					}
					else if (explode == ET_COIN)
					{
						// Coin katching

						enemies[i].type = ET_DROPPING_COIN;
						enemies[i].phase = 248;
						sidfx_play(2, SIDFXKatching, 5);
					}
					else if (explode == ET_POWERUP)
					{
						// Powerup yeah, type is encoded in vx field

						switch ((PowerUp)enemies[i].vx)
						{
							case PWUP_SLOWDOWN:
								// Reduce speed of scrolling
								playfield.vx = (playfield.vx + minvx) >> 1;
								break;
							case PWUP_SHIELD:
								// Init shield counter
								game.shield = 255;
								xspr_color(0, VCOL_LT_GREY);
								break;
							case PWUP_MAGNET:
								// Init magent counter
								game.magnet = 255;
								break;
							case PWUP_BUBBLE:
								// Init bubble counter
								game.bubble = 255;
								xspr_color(1, VCOL_LT_BLUE);								
								break;
						}

						// Remove powerup without a trace
						enemies[i].type = ET_NONE;
						xspr_move(i + 5, 0, 0);
					}
					else	
					{
						// Start explosion of enemy
						enemies[i].type = ET_EXPLODE;
						enemies[i].phase = 0;
					}
				}
			}
		}
	}

	// Play boing sound when bouncing

	if (explode == ET_NONE && boing)
		sidfx_play(2, SIDFXBoing, 1);

	// Return type of collision

	return explode;
}

// Check collision of ball with enemies

bool ball_collision(void)
{
	// Not passing through bottom
	if (ball.py > (192 << PBITS))
	{
		ball.py = 192 << PBITS;
		ball.vy = 0;
	}

	// Sprite position of ball
	char	biy = asr4(ball.py) + (50 - 12);
	int		bix = asr4(ball.px) + (24 - 12);

	for(char i=0; i<3; i++)
	{
		// Check if enemy collides with ball using flag in type

		if (enemies[i].type & ET_BALL_COLLISION)
		{
			char	py = enemies[i].py;

			if (biy + 20 > py && biy < (char)(py + enemies[i].height))
			{
				if (bix + 24 > enemies[i].px && bix < enemies[i].px + 24)
				{
					// Explode object, create a powerup if it was an enemy

					if (enemies[i].type & ET_LETHAL)
						enemies[i].type = ET_EXPLODE_POWERUP;
					else
						enemies[i].type = ET_EXPLODE;

					enemies[i].phase = 0;
					sidfx_play(2, SIDFXExplosion, 1);
				}
			}
		}
	}
}

// Advance player and ball position

void player_advance(void)
{
	// Velocity has 8 fractional digits, position only 4, so
	// we have to shift the velocity by four to the right
	// before adding

	player.px += asr4(player.vx);
	player.py += asr4(player.vy);

	ball.px += asr4(ball.vx);
	ball.py += asr4(ball.vy);
}

// Exploding player will not fall through the bottom

void player_exploding(void)
{
	if (player.py > (192 << PBITS))
		player.py = 192 << PBITS;
}

// Apply player and ball physics

void player_physics(void)
{
	// Apply friction to player by reducing velocity

	if (player.vx > 0)
		player.vx -= asr4(player.vx + 15);
	else if (player.vx < 0)
		player.vx -= asr4(player.vx);

	if (player.vy > 0)
		player.vy -= asr4(player.vy + 15);
	else if (player.vy < 0)
		player.vy -= asr4(player.vy);

	// Let a bit of gravity do its thing

	player.vy += 16;

	// Check if the ball is in bubble mode
	if (game.bubble)
	{
		// Bubble has significant slow down

		ball.vx -= (ball.vx + 2) >> 2;
		ball.vy -= (ball.vy + 2) >> 2;
	}
	else
	{
		// Apply friction to ball

		if (ball.vx > 0)
			ball.vx -= asr4(ball.vx + 15);
		else if (ball.vx < 0)
			ball.vx -= asr4(ball.vx);

		if (ball.vy > 0)
			ball.vy -= asr4(ball.vy + 15);
		else if (ball.vy < 0)
			ball.vy -= asr4(ball.vy);

		// Let gravity do its thing

		ball.vy += 32;
	}
}

// Apply chain physics to player and ball

void chain_physics(void)
{
	// Vector from player to ball

	int	dx = asr4(ball.px - player.px);
	int	dy = asr4(ball.py - player.py);

	// Square of length of chain

	unsigned	rq = usquare(dx) + usquare(dy);

	// Chain is not shorted

	if (rq > 0)
	{
		// Square root to get actual length of chain

		char	r = tsqrt(rq);

		// Calculate target force amount

		char	t = 0;
		if (game.pulling)
		{
			// Short chain acts as a spring when longer than 16
			if (r > 16)
				t = r - 16;
		}
		else if (game.throw)
		{
			// Throwing force
			t = 64;
		}
		else if (r > 32)
		{
			// Long chain acts a s spring when longer than 32 
			t = r - 32;
		}

		if (t)
		{
			// Calculate force vector using delta vector and force amount
			// Multiply normalized vector with force, and restore sign

			int		fx = dx < 0 ? -lmuldiv8by8(-dx, t, r) : lmuldiv8by8(dx, t, r);
			int		fy = dy < 0 ? -lmuldiv8by8(-dy, t, r) : lmuldiv8by8(dy, t, r);

			char 	weight = game.bubble ? 1 : ball.weight;

			// Weighted force based on weight of the ball

			int		bx = fx * weight;
			int		by = fy * weight;

			// Apply force to player and ball velocity

			player.vx += bx;
			player.vy += by;
			ball.vx -= fx * 32 - bx;
			ball.vy -= fy * 32 - by;
		}
	}
}

// Initialize the playfield

void playfield_init(void)
{
	// Slow start

	playfield.px = 0;
	playfield.vx = minvx;
	playfield.phase = PPHASE_IDLE;
	playfield.wsize = 12;
	playfield.wfreq = 16;

	// Empty column

	ctop = 0;
	cbottom = 25;
	csize = 0;

	bimg = 0;
	bcnt = 0;
	cimg = 0;
	cimgy = 8;
	ccnt = 0;
	cdist = 5;
	cframe = true;
	cscreen = Screen0;

	// Screen and srite colors

	vic.color_back = VCOL_DARK_GREY;
	vic.color_back1 = VCOL_LT_GREY;
	vic.color_back2 = VCOL_BLACK;

	vic.spr_mcolor0 = VCOL_BLACK;
	vic.spr_mcolor1 = VCOL_WHITE;

	vic_waitTop();
	vic_waitBottom();

	// All sprites off screen

	for(char i=0; i<8; i++)
		xspr_move(i, 0, 0);

	// Clear screen

	memset(Screen1, 0xc1, 1000);
	memset(Color, 0x08, 1000);

	// Setup display mode

	vic_waitBottom();
	vic_setmode(VICM_TEXT_MC, Screen1, Font);

	vic.ctrl2 = VIC_CTRL2_MCM | 7;
	vic.memptr = 0x38;
	rirq_data(irq_bottom, 18, 0x38);
	rirq_data(&irq_center, 0, 0x3a);

	// Init first frame

	playfield_fill();

	vic_waitTop();
	vic_waitBottom();

	// Flip screen
	vic.memptr = 0x28;
	rirq_data(irq_bottom, 18, 0x28);
	rirq_data(&irq_center, 0, 0x2a);

	// Start scrolling

	playfield_column();
	playfield_column1();
	playfield_scroll1();

	vic_waitTop();
	vic_waitBottom();
}

// Advance playfield with current speed

void playfield_advance(void)
{
	// Four fractional bits in scroll speed

	char	px = playfield.px >> 4;

	playfield.px += playfield.vx;

	// Move enemies in parallel

	enemies_scroll((char)(playfield.px >> 4) - px);
}

// Prepare scrolling of playfield

void playfield_prescroll(void)
{
	if (playfield.phase == PPHASE_SCROLLED)
	{
		// Create next column
		playfield_column();

		// Copy new column to screen
		if (cframe)
			playfield_column1();
		else
			playfield_column0();
	}
}

// Do scroll playfield

void playfield_scroll(void)
{
	// Four fractional bits, so bit 8 is a full character scrolled

	if (playfield.px & 128)
	{
		// Remove msb
		playfield.px &= 127;
		playfield.phase = PPHASE_SCROLLED;
		vic.ctrl2 = VIC_CTRL2_MCM | ((playfield.px >> 4) ^ 7);

		// Flip frame
		char	m;
		if (cframe)
		{
			m = 0x30;
			cscreen = Screen1;
		}
		else
		{
			m = 0x20;
			cscreen = Screen0;
		}
		vic.memptr = m | 0x08;
		rirq_data(irq_bottom, 18, m | 0x08);
		rirq_data(&irq_center, 0, m | 0x0a);

		cframe = !cframe;

		// Scroll color ram
		playfield_scrollc();
	}
	else
	{
		// Scrolling only on pixel level

		vic.ctrl2 = VIC_CTRL2_MCM | ((playfield.px >> 4) ^ 7);

		// Scroll the double buffer now, if not already scrolled

		if (playfield.phase == PPHASE_SCROLLED)
		{
			playfield.phase = PPHASE_COPIED;
			if (cframe)
				playfield_scroll1();
			else
				playfield_scroll0();
		}
		else
		{
			playfield.phase = PPHASE_IDLE;
			vic_waitTop();
		}
	}
}

// Advance game state
void game_state(GameState state)
{
	
	do
	{
		// Set new state
		game.state = state;

		switch (state)
		{
		case GS_TITLE:
			titlescreen_show();
			state = GS_START;
			break;

		case GS_START:

			// Start the ingame music

			music_init(0);
			music_patch_voice3(false);

			sidfx_init();

			math_init();

			playfield_init_font();

			playfield_initirqs();

			xspr_init();

			score_init();

			// Switch screen
			vic_waitBottom();
			state = GS_READY;
			break;

		case GS_READY:
			game_init();
			score_init();
			playfield_init();
			enemies_init();
			game.count = 120;
			break;

		case GS_PLAYING:
			player_init();
			game.count = 150;
			break;

		case GS_EXPLODING:
			player.vx >>= 4;
			game.count = 0;
			sidfx_play(2, SIDFXPlayerExplosion, 4);
			break;

		case GS_GAME_OVER:
			if (highscore_show())
				state = GS_START;
			else
				state = GS_TITLE;
			break;
		}
	} while (state != game.state);
}

byte rirq_pcount;

// Work for current frame
void game_loop()
{
	EnemyType	et;

	switch (game.state)
	{
	case GS_START:
		break;

	case GS_READY:
		// Wait for game to start
		vic_waitBottom();
		vic_waitTop();

		if (!--game.count)
			game_state(GS_PLAYING);
		break;
	case GS_PLAYING:		

		// Prepare scrolling

		playfield_prescroll();

//		if (!ntsc)
		{
			while ((vic.ctrl1 & VIC_CTRL1_RST8) || vic.raster < 58)
				;
		}

		// Move enemies

		playfield_advance();

		// Throttle counters in NTSC mode

		if (physics56 != 4)
		{
			// Advance game level every 3 seconds

			if (!--game.count)
			{
				game_level();
				game.count = 150;
			}

			// Decrease bonus effects with half frame rate

			if (game.count & 1)
			{
				if (game.magnet)
					game.magnet--;
				if (game.shield)
				{
					game.shield--;
					if (game.shield < 32)
						xspr_color(0, game.shield & 2 ? VCOL_LT_GREY : VCOL_ORANGE);
				}
				if (game.bubble)
				{
					game.bubble--;
					if (game.bubble < 32)
						xspr_color(1, game.bubble & 2 ? VCOL_LT_BLUE : VCOL_MED_GREY);			
				}
			}
		}

		// Wait for frame to have passed

		while (rirq_count == rirq_pcount)
			;

		rirq_pcount = rirq_count;

		while (!(vic.ctrl1 & VIC_CTRL1_RST8) && (char)(vic.raster - 40) < 208)
			;

		// Show player sprite
		player_show();

		// Scroll the playfield

		playfield_scroll();

		// Move the player

		if (physics56 != 3)		
		{
			player_control();
			player_advance();
		}

		// Check for collisions

		ball_collision();

		et = player_collision();

		if (game.shield && (et & ET_LETHAL))
		{
			game.shield = 0;
			et = ET_NONE;
			xspr_color(0, VCOL_ORANGE);
		}

		if (et & ET_LETHAL)
			game_state(GS_EXPLODING);
		else
		{
			if (et == ET_STAR)
				star_inc();
			else if (et == ET_COIN)
				score_add(100);

			// Apply game physics

			if (physics56 == 6)
				physics56 = 0;
			else
			{
				player_physics();
				chain_physics();
			}
		}

		if (ntsc)
			physics56++;

		break;

	case GS_EXPLODING:

		vic_waitBottom();

		player_show();

		enemies_scroll(0);

		vic_waitTop();

		// Some phyiscs continues

		player_advance();
		ball_collision();
		player_exploding();
		player_physics();

		// Explosion animation

		game.count++;
		if (game.count < 32)
		{
			xspr_image(0, 72 + (game.count >> 3));
		}
		else if (game.count < 128)
		{		
			xspr_move(0, 0, 0);
			if (game.count > 64 && game.count <= 96)
				music_patch_volume((96 - game.count) >> 1);
		}
		else
		{
			// And off to the game voer screen

			sid.voices[0].ctrl = 0;
			sid.voices[1].ctrl = 0;
			sid.fmodevol = 15;
			music_patch_volume(15);

			xspr_move(1, 0, 0);
			game_state(GS_GAME_OVER);
		}
		break;
	case GS_GAME_OVER:
		break;
	}
}

// Initial blank screen animation

void black_screen(void)
{
	vic_waitBottom();

	// Close the blinds

	for(char i=0; i<100; i+=4)
	{
		vic_waitTop();
		vic_waitLine(50 + i);
		vic.color_back = VCOL_BLUE;
		vic_waitLine(250 - i);
		vic.color_back = VCOL_LT_BLUE;
		vic_waitBottom();
	}

	vic.ctrl1 = VIC_CTRL1_RST8;
}

int main(void)
{
	// Turn off CIA interrupts
	cia_init();

	__asm { sei }

	// Clear screen

	black_screen();

	zseed = 31232;

	// Check for NTSC mode, wait for beam to reach bottom

	vic_waitTop();
	vic_waitBottom();

	// Now find max raster line with MSB set to 1

	char	max = 0;
	while (vic.ctrl1 & VIC_CTRL1_RST8)
	{
		if (vic.raster > max)
			max = vic.raster;
	}

	// More than 8 lines, must be PAL

	ntsc = max < 8;
	minvx = ntsc ? 13 : 16;
	maxvx = ntsc ? 53 : 64;


	// Copy static spriteset under IO, freeing 0xc000..0xcfff
	mmap_set(MMAP_CHAR_ROM);

	memcpy(Sprites, spriteset, 4096);

	mmap_set(MMAP_NO_ROM);

	rirq_init(false);

	vic.color_border = VCOL_BLACK;
	vic.color_back = VCOL_DARK_GREY;
	vic.color_back1 = VCOL_LT_GREY;
	vic.color_back2 = VCOL_BLACK;

	vic.spr_mcolor0 = VCOL_BLACK;
	vic.spr_mcolor1 = VCOL_WHITE;

	sid.fmodevol = 15;

	tileset_init();

	// Start the game

	game_state(GS_TITLE);

	// Main game loop
	for(;;)		
	{	
		// Increment score 50 times per second
		if (physics56 != 2)	
			score_inc();

		// Advance sound effects
		sidfx_loop_2();

		// Advance game
		game_loop();

	}

	return 0;
}
