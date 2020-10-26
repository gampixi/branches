#include "pch.h"
#include "raylib.h"
#include <iostream>
#include <list>
#include <array>
#include <ctime>
#include <cstdlib>
#include <chrono>

constexpr auto CHUNK_SIZE = 1024;

enum Direction {
	UP,
	RIGHT,
	DOWN,
	LEFT
};

bool ColorEqual(Color a, Color b) {
	if (a.r != b.r) return false;
	if (a.g != b.g) return false;
	if (a.b != b.b) return false;
	return true;
}

void PrintColor(Color c) {
	std::cout << "R: " << (int)c.r << "G: " << (int)c.g << "B: " << (int)c.b;
}

class Chunk {
public:
	Image image;
	Texture2D texture;
	Chunk(Color fill);
	void begin_draw();
	void end_draw();
	void draw_pixel(int x, int y, Color color);
private:
	Color* rawPixelsForDraw;
};

Chunk::Chunk(const Color fill)
{
	this->image = GenImageColor(CHUNK_SIZE, CHUNK_SIZE, fill);
	this->texture = LoadTextureFromImage(this->image);
}

void Chunk::begin_draw()
{
	this->rawPixelsForDraw = GetImageData(this->image);
}

void Chunk::end_draw()
{
	UnloadImage(this->image);
	this->image = LoadImageEx(this->rawPixelsForDraw, CHUNK_SIZE, CHUNK_SIZE);
	UpdateTexture(this->texture, this->rawPixelsForDraw);
	free(this->rawPixelsForDraw);
}

void Chunk::draw_pixel(int x, int y, Color color)
{
	// ImageDrawPixel is FUCKING SLOW
	// this tries to mitigate that
	// but dont forget to call begin_draw and end_draw
	this->rawPixelsForDraw[y*(int)CHUNK_SIZE + x] = color;
}

class Pen {
public:
	unsigned long id;
	int x, y;
	Direction movement;
	void step(std::list<Pen>& out);
	Pen(const int x, const int y, const Direction movement);
	bool operator==(const Pen& rhs) {
		return this->id == rhs.id;
	}
};

void Pen::step(std::list<Pen>& out)
{
	switch (this->movement) {
	case Direction::UP:
		this->y -= 1;
		break;
	case Direction::DOWN:
		this->y += 1;
		break;
	case Direction::LEFT:
		this->x -= 1;
		break;
	case Direction::RIGHT:
		this->x += 1;
		break;
	}

	if (this->x >= CHUNK_SIZE) {
		this->x = 0;
	} else if (this->x < 0) {
		this->x = CHUNK_SIZE-1;
	}
	if (this->y >= CHUNK_SIZE) {
		this->y = 0;
	} else if (this->y < 0) {
		this->y = CHUNK_SIZE - 1;
	}

	// We need to create new pens before moving this one so that they appear to emerge from a common point
	if (rand() % 1000 > 975) {
		int type = rand() % 3;
		switch (type) {
		case 0:
			// Create new clockwise
			out.push_back(Pen(this->x, this->y, static_cast<Direction>((this->movement + 1) % 4)));
			break;
		case 1:
			// Create new counterclockwise
			out.push_back(Pen(this->x, this->y, static_cast<Direction>((this->movement + 3) % 4)));
			break;
		case 2:
			// Create new bothwise
			out.push_back(Pen(this->x, this->y, static_cast<Direction>((this->movement + 1) % 4)));
			out.push_back(Pen(this->x, this->y, static_cast<Direction>((this->movement + 3) % 4)));
			break;
		}
	}
	else if (rand() % 1000 > 970) {
		int type = rand() % 2;
		switch (type) {
		case 0:
			this->movement = static_cast<Direction>((this->movement + 1) % 4);
			break;
		case 1:
			this->movement = static_cast<Direction>((this->movement + 3) % 4);
			break;
		}
	}
}

Pen::Pen(const int x, const int y, const Direction movement) {
	static unsigned long current_id = 0;
	this->id = current_id++;
	this->x = x;
	this->y = y;
	this->movement = movement;
}

auto pens = std::list<Pen>();
const int w = 1024, h = 1024;
std::chrono::time_point<std::chrono::steady_clock> time_start, time_update_end, time_draw_end;

void update(Chunk& img) {
	static float current_hue = 0.0f;
	img.begin_draw();
	auto newPens = std::list<Pen>();
	auto deletePens = std::list<Pen>();
	// CurrentImage is a snapshot of what it looked like before new pixels were added
	Color* currentImage = GetImageData(img.image);
	for (auto& p : pens) {
		if (!ColorEqual(currentImage[p.y*CHUNK_SIZE + p.x], BLACK)
			&& !ColorEqual(currentImage[p.y*CHUNK_SIZE + p.x], RED)) {
			deletePens.push_back(p);
			continue;
		}
		img.draw_pixel(p.x, p.y, ColorFromHSV(Vector3{ current_hue, 0.7f, 1.0f }));
		p.step(newPens);
		if (ColorEqual(currentImage[p.y*CHUNK_SIZE + p.x], BLACK)) {
			img.draw_pixel(p.x, p.y, RED);
		}
	}
	free(currentImage);
	img.end_draw();

	for (auto& p : deletePens) {
		pens.remove(p);
	}
	// After drawing the current pens, pass the new ones to the current painting context
	pens.splice(pens.end(), newPens);
	current_hue += 1.0f;
	if (current_hue >= 360.0f) {
		current_hue = 0.0f;
	}
	time_update_end = std::chrono::high_resolution_clock::now();
}

Camera2D camera = Camera2D{
	Vector2 {w / 2.0f, h / 2.0f}, //Target
	Vector2 {w / 2.0f, h / 2.0f}, //Offset
	0.0f, //Rotation,
	1.0f //Zoom
};
void draw(Chunk& img) {
	BeginDrawing();
	ClearBackground(DARKGRAY);
	BeginMode2D(camera);
	DrawTexture(img.texture, 0, 0, WHITE);
	EndMode2D();

	// Only thing left to draw is the debug text
	time_draw_end = std::chrono::high_resolution_clock::now();

	char debugText[100];
	sprintf_s(debugText, "pens: %d\nupdate total: %.2f ms\ndraw: %.2f ms", pens.size(),
		std::chrono::duration_cast<std::chrono::microseconds>(time_update_end - time_start).count() / 1000.0f,
		std::chrono::duration_cast<std::chrono::microseconds>(time_draw_end - time_update_end).count() / 1000.0f
	);
	DrawText(debugText, 10, 10, 10, RED);

	EndDrawing();
	
}

constexpr auto FINISH_ANIM_LENGTH = 300;
int finishAnimFrame = 0;
int main()
{
	time_start = std::chrono::high_resolution_clock::now();
	time_update_end = std::chrono::high_resolution_clock::now();
	time_draw_end = std::chrono::high_resolution_clock::now();
	std::srand(std::time(NULL));

	InitWindow(w, h, "branches by dankons | gampixi");
	Chunk img = Chunk(BLACK);
	SetTargetFPS(60);

	pens.push_back(Pen(CHUNK_SIZE/2, CHUNK_SIZE/2, static_cast<Direction>(rand() % 4)));

	while (!WindowShouldClose()) {
		time_start = std::chrono::high_resolution_clock::now();
		if (pens.size() > 0) {
			update(img);
		} else if (finishAnimFrame < FINISH_ANIM_LENGTH) {
			finishAnimFrame++;
			if (finishAnimFrame > FINISH_ANIM_LENGTH / 3) {
				float progress = (float)(finishAnimFrame - FINISH_ANIM_LENGTH / 3) / (float)(FINISH_ANIM_LENGTH - FINISH_ANIM_LENGTH / 3);
				if (progress < 0.001f) {
					progress = 0.001f;
				}
				float zoom = 1.0f / (1.0f-progress);
				camera.zoom = zoom;
			}
			time_update_end = std::chrono::high_resolution_clock::now();
		} else {
			UnloadTexture(img.texture); // VRAM memory does not get freed automatically
			img = Chunk(BLACK);
			pens.push_back(Pen(CHUNK_SIZE / 2, CHUNK_SIZE / 2, static_cast<Direction>(rand() % 4)));
			camera.zoom = 1.0f;
			finishAnimFrame = 0;
		}

		draw(img);
	}

	return 0;
}