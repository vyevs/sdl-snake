#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "SDL.h"
#include "SDL_error.h"
#include "SDL_video.h"
#include "SDL_timer.h"
#include "SDL_thread.h"

#include "types.h"

#define GRID_WIDTH 40
#define GRID_HEIGHT 40

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800

#define INITIAL_SNAKE_LEN 10

struct snake_piece {
    s32 x, y;
    struct snake_piece *next;
};

struct vec2 {
    s32 x, y;
};

#define VEC2S_EQUAL(v1, v2) ((v1.x) == (v2.x) && (v1.y) == (v2.y))

const struct vec2 DIRECTION_UP = {
    .x = 0, .y = -1
};
const struct vec2 DIRECTION_DOWN = {
    .x = 0, .y = 1
};
const struct vec2 DIRECTION_LEFT = {
    .x = -1, .y = 0
};
const struct vec2 DIRECTION_RIGHT = {
    .x = 1, .y = 0
};

struct vec2 directions[4] = {
    DIRECTION_UP, DIRECTION_DOWN, DIRECTION_LEFT, DIRECTION_RIGHT
};

struct snake {
    // snake head is where new position is placed, tail is where last position is removed
    struct snake_piece *head, *tail;

    struct vec2 direction;

    u32 bound_x, bound_y;

    struct vec2 food_pos;

    u32 score;

    bool died;
};

void init_snake(struct snake *snake) {
    snake->bound_x = GRID_WIDTH;
    snake->bound_y = GRID_HEIGHT;

    snake->direction = directions[rand() % 4];


    // want to draw the snake tail in the opposite direction of the initial direction
    struct vec2 tail_direction;
    struct vec2 dir = snake->direction;
    if (VEC2S_EQUAL(dir, DIRECTION_UP))
	tail_direction = DIRECTION_DOWN;
    else if (VEC2S_EQUAL(dir, DIRECTION_DOWN))
	tail_direction = DIRECTION_UP;
    else if (VEC2S_EQUAL(dir, DIRECTION_LEFT))
	tail_direction = DIRECTION_RIGHT;
    else if (VEC2S_EQUAL(dir, DIRECTION_RIGHT))
	tail_direction = DIRECTION_LEFT;
    else
	assert(false);


    snake->tail = NULL;

    for (u32 i=0; i<INITIAL_SNAKE_LEN; i++) {
	struct snake_piece *new_tail = malloc(sizeof(*new_tail));
	assert(new_tail);

	if (snake->tail) {
	    new_tail->x = snake->tail->x + tail_direction.x;
	    if (new_tail->x < 0)
		new_tail->x = snake->bound_x - 1;
	    else if (new_tail->x >= snake->bound_x)
		new_tail->x = 0;


	    new_tail->y = snake->tail->y + tail_direction.y;
	    if (new_tail->y < 0)
		new_tail->y = snake->bound_y - 1;
	    else if (new_tail->y >= snake->bound_y)
		new_tail->y = 0;

	   
	    new_tail->next = snake->tail;
	    snake->tail = new_tail;
	} else {
	    new_tail->next = NULL;
	    new_tail->x = rand() % snake->bound_x;
	    new_tail->y = rand() % snake->bound_y;

	    snake->head = snake->tail = new_tail;
	}
    }

    snake->food_pos.x = rand() % snake->bound_x;
    snake->food_pos.y = rand() % snake->bound_y;

    snake->score = 0;

    snake->died = false;
}

struct vec2 next_food_pos(const struct snake *snake)
{
    struct vec2 result;

    while (1) {
	result.x = rand() % snake->bound_x;
	result.y = rand() % snake->bound_y;

	for (struct snake_piece *walk = snake->tail; walk; walk = walk->next)
	    if (result.x == walk->x && result.y == walk->y)
		continue;
	
	break;
    }


    return result;
}

void move_snake(struct snake *snake) {
    assert(snake);

    struct snake_piece *new_piece = malloc(sizeof(*new_piece));
    assert(new_piece);

    new_piece->next = NULL;

    new_piece->x = snake->head->x + snake->direction.x;
    if (new_piece->x < 0)
	new_piece->x = snake->bound_x - 1;
    else if (new_piece->x >= snake->bound_x)
	new_piece->x = 0;

    new_piece->y = snake->head->y + snake->direction.y;
    if (new_piece->y < 0)
	new_piece->y = snake->bound_y - 1;
    else if (new_piece->y >= snake->bound_y)
	new_piece->y = 0;

    for (struct snake_piece *walk = snake->tail; walk; walk = walk->next) {
	if (new_piece->x == walk->x && new_piece->y == walk->y) {
	    snake->died = true;
	    return;
        }
    }

    snake->head->next = new_piece;
    snake->head = new_piece;


    bool ate = false;
    if (new_piece->x == snake->food_pos.x && new_piece->y == snake->food_pos.y) {
	ate = true;
	snake->score++;

	snake->food_pos = next_food_pos(snake);
    }


    // only remove a tail piece if we have not just consumed food
    // if we ate, this increases the length of the snake by 1
    if (!ate) {
	struct snake_piece *old_tail = snake->tail;
	snake->tail = old_tail->next;

	free(old_tail);
    }
}

void fatal(const char *msg) {
    assert(msg);
    const char *err_str = SDL_GetError();
    fprintf(stderr, "%s: %s\n", msg, err_str);
    exit(EXIT_FAILURE);
}

void draw_snake_to_surface(const struct snake *snake, SDL_Surface *surface) {
    assert(snake);
    assert(surface);

    if (SDL_FillRect(surface, NULL, 0xFFFFFF)  < 0)
	fatal("SDL_FillRect");

    u32 *pixels = (u32 *) surface->pixels;

    for (struct snake_piece *walk = snake->tail; walk; walk = walk->next) {
	u32 pixel_idx = walk->y * snake->bound_x + walk->x;

	pixels[pixel_idx] = 0;
    }

    u32 food_pixel_idx = snake->food_pos.y * snake->bound_x + snake->food_pos.x;
    pixels[food_pixel_idx] = 0;
}

struct audio_data {
    SDL_atomic_t len;
    u8 *pos;
};

void audio_callback(void *userdata, unsigned char *stream, int len) {
    struct audio_data *data = (struct audio_data *) userdata;

    SDL_memset(stream, 0, len);

    if (SDL_AtomicGet(&data->len) == 0)
	return;

    u32 data_len = SDL_AtomicGet(&data->len);

    if (len > data_len)
	len = data_len;

    SDL_MixAudio(stream, data->pos, len, SDL_MIX_MAXVOLUME);

    data->pos += len;
    SDL_AtomicAdd(&data->len, -len);
}

int audio(void *data) {
    const char *wav_file = "lux_aeterna.wav";

    SDL_AudioSpec wav_spec;

    u32 len;
    u8 *wav_buf;

    if (SDL_LoadWAV(wav_file, &wav_spec, &wav_buf, &len) == NULL)
	return 1;

    while (true) {
	printf("starting audio playback\n");
	struct audio_data audio_data;

	audio_data.pos = wav_buf;
	SDL_AtomicSet(&audio_data.len, len);

	wav_spec.callback = audio_callback;
	wav_spec.userdata = &audio_data;

	if (SDL_OpenAudio(&wav_spec, NULL) < 0) {
	    fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
	    return 1;
	}

	SDL_PauseAudio(0);

	while(SDL_AtomicGet(&audio_data.len) > 0)
	    SDL_Delay(100);
	
	SDL_CloseAudio();
    }

    return 0;
}


int main(int argc, char *argv[]) {
    srand(time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
	fatal("SDL_Init");


    SDL_Window *window = SDL_CreateWindow("Window", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 
	    WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if (!window)
	fatal("SDL_CreateWindow");

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
	fatal("SDL_GL_CreateContext");

    if (SDL_GL_SetSwapInterval(1) < 0)
	fatal("SDL_GL_SetSwapInterval");


    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer)
	fatal("SDL_CreateRenderer");

    SDL_Surface *window_surface = SDL_GetWindowSurface(window);
    if (!window_surface)
	fatal("SDL_GetWindowSurface");

    SDL_PixelFormat *window_surface_format = window_surface->format;

    SDL_Surface *grid_surface = SDL_CreateRGBSurfaceWithFormat(0, GRID_WIDTH, GRID_HEIGHT, 
	    window_surface_format->BitsPerPixel, window_surface_format->format);

    bool running = true;
    bool paused = false;

    struct snake snake;

    init_snake(&snake);


    SDL_Thread *thread = SDL_CreateThread(audio, "audio", NULL);
    if (!thread)
	fprintf(stderr, "unable to create audio thread: %s\n", SDL_GetError());

    // we make a snake move every target_ms ms
    f64 target_ms = 50;
    f64 accumulated_ms = 0;

    // this is to prevent input issues, e.g. left -> up -> right in quick succession could make the snake
    // start moving backwards into itself the next time a snake move is done
    // this makes sure we only allow one direction change per snake move
    bool moved_since_last_dir_change = true;

    while (running) {
	u64 start = SDL_GetPerformanceCounter();

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
	    if (event.type == SDL_QUIT) {
		running = false;

	    } else if (event.type == SDL_KEYDOWN) {
		switch (event.key.keysym.scancode) {
		    case SDL_SCANCODE_SPACE:
			paused = !paused;
			break;

		    case SDL_SCANCODE_UP:
			if (moved_since_last_dir_change && snake.direction.y != 1) {
			    moved_since_last_dir_change = false;
			    snake.direction.y = -1;
			    snake.direction.x = 0;
		        }
			break;

		    case SDL_SCANCODE_DOWN:
			if (moved_since_last_dir_change && snake.direction.y != -1) {
			    moved_since_last_dir_change = false;
			    snake.direction.y = 1;
			    snake.direction.x = 0;
		        }
			break;

		    case SDL_SCANCODE_LEFT:
			if (moved_since_last_dir_change && snake.direction.x != 1) {
			    moved_since_last_dir_change = false;
			    snake.direction.x = -1;
			    snake.direction.y = 0;
		        }
			break;

		    case SDL_SCANCODE_RIGHT:
			if (moved_since_last_dir_change && snake.direction.x != -1) {
			    moved_since_last_dir_change = false;
			    snake.direction.x = 1;
			    snake.direction.y = 0;
		        }
			break;

		    default:
			break;
		}

	    } else if (event.type == SDL_WINDOWEVENT) {
		if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
		    window_surface = SDL_GetWindowSurface(window);
		    SDL_FillRect(window_surface, NULL, 0xFFFFFF);
		}
	    } 
	}

	if (!paused && accumulated_ms > target_ms) {
	    accumulated_ms -= 50;
	    moved_since_last_dir_change = true;

	    move_snake(&snake);

	    if (snake.died) {
		printf("You died! Score: %d\n", snake.score);
		return EXIT_SUCCESS;
	    }

	    draw_snake_to_surface(&snake, grid_surface);

	    if (SDL_BlitScaled(grid_surface, NULL, window_surface, NULL) < 0)
		fatal("SDL_BlitScaled");

	    if (SDL_UpdateWindowSurface(window) < 0)
		fatal("SDL_UpdateWindowSurface");
	}

	u64 end = SDL_GetPerformanceCounter();

	f64 delta_time = 1000 * ((f64)end-start) / SDL_GetPerformanceFrequency(); 

	// do not accumulate if we are paused, otherwise the snake will travel forward really quick to catch up
	// once the game is unpaused
	if (!paused)
	    accumulated_ms += delta_time;
    }

    printf("Score: %d\n", snake.score);

    return EXIT_SUCCESS;
}
