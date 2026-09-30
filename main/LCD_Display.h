#include <stdint.h>
#define BLACK 0x0000
#define WHITE 0xffff
#define BLUE 0x25e5
#define YELLOW 0xff80
#define RED 0xf800
#define GREEN 0x053f

void init_lcd(void);
void fill_screen(uint16_t color);
void draw_seven_seg(int size, int x_pos, int y_pos, uint16_t color, uint16_t *data);
void draw_pixel_map(int scale, int x_pos, int y_pos, int width, int height, uint16_t color, uint16_t *data);
void draw_string_3x5(int scale, int x_pos, int y_pos, int array_size, uint16_t color, char *data);

struct pixel_image {
    int scale;
    int x_pos; 
    int y_pos;
    int width;
    int height;
    uint16_t color;
    uint16_t *data
};
