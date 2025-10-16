#include <raylib.h>
#include <raymath.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#define CELL_SIZE 1
// #define GRID_W 1024
// #define GRID_H 1024
#define GRID_W 512
#define GRID_H 512
#define MAX_ANIMATION 50
// #define MAX_BEINGS 10000
#define MAX_BEINGS 5000
#define MAX_BELIEF 100
#define MAX_FROZEN 10
#define MAX_VITALITY 2
#define MAX_ZOMBIE_LIFE 1000
// #define MAX_ZOMBIE_LIFE 100
#define ZOMBIE_LIFE_VARIANCE 2
#define CHUNK_SIZE 16

enum Being_Type {
	TYPE_ZOMBIE = 0,
	TYPE_HUMAN = 1,
	TYPE_HUMAN_PANIC = 2,
	TYPE_DEAD = 3,
};

enum Direction {
	N = 0,
	E = 1,
	S = 2,
	W = 3,
};

enum Look_Distance {
	DIST_ZOMBIE = 20,
	DIST_HUMAN = 10,
};

enum Look_Target {
	LOOK_CLEAR = 0,
	LOOK_WALL = 1,
	LOOK_ZOMBIE = 2,
	LOOK_HUMAN = 3,
	LOOK_OUT_BOUNDS = 4,
};

enum City_Layout {
	OPEN_CITY = 0,
	CLOSED_CITY = 1,
};

typedef struct {
	int x, y;
	enum Direction dir;
	enum Being_Type type;
	int belief, vitality, zombieLife;
	int animationTimer;
	int id;
} Being;

typedef struct {
	Being **beings;
	int count;
	int capacity;
} Chunk;

typedef struct {
	Chunk chunks[GRID_W / CHUNK_SIZE][GRID_H / CHUNK_SIZE];
	int chunksW, chunksH;
	Being beings[MAX_BEINGS];
	int beingCount, humanCount, zombieCount, deadCount;
	enum City_Layout layout;
	unsigned char *wallMap;
	int wallW, wallH;
} Grid;

static Grid grid;
static Color colorZombie = {0, 255, 0, 255};
static Color colorHuman = {200, 0, 200, 255};
static Color colorHumanPanic = {255, 120, 255, 255};
static Color colorWall = {50, 50, 60, 255};
static Color colorDead = {128, 30, 30, 255};

static inline void GetChunkIndex(int x, int y, int *cx, int *cy) {
	*cx = x / CHUNK_SIZE;
	*cy = y / CHUNK_SIZE;
}

static void AddBeingToChunk(Grid *g, Being *b) {
	int cx, cy;
	GetChunkIndex(b->x, b->y, &cx, &cy);

	if (cx < 0 || cx >= g->chunksW || cy < 0 || cy >= g->chunksH) return;

	Chunk *c = &g->chunks[cx][cy];
	if (c->count >= c->capacity) {
		c->capacity = (c->capacity == 0) ? 4 : c->capacity * 2;
		c->beings = realloc(c->beings, c->capacity * sizeof(Being*));
	}
	c->beings[c->count++] = b;
}

static void RemoveBeingFromChunk(Grid *g, Being *b, int oldX, int oldY) {
	int cx, cy;
	GetChunkIndex(oldX, oldY, &cx, &cy);

	if (cx < 0 || cx >= g->chunksW || cy < 0 || cy >= g->chunksH) return;

	Chunk *c = &g->chunks[cx][cy];
	for (int i = 0; i < c->count; i++) {
		if (c->beings[i] == b) {
			c->beings[i] = c->beings[--c->count];
			return;
		}
	}
}

void MoveBeing(Grid *g, Being *b, int newX, int newY) {
	int oldX = b->x, oldY = b->y;
	int oldCx, oldCy, newCx, newCy;
	GetChunkIndex(oldX, oldY, &oldCx, &oldCy);
	GetChunkIndex(newX, newY, &newCx, &newCy);

	if (oldCx != newCx || oldCy != newCy) {
		RemoveBeingFromChunk(g, b, oldX, oldY);
	}

	b->x = newX;
	b->y = newY;

	if (oldCx != newCx || oldCy != newCy) {
		AddBeingToChunk(g, b);
	}
}

void GetNearbyBeings(Grid *g, int x, int y, int radius, Being **out, int *outCount) {
	*outCount = 0;
	int cx, cy;
	GetChunkIndex(x, y, &cx, &cy);

	for (int dx = -1; dx <= 1; dx++) {
		for (int dy = -1; dy <= 1; dy++) {
			int nx = cx + dx;
			int ny = cy + dy;
			if (nx >= 0 && nx < g->chunksW && ny >= 0 && ny < g->chunksH) {
				Chunk *c = &g->chunks[nx][ny];
				for (int i = 0; i < c->count; i++) {
					Being *b = c->beings[i];
					int dx = b->x - x;
					int dy = b->y - y;
					if (dx*dx + dy*dy <= radius*radius) {
						out[(*outCount)++] = b;
					}
				}
			}
		}
	}
}

Being* GetBeingAt(Grid *g, int x, int y) {
	int cx, cy;
	GetChunkIndex(x, y, &cx, &cy);

	if (cx < 0 || cx >= g->chunksW || cy < 0 || cy >= g->chunksH) return NULL;

	Chunk *c = &g->chunks[cx][cy];
	for (int i = 0; i < c->count; i++) {
		if (c->beings[i]->x == x && c->beings[i]->y == y) {
			return c->beings[i];
		}
	}
	return NULL;
}

static inline int IsWall(Grid *g, int x, int y) {
	if (x < 0 || x >= g->wallW || y < 0 || y >= g->wallH) return 1;
	int idx = y * g->wallW + x;
	int byteIndex = idx / 8;
	int bitIndex = idx % 8;
	return (g->wallMap[byteIndex] >> bitIndex) & 1;
}

static inline void SetWall(Grid *g, int x, int y, int val) {
	if (x < 0 || x >= g->wallW || y < 0 || y >= g->wallH) return;
	int idx = y * g->wallW + x;
	int byteIndex = idx / 8;
	int bitIndex = idx % 8;
	unsigned char mask = 1U << bitIndex;
	if (val) {
		g->wallMap[byteIndex] |= mask;
	} else {
		g->wallMap[byteIndex] &= ~mask;
	}
}

typedef struct {
	int x, y, w, h;
} WallRect;

// Temporary grid to track which cells have been merged
static unsigned char *mergedMap = NULL;
static int mergedMapSize = 0;

static inline int IsMergedCell(int x, int y, int gridW) {
	int idx = y * gridW + x;
	return (mergedMap[idx / 8] >> (idx % 8)) & 1;
}

static inline void MarkMergedCell(int x, int y, int gridW) {
	int idx = y * gridW + x;
	int byteIndex = idx / 8;
	int bitIndex = idx % 8;
	unsigned char mask = 1U << bitIndex;
	mergedMap[byteIndex] |= mask;
}

// Greedy rectangle merging: expand each rectangle as wide as possible, then as tall as possible
static WallRect MergeRectangleAt(Grid *g, int startX, int startY) {
	WallRect rect = {startX, startY, 0, 0};

	// Expand width
	int x = startX;
	while (x < g->wallW && IsWall(g, x, startY) && !IsMergedCell(x, startY, g->wallW)) {
		x++;
	}
	rect.w = x - startX;

	// Expand height (only cells that match the width we just found)
	int y = startY;
	while (y < g->wallH) {
		// Check if entire row is wall and unmerged
		int canExpand = 1;
		for (int checkX = startX; checkX < startX + rect.w; checkX++) {
			if (!IsWall(g, checkX, y) || IsMergedCell(checkX, y, g->wallW)) {
				canExpand = 0;
				break;
			}
		}
		if (!canExpand) break;
		y++;
	}
	rect.h = y - startY;

	// Mark all cells in this rectangle as merged
	for (int mx = rect.x; mx < rect.x + rect.w; mx++) {
		for (int my = rect.y; my < rect.y + rect.h; my++) {
			MarkMergedCell(mx, my, g->wallW);
		}
	}

	return rect;
}

// Generate merged rectangles for visible area only
void GenerateMergedWallRects(Grid *g, int startX, int startY, int endX, int endY, WallRect *rects, int *rectCount, int maxRects) {
	*rectCount = 0;

	// Allocate merged map if needed
	int mapSize = (g->wallW * g->wallH + 7) / 8;
	if (mergedMapSize < mapSize) {
		mergedMap = realloc(mergedMap, mapSize);
		mergedMapSize = mapSize;
	}
	memset(mergedMap, 0, mapSize);

	// Scan visible area for unmerged walls
	for (int y = startY; y < endY; y++) {
		for (int x = startX; x < endX; x++) {
			if (IsWall(g, x, y) && !IsMergedCell(x, y, g->wallW)) {
				if (*rectCount < maxRects) {
					rects[(*rectCount)++] = MergeRectangleAt(g, x, y);
				}
			}
		}
	}
}

char * GetLayoutString(Grid *g) {
	if (g->layout == OPEN_CITY) {
		return "Open City";
	}
	return "Closed City";
}

void KillBeing(Grid *g, Being *b) {
	if (b->type == TYPE_HUMAN_PANIC || b->type == TYPE_HUMAN) {
		g->humanCount--;
	} else if (b->type == TYPE_ZOMBIE) {
		g->zombieCount--;
	}
	b->type = TYPE_DEAD;
	g->deadCount++;
}

void Blast(Grid *g, Camera2D *cam) {
	Vector2 screenPos = GetMousePosition();
	float worldX = (screenPos.x - cam->offset.x) / cam->zoom + cam->target.x;
	float worldY = (screenPos.y - cam->offset.y) / cam->zoom + cam->target.y;
	int mx = (int)worldX;
	int my = (int)worldY;
	int radius = GetRandomValue(8, 16);
	Being *nearby[1000];
	int nearbyCount = 0;
	GetNearbyBeings(g, mx, my, radius, nearby, &nearbyCount);

	for (int dy = -radius; dy <= radius; dy++){
		for (int dx = -radius; dx <= radius; dx++){
			int x = mx + dx;
			int y = my + dy;
			int d2 = dx * dx + dy * dy;
			if (d2 <= radius * radius && (x > 0 && x <= GRID_W && y > 0 && y <= GRID_H)) {
				SetWall(g, x, y, 0);
			}
		}
	}

	for (int i = 0; i < nearbyCount; i++){
		int dx = nearby[i]->x - mx;
		int dy = nearby[i]->y - my;
		int diff = dx * dx + dy * dy;
		if (diff <= radius * radius){
			KillBeing(g, nearby[i]);
		}
	}
}

void ConvertHumanToZombie(Grid *g, Being *b, int instantTurn) {
	if (!instantTurn && b->vitality > 0) {
		b->vitality--;
		b->belief = MAX_BELIEF;
		b->animationTimer = MAX_ANIMATION;
		if (b->type == TYPE_HUMAN) {
			b->type = TYPE_HUMAN_PANIC;
		}
	} else {
		b->type = TYPE_ZOMBIE;
		float r = (float)rand()/(float)(RAND_MAX/(ZOMBIE_LIFE_VARIANCE * 100));
		int a = roundf(r);
		b->zombieLife = MAX_ZOMBIE_LIFE + a;
		b->animationTimer = MAX_ANIMATION;
		g->zombieCount++;
		g->humanCount--;
	}
}

void DrawGameGrid(Grid *g, Camera2D *cam, int cellSize) {
	// Calculate visible bounds in world coordinates
	float screenWidth = GetScreenWidth() / cam->zoom;
	float screenHeight = GetScreenHeight() / cam->zoom;

	int startX = (int)(cam->target.x - screenWidth / 2.0f) - 1;
	int startY = (int)(cam->target.y - screenHeight / 2.0f) - 1;
	int endX = (int)(cam->target.x + screenWidth / 2.0f) + 2;
	int endY = (int)(cam->target.y + screenHeight / 2.0f) + 2;

	// Clamp to grid bounds
	startX = (startX < 0) ? 0 : startX;
	startY = (startY < 0) ? 0 : startY;
	endX = (endX > g->wallW) ? g->wallW : endX;
	endY = (endY > g->wallH) ? g->wallH : endY;

	// Generate merged wall rectangles
	WallRect wallRects[10000];  // Adjust size as needed
	int wallRectCount = 0;
	GenerateMergedWallRects(g, startX, startY, endX, endY, wallRects, &wallRectCount, 10000);

	// Draw merged walls
	for (int i = 0; i < wallRectCount; i++) {
		WallRect r = wallRects[i];
		DrawRectangle(r.x, r.y, r.w * cellSize, r.h * cellSize, colorWall);
		DrawRectangleLines(r.x, r.y, r.w * cellSize, r.h * cellSize, DARKGRAY);
	}

	// Draw beings using chunks
	int startCx = startX / CHUNK_SIZE;
	int startCy = startY / CHUNK_SIZE;
	int endCx = (endX / CHUNK_SIZE) + 1;
	int endCy = (endY / CHUNK_SIZE) + 1;

	// Clamp chunk indices to valid range
	startCx = (startCx < 0) ? 0 : startCx;
	startCy = (startCy < 0) ? 0 : startCy;
	endCx = (endCx > g->chunksW) ? g->chunksW : endCx;
	endCy = (endCy > g->chunksH) ? g->chunksH : endCy;

	// Draw beings
	for (int cx = startCx; cx < endCx; cx++) {
		for (int cy = startCy; cy < endCy; cy++) {
			Chunk *c = &g->chunks[cx][cy];
			for (int i = 0; i < c->count; i++) {
				Being *b = c->beings[i];
				Color col = colorZombie;
				switch (b->type) {
					case TYPE_HUMAN: col = colorHuman; break;
					case TYPE_HUMAN_PANIC: col = colorHumanPanic; break;
					case TYPE_DEAD: col = colorDead; break;
					case TYPE_ZOMBIE: col = colorZombie; break;
				}
				DrawRectangle(b->x, b->y, cellSize, cellSize, col);
				if (b->animationTimer > 0) {
					DrawCircleLines(b->x, b->y, cellSize + 3, col);
					b->animationTimer--;
				}
			}
		}
	}
}

int GetRandomDir() {
	int dirs[] = {N, E, S, W};
	return dirs[GetRandomValue(0, 3)];
}

void SetupSim(Grid *g) {
	memset(g, 0, sizeof(*g));
	int r = GetRandomValue(0, 1);
	g->layout = r;

	g->wallW = GRID_W;
	g->wallH = GRID_H;
	g->wallMap = calloc((GRID_W * GRID_H + 7) / 8, 1);

	g->chunksW = GRID_W / CHUNK_SIZE;
	g->chunksH = GRID_H / CHUNK_SIZE;

	int midX = GRID_W / 2;
	int midY = GRID_H / 2;

	// Borders
	if (g->layout == OPEN_CITY) {
		for (int x = 0; x < GRID_W; x++) {
			for (int y = 0; y < GRID_H; y++) {
				if (x == 0 || x == GRID_W - 1 || y == 0 || y == GRID_H - 1) {
					SetWall(g, x, y, 1);
				}
			}
		}

		// Big Rooms
		for (int i = 0; i < GRID_W / 20; i++) {
			int rx = GetRandomValue(1, GRID_W - 1);
			int ry = GetRandomValue(1, GRID_H - 1);
			int rw = GetRandomValue(GRID_W / 50, GRID_W / 10);
			int rh = GetRandomValue(GRID_H / 50, GRID_H / 10);
			for (int x = rx; x < rx + rw; x++) {
				for (int y = ry; y < ry + rh; y++) {
					if (x < GRID_W - 1 && y < GRID_H - 1) {
						SetWall(g, x, y, 1);
					}
				}
			}
		}
	} else {
		for (int x = 0; x < GRID_W; x++) {
			for (int y = 0; y < GRID_H; y++) {
				SetWall(g, x, y, 1);
			}
		}

		// Big Rooms
		for (int i = 0; i < GRID_W / 50; i++) {
			int rx = GetRandomValue(1, GRID_W - 1);
			int ry = GetRandomValue(1, GRID_H - 1);
			int rw = GetRandomValue(GRID_W / 50, GRID_W / 5);
			int rh = GetRandomValue(GRID_H / 50, GRID_H / 5);
			for (int x = rx; x < rx + rw; x++) {
				for (int y = ry; y < ry + rh; y++) {
					if (x < GRID_W - 1 && y < GRID_H - 1) {
						SetWall(g, x, y, 0);
					}
				}
			}
		}

		// Halls
		for (int i = 0; i < GRID_W / 10; i++) {
			int rx = GetRandomValue(1, GRID_W - 1);
			int ry = GetRandomValue(1, GRID_H - 1);
			int rw = GetRandomValue(3, GRID_W / 5);
			int rh = GetRandomValue(3, GRID_H / 5);

			if (rw > rh) {
				rh = 3;
			} else if (rh > rw) {
				rw = 3;
			}

			for (int x = rx; x < rx + rw; x++) {
				for (int y = ry; y < ry + rh; y++) {
					if (x < GRID_W - 1 && y < GRID_H - 1) {
						SetWall(g, x, y, 0);
					}
				}
			}
		}
	}

	for (int i = 0; i < MAX_BEINGS; i++) {
		for (int ok = 0; ok < 100; ok++) {
			int x = GetRandomValue(2, GRID_W-2);
			int y = GetRandomValue(2, GRID_H-2);
			if (!IsWall(g, x, y)) {
				g->beings[i].x = x;
				g->beings[i].y = y;
				g->beings[i].dir = GetRandomDir();
				g->beings[i].type = TYPE_HUMAN;
				g->beings[i].vitality = MAX_VITALITY;
				g->beings[i].id = g->beingCount;
				AddBeingToChunk(g, &g->beings[g->beingCount]);
				g->beingCount++;
				g->humanCount++;
				ok = 100;
			}
		}
	}

	ConvertHumanToZombie(g, &g->beings[0], 1);
	ConvertHumanToZombie(g, &g->beings[1], 1);
	ConvertHumanToZombie(g, &g->beings[2], 1);
	ConvertHumanToZombie(g, &g->beings[3], 1);
}

Being* LookAheadForBeing(Grid *g, Being *b, int dist, int *result) {
	int x = b->x;
	int y = b->y;

	for (int i = 0; i < dist; i++) {
		if (b->dir == N) y--;
		if (b->dir == E) x++;
		if (b->dir == S) y++;
		if (b->dir == W) x--;

		if (x < 0 || x >= GRID_W || y < 0 || y >= GRID_H) {
			*result = LOOK_OUT_BOUNDS;
			return NULL;
		}

		if (IsWall(g, x, y)) {
			*result = LOOK_WALL;
			return NULL;
		}

		Being *target = GetBeingAt(g, x, y);
		if (target != NULL) {
			*result = LOOK_CLEAR;
			return target;  // Found a being
		}
	}

	*result = LOOK_CLEAR;
	return NULL;  // Nothing found
}

int FindNearestHuman(Grid *g, Being *zombie, int *bestDir) {
	Being *nearby[1000];
	int nearbyCount = 0;
	GetNearbyBeings(g, zombie->x, zombie->y, DIST_ZOMBIE, nearby, &nearbyCount);
	int closestDist = INT_MAX;
	int closestIndex = -1;

	for (int i = 0; i < nearbyCount; i++) {
		if (nearby[i]->type == TYPE_HUMAN || nearby[i]->type == TYPE_HUMAN_PANIC) {
			int dx = nearby[i]->x - zombie->x;
			int dy = nearby[i]->y - zombie->y;
			int dist = dx*dx + dy*dy;

			if (dist < closestDist) {
				closestDist = dist;
				closestIndex = i;
			}
		}
	}

	if (closestIndex == -1) return 0;

	int dx = nearby[closestIndex]->x - zombie->x;
	int dy = nearby[closestIndex]->y - zombie->y;

	if (abs(dy) > abs(dx)) {
		*bestDir = (dy < 0) ? N : S;
	} else {
		*bestDir = (dx > 0) ? E : W;
	}

	return 1;
}

int FindNearestZombie(Grid *g, Being *human, int *bestDir) {
	Being *nearby[1000];
	int nearbyCount = 0;
	GetNearbyBeings(g, human->x, human->y, DIST_HUMAN, nearby, &nearbyCount);
	int closestDist = INT_MAX;
	int closestIndex = -1;

	for (int i = 0; i < nearbyCount; i++) {
		if (nearby[i]->type == TYPE_ZOMBIE) {
			int dx = nearby[i]->x - human->x;
			int dy = nearby[i]->y - human->y;
			int dist = dx*dx + dy*dy;

			if (dist < closestDist) {
				closestDist = dist;
				closestIndex = i;
			}
		}
	}

	if (closestIndex == -1) return 0;

	int dx = nearby[closestIndex]->x - human->x;
	int dy = nearby[closestIndex]->y - human->y;

	if (abs(dy) > abs(dx)) {
		*bestDir = (dy > 0) ? N : S;
	} else {
		*bestDir = (dx < 0) ? E : W;
	}

	return 1;
}

void StepBeing(Grid *g, Being *b) {
	if (b->type == TYPE_DEAD) return;

	if (b->type == TYPE_ZOMBIE) {
		b->zombieLife--;
		if (b->zombieLife <= 0) {
			KillBeing(g, b);
			return;
		}
	}

	if (b->type == TYPE_HUMAN_PANIC) {
		if (b->belief > 0) {
			b->belief--;
		} else {
			b->type = TYPE_HUMAN;
		}
	}

	int shouldMove = 0;
	int shouldTurn = 0;

	if (b->type == TYPE_HUMAN) {
		shouldMove = GetRandomValue(0, 5) == 0;
		shouldTurn = GetRandomValue(0, 2) == 0;
	} else if (b->type == TYPE_HUMAN_PANIC) {
		shouldMove = GetRandomValue(0, 2) == 0;
		shouldTurn = GetRandomValue(0, 3) == 0;
	} else {
		shouldMove = GetRandomValue(0, 3) == 0;
		shouldTurn = GetRandomValue(0, 1) == 0;
	}

	if (b->type == TYPE_ZOMBIE) {
		int bestDir;
		if (FindNearestHuman(g, b, &bestDir)) {
			b->dir = bestDir;
		} else if (shouldTurn) {
			b->dir = GetRandomDir();
		}
	} else if (b->type == TYPE_HUMAN_PANIC) {
		int bestDir;
		if (FindNearestZombie(g, b, &bestDir)) {
			b->dir = bestDir;
		}
	} else if (shouldTurn) {
		b->dir = GetRandomDir();
	}

	if (shouldMove) {
		int lookResult;
		Being *targetBeing = LookAheadForBeing(g, b, 1, &lookResult);

		int xx = b->x, yy = b->y;
		if (b->dir == N) yy--;
		if (b->dir == E) xx++;
		if (b->dir == S) yy++;
		if (b->dir == W) xx--;

		if (b->type == TYPE_HUMAN || b->type == TYPE_HUMAN_PANIC) {
			if (lookResult == LOOK_CLEAR && targetBeing == NULL) {
				MoveBeing(g, b, xx, yy);
			}
		} else if (b->type == TYPE_ZOMBIE) {
			if (targetBeing != NULL) {
				if (targetBeing->type == TYPE_HUMAN || targetBeing->type == TYPE_HUMAN_PANIC) {
					ConvertHumanToZombie(g, targetBeing, 0);
				}
				// MoveBeing(g, b, xx, yy);
			} else if (lookResult == LOOK_WALL) {
				SetWall(g, xx, yy, 0);
				MoveBeing(g, b, xx, yy);
			} else if (lookResult == LOOK_CLEAR) {
				MoveBeing(g, b, xx, yy);
			}
		}
	}
}

int main(void) {
	SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
	InitWindow(GetScreenWidth(), GetScreenHeight(), "Zombie Simulation");
	SetTargetFPS(60);
	srand((unsigned int)time(NULL));

	int frozenCountdown = MAX_FROZEN;
	SetupSim(&grid);

	Camera2D camera = {0};
	camera.offset = (Vector2){GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
	camera.target = (Vector2){GRID_W / 2.0f, GRID_H / 2.0f};
	camera.zoom = 1.0f;

	while (!WindowShouldClose()) {
		if (frozenCountdown == 0) {
			if (camera.zoom > 3.0f) camera.zoom = 3.0f;
			else if (camera.zoom < 0.5f) camera.zoom = 0.5f;
			camera.zoom = expf(logf(camera.zoom) + ((float)GetMouseWheelMove() * 0.1f));

			if (IsKeyDown(KEY_W)) camera.target.y -= 10;
			if (IsKeyDown(KEY_S)) camera.target.y += 10;
			if (IsKeyDown(KEY_A)) camera.target.x -= 10;
			if (IsKeyDown(KEY_D)) camera.target.x += 10;
			if (IsKeyDown(KEY_Z)) {
				frozenCountdown = MAX_FROZEN;
				SetupSim(&grid);
			}

			if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
				Blast(&grid, &camera);
			}

			for (int i = 0; i < grid.beingCount; i++) {
				StepBeing(&grid, &grid.beings[i]);
			}
		} else {
			frozenCountdown--;
		}

		BeginDrawing();
			ClearBackground(BLACK);
			BeginMode2D(camera);
				DrawGameGrid(&grid, &camera, CELL_SIZE);
			EndMode2D();
			DrawText(TextFormat("FPS: %d | Zoom: %f | City Layout: %d (%s)", GetFPS(), camera.zoom, grid.layout, GetLayoutString(&grid)), 10, 10, 20, WHITE);
			DrawText(TextFormat("Zombies: %d", grid.zombieCount), 10, GetScreenHeight() - 25, 20, colorZombie);
			DrawText(TextFormat("Humans: %d", grid.humanCount), 10, GetScreenHeight() - 50, 20, colorHuman);
			DrawText(TextFormat("Dead: %d", grid.deadCount), 10, GetScreenHeight() - 75, 20, colorDead);
		EndDrawing();
	}

	CloseWindow();
	return 0;
}
