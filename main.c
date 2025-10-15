#include <raylib.h>
#include <raymath.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
// #include <time.h>

#define CELL_SIZE 4
#define GRID_W 500
#define GRID_H 500
#define MAX_BEINGS 10000
#define MAX_BELIEF 500
#define MAX_ZOMBIE_LIFE 1500

enum {
	TYPE_NULL = 0,
	TYPE_ZOMBIE = 1,
	TYPE_HUMAN = 2,
	TYPE_HUMAN_PANIC = 3,
	TYPE_DEAD = 4,
};

typedef struct {
	int x, y;
	int dir;
	int type;
	int belief;
	int zombieLife;
	int id;
} Being;

typedef struct {
	Vector2 position;
	float zoom;
} Camera2DWorld;

typedef struct {
	Being being;
} GridCell;

typedef struct {
	GridCell cells[GRID_W][GRID_H];
	int width, height;
} Grid;

Grid grid;
Being beings[MAX_BEINGS];
int clock = 0;
Color colorZombie = {0, 255, 0, 255};
Color colorHuman = {200, 0, 200, 255};
Color colorHumanPanic = {255, 120, 255, 255};
Color colorWall = {50, 50, 60, 255};
Color colorDead = {128, 30, 30, 255};
Color colorHit = {128, 128, 0, 255};

void DrawGameGrid(Grid *g, Camera2DWorld *cam, int cellSize) {
	int startX = (int)(cam->position.x / cellSize);
	int startY = (int)(cam->position.y / cellSize);
	int endX = (int)((cam->position.x + GetScreenWidth() / cam->zoom) / cellSize) + 1;
	int endY = (int)((cam->position.y + GetScreenHeight() / cam->zoom) / cellSize) + 1;

	if (startX < 0) startX = 0;
	if (startY < 0) startY = 0;
	if (endX > g->width) endX = g->width;
	if (endY > g->height) endY = g->height;

	for (int y = startY; y < endY; y++) {
		for (int x = startX; x < endX; x++) {
			Being *b = &g->cells[y][x].being;
			if (b->type == TYPE_NULL) continue;

			Color c = RAYWHITE;
			switch (b->type) {
				case TYPE_ZOMBIE:
					c = colorZombie;
					break;
				case TYPE_HUMAN:
					c = colorHuman;
					break;
				case TYPE_HUMAN_PANIC:
					c = colorHumanPanic;
					break;
				case TYPE_DEAD:
					c = colorDead;
					break;
			}

			float screenX = (x * cellSize - cam->position.x) * cam->zoom;
			float screenY = (y * cellSize - cam->position.y) * cam->zoom;

			DrawRectangle(
				(int)screenX,
				(int)screenY,
				(int)(cellSize * cam->zoom),
				(int)(cellSize * cam->zoom),
				c
			);
		}
	}
}

void MoveBeing(Grid *g, Being *b, int newX, int newY) {
	g->cells[b->x][b->y].being.type = TYPE_NULL;
	g->cells[newX][newY].being = *b;
	b->x = newX;
	b->y = newY;
}

void SetBeingDead(Being *b) {
	b->type = TYPE_DEAD;
}

void SetupSim(Grid *g) {
	memset(g, 0, sizeof(*g));

	g->width = GRID_W;
	g->height = GRID_H;

	int midX = g->width / 2;
	int midY = g->height / 2;

	g->cells[midX][midY].being.type = TYPE_ZOMBIE;

	for (int i = -2; i <= 2; i++) {
		for (int j = -2; j <= 2; j++) {
			if (i == 0 && j == 0) continue;
			int x = midX + i;
			int y = midY + j;
			if (x >= 0 && y >= 0 && x < GRID_W && y < GRID_H) {
				g->cells[x][y].being.type = TYPE_HUMAN;
			}
		}
	}
}

int main(void) {
	SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
	InitWindow(GetScreenWidth(), GetScreenHeight(), "Zombie3 Raylib Port");
	SetTargetFPS(60);

	Camera2DWorld camera = { .position = {0, 0}, .zoom = 1.0f };
	SetupSim(&grid);

	while (!WindowShouldClose()) {
		camera.zoom += GetMouseWheelMove() * (0.1f * camera.zoom);
		if (camera.zoom < 0.1f) {
			camera.zoom = 0.1f;
		}

		if (IsKeyDown(KEY_W)) camera.position.y -= 10;
		if (IsKeyDown(KEY_S)) camera.position.y += 10;
		if (IsKeyDown(KEY_A)) camera.position.x -= 10;
		if (IsKeyDown(KEY_D)) camera.position.x += 10;

		BeginDrawing();
			ClearBackground(GRAY);
			DrawGameGrid(&grid, &camera, CELL_SIZE);
			DrawText(TextFormat("Camera zoom: %f", camera.zoom), 10, 10, 10, BLACK);
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
