#include <c64/joystick.h>
#include <c64/vic.h>
#include <c64/sprites.h>
#include <c64/memmap.h>
#include <c64/rasterirq.h>
#include <c64/sid.h>
#include <math.h>
#include <fixmath.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>


char spriteset[2048] = {
	#embed "ballnchain - Sprites.bin"
};

char charset[2048] = {
	#embed "ballnchain_back - Chars.bin"
	#embed "ballnchain_front - Chars.bin"
};

byte * const Screen = (byte *)0xc800;
byte * const Font = (byte *)0xd000;
byte * const Color = (byte *)0xd800;
byte * const Sprites = (byte *)0xd800;

byte	asrtab4[256], lsrtab4[256], asltab4[256];
byte	sqrtabl[256], sqrtabh[256];

#pragma align(asrtab4, 256)
#pragma align(lsrtab4, 256)
#pragma align(asltab4, 256)
#pragma align(sqrtabl, 256)
#pragma align(sqrtabh, 256)

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

struct Body
{
	int		px, py, vx, vy;
}	player, ball;

static const unsigned PBITS = 4;
static const unsigned VBITS = 4;

static inline int asr4(int v)
{
	byte	hv = (unsigned)v >> 8;
	byte	lv = v & 0xff;

	return (asrtab4[hv] << 8) | (asltab4[hv] | lsrtab4[lv]);
}

static const float DT = 0.04;

char	scr_column[25];
char	ctop, cbottom ,csize, bcol, bimg, bcnt;

struct Playfield
{
	char	px, vx;
	char	cx;
	bool	scrolled;
}	playfield;

void playfield_column(void)
{
	scr_column[0] = 0x82;
	for(char i=1; i<24; i++)
		scr_column[i] = 0x80;
	scr_column[24] = 0x83;

	for(char i=0; i<4; i++)
	{
		scr_column[bcol + i] = bimg + 4 * i + bcnt;
	}
	bcnt++;

	if (bcnt == 4)
	{
		bimg = (rand() & 7) * 16;
		bcnt = 0;
		do
		{
			bcol = rand() & 31
		} while (bcol > 19);
		bcol++;
	}

	if (csize > 0)
	{
		csize--;

		if (ctop < cbottom)
		{
			char j = 255;
			for(char i=ctop; i>0; i--)
			{
				if (j & 2)
					scr_column[i-1] = 0xc8 + (j & 1) * 4 + (3 - csize);
				else
					scr_column[i-1] = 0xc0 + (j & 1) * 4 + (3 - csize);
				j--;
			}
			if (ctop > 0 && csize < 3)
				scr_column[ctop] = 0x81;

			j = 0;
			for(char i=cbottom; i<25; i++)
			{
				if (j & 2)
					scr_column[i] = 0xc8 + (j & 1) * 4 + (3 - csize);
				else
					scr_column[i] = 0xc0 + (j & 1) * 4 + (3 - csize);
				j++;
			}
		}
		else
		{
			char j = 0;
			for(char i=cbottom; i<ctop; i++)
			{
				if (j & 2)
					scr_column[i] = 0xc8 + (j & 1) * 4 + (3 - csize);
				else
					scr_column[i] = 0xc0 + (j & 1) * 4 + (3 - csize);
				j++;
			}
			if (csize < 3)
				scr_column[ctop] = 0x81;
		}
	}
	else if (ctop == 0 && cbottom == 25)
	{
		if (rand() & 3)
		{
			ctop = rand() & 15;
			cbottom = ctop + 7 + (rand() & 3);
		}
		else
		{
			cbottom = rand() & 7;
			ctop = cbottom + 8 + 2 * (rand() & 3);
		}
		csize = 4;
	}
	else
	{
		if (ctop < cbottom)
		{
			for(char i=0; i<=ctop; i++)
				scr_column[i] = 0x81;
			for(char i=cbottom+1; i<25; i++)
				scr_column[i] = 0x81;
		}
		else
		{
			for(char i=cbottom+1; i<=ctop; i++)
				scr_column[i] = 0x81;
		}

		csize = 8 + (rand() & 15);
		ctop = 0;
		cbottom = 25;
	}
}

void playfield_scroll(void)
{
	for(char x=0; x<39; x++)
	{
	#assign ry 0
	#repeat		
		Screen[40 * ry + x] = Screen[40 * ry + x + 1];
	#assign ry ry + 1
	#until ry == 12
	}

	for(char x=0; x<39; x++)
	{
	#repeat		
		Screen[40 * ry + x] = Screen[40 * ry + x + 1];
	#assign ry ry + 1
	#until ry == 25
	#undef ry
	}

	#assign ry 0
	#repeat		
		Screen[40 * ry + 39] = scr_column[ry];
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

static unsigned csquare(char c)
{
	return sqrtabl[c] | (sqrtabh[c] << 8);
}

static unsigned usquare(int i)
{
	if (i < 0)
		return csquare(- i);
	else
		return csquare(i);
}


// Game state
enum GameState
{
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
	char		pulling;

}	game;

void game_init(void)
{
}

void player_init(void)
{
	player.px = 160 << PBITS;
	player.py = 100 << PBITS;
	player.vx = 0;
	player.vy = 0;

	ball.px = 200 << PBITS;
	ball.py = 120 << PBITS;
	ball.vx = 0;
	ball.vy = 0;

	game.pulling = 0;

	spr_set(0, true, 0, 0, 97, VCOL_RED, true, false, false);
	spr_set(1, true, 0, 0, 98, VCOL_MED_GREY, true, false, false);

	spr_set(2, true, 0, 0, 99, VCOL_YELLOW, true, false, false);
	spr_set(3, true, 0, 0, 99, VCOL_YELLOW, true, false, false);
	spr_set(4, true, 0, 0, 99, VCOL_YELLOW, true, false, false);
}

void player_show(void)
{
	int	pix = asr4(player.px), piy = asr4(player.py);
	int bix = asr4(ball.px), biy = asr4(ball.py);


	int ix2 = (pix + bix) >> 1, iy2 = (piy + biy) >> 1;
	int ix1 = (pix + ix2) >> 1, iy1 = (piy + iy2) >> 1;
	int ix3 = (ix2 + bix) >> 1, iy3 = (iy2 + biy) >> 1;

	spr_move(0, pix + (24 - 8), piy + (50 - 8));
	spr_move(1, bix + (24 - 12), biy + (50 - 12));
	spr_move(2, ix1 + (24 - 6), iy1 + (50 - 5));
	spr_move(3, ix2 + (24 - 6), iy2 + (50 - 5));
	spr_move(4, ix3 + (24 - 6), iy3 + (50 - 5));
}

void player_control(void)
{	
	joy_poll(0);

	player.vx += joyx[0] << (PBITS + 2);
	player.vy += joyy[0] << (PBITS + 3);

	if (game.pulling)
		game.pulling--;
	else if (joyb[0])
		game.pulling = 20;
}

bool player_collision(void)
{
	bool	explode = false;

	if (player.px < (8 << PBITS))
	{
		player.px = (8 << PBITS);
		player.vx = - player.vx;
	}
	else if (player.px > (312 << PBITS))
	{
		player.px = 312 << PBITS;
		player.vx = - player.vx;
	}

	if (player.py < (8 << PBITS))
	{
		player.py = (8 << PBITS);
		player.vy = - player.vy;
	}
	else if (player.py > (192 << PBITS))
	{
		player.py = 192 << PBITS;
		player.vy = - player.vy;
	}

	char	cix = (player.px - (4 << PBITS)) >> (3 + PBITS);
	char	ciy = (player.py - (4 << PBITS)) >> (3 + PBITS);

	bool	c00 = Screen[40 * ciy + cix +  0] >= 0xc0;
	bool	c01 = Screen[40 * ciy + cix +  1] >= 0xc0;
	bool	c10 = Screen[40 * ciy + cix + 40] >= 0xc0;
	bool	c11 = Screen[40 * ciy + cix + 41] >= 0xc0;

	if ((c10 || c11) && !(c00 || c01) && player.vy > 0)
		player.vy = - player.vy;
	else if ((c00 || c01) && !(c10 || c11) && player.vy < 0)
		player.vy = - player.vy;
	else
	{
		player.vx += playfield.vx << (PBITS + VBITS - 4);
		if ((c01 || c11) && player.vx > 0)
		{
			if (player.vx > (16 << (PBITS + VBITS)))
				explode = true;
			else
				player.vx = -player.vx;
		}
		else if ((c00 || c10) && player.vx < 0)
		{
			player.vx = -player.vx;
			if (player.vx > (16 << (PBITS + VBITS)))
				explode = true;
		}

		player.vx -= playfield.vx << (PBITS + VBITS - 4);
	}

	return explode;
}

bool player_advance(void)
{
	player.px += asr4(player.vx);
	player.py += asr4(player.vy);

	ball.px += asr4(ball.vx);
	ball.py += asr4(ball.vy);
}

void player_physics(void)
{
	if (player.vx > 0)
		player.vx -= (player.vx + 31) >> 5;
	else if (player.vx < 0)
		player.vx -= player.vx >> 5;

	if (player.vy > 0)
		player.vy -= (player.vy + 31) >> 5;
	else if (player.vy < 0)
		player.vy -= player.vy >> 5;

	player.vy += 16;

#if 1
	if (ball.vx > 0)
		ball.vx -= (ball.vx + 15) >> 4;
	else if (ball.vx < 0)
		ball.vx -= ball.vx >> 4;

	if (ball.vy > 0)
		ball.vy -= (ball.vy + 15) >> 4;
	else if (ball.vy < 0)
		ball.vy -= ball.vy >> 4;

	ball.vy += 32;
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

	char	r = usqrt(usquare(dx) + usquare(dy));

	char	t = 0;
	if (r > 0)
	{
		if (r > 32)
			t = r - 32;
		if (game.pulling > 16)
			t += 32;

		if (t)
		{
			int		fx = dx < 0 ? -lmuldiv8by8(-dx, t, r) : lmuldiv8by8(dx, t, r);
			int		fy = dy < 0 ? -lmuldiv8by8(-dy, t, r) : lmuldiv8by8(dy, t, r);

			player.vx += fx * 16;
			player.vy += fy * 16;
			ball.vx -= fx * 16;
			ball.vy -= fy * 16;
		}
	}
}


void playfield_init(void)
{
	playfield.px = 0;
	playfield.vx = 64; //16;
	playfield.cx = 0;
	playfield.scrolled = false;

	ctop = 0;
	cbottom = 25;
	csize = 10;

	playfield_column();

	memset(Screen, 0, 1000);	
	memset(Color, 8, 1000);
}

void playfield_advance(void)
{
	playfield.px += playfield.vx;

	if (playfield.scrolled)
	{
		playfield.scrolled = false;
		playfield_column();
	}

	if (playfield.px & 128)
	{
		playfield.px &= 127;
		playfield.scrolled = true;
		vic.ctrl2 = VIC_CTRL2_MCM | ((playfield.px >> 4) ^ 7);

		playfield.cx++;
		if (playfield.cx == 10)
		{
			playfield.cx = 0;
			if (playfield.vx < 64)
				playfield.vx++;
		}

		playfield_scroll();
	}
	else
	{
		vic.ctrl2 = VIC_CTRL2_MCM | ((playfield.px >> 4) ^ 7);

		vic_waitTop();
	}
}

// Advance game state
void game_state(GameState state)
{
	// Set new state
	game.state = state;

	switch (state)
	{
	case GS_START:
		game_init();
		game_state(GS_READY);
		break;

	case GS_READY:
		playfield_init();
		game.count = 120;
		break;

	case GS_PLAYING:
		player_init();
		break;

	case GS_EXPLODING:
		player.vx >>= 4;
		spr_show(2, false);
		spr_show(3, false);
		spr_show(4, false);
		game.count = 0;
		break;

	case GS_GAME_OVER:
		game.count = 120;
		break;
	}
}

// Work for current frame
void game_loop()
{
	switch (game.state)
	{
	case GS_START:
		break;

	case GS_READY:
		if (!--game.count)
			game_state(GS_PLAYING);
		break;
	case GS_PLAYING:		
		player_show();

		playfield_advance();

		player_control();
		player_advance();
		if (player_collision())
			game_state(GS_EXPLODING);
		else
		{
			player_physics();
			chain_physics();
		}
		break;

	case GS_EXPLODING:


		player_show();

		vic_waitTop();

		player_advance();
		player_physics();

		game.count++;
		if (game.count < 32)
		{
			spr_image(0, 104 + (game.count >> 3));
		}
		else if (game.count < 128)
		{
			spr_show(0, false);
		}
		else
		{
			spr_show(1, false);
			game_state(GS_READY);
		}
		break;
	case GS_GAME_OVER:
		break;
	}
}

int main(void)
{
	math_init();

	mmap_trampoline();

	// Install character set
	mmap_set(MMAP_CHAR_ROM);
	memcpy(Font, charset, 2048);
	memcpy(Sprites, spriteset, 2048);
	mmap_set(MMAP_NO_BASIC);

	// Switch screen
	vic_setmode(VICM_TEXT_MC, Screen, Font);

	spr_init(Screen);

	vic.color_border = VCOL_BLACK;
	vic.color_back = VCOL_DARK_GREY;
	vic.color_back1 = VCOL_LT_GREY;
	vic.color_back2 = VCOL_WHITE;

	vic.spr_mcolor0 = VCOL_BLACK;
	vic.spr_mcolor1 = VCOL_WHITE;


	game_state(GS_START);

	for(;;)		
	{		
		vic_waitBottom();
		vic.color_border++;
		game_loop();
		vic.color_border--;
	}

	return 0;
}
