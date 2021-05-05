#include <SDL2/SDL.h>

#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>



#define WIDTH 64
#define HEIGHT 32

#define ROM_START 0x200
#define FONT_START 0x50

const uint8_t keyMap[16] = {
    SDL_SCANCODE_X,
    SDL_SCANCODE_1,
    SDL_SCANCODE_2,
    SDL_SCANCODE_3,
    SDL_SCANCODE_Q,
    SDL_SCANCODE_W,
    SDL_SCANCODE_E,
    SDL_SCANCODE_A,
    SDL_SCANCODE_S,
    SDL_SCANCODE_D,
    SDL_SCANCODE_Z,
    SDL_SCANCODE_C,
    SDL_SCANCODE_4,
    SDL_SCANCODE_R,
    SDL_SCANCODE_F,
    SDL_SCANCODE_V
};

const uint8_t font[] =
{
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

struct {
    // Memory
    uint8_t heap[4096];
    uint16_t stack[16];

    // Registers
    uint8_t v[16]; // Registers
    uint16_t i; // Index Register
    uint16_t pc; // Program Counter
    uint8_t sp; // Stack Pointer

    // Timers
    uint8_t delay_timer; // Delay Timer
    uint8_t sound_timer; // Sound Timer

} chip8;

int main(int argc, char *argv[]) {
    // Seed random generator with time
    srand(time(NULL));

    // Load Fonts
    memcpy(chip8.heap + FONT_START, font, sizeof(font));

    // Initialize PC
    chip8.pc = ROM_START;

    // Load ROM
    if (argc <= 1) {
        fprintf(stderr, "Missing file argument\n");
        return 1;
    }
    
    FILE* file = fopen(argv[1], "rb");
    if(!file) {
        fprintf(stderr, "fopen failed: %s\n", strerror(errno));
        return 1;
    }

    fread(chip8.heap + ROM_START, 1, sizeof(chip8.heap) - ROM_START, file);
    if(ferror(file)) {
        fprintf(stderr, "fread failed: %s\n", strerror(errno));
        return 1;
    }

    if(fclose(file)) fprintf(stderr, "Warning fclose failed: %s\n", strerror(errno));


    // Initialize SDL
    if(SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    //Create Window
    SDL_Window* window = SDL_CreateWindow("Chip-8 Emulator",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            WIDTH, HEIGHT,
            0);
    
    if(!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }

    // Create Window Surface
    SDL_Surface* windowSurface = SDL_GetWindowSurface(window);

    if(!windowSurface) {
        fprintf(stderr, "SDL_GetWindowSurface failed: %s\n", SDL_GetError());
        return 1;
    }
    if(windowSurface->format->BytesPerPixel != 4) {
        fprintf(stderr, "Unsupported pixel format");
        return 1;
    }

    bool flip = false;
    const uint8_t* keys = SDL_GetKeyboardState(NULL);
    if(!keys) {
        fprintf(stderr, "SDL_GetKeyboardState failed: %s\n", SDL_GetError());
        return 1;
    }

    uint8_t fake_sp = 0;
    // Main Loop
    while(true) {
        // Events
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if(event.type == SDL_QUIT) {
                SDL_Quit();
                return 0;
            }
        }


        // Fetch
        const uint16_t opcode = ((uint16_t)chip8.heap[chip8.pc & 0x0FFFu] << 8u) | (uint16_t)chip8.heap[(chip8.pc + 1) & 0x0FFFu];
        chip8.pc += 2;
        //Decode and Execute
        switch (opcode >> 12u) {
            case 0x0u:
                switch (opcode & 0x00FF) {
                    case 0xe0u: // CLS
                        memset(windowSurface->pixels, 0, windowSurface->w * windowSurface->h * windowSurface->format->BytesPerPixel);
                    break;
                    case 0xeeu: //RET
                        chip8.pc = chip8.stack[--chip8.sp & 0x0Fu];
                    break;
                    default:
                        fprintf(stderr, "Unknown Instruction 0x%x\n", opcode);
                    break;
                }
                
            break;

            case 0x1u: // JP
                chip8.pc = opcode & 0x0FFFu;
            break;

            case 0x2u: // CALL
                chip8.stack[chip8.sp++ & 0x0Fu] = chip8.pc;
                chip8.pc = opcode & 0x0FFFu;
            break;

            case 0x3u: // SE
                if(chip8.v[(opcode >> 8u) & 0x0Fu] == (opcode & 0x00FFu)) chip8.pc += 2;
            break;

            case 0x4u: // SNE
                if(chip8.v[(opcode >> 8u) & 0x0Fu] != (opcode & 0x00FFu)) chip8.pc += 2;
            break;

            case 0x5u: // SE
                if(chip8.v[(opcode >> 8u) & 0x0Fu] == chip8.v[(opcode >> 4u) & 0x00Fu]) chip8.pc += 2;
            break;

            case 0x6u: // LD 
                chip8.v[(opcode >> 8u) & 0x0Fu] = opcode & 0x00FFu;
            break;

            case 0x7u: // ADD
                chip8.v[(opcode >> 8u) & 0x0Fu] += opcode & 0x00FFu;
            break;

            case 0x8u: 
                switch (opcode & 0x000Fu) {
                    case 0x0u: // LD
                        chip8.v[(opcode >> 8u) & 0x0Fu] = chip8.v[(opcode >> 4u) & 0x00Fu];
                    break;
                    case 0x1u: // OR
                        chip8.v[(opcode >> 8u) & 0x0Fu] |= chip8.v[(opcode >> 4u) & 0x00Fu];
                    break;
                    case 0x2u: // AND
                        chip8.v[(opcode >> 8u) & 0x0Fu] &= chip8.v[(opcode >> 4u) & 0x00Fu];
                    break;
                    case 0x3u: // XOR
                        chip8.v[(opcode >> 8u) & 0x0Fu] ^= chip8.v[(opcode >> 4u) & 0x00Fu];
                    break;

                    case 0x4u: // ADD
                        chip8.v[0xFu] = __builtin_add_overflow(chip8.v[(opcode >> 8u) & 0x0Fu], chip8.v[(opcode >> 4u) & 0x00Fu], &chip8.v[(opcode >> 8u) & 0x0Fu]);
                    break;

                    case 0x5u: // SUB
                        chip8.v[0xFu] = !__builtin_sub_overflow(chip8.v[(opcode >> 8u) & 0x0Fu], chip8.v[(opcode >> 4u) & 0x00Fu], &chip8.v[(opcode >> 8u) & 0x0Fu]);
                    break;

                    case 0x6u: // SHR
                        chip8.v[0xFu] = chip8.v[(opcode >> 8u) & 0x0Fu] & 0x1u;
                        chip8.v[(opcode >> 8u) & 0x0Fu] >>= 1;
                    break;

                    case 0x7u: // SUBN
                        chip8.v[0xFu] = !__builtin_sub_overflow(chip8.v[(opcode >> 4u) & 0x00Fu], chip8.v[(opcode >> 8u) & 0x0Fu], &chip8.v[(opcode >> 8u) & 0x0Fu]);
                    break;

                    case 0xEu: // SHL
                        chip8.v[0xFu] = chip8.v[(opcode >> 8u) & 0x0Fu] >> 7u;
                        chip8.v[(opcode >> 8u) & 0x0Fu] <<= 1;
                    break;

                    default:
                        fprintf(stderr, "Unknown Instruction 0x%x\n", opcode);
                    break;
                }
            break;
            case 0x9u: // SNE
                if(chip8.v[(opcode >> 8u) & 0x0Fu] != chip8.v[(opcode >> 4u) & 0x00Fu]) chip8.pc += 2;
            break;
            case 0xAu: // LD
                chip8.i = opcode & 0x0FFFu;
            break;
            case 0xBu: // JP
                chip8.pc = opcode & 0x0FFFu + (uint16_t)chip8.v[0x0u];
            break;
            case 0xCu: // RND
                chip8.v[(opcode >> 8u) & 0x0Fu] = (uint8_t)(rand() % 0x100) & (uint8_t)(opcode & 0x00FF);
            break;
            case 0xDu:{ // DRW
                flip = true;
                uint8_t x = chip8.v[(opcode >> 8u) & 0x0Fu] % windowSurface->w;
                uint8_t y = chip8.v[(opcode >> 4u) & 0x00Fu] % windowSurface->h;
                chip8.v[0xFu] = 0u;
                for (uint8_t row = 0; row < (uint8_t)(opcode & 0x000F); row++) {
                    for (uint8_t column = 0; column < 8; column++) {
                        uint32_t* screenPixel = (uint32_t*)windowSurface->pixels + ((y + row) * windowSurface->w) + (x + column);
                        bool spritePixel = (chip8.heap[(chip8.i + row) & 0x0FFFu]) & (0x80u >> column);
                        if(spritePixel) {
                            if(*screenPixel) {
                                chip8.v[0xFu] = 1u;
                            }
                            *screenPixel ^= 0x00FFFFFFu;
                        }
                    }
                }
            }

            break;

            case 0xEu: 
                switch (opcode & 0x00FFu) {
                    case 0x9Eu:  // SKP
                        if(keys[keyMap[chip8.v[(opcode >> 8u) & 0x0Fu]]])
                            chip8.pc += 2;
                    break;
                    case 0xA1u: //SKNP
                        if(!keys[keyMap[chip8.v[(opcode >> 8u) & 0x0Fu]]])
                            chip8.pc += 2;
                    break;
                    default:
                        fprintf(stderr, "Unknown instruction 0x%x\n", opcode);
                    break;
                }
            break;

            case 0xFu:
                switch (opcode & 0x00FFu) {

                    case 0x07u: // LD Vx, DT
                        chip8.v[(opcode >> 8u) & 0x0Fu] = chip8.delay_timer;
                    break;
                    case 0x0Au: { 
                        bool keyPressed = false;
                        while (!keyPressed) {
                            for (uint8_t i = 0; i < sizeof(keyMap)/sizeof(keyMap[0]); i++) {
                                if(keys[keyMap[i]]) {
                                    chip8.v[(opcode >> 8u) & 0x0Fu] = i;
                                    keyPressed = true;
                                    break;
                                }
                            }
                            if(!keyPressed) {
                                bool wait = true;
                                while (wait) {
                                    if(!SDL_WaitEvent(&event)) fprintf(stderr, "Warning SDL_WaitEvent failed: %s", SDL_GetError());

                                    switch (event.type) {
                                        case SDL_QUIT:
                                            SDL_Quit();
                                            return 0;
                                        break;
                                        case SDL_KEYDOWN:
                                            wait = false;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    break;
                    case 0x15u:
                        chip8.delay_timer = chip8.v[(opcode >> 8u) & 0x0Fu];
                    break;
                    case 0x18u:
                        chip8.sound_timer = chip8.v[(opcode >> 8u) & 0x0Fu];
                    break;
                    case 0x1Eu:
                        chip8.i += chip8.v[(opcode >> 8u) & 0x0Fu];
                    break;
                    case 0x29u:
                        chip8.i = FONT_START + (5u * chip8.v[(opcode >> 8u) & 0x0Fu]);
                    break;
                    case 0x33u: {
                        uint8_t value = chip8.v[(opcode >> 8u) & 0x0Fu];
                        chip8.heap[(chip8.i + 2) & 0x0FFFu] = value % 10;
                        chip8.heap[(chip8.i + 1) & 0x0FFFu] = (value /= 10u) % 10;
                        chip8.heap[(chip8.i) & 0x0FFFu] = (value /= 10u) % 10;
                    }  
                    break;
                    case 0x55:
                        for (uint8_t i = 0; i <= ((opcode >> 8u) & 0x0Fu); i++) {
                            chip8.heap[(chip8.i + i) & 0x0FFFu] = chip8.v[i];
                        }
                    break;
                    case 0x65:
                        for (uint8_t i = 0; i <= ((opcode >> 8u) & 0x0Fu); i++) {
                            chip8.v[i] = chip8.heap[(chip8.i + i) & 0x0FFFu];
                        }
                    break;
                    default:
                        fprintf(stderr, "Unknown instruction 0x%x\n", opcode);
                    break;
                }
            break;
            default:
                fprintf(stderr, "Unknown instruction 0x%x\n", opcode);
            break;
        }

        if(flip) {
            SDL_UpdateWindowSurface(window);
            flip = false;
        }
        
        if (chip8.delay_timer > 0) chip8.delay_timer--;

        if (chip8.sound_timer > 0) {
            // TODO: Implement Sound
            chip8.sound_timer--;
        }

        SDL_Delay(1);
        
    }
    
    return 0;
}