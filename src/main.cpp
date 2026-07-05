#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <iostream>
#include <vector>

#include "RenderWindow.h"
#include "Entity.h"
#include "Ball.h"	
#include "Tile.h"
#include "Hole.h"
#include "level_gen.cuh" // <-- WE INCLUDE CUDA HERE

bool init()
{
	if (SDL_Init(SDL_INIT_VIDEO) > 0)
		std::cout << "HEY.. SDL_Init HAS FAILED. SDL_ERROR: " << SDL_GetError() << std::endl;
	if (!(IMG_Init(IMG_INIT_PNG)))
		std::cout << "IMG_init has failed. Error: " << SDL_GetError() << std::endl;
	if (!(TTF_Init()))
		std::cout << "TTF_init has failed. Error: " << SDL_GetError() << std::endl;
	Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
	return true;
}

bool SDLinit = init();

RenderWindow window("Twini-Golf", 640, 480);

// Load Textures
SDL_Texture* ballTexture = window.loadTexture("res/gfx/ball.png");
SDL_Texture* holeTexture = window.loadTexture("res/gfx/hole.png");
SDL_Texture* pointTexture = window.loadTexture("res/gfx/point.png");
SDL_Texture* tileDarkTexture32 = window.loadTexture("res/gfx/tile32_dark.png");
SDL_Texture* ballShadowTexture = window.loadTexture("res/gfx/ball_shadow.png");
SDL_Texture* bgTexture = window.loadTexture("res/gfx/bg.png");
SDL_Texture* uiBgTexture = window.loadTexture("res/gfx/UI_bg.png");
SDL_Texture* levelTextBgTexture = window.loadTexture("res/gfx/levelText_bg.png");
SDL_Texture* powerMeterTexture_FG = window.loadTexture("res/gfx/powermeter_fg.png");
SDL_Texture* powerMeterTexture_BG = window.loadTexture("res/gfx/powermeter_bg.png");
SDL_Texture* powerMeterTexture_overlay = window.loadTexture("res/gfx/powermeter_overlay.png");
SDL_Texture* logoTexture = window.loadTexture("res/gfx/logo.png");
SDL_Texture* click2start = window.loadTexture("res/gfx/click2start.png");
SDL_Texture* endscreenOverlayTexture = window.loadTexture("res/gfx/end.png");
SDL_Texture* splashBgTexture = window.loadTexture("res/gfx/splashbg.png");

Mix_Chunk* chargeSfx = Mix_LoadWAV("res/sfx/charge.mp3");
Mix_Chunk* swingSfx = Mix_LoadWAV("res/sfx/swing.mp3");
Mix_Chunk* holeSfx = Mix_LoadWAV("res/sfx/hole.mp3");

SDL_Color white = { 255, 255, 255 };
SDL_Color black = { 0, 0, 0 };
TTF_Font* font32 = TTF_OpenFont("res/font/font.ttf", 32);
TTF_Font* font48 = TTF_OpenFont("res/font/font.ttf", 48);
TTF_Font* font24 = TTF_OpenFont("res/font/font.ttf", 24);

Ball balls[2] = {Ball(Vector2f(0, 0), ballTexture, pointTexture, powerMeterTexture_FG, powerMeterTexture_BG, 0), Ball(Vector2f(0, 0), ballTexture, pointTexture, powerMeterTexture_FG, powerMeterTexture_BG, 1)};
std::vector<Hole> holes = {Hole(Vector2f(0, 0), holeTexture), Hole(Vector2f(0, 0), holeTexture)};

// Create an empty tiles vector. We NO LONGER use loadTiles()
std::vector<Tile> tiles;

int level = 0;
bool gameRunning = true;
bool mouseDown = false;
bool mousePressed = false;
bool swingPlayed = false;
bool secondSwingPlayed = false;

SDL_Event event;
int state = 0; //0 = title screen, 1 = game, 2 = end screen
Uint64 currentTick = SDL_GetPerformanceCounter();
Uint64 lastTick = 0;
double deltaTime = 0;

void loadLevel(int current_level_idx)
{
	// Instead of a limit, let's make it infinite procedural golf!
    
	balls[0].setVelocity(0, 0);
	balls[1].setVelocity(0,0);
    balls[0].setScale(1, 1);
	balls[1].setScale(1, 1);
	balls[0].setWin(false);
	balls[1].setWin(false);

    // 1. CLEAR OLD TILES AND GENERATE NEW ONES VIA CUDA
    tiles.clear();
    LevelData genData = GenerateAndVerifyLevel(3);

    // 2. SETUP PLAYER 1 (Left Screen)
    balls[0].setPos(genData.startPos.x, genData.startPos.y);
    holes.at(0).setPos(genData.holePos.x, genData.holePos.y);

    for (const auto& obs : genData.obstacles) {
        // Map CUDA obstacles directly into C++ OOP Tiles
        tiles.push_back(Tile(Vector2f(obs.x, obs.y), tileDarkTexture32));
    }

    // 3. SETUP PLAYER 2 (Right Screen)
    float rightOffset = 320.0f; // Shift everything to the right half
    balls[1].setPos(genData.startPos.x + rightOffset, genData.startPos.y);
    holes.at(1).setPos(genData.holePos.x + rightOffset, genData.holePos.y);

    for (const auto& obs : genData.obstacles) {
        tiles.push_back(Tile(Vector2f(obs.x + rightOffset, obs.y), tileDarkTexture32));
    }
}

enum GameState { STATE_LOGO, STATE_MENU, STATE_LOADING, STATE_PLAYING, STATE_GAMEOVER };

struct Ball {
    Vec2 pos;
    Vec2 vel;
    float powerSetting = 50.0f;
    float aimAngle = 0.0f;
    bool isMoving = false;
    int strokesTaken = 0;
};

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    SDL_Window* window = SDL_CreateWindow("Twini Cuda Golf", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (TTF_Init() == -1) {
        std::cerr << "TTF_Init Error: " << TTF_GetError() << std::endl;
        return 1;
    }

    // BULLETPROOF ASSET PATHS
    char* basePath = SDL_GetBasePath();
    std::string resPath = basePath ? std::string(basePath) + "res/" : "res/";
    if (basePath) SDL_free(basePath);

    // Load Font
    TTF_Font* mainFont = TTF_OpenFont((resPath + "font/font.ttf").c_str(), 28);
    if (!mainFont) std::cerr << "Failed to load font! " << TTF_GetError() << std::endl;

    // Load Textures
    SDL_Texture* texLogo = IMG_LoadTexture(renderer, (resPath + "gfx/logo.png").c_str());
    SDL_Texture* texBall = IMG_LoadTexture(renderer, (resPath + "gfx/ball.png").c_str());
    SDL_Texture* texHole = IMG_LoadTexture(renderer, (resPath + "gfx/hole.png").c_str());
    SDL_Texture* texBg = IMG_LoadTexture(renderer, (resPath + "gfx/bg.png").c_str());
    SDL_Texture* texTileLight = IMG_LoadTexture(renderer, (resPath + "gfx/tile32_light.png").c_str());
    
    if (!texLogo || !texBall || !texHole) {
        std::cerr << "Failed to load one or more textures! " << IMG_GetError() << std::endl;
    }

    GameState state = STATE_LOGO;
    Uint32 logoStartTime = SDL_GetTicks();
    
    Ball playerBall;
    LevelData currentLevel;
    int selectedDifficulty = 1;

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;

            if (state == STATE_MENU) {
                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_1) { selectedDifficulty = 1; state = STATE_LOADING; }
                    if (event.key.keysym.sym == SDLK_2) { selectedDifficulty = 2; state = STATE_LOADING; }
                    if (event.key.keysym.sym == SDLK_3) { selectedDifficulty = 3; state = STATE_LOADING; }
                    if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
                }
            } 
            else if (state == STATE_PLAYING && !playerBall.isMoving) {
                int mx, my;
                SDL_GetMouseState(&mx, &my);
                playerBall.aimAngle = atan2(my - playerBall.pos.y, mx - playerBall.pos.x);

                if (event.type == SDL_MOUSEWHEEL) {
                    playerBall.powerSetting += event.wheel.y * 5.0f; 
                    if(playerBall.powerSetting < 10.0f) playerBall.powerSetting = 10.0f;
                    if(playerBall.powerSetting > 150.0f) playerBall.powerSetting = 150.0f;
                }
                
                if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                    playerBall.vel.x = cos(playerBall.aimAngle) * playerBall.powerSetting;
                    playerBall.vel.y = sin(playerBall.aimAngle) * playerBall.powerSetting;
                    playerBall.isMoving = true;
                    playerBall.strokesTaken++;
                }
            }
        }

        // --- UPDATES ---
        if (state == STATE_LOGO) {
            if (SDL_GetTicks() - logoStartTime > 3000) state = STATE_MENU;
        } 
        else if (state == STATE_LOADING) {
            currentLevel = GenerateAndVerifyLevel(selectedDifficulty);
            
            playerBall.pos = currentLevel.startPos;
            playerBall.vel = {0,0};
            playerBall.strokesTaken = 0;
            playerBall.isMoving = false;
            
            state = STATE_PLAYING;
        }
        else if (state == STATE_PLAYING && playerBall.isMoving) {
            playerBall.pos.x += playerBall.vel.x * 0.016f;
            playerBall.pos.y += playerBall.vel.y * 0.016f;
            
            playerBall.vel.x *= 0.98f;
            playerBall.vel.y *= 0.98f;
            
            if (abs(playerBall.vel.x) < 2.0f && abs(playerBall.vel.y) < 2.0f) {
                playerBall.isMoving = false;
                if (playerBall.strokesTaken >= currentLevel.maxStrokes) {
                    state = STATE_GAMEOVER;
                }
            }
        }

        // --- RENDERING ---
        SDL_SetRenderDrawColor(renderer, 50, 150, 50, 255);
        SDL_RenderClear(renderer);

        if (state == STATE_LOGO) {
            SDL_RenderCopy(renderer, texBg, NULL, NULL);
            SDL_Rect logoRect = {200, 200, 400, 200};
            SDL_RenderCopy(renderer, texLogo, NULL, &logoRect);
        } 
        else if (state == STATE_MENU) {
            SDL_RenderCopy(renderer, texBg, NULL, NULL);
            
            // DRAW THE MENU TEXT
            if (mainFont) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Surface* textSurf = TTF_RenderText_Solid(mainFont, "Press 1 (Easy), 2 (Med), 3 (Hard)", textColor);
                if (textSurf) {
                    SDL_Texture* textTex = SDL_CreateTextureFromSurface(renderer, textSurf);
                    SDL_Rect textRect = { 400 - (textSurf->w / 2), 300 - (textSurf->h / 2), textSurf->w, textSurf->h };
                    SDL_RenderCopy(renderer, textTex, NULL, &textRect);
                    SDL_FreeSurface(textSurf);
                    SDL_DestroyTexture(textTex);
                }
            }
        } 
        else if (state == STATE_LOADING) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_Rect loadBar = {200, 280, 400, 40};
            SDL_RenderFillRect(renderer, &loadBar);
        }
        else if (state == STATE_PLAYING) {
            SDL_RenderCopy(renderer, texBg, NULL, NULL);

            SDL_Rect holeRect = {(int)currentLevel.holePos.x - 16, (int)currentLevel.holePos.y - 16, 32, 32};
            SDL_RenderCopy(renderer, texHole, NULL, &holeRect);

            for (auto& obs : currentLevel.obstacles) {
                SDL_Rect r = {(int)obs.x, (int)obs.y, (int)obs.width, (int)obs.height};
                SDL_RenderCopy(renderer, texTileLight, NULL, &r);
            }

            SDL_Rect ballRect = {(int)playerBall.pos.x - 8, (int)playerBall.pos.y - 8, 16, 16};
            SDL_RenderCopy(renderer, texBall, NULL, &ballRect);

            if (!playerBall.isMoving) {
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                int lineEndX = playerBall.pos.x + cos(playerBall.aimAngle) * playerBall.powerSetting;
                int lineEndY = playerBall.pos.y + sin(playerBall.aimAngle) * playerBall.powerSetting;
                SDL_RenderDrawLine(renderer, playerBall.pos.x, playerBall.pos.y, lineEndX, lineEndY);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyTexture(texLogo);
    SDL_DestroyTexture(texBall);
    SDL_DestroyTexture(texHole);
    SDL_DestroyTexture(texBg);
    SDL_DestroyTexture(texTileLight);
    TTF_CloseFont(mainFont);
    TTF_Quit();
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}