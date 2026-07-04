#include <SDL.h>
#include <SDL_image.h>
#include <iostream>
#include <cmath>
#include "level_gen.cuh"
#include <SDL_ttf.h>

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

    GameState state = STATE_LOGO;
    Uint32 logoStartTime = SDL_GetTicks();
    
    Ball playerBall;
    LevelData currentLevel;
    int selectedDifficulty = 1; // 1: Easy, 2: Medium, 3: Hard

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;

            if (state == STATE_MENU) {
                // Simplified Menu Input
                if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_1) { selectedDifficulty = 1; state = STATE_LOADING; }
                    if (event.key.keysym.sym == SDLK_2) { selectedDifficulty = 2; state = STATE_LOADING; }
                    if (event.key.keysym.sym == SDLK_3) { selectedDifficulty = 3; state = STATE_LOADING; }
                    if (event.key.keysym.sym == SDLK_ESCAPE) running = false;
                }
            } 
            else if (state == STATE_PLAYING && !playerBall.isMoving) {
                // Gameplay Mechanics: Aiming and Power
                int mx, my;
                SDL_GetMouseState(&mx, &my);
                playerBall.aimAngle = atan2(my - playerBall.pos.y, mx - playerBall.pos.x);

                if (event.type == SDL_MOUSEWHEEL) {
                    // Set power forwards/backwards
                    playerBall.powerSetting += event.wheel.y * 5.0f; 
                    if(playerBall.powerSetting < 10.0f) playerBall.powerSetting = 10.0f;
                    if(playerBall.powerSetting > 150.0f) playerBall.powerSetting = 150.0f;
                }
                
                if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                    // Shoot ball
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
            // Loading Screen Update: Offload to CUDA to generate and verify
            // Note: In a true production app, you'd run this on a separate thread 
            // so the SDL renderer can draw the loading bar animating.
            currentLevel = GenerateAndVerifyLevel(selectedDifficulty);
            
            playerBall.pos = currentLevel.startPos;
            playerBall.vel = {0,0};
            playerBall.strokesTaken = 0;
            playerBall.isMoving = false;
            
            state = STATE_PLAYING;
        }
        else if (state == STATE_PLAYING && playerBall.isMoving) {
            // Apply Physics CPU-side for the actual gameplay
            playerBall.pos.x += playerBall.vel.x * 0.016f;
            playerBall.pos.y += playerBall.vel.y * 0.016f;
            
            playerBall.vel.x *= 0.98f; // Friction
            playerBall.vel.y *= 0.98f;
            
            if (abs(playerBall.vel.x) < 2.0f && abs(playerBall.vel.y) < 2.0f) {
                playerBall.isMoving = false; // Ball stopped
                if (playerBall.strokesTaken >= currentLevel.maxStrokes) {
                    state = STATE_GAMEOVER; // Loss condition
                }
            }
        }

        // --- RENDERING ---
        SDL_SetRenderDrawColor(renderer, 50, 150, 50, 255); // Grass green
        SDL_RenderClear(renderer);

        if (state == STATE_LOGO) {
            // Draw logo image (Load using IMG_LoadTexture in setup)
        } 
        else if (state == STATE_MENU) {
            // Draw text: "Press 1 (Easy), 2 (Medium), 3 (Hard) to Play, ESC to Quit"
        } 
        else if (state == STATE_LOADING) {
            // Draw loading bar
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_Rect loadBar = {200, 280, 400, 40};
            SDL_RenderFillRect(renderer, &loadBar);
        }
        else if (state == STATE_PLAYING) {
            // Draw Hole
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_Rect holeRect = {(int)currentLevel.holePos.x - 10, (int)currentLevel.holePos.y - 10, 20, 20};
            SDL_RenderFillRect(renderer, &holeRect);

            // Draw Obstacles
            SDL_SetRenderDrawColor(renderer, 100, 50, 0, 255);
            for (auto& obs : currentLevel.obstacles) {
                SDL_Rect r = {(int)obs.x, (int)obs.y, (int)obs.width, (int)obs.height};
                SDL_RenderFillRect(renderer, &r);
            }

            // Draw Ball
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_Rect ballRect = {(int)playerBall.pos.x - 5, (int)playerBall.pos.y - 5, 10, 10};
            SDL_RenderFillRect(renderer, &ballRect);

            // Draw Aiming Line & Cursor
            if (!playerBall.isMoving) {
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                int lineEndX = playerBall.pos.x + cos(playerBall.aimAngle) * playerBall.powerSetting;
                int lineEndY = playerBall.pos.y + sin(playerBall.aimAngle) * playerBall.powerSetting;
                SDL_RenderDrawLine(renderer, playerBall.pos.x, playerBall.pos.y, lineEndX, lineEndY);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
    return 0;
}