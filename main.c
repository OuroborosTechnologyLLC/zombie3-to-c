// zombie_raylib.c
// Raylib port of "Zombie Infection Simulation" (Processing 2003)
// Port by ChatGPT — attempts to stay faithful to original behaviour.

#include <raylib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#define SCR_W 800
#define SCR_H 400
#define MAX_BEINGS 8000

// Processing-like global state
int freeze = 0;
int num = 4000;
int speed = 1;
int panic = 5;
int human_col, panichuman_col, zombie_col, wall_col, empty_col, dead_col, hit_col;
int clock_count = 0;

typedef uint32_t Color32;
static Color32 pixelbuf[SCR_W * SCR_H];

typedef struct Being {
	int xpos, ypos, dir;
	int type, active;
	int belief, maxBelief;
	int zombielife;
	int rest;
} Being;

Being beings[MAX_BEINGS];

// type constants (match original integer mapping)
enum { T_ZOMBIE = 1, T_HUMAN = 2, T_PANICHUM = 4, T_DEAD = 5 };

// helper: pack RGB into 0xAARRGGBB (we'll use opaque)
static inline Color32 c32(int r,int g,int b) {
	return (0xFFu << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

// pixel utilities (x,y origin top-left)
static inline int in_bounds(int x,int y){ return x>=0 && x< SCR_W && y>=0 && y< SCR_H; }
static inline Color32 get_pixel(int x,int y){
	if (!in_bounds(x,y)) return c32(0,0,0);
	return pixelbuf[y*SCR_W + x];
}
static inline void set_pixel(int x,int y, Color32 c){
	if (!in_bounds(x,y)) return;
	pixelbuf[y*SCR_W + x] = c;
}

// random helpers
static inline int rand_int(int a,int b){ // inclusive [a,b]
	return a + rand() % (b - a + 1);
}
static inline float randf(){ return (float)rand() / (float)RAND_MAX; }

// Processing-like 'look' (returns same numeric codes as your code)
// d: 1 up, 2 right, 3 down, 4 left
int look_func(int x, int y, int d, int dist) {
	for (int i=0;i<dist;i++){
		if (d==1) y--;
		else if (d==2) x++;
		else if (d==3) y++;
		else if (d==4) x--;
		if (x > SCR_W-1 || x < 1 || y > SCR_H-31 || y < 1) return 3; // wall/edge
		Color32 p = get_pixel(x,y);
		if (p == (Color32)wall_col) return 3;
		if (p == (Color32)panichuman_col) return 4;
		if (p == (Color32)human_col) return 2;
		if (p == (Color32)zombie_col) return 1;
		if (p == (Color32)dead_col) return 0;
	}
	return 0;
}

// Being helper functions
void being_die(Being *b){
	b->type = T_DEAD;
}

void being_position(Being *b){
	for (int ok=0; ok<100; ok++){
		b->xpos = rand_int(1, SCR_W-1);
		b->ypos = rand_int(1, SCR_H-31);
		if (get_pixel(b->xpos, b->ypos) == (Color32)empty_col){
			ok = 100;
		}
	}
}

void being_infect_at(Being *b, int x, int y){
	if (x==b->xpos && y==b->ypos) b->type = T_ZOMBIE;
}

void being_infect(Being *b){
	b->type = T_ZOMBIE;
	set_pixel(b->xpos, b->ypos, (Color32)zombie_col);
}

void being_uninfect(Being *b){
	b->type = T_HUMAN;
	b->active = 0;
	b->zombielife = 1000;
	b->belief = 500;
}

// move logic ported from Processing
void being_move(Being *b){
	if (b->type == T_DEAD){
		set_pixel(b->xpos, b->ypos, (Color32)dead_col);
		return;
	}
	int r = rand_int(0,2);
	if ((b->type==T_HUMAN && b->active>0) || r==1){
		set_pixel(b->xpos, b->ypos, (Color32)empty_col);
		if (b->belief <= 0){
			b->active = 0;
			b->maxBelief -= 100;
			b->belief = 0;
			b->rest = 0;
		}
		if (look_func(b->xpos, b->ypos, b->dir, 1) == 0){
			if (b->dir==1) b->ypos--;
			else if (b->dir==2) b->xpos++;
			else if (b->dir==3) b->ypos++;
			else if (b->dir==4) b->xpos--;
		} else {
			b->dir = rand_int(1,4);
		}
		if (b->type == T_ZOMBIE) set_pixel(b->xpos, b->ypos, (Color32)zombie_col);
		else if (b->active > 0) set_pixel(b->xpos, b->ypos, (Color32)panichuman_col);
		else set_pixel(b->xpos, b->ypos, (Color32)human_col);
		if (b->active > 0) b->active--;
	}

	int target = look_func(b->xpos, b->ypos, b->dir, 10);

	if (b->type == T_ZOMBIE){
		b->zombielife--;
		if (target == 2 || target == 4){
			b->active = 10;
		} else if (target == 3){
			if (rand_int(0,7) == 1){
				if (b->dir==1) set_pixel(b->xpos, b->ypos-1, (Color32)empty_col);
				else if (b->dir==2) set_pixel(b->xpos+1, b->ypos, (Color32)empty_col);
				else if (b->dir==3) set_pixel(b->xpos, b->ypos+1, (Color32)empty_col);
				else if (b->dir==4) set_pixel(b->xpos-1, b->ypos, (Color32)empty_col);
			}
			if (look_func(b->xpos, b->ypos, b->dir, 2) == T_ZOMBIE){
				b->dir = b->dir + 1;
				if (b->dir > 4) b->dir = 1;
			}
		} else if (b->active == 0 && target != 1){
			if (rand_int(0,5) > 4){
				if (rand_int(0,1) == 0) b->dir = b->dir + 1;
				else b->dir = b->dir - 1;
				if (b->dir == 5) b->dir = 1;
				if (b->dir == 0) b->dir = 4;
			} else {
				// keep dir
			}
		}
		int victim = look_func(b->xpos, b->ypos, b->dir, 1);
		if (victim == 2 || victim == 4){
			int ix = b->xpos, iy = b->ypos;
			if (b->dir==1) iy--;
			else if (b->dir==2) ix++;
			else if (b->dir==3) iy++;
			else if (b->dir==4) ix--;
			for (int i=0;i<num;i++) being_infect_at(&beings[i], ix, iy);
		}
		if (b->zombielife <= 0) being_die(b);
	} else if (b->type == T_HUMAN){
		if (target == 1){
			b->maxBelief = 500;
			b->belief = 500;
			b->active = 10;
			b->rest = 0;
		}
		if (target == 4){
			b->active = 10;
			if (b->belief > 0) b->belief--;
			else if (b->belief == 0 && b->maxBelief != 0 && b->rest >= 40){
				b->belief = b->maxBelief;
				b->rest = 0;
			} else if (b->belief == 0 && b->rest < 40){
				b->rest++;
			}
		} else if (target != 1 && target != 4 && b->active == 0){
			b->rest++;
		} else if (target == 1){
			b->dir = b->dir + 2; if (b->dir > 4) b->dir = b->dir - 4;
		}
		if (rand_int(0,1) == 1) b->dir = rand_int(1,4);
	}
}

// Setup (map generation, beings, initial infection)
void setup_sim(){
	clock_count = 0;
	// fill entire canvas with wall color initially, then paint many rectangles etc (mimic original)
	for (int y=0;y<SCR_H;y++){
		for (int x=0;x<SCR_W;x++){
			set_pixel(x,y, (Color32)wall_col);
		}
	}
	// draw many random wall rectangles (processing code used some - using simpler approach)
	for (int i=0;i<100;i++){
		int rx = rand_int(-SCR_W/8, SCR_W-1);
		int ry = rand_int(-SCR_H/8, SCR_H-1);
		int rw = rand_int(1, SCR_W/4);
		int rh = rand_int(1, SCR_H/4);
		for (int y=ry; y<ry+rh; y++){
			for (int x=rx; x<rx+rw; x++){
				if (in_bounds(x,y)) set_pixel(x,y,(Color32)wall_col);
			}
		}
	}
	// draw some empty rectangles
	for (int i=0;i<45;i++){
		int rx = rand_int(1, SCR_W-1);
		int ry = rand_int(1, SCR_H-31);
		int rw = rand_int(10, 49);
		int rh = rand_int(10, 49);
		for (int y=ry; y<ry+rh; y++){
			for (int x=rx; x<rx+rw; x++){
				if (in_bounds(x,y)) set_pixel(x,y,(Color32)empty_col);
			}
		}
	}
	// floor area at bottom like original
	for (int y=SCR_H-30;y<SCR_H;y++){
		for (int x=0;x<SCR_W;x++){
			set_pixel(x,y,(Color32)wall_col);
		}
	}
	for (int y=SCR_H-30;y<SCR_H-2;y++){
		for (int x=1;x<SCR_W-1;x++){
			set_pixel(x,y,(Color32)empty_col);
		}
	}

	if (num > MAX_BEINGS) num = MAX_BEINGS;
	for (int i=0;i<num;i++){
		beings[i].dir = rand_int(1,4);
		beings[i].type = T_HUMAN;
		beings[i].active = 0;
		beings[i].belief = 500;
		beings[i].maxBelief = 500;
		beings[i].zombielife = 1300;
		beings[i].rest = 40;
		being_position(&beings[i]);
		// draw them
		set_pixel(beings[i].xpos, beings[i].ypos, (Color32)human_col);
	}

	// infect a few initial
	if (num > 0) being_infect(&beings[0]);
	if (num > 1) being_infect(&beings[1]);
	if (num > 2) being_infect(&beings[2]);
	if (num > 3) being_infect(&beings[3]);
	freeze = 0;
}

// Mouse blast: kill beings within radius
void mouse_blast(int mx,int my){
	int radius = rand_int(6, 15);
	// draw the circle (stroke effect)
	for (int dy=-radius; dy<=radius; dy++){
		for (int dx=-radius; dx<=radius; dx++){
			int x = mx + dx;
			int y = my + dy;
			int d2 = dx*dx + dy*dy;
			if (d2 <= radius*radius && in_bounds(x,y)){
				// mark with empty then overlay stroke color (we'll simply set empty here)
				set_pixel(x,y, (Color32)empty_col);
			}
		}
	}
	for (int i=0;i<num;i++){
		int dx = beings[i].xpos - mx;
		int dy = beings[i].ypos - my;
		int diff = dx*dx + dy*dy;
		if (diff < radius*radius){
			beings[i].type = T_DEAD;
		}
	}
}

// Convert our pixel buffer to a Texture2D quickly: create Image then UpdateTexture
// We build an Image each frame then UpdateTexture (Raylib requires careful handling).
// For simplicity we'll create an Image once and update pixels then Upload texture.
int main(void){
	srand((unsigned)time(NULL));
	InitWindow(SCR_W, SCR_H, "Zombie Infection Simulation (Raylib port)");
	SetTargetFPS(60);

	// initialize colors as in original
	human_col = (int)c32(200,0,200);
	panichuman_col = (int)c32(255,120,255);
	zombie_col = (int)c32(0,255,0);
	wall_col = (int)c32(50,50,60);
	empty_col = (int)c32(0,0,0);
	dead_col = (int)c32(128,30,30);
	hit_col = (int)c32(128,128,0);

	// make a raylib Image and Texture2D for the pixel buffer
	Image img = GenImageColor(SCR_W, SCR_H, BLACK);
	Texture2D tex = LoadTextureFromImage(img);
	UnloadImage(img); // texture has the data now

	setup_sim();

	while (!WindowShouldClose()){
		// input handling
		if (IsKeyPressed(KEY_Z)){
			freeze = 1;
			setup_sim();
		}
		if (IsKeyPressed(KEY_KP_ADD) || IsKeyPressed(KEY_EQUAL)){
			if (num < MAX_BEINGS) num += 100;
		}
		if (IsKeyPressed(KEY_KP_SUBTRACT) || IsKeyPressed(KEY_MINUS)){
			if (num > 4000) num -= 100;
		}
		if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)){
			Vector2 mpos = GetMousePosition();
			// scale mpos down (we drew window double size)
			int mx = (int)(mpos.x / 2.0f);
			int my = (int)(mpos.y / 2.0f);
			mouse_blast(mx, my);
			// mouse_blast(mpos.x, mpos.y);
		}

		// update frame (Processing loop)
		if (freeze == 0){
			// re-draw bottom bars like original each frame
			for (int x=0;x<SCR_W;x++){
				for (int y=SCR_H-30;y<SCR_H;y++){
					set_pixel(x,y,(Color32)wall_col);
				}
			}
			for (int x=1;x<SCR_W-1;x++){
				for (int y=SCR_H-30;y<SCR_H-2;y++){
					set_pixel(x,y,(Color32)empty_col);
				}
			}

			// move beings
			for (int i=0;i<num;i++){
				being_move(&beings[i]);
			}
			clock_count++;
		}

		// build an Image from pixelbuf and update texture
		// Raylib needs bytes in RGBA8888 order; our Color32 is 0xAARRGGBB which matches GetColor order used by DrawTexture...
		// But UpdateTexture requires const void*, we can pass buffer directly (it expects Color).
		UpdateTexture(tex, pixelbuf);

		// Count stats
		int numZ = 0, numD = 0;
		for (int i=0;i<num;i++){
			if (beings[i].type == T_ZOMBIE) numZ++;
			if (beings[i].type == T_DEAD) numD++;
		}

		// draw scaled-up texture for visibility
		BeginDrawing();
			ClearBackground(RAYWHITE);
			DrawTexturePro(tex,
				(Rectangle){0,0,(float)SCR_W,(float)SCR_H},
				(Rectangle){0,0,(float)GetScreenWidth(),(float)GetScreenHeight()},
				(Vector2){0,0}, 0.0f, WHITE);

			// draw UI text (use default font)
			DrawText(TextFormat("Humans: %d", (num - (numD + numZ))), 4, GetScreenHeight() - 24, 16, MAROON);
			DrawText(TextFormat("Zombies: %d", numZ), GetScreenWidth() - 240, GetScreenHeight() - 24, 16, GREEN);
			DrawText(TextFormat("Dead: %d", numD), 120, GetScreenHeight() - 24, 16, DARKBROWN);
		EndDrawing();
	}

	UnloadTexture(tex);
	CloseWindow();
	return 0;
}

