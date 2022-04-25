#include <c64/joystick.h>
#include <c64/vic.h>
#include <c64/sprites.h>
#include <c64/memmap.h>
#include <c64/rasterirq.h>
#include <c64/sid.h>
#include <c64/cia.h>
#include <math.h>
#include <fixmath.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <audio/sidfx.h>

#pragma region( main, 0x0a00, 0x9e00, , , {code, data, bss, heap} )
#pragma region( stack, 0x9e00, 0xa000, , , {stack})

#pragma section( xbss, 0, , , bss)

#pragma region( xbss, 0x0400, 0x0800, , , {xbss})

#pragma section( music, 0)

#pragma region( music, 0xa000, 0xc000, , , {music} )

#pragma section( spriteset, 0)

#pragma region( spriteset, 0xc000, 0xd000, , , {spriteset} )

#pragma section( tables, 0, , , bss)

#pragma region( tables, 0xf000, 0xff00, , , {tables})

#pragma data(music)

__export char music[] = {
	#embed 0x2000 0x88 "GameScene.sid"
}

#pragma data(spriteset)

char spriteset[4095] = {
	#embed 4095 0 "ballnchain - Sprites.bin"
};

#pragma data(data)

char charset_center[] = {
	#embed rle "ballnchain_center - Chars.bin"
};

char tileset_center[] = {
	#embed "ballnchain_center - Tiles.bin"	
}

char charattribs_center[] = {
	#embed "ballnchain_center - CharAttribs_L1.bin"	
}

char charset_bottom[] = {
	#embed rle "ballnchain_bottom - Chars.bin"
};

char tileset_bottom[] = {
	#embed "ballnchain_bottom - Tiles.bin"	
}

char charattribs_bottom[] = {
	#embed "ballnchain_bottom - CharAttribs_L1.bin"	
}

char charset_front[] = {
	#embed "ballnchain_front - Chars.bin"
};

char charset_digits[] = {
	#embed "ballnchain_digits - Chars.bin"
};

char titlescreen[] = {
	#embed 8000    0 rle "titlesketch.bin"
	#embed 1000 8000 rle "titlesketch.bin"
	#embed 1000 9000 rle "titlesketch.bin"
}

byte * const Screen0 = (byte *)0xc800;
byte * const Screen1 = (byte *)0xcc00;

byte * const Font = (byte *)0xe000;
byte * const FontBottom = (byte *)0xe800;
byte * const Color = (byte *)0xd800;
byte * const Sprites = (byte *)0xd000;

byte * const DynSprites = (byte *)0xc000;

#pragma bss(tables)

byte	asrtab4[256], lsrtab4[256], asltab4[256];
byte	sqrtabl[256], sqrtabh[256];

#pragma bss(bss)

#pragma align(asrtab4, 256)
#pragma align(lsrtab4, 256)
#pragma align(asltab4, 256)
#pragma align(sqrtabl, 256)
#pragma align(sqrtabh, 256)

bool	cframe;
byte	*	cscreen;

char		score[9];
char		nstars;
unsigned	scorecnt;

#pragma bss(xbss)

RIRQCode		irq_title[7], irq_title_hide, irq_title_show, irq_title_music;

char irq_title_i;
char irq_title_c;
char irq_title_y[7];
char irq_title_s[7];
char irq_title_x[4][7];
char irq_title_msbx[7];


byte * const DynSpriteChars[12] = {
	DynSprites +   0, DynSprites +   1, DynSprites +   2,
	DynSprites + 128, DynSprites + 129, DynSprites + 130,
	DynSprites + 256, DynSprites + 257, DynSprites + 258,
	DynSprites + 384, DynSprites + 385, DynSprites + 386
};

char	scr_column[25], col_column[25], scr_row[40];
char	ctop, cbottom ,csize, bimg, bcnt, cimg, ccnt, cdist, cimgy;
char	nrows;

const char * 	title_tp;
char			title_sy, title_ky, title_ty, title_by[8];
char			title_delay;
int				title_bx, title_cy;

sbyte	title_vx[7];
int		title_px[7];

RIRQCode20		irq_top20, irq_bottom20;
RIRQCode		irq_center, irq_music;

RIRQCode	* const	irq_top = &irq_top20.c;
RIRQCode	* const	irq_bottom = &irq_bottom20.c;


#pragma bss(bss)

static sbyte batyspeed[32] = {
	-1, -1, -1, -2, -2, -3, -2, -2, -1, -1, -1, 0, 0, 0, 0, 0,
	 1,  1,  1,  2,  2,  3,  2,  2,  1,  1,  1, 0, 0, 0, 0, 0
};

static char mineflash[16] = {
	VCOL_RED, VCOL_RED, VCOL_RED, VCOL_RED,
	VCOL_ORANGE, VCOL_YELLOW, VCOL_WHITE, VCOL_ORANGE,
	VCOL_RED, VCOL_RED, VCOL_RED, VCOL_RED,
	VCOL_RED, VCOL_RED, VCOL_RED, VCOL_RED	
};


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

void music_init(char tune)
{
	__asm
	{
		lda		tune
		jsr		$a000
	}
}

void music_play(void)
{
	__asm
	{
		jsr		$a003
	}
}

void music_patch_volume(char vol)
{
	*(char *)0xa103 = vol;	
}

void music_patch_voice3(bool enable)
{
	*(char *)0xa10c = enable ? 0x20 : 0x4c;
}

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

__interrupt void titlescreen_irq(void)
{
	char y = irq_title_y[irq_title_i];
	char s = irq_title_s[irq_title_i];

	vic.spr_pos[0].y = y;		
	vic.spr_pos[1].y = y;		
	vic.spr_pos[2].y = y;		
	vic.spr_pos[3].y = y;

	vic.spr_pos[4].y = y - 1;
	vic.spr_pos[5].y = y - 1;	
	vic.spr_pos[6].y = y - 1;		
	vic.spr_pos[7].y = y - 1;

	Screen1[0x3f8 + 0] = s + 0;
	Screen1[0x3f8 + 4] = s + 1;
	Screen1[0x3f8 + 1] = s + 2;
	Screen1[0x3f8 + 5] = s + 3;
	Screen1[0x3f8 + 2] = s + 4;
	Screen1[0x3f8 + 6] = s + 5;
	Screen1[0x3f8 + 3] = s + 6;
	Screen1[0x3f8 + 7] = s + 7;
}

__interrupt void titlescreen_irqx(void)
{
	char y = irq_title_y[irq_title_i];

	vic.spr_pos[0].y = y;		
	vic.spr_pos[1].y = y;
	vic.spr_pos[2].y = y;		
	vic.spr_pos[3].y = y;

	vic.spr_pos[4].y = y - 1;
	vic.spr_pos[5].y = y - 1;	
	vic.spr_pos[6].y = y - 1;		
	vic.spr_pos[7].y = y - 1;

	vic.spr_pos[4].x = irq_title_x[0][irq_title_i];
	vic.spr_pos[0].x = irq_title_x[0][irq_title_i] + 1;
	vic.spr_pos[5].x = irq_title_x[1][irq_title_i];
	vic.spr_pos[1].x = irq_title_x[1][irq_title_i] + 1;
	vic.spr_pos[6].x = irq_title_x[2][irq_title_i];
	vic.spr_pos[2].x = irq_title_x[2][irq_title_i] + 1;		
	vic.spr_pos[7].x = irq_title_x[3][irq_title_i];
	vic.spr_pos[3].x = irq_title_x[3][irq_title_i] + 1;
	vic.spr_msbx = irq_title_msbx[irq_title_i];

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

const char * rle_decode(char * dp, const char * sp)
{
	char cmd = sp[0];

	do
	{
		if (cmd & 0x80)
		{
			char rep = (cmd & 0x70) >> 4;
			char	c = sp[1];
			for(sbyte i=rep; i>=0; i--)
				dp[i] = c;

			rep++;
			sp += 2;
			dp += rep;

			cmd &= 0x0f;
			for(sbyte i=cmd; i>=0; i--)
				dp[i] = sp[i];

			cmd++;
			sp += cmd;
			dp += cmd;
		}
		else if (cmd & 0x40)
		{
			cmd &= 0x3f;
			sp ++;
			for(sbyte i=cmd; i>=0; i--)
				dp[i] = sp[i];

			cmd++;
			sp += cmd;
			dp += cmd;
		}
		else
		{
			char	c = sp[1];
			for(sbyte i=cmd; i>=0; i--)
				dp[i] = c;

			cmd++;
			sp += 2;
			dp += cmd;
		}

		cmd = sp[0];

	} while (cmd)

	return sp + 1;
}

void titlescreen_char(char x, char y, char c)
{
	char * dp = DynSpriteChars[x];
	if (y & 1)
		dp += 36;
	dp += (y >> 1) * (8 * 64);

	const char * sp = charset_digits + 8 * (c & 0x3f);

	char a, b, c;

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

void titlescreen_line(char y, const char * tp)
{
	for(char i=0; i<12; i++)
		titlescreen_char(i, y, tp[i]);
}

void titlescreen_spaces(char y)
{
	for(char i=0; i<12; i++)
		titlescreen_char(i, y, ' ');
}

void titlescreen_string(char x, char y, const char * p)
{
	while (*p)
		titlescreen_char(x++, y, *p++);
}

void titlescreen_scroll_init(void)
{
	vic.spr_enable = 0xff;
	vic.spr_multi = 0x00;
	vic.spr_expand_x = 0xff;
	vic.spr_expand_y = 0x00;

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

	rirq_build(&irq_title_show, 2);
	rirq_write(&irq_title_show, 0, &vic.spr_msbx, 0x00);
	rirq_write(&irq_title_show, 1, &vic.spr_expand_x, 0xff);
	rirq_set(7, 116, &irq_title_show);

	rirq_build(&irq_title_hide, 2);
	rirq_write(&irq_title_hide, 0, &vic.spr_msbx, 0xff);
	rirq_write(&irq_title_hide, 1, &vic.spr_expand_x, 0x00);
	rirq_set(8, 250, &irq_title_hide);

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

	title_tp = title_text;
	title_sy = 0;
	title_ky = 0;
	title_delay = 0;
	title_ty = 0;
}


bool titlescreen_scroll_step(void)
{
	title_delay++;
	if (title_delay == 3)
	{
		title_delay = 0;

		title_sy++;
		char	ky = title_ky;
		if (title_sy == 25)
		{
			title_ky++;
			if (title_ky == 6)
				title_ky = 0;	
			title_sy = 0;
		}

		char j = title_ky;
		for(char i=0; i<7; i++)
		{
			unsigned	ty =  116 - title_sy + 25 * i;
			if (ty < 245)
				rirq_set(i, i ? ty : 80, irq_title + i);
			else
				rirq_clear(i);

			irq_title_y[i] = ty + 4;
			irq_title_s[i] = 8 * j;
			j++;
			if (j == 6)
				j = 0;
		}

		rirq_sort();

		if (title_sy == 15)
		{
			if (title_tp[0])
			{
				titlescreen_line(2 * ky, title_tp);
				title_tp += 12;
			}
			else
			{
				titlescreen_spaces(2 * ky);
			}
		}
		else if (title_sy == 0)
		{
			if (title_tp[0])
			{
				titlescreen_line(2 * ky + 1, title_tp);
				title_tp += 12;
			}
			else
			{
				titlescreen_spaces(2 * ky + 1);
				title_ty++;
			}


		}
	}

	return title_ty < 6;
}

void titlescreen_scroll_clear(void)
{
	vic.spr_enable = 0x00;

	for(char i=0; i<10; i++)
		rirq_clear(i);
	rirq_sort();

	title_delay = 0;	
}


void titlescreen_highscore_init(void)
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

	for(char i=0; i<6; i++)
	{
		rirq_build(irq_title + i, 2);
		rirq_write(irq_title + i, 0, &irq_title_i, i);
		rirq_call(irq_title + i, 1, titlescreen_irqx);
		rirq_set(i, 105 + 25 * i, irq_title + i);

		irq_title_y[i] = 110 + 25 * i;
		irq_title_s[i] = 8 * i;
		irq_title_msbx[i] = 0xff;
		irq_title_x[0][i] = 254;
		irq_title_x[1][i] = 254;
		irq_title_x[2][i] = 254;
		irq_title_x[3][i] = 254;

		title_px[i] = 4 * (- 24 - 48 * (i + 3));
		title_vx[i] = 64;
	}

	// sort the raster IRQs
	rirq_sort();

	titlescreen_string(2, 0, "HIGHSCORE");

	vic.spr_enable = 0xff;

	title_delay = 0;
	title_ty = 0;
}

bool titlescreen_highscore_step(void)
{
	title_delay++;
	if (!title_delay)
		title_ty++;

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

	for(char i=0; i<6; i++)
	{
		int			x = 2 * (title_px[i] >> 2);
		char		msb = 0;

		for(char j=0; j<4; j++)
		{
			if (x < -24 || x > 344)
			{
				irq_title_x[j][i] = 254;
				msb |= 0x11 << j;				
			}
			else if (x < 0)
			{
				irq_title_x[j][i] = x - 8;
				msb |= 0x11 << j;
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

		title_px[i] += title_vx[i] >> 2;

		if (title_ty == 0)
		{
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
			if (i < (title_delay >> 3))
			{
				if (title_vx[i] < 64)
					title_vx[i]++;
			}
		}
	}

	return title_ty < 3;
}

void titlescreen_highscore_clear(void)
{
	vic.spr_enable = 0x00;

	for(char i=0; i<10; i++)
		rirq_clear(i);
	rirq_sort();	

	title_delay = 0;
}

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

	for(char i=0; i<2; i++)
	{
		rirq_build(irq_title + i, 2);
		rirq_write(irq_title + i, 0, &irq_title_i, i);
		rirq_call(irq_title + i, 1, titlescreen_irqx);
		rirq_set(i, 105 + 25 * i, irq_title + i);

		irq_title_y[i] = 110 + 25 * i;
		irq_title_s[i] = 8 * i;
		irq_title_msbx[i] = 0xff;
		irq_title_x[0][i] = 254;
		irq_title_x[1][i] = 254;
		irq_title_x[2][i] = 254;
		irq_title_x[3][i] = 254;
	}

	// sort the raster IRQs
	rirq_sort();

	vic.spr_enable = 0xff;

	title_delay = 0;
	title_ty = 0;
	title_sy = 0;
	title_tp = credits_text;
}

bool titlescreen_credits_step(void)
{
	switch (title_ty)
	{
	case 0:
	case 2:
	case 4:
	case 6:
	case 8:
		if (title_sy & 1)
		{
			for(char j=0; j<12; j++)
				titlescreen_char(j, title_sy >> 1, title_tp[j]);
			title_tp += 12;
		}

		title_sy ++;
		if (title_sy == 8)
		{
			title_ty++;
			title_sy = 0;			

			char	k = title_ty >> 1;
			title_bx = 2 * credits[k].px;
			title_cy = 2 * credits[k].py;

			vic.spr_color[0] = 
			vic.spr_color[1] = 
			vic.spr_color[2] = 
			vic.spr_color[3] = credits[k].color;
		}
		break;
	case 1:
	case 3:
	case 5:
	case 7:
	case 9:
	{
		char	k = title_ty >> 1;
		sbyte	dx = credits[k].dx, dy = credits[k].dy;

		title_bx += dx;
		title_cy += dy;

		if (dx < 0 && title_bx < -408 || dx > 0 && title_bx >= 688 ||
			dy < 0 && title_cy < -20 || dy > 0 && title_cy >= 490)
		{
			title_ty++;
		}
		else
		{
			for(char i=0; i<2; i++)
			{
				int			x = title_bx >> 1;
				char		msb = 0;

				for(char j=0; j<4; j++)
				{
					if (x < -24 || x > 344)
					{
						irq_title_x[j][i] = 254;
						msb |= 0x11 << j;				
					}
					else if (x < 0)
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

				unsigned	ty =  (title_cy >> 1) + 25 * i;
				if (ty > 0 && ty < 245)
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

void titlescreen_credits_clear(void)
{
	vic.spr_enable = 0x00;

	for(char i=0; i<10; i++)
		rirq_clear(i);
	rirq_sort();	

	title_delay = 0;
}

static char batyoffset[8] = {30, 20, 0, 40, 10, 37, 90, 42};

static sbyte sintab64[64] = {
	0, 6, 12, 19, 24, 30, 36, 41, 45, 49, 53, 56, 59, 61, 63, 64, 
	64, 64, 63, 61, 59, 56, 53, 49, 45, 41, 36, 30, 24, 19, 12, 6, 
	0, -6, -12, -19, -24, -30, -36, -41, -45, -49, -53, -56, -59, -61, -63, -64, 
	-64, -64, -63, -61, -59, -56, -53, -49, -45, -41, -36, -30, -24, -19, -12, -6
};

void titlescreen_bats_init(void)
{
	spr_init(Screen1);

	vic.spr_mcolor0 = VCOL_BLACK;
	vic.spr_mcolor1 = VCOL_WHITE;

	for(char i=0; i<8; i++)
	{
		spr_set(i, true, 400, 0, 112, VCOL_BROWN, true, false, false);		
		title_by[i] = 90 + batyoffset[i];
	}

	title_delay = 0;
	title_bx = 360;
}

bool titlescreen_bats_step(void)
{
	title_bx--;

	for(char i=0; i<8; i++)
	{
		char k = title_delay + 4 * i;

		title_by[i] += batyspeed[(k & 63) >> 1];
		spr_move(i, title_bx + 20 * i, title_by[i]);
		spr_image(i, 112 + ((k & 15) >> 2));
	}

	title_delay++;

	return title_bx > -150;
}

void titlescreen_bats_clear(void)
{
	vic.spr_enable = 0x00;
	title_delay = 0;
}

struct TitleBall
{
	int	px, py, dx, dy;
}	titleBalls[8];

void titlescreen_balls_init(void)
{
	spr_init(Screen1);

	vic.spr_mcolor0 = VCOL_BLACK;
	vic.spr_mcolor1 = VCOL_WHITE;

	for(char i=0; i<8; i++)
	{
		spr_set(i, true, 400, 0, 64 + i, VCOL_ORANGE, true, false, false);

		if (i & 1)
		{
			titleBalls[i].px = 0;
			titleBalls[i].dx = (rand() & 31) + 8;
		}
		else
		{
			titleBalls[i].px = 344 * 16;
			titleBalls[i].dx = - 8 - (rand() & 31);
		}

		titleBalls[i].py = (rand() & 1023) + 800;
		titleBalls[i].dy = 0;
	}

	title_delay = 0;
}

bool titlescreen_balls_step(void)
{
	for(char i=0; i<8; i++)
	{
		int	dx = titleBalls[i].dx;
		int dy = titleBalls[i].dy;

		dy += 2;

		int x = titleBalls[i].px + dx;
		int	y = titleBalls[i].py + dy;

		if (dy > 0 && y > 234 * 16)
		{
			dy = 5 - dy;
			y += dy;
		}

		if (dx < 0 && x < 0)
		{
			x = 344 * 16;
			dx = - 8 - (rand() & 31);
			y = (rand() & 1023) + 800;
			dy = 0;

			if (++title_delay >= 32)
				spr_show(i, false);
		}
		else if (dx > 0 && x > 344 * 16)
		{
			x = 0;
			dx = (rand() & 31) + 8;
			y = (rand() & 1023) + 800;
			dy = 0;						

			if (++title_delay >= 32)
				spr_show(i, false);
		}

		titleBalls[i].dx = dx;
		titleBalls[i].dy = dy;

		titleBalls[i].px = x;
		titleBalls[i].py = y;

		spr_move(i, x >> 4, y >> 4);
	}

	return vic.spr_enable != 0;
}

void titlescreen_balls_clear(void)
{
	vic.spr_enable = 0x00;
	title_delay = 0;
}

void titlescreen_chase_init(void)
{
	spr_init(Screen1);

	vic.spr_mcolor0 = VCOL_BLACK;
	vic.spr_mcolor1 = VCOL_WHITE;

	spr_set(0, true, 400, 0, 68, VCOL_ORANGE, true, false, false);		
	spr_set(1, true, 400, 0, 76, VCOL_MED_GREY, true, false, false);

	for(char i=0; i<3; i++)
	{
		spr_set(2 + i, true, 400, 0, 77, VCOL_YELLOW, true, false, false);
		spr_set(5 + i, true, 400, 0, 120, VCOL_LT_BLUE, true, false, false);
	}

	title_delay = 0;
	title_bx = 320;
	title_ty = 0;
}

bool titlescreen_chase_step(void)
{
	title_delay++;

	int	pix, piy, bix, biy, gix, giy;
	int	ix1, ix2, ix3, iy1, iy2, iy3;

	if (title_ty == 0)
	{
		title_bx--;

		pix = title_bx;
		piy = 90 + (sintab64[(title_delay * 3) & 63] >> 4);
		bix = title_bx + 50 + (sintab64[title_delay & 63] >> 2);
		biy = 90 + 20 + (sintab64[(title_delay * 4) & 63] >> 5);

		gix = title_bx + 80;
		giy = 90 - 10;

		ix2 = (pix + bix) >> 1; iy2 = (piy + biy) >> 1;
		ix1 = (pix + ix2) >> 1; iy1 = (piy + iy2) >> 1;
		ix3 = (ix2 + bix) >> 1; iy3 = (iy2 + biy) >> 1;

		if (title_bx < -160)
		{
			title_ty++;
			spr_image(0, 64);
		}
	}
	else if (title_ty == 1)
	{
		title_bx++;

		pix = title_bx;
		piy = 90 + (sintab64[(title_delay * 3) & 63] >> 4);
		bix = pix + (sintab64[title_delay & 63] >> 1);
		biy = piy - (sintab64[(title_delay + 16) & 63] >> 1);

		gix = title_bx + 80;
		giy = 90 - 10;

		ix2 = (pix + bix) >> 1; iy2 = (piy + biy) >> 1;
		ix1 = (pix + ix2) >> 1; iy1 = (piy + iy2) >> 1;
		ix3 = (ix2 + bix) >> 1; iy3 = (iy2 + biy) >> 1;

		if (title_bx > 330)
		{
			title_ty++;
		}
	}

	spr_move(0, pix + (24 - 8), piy + (50 - 8));
	spr_move(1, bix + (24 - 12), biy + (50 - 12));
	spr_move(2, ix1 + (24 - 6), iy1 + (50 - 5));
	spr_move(3, ix2 + (24 - 6), iy2 + (50 - 5));
	spr_move(4, ix3 + (24 - 6), iy3 + (50 - 5));

	spr_move(5, gix + (24 - 6) +  0, giy + (50 - 5) + 10 + (sintab64[title_delay & 63] >> 4));
	spr_move(6, gix + (24 - 6) + 30, giy + (50 - 5) + 30 + (sintab64[(title_delay + 7) & 63] >> 4));
	spr_move(7, gix + (24 - 6) + 55, giy + (50 - 5) + 20 + (sintab64[(title_delay + 29) & 63] >> 4));

	spr_image(5, 120 + ((title_delay & 15) >> 2));
	spr_image(6, 120 + ((title_delay & 15) >> 2));
	spr_image(7, 120 + ((title_delay & 15) >> 2));


	return title_ty < 2;

}

void titlescreen_chase_clear(void)
{
	vic.spr_enable = 0x00;
	title_delay = 0;
}


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

void titlescreen_show(void)
{
	music_patch_voice3(true);
	music_init(1);

	const char * sp = titlescreen;

	sp = rle_decode(Font, sp);
	sp = rle_decode(Screen1, sp);
	sp = rle_decode(Color, sp);

	vic.spr_enable = 0x00;

	vic_setmode(VICM_HIRES_MC, Screen1, Font);
	vic.color_border = VCOL_BLACK;
	vic.color_back = VCOL_BLACK;

	memset(DynSprites, 0x00, 48 * 64);

	title_delay = 0;
	TitleScreenAnim	anim = TSA_INTRO_TEXT_0;

	rirq_sort();

	// start raster IRQ processing
	rirq_start();

	do {
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

		vic_waitTop();
		music_play();

		while (vic.raster < 150)
			;

		music_play();

		joy_poll(0);
	} while (!joyb[0]);

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

	for(int i=0; i<256; i++)
		Screen0[i] = darker[i & 0x0f] | (darker[(i & 0xf0) >> 4] << 4);

	for(char i=0; i<4; i++)
	{
		char j = 0;
		do
		{
			Color  [j + 0x000] = Screen0[Color  [j + 0x000]];
			Color  [j + 0x100] = Screen0[Color  [j + 0x100]];
			Color  [j + 0x200] = Screen0[Color  [j + 0x200]];
			Color  [j + 0x300] = Screen0[Color  [j + 0x300]];
			j++;
		} while (j)

		vic_waitFrame();

		sid.fmodevol = 14 - 4 * i;

		j = 0;
		do
		{
			Screen1[j + 0x000] = Screen0[Screen1[j + 0x000]];
			Screen1[j + 0x100] = Screen0[Screen1[j + 0x100]];
			Screen1[j + 0x200] = Screen0[Screen1[j + 0x200]];
			Screen1[j + 0x300] = Screen0[Screen1[j + 0x300]];
			j++;
		} while (j)

		vic_waitFrame();

		sid.fmodevol = 12 - 4 * i;

	}

	vic_waitBottom();
	vic.ctrl1 = VIC_CTRL1_RST8;

	sid.voices[0].ctrl = 0;
	sid.voices[1].ctrl = 0;
	sid.voices[2].ctrl = 0;
}

void column_down(void)
{
	for(char i=0; i<5; i++)
	{
		char j = (rand() >> 8) & 15;
		char x = scr_row[j];
		while (j < 39)
		{
			scr_row[j] = scr_row[j + 1];
			j++;
		}
		scr_row[39] = x;

	#assign yi 23
	#repeat
		Screen1[yi * 40 + 40 + x] = Screen1[yi * 40 + x];
		Color[yi * 40 + 40 + x] = Color[yi * 40 + x];
	#assign yi yi - 1
	#until yi < 0
	#undef yi
		Screen1[x] = 0xc1;
	}
}

bool highscore_greater(char n)
{
	for(char k=0; k<6; k++)
		if (score[k] > highscores[n].score[k])
			return true;
		else if (score[k] < highscores[n].score[k])
			return false;

	return false;
}

static const char greys[4] = {
	VCOL_LT_GREY,
	VCOL_MED_GREY,
	VCOL_DARK_GREY,
	VCOL_BLACK
};

bool highscore_show(void)
{
	music_patch_voice3(true);

	char hi = 5;
	while (hi > 0 && highscore_greater(hi - 1))
	{
		if (hi != 5)
			highscores[hi] = highscores[hi - 1];
		hi--;
	}
	if (hi != 5)
	{
		for(char i=0; i<6; i++)
			highscores[hi].score[i] = score[i];
		for(char i=0; i<3; i++)		
			highscores[hi].name[i] = highscoreName[i];
	}

	if (hi == 5)		
		music_init(rand() & 1 ? 3 : 4);
	else
		music_init(rand() & 1 ? 2 : 5);

	if (cframe)
		memcpy(Screen1, Screen0, 1000);

	vic_waitBottom();

	vic.spr_enable = 0x00;

	rirq_wait();

	for(char i=0; i<10; i++)
		rirq_clear(i);

	rirq_build(&irq_title_show, 1);
	rirq_write(&irq_title_show, 0, &vic.memptr, 0x3a);
	rirq_set(6, 178, &irq_title_show);

	rirq_build(&irq_title_hide, 1);
	rirq_write(&irq_title_hide, 0, &vic.memptr, 0x38);
	rirq_set(7, 250, &irq_title_hide);

	vic.spr_multi = 0x00;
	vic.spr_expand_x = 0xff;
	vic.spr_expand_y = 0x00;

	for(char i=0; i<4; i++)
	{
		vic_sprxy(i    , 88 + 48 * i, 110);
		vic_sprxy(i + 4, 87 + 48 * i, 110);
		vic.spr_color[i] = VCOL_WHITE;
		vic.spr_color[i + 4] = VCOL_BLACK;
	}

	irq_title_c = VCOL_BLACK;

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

	memset(DynSprites, 0x00, 48 * 64);	

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

	for(char i=0; i<40; i++)
		scr_row[i] = i;

	title_delay = 50;
	title_ty = 20;

	char	hix = 0;
	char	hic = 0;

	bool	down = false;
	bool	restart = false;

	do {			
		vic_waitTop();
		music_play();

//		vic.color_border++;
		column_down();
//		vic.color_border--;

		while (vic.raster < 150)
			;

		music_play();

//		vic.color_border++;
		column_down();
//		vic.color_border--;

		vic_waitBottom();

		joy_poll(0);
		if (joyx[0] || joyy[0] || joyb[0])
		{
			title_delay = 50;
			title_ty = 20;
		}

		if (hi < 5)
		{
			if ((hic & 0x1f) == 0x00)
				titlescreen_char(hix, 2 * hi + 2, 0x1c);
			else if ((hic & 0x1f) == 0x10)
				titlescreen_char(hix, 2 * hi + 2, highscores[hi].name[hix]);

			hic++;
			if (hic == 0x20)
				hic = 0x00;

			if (hic < 0x80)
			{
				if (joyy[0] > 0)
				{
					if (highscores[hi].name[hix] == 'A')
						highscores[hi].name[hix] = 'Z';
					else
						highscores[hi].name[hix]--;
					hic = 0xf0;
				}
				else if (joyy[0] < 0)
				{
					if (highscores[hi].name[hix] == 'Z')
						highscores[hi].name[hix] = 'A';
					else
						highscores[hi].name[hix]++;
					hic = 0xf0;
				}
				else if (hix < 2 && (joyb[0] && !down || joyx[0] > 0))
				{
					titlescreen_char(hix, 2 * hi + 2, highscores[hi].name[hix]);
					hix++;
					hic = 0xe0;
				}
				else if (hix > 0 && joyx[0] < 0)
				{
					titlescreen_char(hix, 2 * hi + 2, highscores[hi].name[hix]);
					hix--;
					hic = 0xe0;
				}
				else if (joyb[0] && !down)
				{
					titlescreen_char(hix, 2 * hi + 2, highscores[hi].name[hix]);
					for(char i=0; i<3; i++)		
						highscoreName[i] = highscores[hi].name[i];
					hi = 0xff;
				}
			}
			else if (joyx[0] == 0 && joyy[0] == 0)
				hic &= 0x1f;

		}
		else if (joyb[0] && !down)
		{
			restart = true;
			break;
		}

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

	vic_waitBottom();
	vic.ctrl1 = VIC_CTRL1_RST8;

	return restart;
}

void tileset_init(void)
{
	for(char i=0; i<8; i++)
	{
		for(char j=0; j<8; j++)
		{
			for(char k=0; k<j; k++)
			{
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
	8, 40
}};

SIDFX	SIDFXPlayerExplosion[4] = {{
	100, 1000, 
	SID_CTRL_GATE | SID_CTRL_SAW,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_1500,
	0, 0,
	2, 0
},{
	300, 1000, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_1500,
	100, 0,
	8, 0
},{
	1000, 1000, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_1500,
	200, 0,
	4, 20
},{
	2000, 1000, 
	SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_1500,
	0, 0,
	0, 40
}};

SIDFX	SIDFXBoing[1] = {{
	3200, 2048 + 300, 
	SID_CTRL_GATE | SID_CTRL_RECT,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_750,
	-80, -40,
	4, 60
}};

SIDFX	SIDFXStar[4] = {{
	NOTE_C(8), 3072, 
	SID_CTRL_GATE | SID_CTRL_SAW,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_6,
	0, 0,
	1, 0
},{
	NOTE_E(8), 3072, 
	SID_CTRL_GATE | SID_CTRL_SAW,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_6,
	0, 0,
	1, 0
},{
	NOTE_G(8), 3072, 
	SID_CTRL_GATE | SID_CTRL_SAW,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_6,
	0, 0,
	1, 0
},{
	NOTE_C(9), 3072, 
	SID_CTRL_GATE | SID_CTRL_SAW,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_300,
	0, 0,
	4, 32
}};

SIDFX	SIDFXShuriken[4] = {{
	8000, 3072, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xf0  | SID_DKY_168,
	-400, 0,
	6, 0,
},{
	8000, 3072, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xc0  | SID_DKY_168,
	-500, 0,
	6, 0,
},{
	8000, 3072, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0xa0  | SID_DKY_168,
	-600, 0,
	5, 0,
},{
	8000, 3072, 
	SID_CTRL_GATE | SID_CTRL_NOISE,
	SID_ATK_2 | SID_DKY_6,
	0x80  | SID_DKY_750,
	-800, 0,
	1, 8
}};


void math_init(void)
{
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

__zeropage struct Body
{
	int		px, py, vx, vy;
	char	weight;

}	player, ball;

static const unsigned PBITS = 4;
static const unsigned VBITS = 4;

static inline int asr4(int v)
{
	byte	hv = (unsigned)v >> 8;
	byte	lv = v & 0xff;

	return (asrtab4[hv] << 8) | (asltab4[hv] | lsrtab4[lv]);
}

struct Playfield
{
	char	px, vx;
	char	cx;
	bool	scrolled;
	char	wsize, wfreq;

}	playfield;

enum PowerUp
{
	PWUP_SHIELD,
	PWUP_SLOWDOWN,
	PWUP_MAGNET,
	PWUP_BUBBLE,
};

static const char PowerUpColors[] = {VCOL_LT_GREY, VCOL_GREEN, VCOL_YELLOW, VCOL_LT_BLUE};

#define ET_BALL_COLLISION	0x10
#define ET_PLAYER_COLLISION	0x20
#define ET_LETHAL			0x40

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

EnemyEvent	eventMatrix[64];

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

struct Enemy
{
	char		py;
	unsigned	px;
	char		phase, height;
	EnemyType	type;
	sbyte		vy, vx;

}	enemies[3];

char		nenemy;

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

char	xspr_msb;

void playfield_init_font(void)
{
	rle_decode(Font, charset_center);
	memcpy(Font + 0xc0 * 8, charset_front, 64 * 8)
	rle_decode(FontBottom, charset_bottom);
	memcpy(FontBottom + 0xc0 * 8, charset_front, 64 * 8)
	memset(DynSprites, 0, 2048);

	memset(Screen0, 0xc1, 1000);
	memset(Screen1, 0xc1, 1000);
}

void playfield_initirqs(void)
{
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

	rirq_build(&irq_center, 1);
	rirq_write(&irq_center, 0, &vic.memptr, 0x2a);
	rirq_set(1, 178, &irq_center);

	rirq_build(&irq_music, 1);
	rirq_call(&irq_music, 0, music_play);
	rirq_set(2, 220, &irq_music);

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

inline void xspr_move(char sp, int xpos, int ypos)
{
	__assume(sp < 8);

	if (sp < 5)
	{
		vic.spr_pos[sp].y = ypos;
		vic.spr_pos[sp].x = xpos;
	}
	else
	{
		rirq_data(irq_top, sp - 5 + 0, ypos);
		rirq_data(irq_top, sp - 5 + 3, xpos);		
	}

	if (xpos & 0x100)
		xspr_msb |= 1 << sp;
	else
		xspr_msb &= ~(1 << sp);

	rirq_data(irq_top, 15, xspr_msb);
	rirq_data(irq_bottom, 15, xspr_msb & 0x1f | 0x80);
}

inline void xspr_image(char sp, char img)
{
	__assume(sp < 8);

	if (sp < 5)
	{
		Screen0[0x3f8 + sp] = img;
		Screen1[0x3f8 + sp] = img;
	}
	else
	{
		rirq_data(irq_top, sp - 5 +  9, img);
		rirq_data(irq_top, sp - 5 + 12, img);		
	}
}

inline void xspr_color(char sp, char color)
{
	__assume(sp < 8);

	if (sp < 5)
		vic.spr_color[sp] = color;
	else
		rirq_data(irq_top, sp - 5 + 6, color);
}

void xspr_init(void)
{
	xspr_msb = 0;
	vic.spr_expand_x = 0x00;
	vic.spr_expand_y = 0x00;
	vic.spr_enable = 0xff;
}

inline void score_draw(char ci)
{
	char * dp = DynSprites + doffset[ci];
	const char * sp = charset_digits + 8 * score[ci];

	dp[ 0] = sp[0];
	dp[ 3] = sp[1];
	dp[ 6] = sp[2];
	dp[ 9] = sp[3];
	dp[12] = sp[4];
	dp[15] = sp[5];
	dp[18] = sp[6];
	dp[21] = sp[7];
}

void score_init(void)
{
	for(char i=0; i<9; i++)
		score[i] = 0x30;

	score[6] = 0x00;
	nstars = 1;
	scorecnt = 0;

	for(char i=0; i<9; i++)
		score_draw(i);
}


void score_add(char amount)
{
	scorecnt += amount << 8;
}

void score_inc(void)
{
	scorecnt += 32 * nstars;
	if (scorecnt & 0xff00)
	{
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

void star_inc(void)
{
	nstars++;

	score[8]++;
	if (score[8] == 0x3a)
	{
		score[8] = 0x30;
		score[7]++;		
		score_draw(7);
	}

	score_draw(8);

	if (!(nstars & 3))
	{	
		player.weight--;
		ball.weight++;
	}
}

void enemies_init(void)
{
	enemies[0].type = ET_NONE;
	enemies[1].type = ET_NONE;
	enemies[2].type = ET_NONE;

	xspr_move(5, 0, 0);
	xspr_move(6, 0, 0);
	xspr_move(7, 0, 0);
}

void enemies_add(EnemyType type, char y)
{
	if (enemies[nenemy].type == ET_NONE)
	{
		enemies[nenemy].px = 354;
		enemies[nenemy].py = y;
		enemies[nenemy].type = type;
		enemies[nenemy].phase = 0;
		enemies[nenemy].vx = 0;
		enemies[nenemy].vy = 0;

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

		xspr_move(5 + nenemy, enemies[nenemy].px, enemies[nenemy].py);

		nenemy++;
		if (nenemy == 3)
			nenemy = 0;
	}
}



void enemies_event(EnemyEvent ee)
{
	switch (ee)
	{
	case EE_NONE:
		break;

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

	case EE_STAR_MID:
		enemies_add(ET_STAR, (ctop + cbottom) * 4 + 40);
		break;
	case EE_STAR_TOP:
		enemies_add(ET_STAR, ctop * 8 + 64);
		break;
	case EE_STAR_BOTTOM:
		enemies_add(ET_STAR, cbottom * 8 + 30);
		break;

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
		char yp = asr4(player.py);
		if (yp < 100)
			enemies_add(ET_SHURIKEN_UP, yp + 50 + ((320 - asr4(player.px)) >> 2));
		else
			enemies_add(ET_SHURIKEN_DOWN, yp + 50 - ((320 - asr4(player.px)) >> 2));
	}
		break;

	case EE_BAT:
		enemies_add(ET_BAT, 92 + (rand() & 127));
		break;

	case EE_SPRING:
		enemies_add(ET_SPRING, cbottom * 8 + 37);
		break;

	case EE_GHOST:
		enemies_add(ET_GHOST, 90);
		break;
	}
}

void enemies_scroll(char n)
{
	for(char i=0; i<3; i++)
	{
		if (enemies[i].type != ET_NONE)
		{
			if (enemies[i].px > n)	
				enemies[i].px -= n;
			else
				enemies[i].type = ET_NONE;

			switch (enemies[i].type)
			{
				case ET_MINE:
					enemies[i].phase++;
					xspr_color(5 + i, mineflash[enemies[i].phase & 0x0f]);
					break;

				case ET_LOWER_SPIKE:
					if (enemies[i].phase & 4)
						enemies[i].py--;
					else
						enemies[i].py++;
					enemies[i].phase++;
					break;

				case ET_UPPER_SPIKE:
					if (enemies[i].phase & 4)
						enemies[i].py++;
					else
						enemies[i].py--;
					enemies[i].phase++;
					break;

				case ET_EXPLODE:
					xspr_image(5 + i, 88 + enemies[i].phase);
					enemies[i].phase++;
					if (enemies[i].phase == 17)
					{
						enemies[i].type = ET_NONE;
						xspr_move(5 + i, 0, 0);
					}
					break;					

				case ET_EXPLODE_POWERUP:
					xspr_image(5 + i, 88 + enemies[i].phase);
					enemies[i].phase++;
					if (enemies[i].phase == 17)
					{
						enemies[i].type = ET_POWERUP_ESCAPE;
						xspr_image(5 + i, 118);						
					}
					break;

				case ET_POWERUP_ESCAPE:
					enemies[i].px += 4;
					if (enemies[i].px > asr4(player.px) + 64)
					{
						enemies[i].type = ET_POWERUP;
						enemies[i].phase = 40;
						enemies[i].vx = rand() & 3;						
						xspr_image(5 + i, 124);
						xspr_color(5 + i, PowerUpColors[(PowerUp)enemies[i].vx]);
					}
					break;

				case ET_POWERUP:
					enemies[i].px += 2;
					enemies[i].py += (sintab64[enemies[i].phase & 63] + 16) >> 5;
					xspr_image(5 + i, 124 + ((enemies[i].phase & 15) >> 2));
					enemies[i].phase++;
					if (enemies[i].px >= 344 || enemies[i].py < 20)
					{
						enemies[i].type = ET_NONE;
						xspr_move(5 + i, 0, 0);
					}
					break;

				case ET_RISING_STAR:
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
					xspr_image(5 + i, 104 + ((enemies[i].phase & 15) >> 2));
					enemies[i].phase++;
					break;

				case ET_DROPPING_COIN:
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
					xspr_image(5 + i, 108 + ((enemies[i].phase & 15) >> 2));
					enemies[i].phase++;
					if (game.magnet)
					{
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
					if (enemies[i].px < asr4(player.px) + 32)
					{
						enemies[i].type = ET_SPRING_JUMP;
						enemies[i].phase = -32;
						xspr_image(5 + i, 117);
					}
					else if (enemies[i].px < asr4(player.px) + 96)
					{
						enemies[i].phase++;						
						if (enemies[i].phase & 2)
							enemies[i].px++;
						else
							enemies[i].px--;
					}
					break;

				case ET_SPRING_JUMP:
					if (enemies[i].py > 20 && enemies[i].py < 250)
					{
						enemies[i].py += (sbyte)enemies[i].phase >> 2;
						enemies[i].phase ++;
					}
					else
						enemies[i].type = ET_NONE;
					break;

				case ET_GHOST:
					xspr_image(5 + i, 120 + ((enemies[i].phase & 15) >> 2));
					enemies[i].phase++;

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
				xspr_move(5 + i, enemies[i].px, enemies[i].py);
			}
		}
	}
}

const char ctileybase[16] = {5, 5, 5, 5, 5, 5, 5, 8, 8, 8, 1, 1, 6, 6, 7, 7};
const char ctileymask[16] = {3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 7, 7, 0, 0, 0, 0};

void playfield_column(void)
{
	scr_column[0] = 0xc2;
	for(char i=1; i<17; i++)
		scr_column[i] = 0xc0;

	unsigned q = rand();
	scr_column[1 + (char)(q & 15)] = 0xc9 + ((char)q >> 5);
	q >>= 8;
	scr_column[4 + (char)(q & 15)] = 0xc9 + ((char)q >> 5);

	for(char i=0; i<25; i++)
		col_column[i] = 9;

	__assume(bcnt < 8 && bimg < 8);

	for(char i=0; i<8; i++)
	{
		char c = tileset_bottom[i + 8 * bcnt + 64 * bimg];
		scr_column[i + 17] = c;
	}

	bcnt++;
	if (bcnt == 8)
	{
		bimg = (bimg + (rand() & 3) + 1) & 7;
		bcnt = 0;
	}

	if (csize == 0 && ccnt == 8)
	{
		if (cdist == 0)
		{
			if (rand() & 3)
			{
				ctop = (rand() & 15) + 2;
				cbottom = ctop + playfield.wsize + (rand() & 3);
				if (cbottom > 23)
				{
					ctop -= cbottom - 23;
					cbottom = 23;
				}

				enemies_event(eventMatrix[rand() & 63]);
			}
			else
			{
				cbottom = rand() & 7;
				ctop = cbottom + 8 + 2 * (rand() & 3);
			}
			csize = 5;
			cdist = (playfield.wfreq + (rand() & 7)) >> 1;
		}
		else
		{
			cimg = (rand() & 255) & 15;
			ccnt = 0;
			cdist--;
			cimgy = ctileybase[cimg] + (rand() & ctileymask[cimg]);

			ctop = 0;
			cbottom = 25;
			enemies_event(eventMatrix[rand() & 63]);
		}
	}

	if (csize > 0)
	{
		csize--;
		if (csize == 0)
		{
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
			char j = 255;
			if (ctop > 0)
			{
				for(char i=ctop-1; i>0; i--)
				{
					scr_column[i] = 0xe0 + (j & 3) * 4 + (4 - csize);
					col_column[i] = 9;
					j--;
				}
				scr_column[0] = 0xf0 + (j & 3) * 4 + (4 - csize);

				if (csize == 3)
					scr_column[ctop] = 0xc6;
				else if (csize < 3)
					scr_column[ctop] = 0xc7;
			}

			j = 0;
			for(char i=cbottom; i<25; i++)
			{
				scr_column[i] = 0xe0 + (j & 3) * 4 + (4 - csize);
				col_column[i] = 9;
				j++;
			}
		}
		else
		{
			char j = 0;
			for(char i=cbottom; i<ctop; i++)
			{
				scr_column[i] = 0xe0 + (j & 3) * 4 + (4 - csize);
				col_column[i] = 9;
				j++;
			}

			if (csize == 3)
				scr_column[ctop] = 0xc6;
			else if (csize < 3)
				scr_column[ctop] = 0xc7;
		}
	}
	else
	{
		__assume(ccnt < 8 && cimg < 16);		
		for(char i=0; i<8; i++)
		{
			char c = tileset_center[i + 8 * ccnt + 64 * cimg];
			scr_column[i + cimgy] = c;
			col_column[i + cimgy] = charattribs_center[c];

		}
		ccnt++;
	}
}

void playfield_scroll0(void)
{
	for(char x=0; x<39; x++)
	{
	#assign ry 0
	#repeat		
		Screen0[40 * ry + x] = Screen1[40 * ry + x + 1];
	#assign ry ry + 1
	#until ry == 25
	#undef ry
	}	

	#assign ry 0
	#repeat		
		Screen0[40 * ry + 39] = scr_column[ry];
	#assign ry ry + 1
	#until ry == 25
	#undef ry
}

void playfield_scroll1(void)
{
	for(char x=0; x<39; x++)
	{
	#assign ry 0
	#repeat		
		Screen1[40 * ry + x] = Screen0[40 * ry + x + 1];
	#assign ry ry + 1
	#until ry == 25
	#undef ry
	}	

	#assign ry 0
	#repeat		
		Screen1[40 * ry + 39] = scr_column[ry];
	#assign ry ry + 1
	#until ry == 25
	#undef ry
}

void playfield_fill(void)
{
	for(char x=0; x<40; x++)
	{
		playfield_column();
		for(char y=0; y<25; y++)
		{
			Screen0[40 * y + x] = scr_column[y];
			Color[40 * y + x] = col_column[y];
		}
	}
}


void playfield_scrollc(void)
{
	for(char x=0; x<39; x++)
	{
	#assign ry 0
	#repeat		
		Color[40 * ry + x] = Color[40 * ry + x + 1];
	#assign ry ry + 1
	#until ry == 6
	}

	for(char x=0; x<39; x++)
	{
	#repeat		
		Color[40 * ry + x] = Color[40 * ry + x + 1];
	#assign ry ry + 1
	#until ry == 12
	}

	for(char x=0; x<39; x++)
	{
	#repeat		
		Color[40 * ry + x] = Color[40 * ry + x + 1];
	#assign ry ry + 1
	#until ry == 18
	}

	for(char x=0; x<39; x++)
	{
	#repeat		
		Color[40 * ry + x] = Color[40 * ry + x + 1];
	#assign ry ry + 1
	#until ry == 25
	#undef ry
	}

	#assign ry 0
	#repeat		
		Color[40 * ry + 39] = col_column[ry];
	#assign ry ry + 1
	#until ry == 25
	#undef ry
}

unsigned usqrt(unsigned n)
{
	unsigned p, q, r, h

	p = 0;
    r = n;

   	q = 0x4000;
   	while (q > r)
   		q >>= 2;

    while (q != 0)
    {
        h = p + q;
        p >>= 1;
        if (r >= h)
        {
            p += q;
            r -= h;
        } 
        q >>= 2;
    }

    return p;
}

inline unsigned csquare(char c)
{
	return sqrtabl[c] | (sqrtabh[c] << 8);
}

inline unsigned usquare(int i)
{
	if (i < 0)
		return csquare(- i);
	else
		return csquare(i);
}


void game_init(void)
{
	game.level = 0;
	game.pulling = false;
	game.throw = false;

	game.shield = 0;
	game.bubble = 0;
	game.magnet = 0;

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

void game_level(void)
{
	if (game.level < 100)	
	{
		EnemyEvent	ee = eventLevels[game.level];

		switch (ee)
		{
			case EE_NONE:
				break;

			case EE_SHRINK:
				playfield.wsize--;
				break;

			case EE_FREQUENCY:
				playfield.wfreq--;
				break;

			default:
				eventMatrix[rand() & 63] = ee;
		}


		game.level++;
		if ((game.level & 1) && playfield.vx < 64)
			playfield.vx++;
	}
}

void player_init(void)
{
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

	xspr_image(0, 64); xspr_color(0, VCOL_ORANGE);
	xspr_image(1, 76); xspr_color(1, VCOL_MED_GREY);
	xspr_image(2, 77); xspr_color(2, VCOL_YELLOW);
	xspr_image(3, 77); xspr_color(3, VCOL_YELLOW);
	xspr_image(4, 77); xspr_color(4, VCOL_YELLOW);
}

void player_show(void)
{
	int	pix = asr4(player.px), piy = asr4(player.py);
	int bix = asr4(ball.px), biy = asr4(ball.py);


	int ix2 = (pix + bix) >> 1, iy2 = (piy + biy) >> 1;
	int ix1 = (pix + ix2) >> 1, iy1 = (piy + iy2) >> 1;
	int ix3 = (ix2 + bix) >> 1, iy3 = (iy2 + biy) >> 1;

	xspr_move(0, pix + (24 - 8), piy + (50 - 8));
	xspr_move(1, bix + (24 - 12), biy + (50 - 12));
	xspr_move(2, ix1 + (24 - 6), iy1 + (50 - 5));
	xspr_move(3, ix2 + (24 - 6), iy2 + (50 - 5));
	xspr_move(4, ix3 + (24 - 6), iy3 + (50 - 5));
}

void player_control(void)
{	
	joy_poll(0);

	if (joyx[0] < 0)
		xspr_image(0, 67 + joyy[0]);
	else if (joyx[0] > 0)
		xspr_image(0, 70 + joyy[0]);
	else if (joyy[0] < 0)
		xspr_image(0, 65);
	else if (joyy[0] > 0)
		xspr_image(0, 64);

	player.vx += joyx[0] << (PBITS + 2);
	player.vy += joyy[0] << (PBITS + 3);

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

EnemyType player_collision(void)
{
	EnemyType	explode = ET_NONE;
	bool		boing = false;

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

	char	cix = (player.px - (4 << PBITS)) >> (3 + PBITS);
	char	ciy = (player.py - (4 << PBITS)) >> (3 + PBITS);

	bool	c00 = cscreen[40 * ciy + cix +  0] >= 0xe0;
	bool	c01 = cscreen[40 * ciy + cix +  1] >= 0xe0;
	bool	c10 = cscreen[40 * ciy + cix + 40] >= 0xe0;
	bool	c11 = cscreen[40 * ciy + cix + 41] >= 0xe0;

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
		player.vx += playfield.vx << (PBITS + VBITS - 4);
		if ((c01 || c11) && player.vx > 0)
		{
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

		player.vx -= playfield.vx << (PBITS + VBITS - 4);
	}

	char	siy = asr4(player.py) + (50 - 8);
	int		six = asr4(player.px) + (24 - 8);

	for(char i=0; i<3; i++)
	{
		char	py = enemies[i].py;
		if (enemies[i].type & ET_PLAYER_COLLISION)
		{
			if ((char)(siy + 16) > py && siy < (char)(py + enemies[i].height))
			{
				if (six + 16 > enemies[i].px && six < enemies[i].px + 16)
				{
					explode = enemies[i].type;
					if (explode == ET_STAR)
					{
						enemies[i].type = ET_RISING_STAR;
						sidfx_play(2, SIDFXStar, 4);
					}
					else if (explode == ET_COIN)
					{
						enemies[i].type = ET_DROPPING_COIN;
						enemies[i].phase = 248;
						sidfx_play(2, SIDFXStar, 4);
					}
					else if (explode == ET_POWERUP)
					{
						switch ((PowerUp)enemies[i].vx)
						{
							case PWUP_SLOWDOWN:
								playfield.vx = (playfield.vx + 16) >> 1;
								break;
							case PWUP_SHIELD:
								game.shield = 255;
								xspr_color(0, VCOL_LT_GREY);
								break;
							case PWUP_MAGNET:
								game.magnet = 255;
								break;
							case PWUP_BUBBLE:
								game.bubble = 255;
								xspr_color(1, VCOL_LT_BLUE);								
								break;
						}

						enemies[i].type = ET_NONE;
						xspr_move(i + 5, 0, 0);
					}
					else	
					{
						enemies[i].type = ET_EXPLODE;
						enemies[i].phase = 0;
					}
				}
			}
		}
	}

	if (explode == ET_NONE && boing)
		sidfx_play(2, SIDFXBoing, 1);

	return explode;
}

bool ball_collision(void)
{
	if (ball.py > (192 << PBITS))
	{
		ball.py = 192 << PBITS;
		ball.vy = 0;
	}

	char	biy = asr4(ball.py) + (50 - 12);
	int		bix = asr4(ball.px) + (24 - 12);

	for(char i=0; i<3; i++)
	{
		if (enemies[i].type & ET_BALL_COLLISION)
		{
			char	py = enemies[i].py;

			if (biy + 20 > py && biy < (char)(py + enemies[i].height))
			{
				if (bix + 24 > enemies[i].px && bix < enemies[i].px + 24)
				{
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

void player_advance(void)
{
	player.px += asr4(player.vx);
	player.py += asr4(player.vy);

	ball.px += asr4(ball.vx);
	ball.py += asr4(ball.vy);
}

void player_exploding(void)
{
	if (player.py > (192 << PBITS))
		player.py = 192 << PBITS;
}

void player_physics(void)
{
#if 1
	if (player.vx > 0)
		player.vx -= asr4(player.vx + 15);
	else if (player.vx < 0)
		player.vx -= asr4(player.vx);

	if (player.vy > 0)
		player.vy -= asr4(player.vy + 15);
	else if (player.vy < 0)
		player.vy -= asr4(player.vy);
#else
	if (player.vx > 0)
		player.vx -= (player.vx + 31) >> 5;
	else if (player.vx < 0)
		player.vx -= player.vx >> 5;

	if (player.vy > 0)
		player.vy -= (player.vy + 31) >> 5;
	else if (player.vy < 0)
		player.vy -= player.vy >> 5;
#endif
	player.vy += 16;

#if 1
	if (game.bubble)
	{
		ball.vx -= (ball.vx + 2) >> 2;
		ball.vy -= (ball.vy + 2) >> 2;
	}
	else
	{
		if (ball.vx > 0)
			ball.vx -= asr4(ball.vx + 15);
		else if (ball.vx < 0)
			ball.vx -= asr4(ball.vx);

		if (ball.vy > 0)
			ball.vy -= asr4(ball.vy + 15);
		else if (ball.vy < 0)
			ball.vy -= asr4(ball.vy);

		ball.vy += 32;
	}
#else
	if (ball.vx > 0)
		ball.vx -= (ball.vx + 7) >> 3;
	else if (ball.vx < 0)
		ball.vx -= ball.vx >> 3;

	if (ball.vy > 0)
		ball.vy -= (ball.vy + 7) >> 3;
	else if (ball.vy < 0)
		ball.vy -= ball.vy >> 3;

	ball.vy += 64;
#endif
}

void chain_physics(void)
{
	int	dx = asr4(ball.px - player.px);
	int	dy = asr4(ball.py - player.py);

	unsigned	rq = usquare(dx) + usquare(dy)

	if (rq > 0)
	{
		char	r = usqrt(rq);

		char	t = 0;
		if (game.pulling)
		{
			if (r > 16)
				t = r - 16;
		}
		else if (game.throw)
		{
			t = 64;
		}
		else if (r > 32)
			t = r - 32;

		if (t)
		{
			int		fx = dx < 0 ? -lmuldiv8by8(-dx, t, r) : lmuldiv8by8(dx, t, r);
			int		fy = dy < 0 ? -lmuldiv8by8(-dy, t, r) : lmuldiv8by8(dy, t, r);
#if 1
			char 	weight = game.bubble ? 1 : ball.weight;

			int		bx = fx * weight;
			int		by = fy * weight;

			player.vx += bx;
			player.vy += by;
			ball.vx -= fx * 32 - bx;
			ball.vy -= fy * 32 - by;
#else
			player.vx += fx * 16;
			player.vy += fy * 16;
			ball.vx -= fx * 16;
			ball.vy -= fy * 16;
#endif			
		}
	}
}


void charset_init(void)
{

}

void playfield_init(void)
{
	playfield.px = 0;
	playfield.vx = 16;
	playfield.scrolled = false;
	playfield.wsize = 12;
	playfield.wfreq = 16;

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

	vic.color_back = VCOL_DARK_GREY;
	vic.color_back1 = VCOL_LT_GREY;
	vic.color_back2 = VCOL_BLACK;

	vic.spr_mcolor0 = VCOL_BLACK;
	vic.spr_mcolor1 = VCOL_WHITE;

	vic_waitTop();
	vic_waitBottom();

	for(char i=0; i<8; i++)
		xspr_move(i, 0, 0);

	memset(Screen1, 0xc1, 1000);
	memset(Color, 0x08, 1000);

	vic_waitBottom();
	vic_setmode(VICM_TEXT_MC, Screen1, Font);

	vic.ctrl2 = VIC_CTRL2_MCM | 7;
	vic.memptr = 0x38;
	rirq_data(irq_bottom, 18, 0x38);
	rirq_data(&irq_center, 0, 0x3a);

	playfield_fill();

	vic_waitTop();
	vic_waitBottom();

	vic.memptr = 0x28;
	rirq_data(irq_bottom, 18, 0x28);
	rirq_data(&irq_center, 0, 0x2a);

	playfield_column();
	playfield_scroll1();

	vic_waitTop();
	vic_waitBottom();
}

void playfield_advance(void)
{
	char	px = playfield.px >> 4;

	playfield.px += playfield.vx;

	enemies_scroll((char)(playfield.px >> 4) - px);
}

void playfield_scroll(void)
{
	if (playfield.px & 128)
	{
		playfield.px &= 127;
		playfield.scrolled = true;
		vic.ctrl2 = VIC_CTRL2_MCM | ((playfield.px >> 4) ^ 7);

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

		playfield_scrollc();
	}
	else
	{
		vic.ctrl2 = VIC_CTRL2_MCM | ((playfield.px >> 4) ^ 7);

		if (playfield.scrolled)
		{
			playfield.scrolled = false;
//	vic.color_border--;
			playfield_column();
//	vic.color_border++;
			if (cframe)
				playfield_scroll1();
			else
				playfield_scroll0();
		}
		else
			vic_waitTop();
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

// Work for current frame
void game_loop()
{
	EnemyType	et;

	switch (game.state)
	{
	case GS_START:
		break;

	case GS_READY:
		vic_waitBottom();
		vic_waitTop();

		if (!--game.count)
			game_state(GS_PLAYING);
		break;
	case GS_PLAYING:		
		playfield_advance();

		if (!--game.count)
		{
			game_level();
			game.count = 150;
		}

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

		vic_waitBottom();

		player_show();
		playfield_scroll();

		player_control();
		player_advance();
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
			player_physics();
			chain_physics();
		}
		break;

	case GS_EXPLODING:

		vic_waitBottom();

		player_show();

		enemies_scroll(0);

		vic_waitTop();

		player_advance();
		ball_collision();
		player_exploding();
		player_physics();

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

void black_screen(void)
{
	vic_waitBottom();

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
	cia_init();

	__asm { sei }

	black_screen();

	// Copy spriteset under IO, freeing 0xc000..0xcfff
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

	game_state(GS_TITLE);

	for(;;)		
	{		
		score_inc();
//	vic.color_border++;
		sidfx_loop_2();
//	vic.color_border++;
		vic_waitBottom();
//	vic.color_border--;
//	vic.color_border--;
		game_loop();

	}

	return 0;
}
