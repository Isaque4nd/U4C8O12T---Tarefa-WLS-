#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "inc/ssd1306.h"
#include "inc/font.h" // Biblioteca para o display SSD1306

// --- Definições de pinos ---
// LED RGB
#define LED_RED_PIN    11
#define LED_BLUE_PIN   12
#define LED_GREEN_PIN  13

// Joystick (eixos analógicos)
#define JOYSTICK_X_PIN 26   // ADC Channel 0
#define JOYSTICK_Y_PIN 27   // ADC Channel 1

// Botões
#define JOYSTICK_BUTTON_PIN 22  // Botão do joystick (usa IRQ)
#define BUTTON_A_PIN         5   // Botão A (usa IRQ)

// Display SSD1306 via I2C
#define I2C_PORT      i2c0
#define I2C_SDA_PIN   14
#define I2C_SCL_PIN   15

// Dimensões do display e do quadrado móvel
#define DISPLAY_WIDTH   128
#define DISPLAY_HEIGHT   64
#define SQUARE_SIZE       8

// Parâmetros PWM para LED (8-bit, wrap = 255)
#define LED_PWM_WRAP    255

// Debouncing: tempo mínimo entre acionamentos (em microsegundos)
#define DEBOUNCE_DELAY_US 200000  // 200 ms

// --- Variáveis Globais (usadas nas interrupções) ---
volatile bool led_green_on = false;     // Estado do LED verde
volatile bool led_pwm_enabled = true;     // Ativa/desativa os LEDs vermelho e azul
volatile uint32_t last_debounce_time_joystick = 0;
volatile uint32_t last_debounce_time_buttonA = 0;
volatile int border_style = 0;            // 0 ou 1 para alternar estilos de borda no display

// Instância global do display
ssd1306_t ssd;

// --- Funções de Configuração ---
// Configura PWM para um pino de LED com wrap definido para LED_PWM_WRAP.
void setup_led_pwm(uint gpio) {
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_wrap(slice_num, LED_PWM_WRAP);
    pwm_set_clkdiv(slice_num, 1.0f);   // Divisor 1: resolução total (8-bit)
    pwm_set_enabled(slice_num, true);
}

// Atualiza o brilho de um LED PWM (valor de 0 a LED_PWM_WRAP)
void set_led_brightness(uint gpio, uint brightness) {
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    uint channel = pwm_gpio_to_channel(gpio);
    pwm_set_chan_level(slice_num, channel, brightness);
}

// Inicializa o ADC e configura os pinos analógicos para o joystick
void setup_adc(void) {
    adc_init();
    adc_gpio_init(JOYSTICK_X_PIN); // ADC Channel 0
    adc_gpio_init(JOYSTICK_Y_PIN); // ADC Channel 1
}

// Inicializa o display SSD1306 via I2C
void setup_display(void) {
    // Inicializa o I2C
    i2c_init(I2C_PORT, 100 * 1000);  // 100 kHz
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Inicializa o display usando a instância global "ssd"
    ssd1306_init(&ssd, DISPLAY_WIDTH, DISPLAY_HEIGHT, false, 0x3C, I2C_PORT);
    ssd1306_config(&ssd);
    ssd1306_clear(&ssd);
    ssd1306_show(&ssd);
}

// --- Funções de Mapeamento ---
// Mapeia o valor do ADC (0 a 4095) para coordenadas do display
int map_adc_to_coord(uint16_t adc_value, int max_coord) {
    return (adc_value * max_coord) / 4095;
}

// Mapeia o valor do ADC para brilho do LED. Quando o ADC estiver em 2048 (centro), o brilho será zero.
uint map_adc_to_brightness(uint16_t adc_value) {
    int diff = (adc_value > 2048) ? (adc_value - 2048) : (2048 - adc_value);
    uint brightness = (diff * LED_PWM_WRAP) / 2048;
    if (brightness > LED_PWM_WRAP) brightness = LED_PWM_WRAP;
    return brightness;
}

// --- Interrupção para o botão do Joystick ---
// Alterna o estado do LED verde e o estilo da borda do display
void joystick_button_callback(uint gpio, uint32_t events) {
    uint32_t now = time_us_32();
    if (now - last_debounce_time_joystick < DEBOUNCE_DELAY_US) return;
    last_debounce_time_joystick = now;

    led_green_on = !led_green_on;
    border_style = (border_style == 0) ? 1 : 0;
}

// --- Interrupção para o botão A ---
// Alterna a ativação dos LEDs PWM (vermelho e azul)
void button_a_callback(uint gpio, uint32_t events) {
    uint32_t now = time_us_32();
    if (now - last_debounce_time_buttonA < DEBOUNCE_DELAY_US) return;
    last_debounce_time_buttonA = now;

    led_pwm_enabled = !led_pwm_enabled;
}

// --- Main ---
int main(void) {
    stdio_init_all();

    // Inicializa os periféricos
    setup_adc();
    setup_display();

    // Configura PWM para os LEDs RGB
    setup_led_pwm(LED_RED_PIN);
    setup_led_pwm(LED_BLUE_PIN);
    setup_led_pwm(LED_GREEN_PIN);

    // Configura os botões com pull-up e habilita interrupções
    gpio_init(JOYSTICK_BUTTON_PIN);
    gpio_set_dir(JOYSTICK_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(JOYSTICK_BUTTON_PIN);
    gpio_set_irq_enabled_with_callback(JOYSTICK_BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &joystick_button_callback);

    gpio_init(BUTTON_A_PIN);
    gpio_set_dir(BUTTON_A_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_A_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_A_PIN, GPIO_IRQ_EDGE_FALL, true, &button_a_callback);

    // Loop principal
    while (true) {
        // --- Leitura do Joystick ---
        adc_select_input(0);  // Canal 0 para eixo X (GPIO 26)
        uint16_t adc_x = adc_read();
        adc_select_input(1);  // Canal 1 para eixo Y (GPIO 27)
        uint16_t adc_y = adc_read();

        // --- Atualiza os LEDs baseados no joystick ---
        uint brightness_red = map_adc_to_brightness(adc_x);
        uint brightness_blue = map_adc_to_brightness(adc_y);
        if (!led_pwm_enabled) {
            brightness_red = 0;
            brightness_blue = 0;
        }
        set_led_brightness(LED_RED_PIN, brightness_red);
        set_led_brightness(LED_BLUE_PIN, brightness_blue);
        set_led_brightness(LED_GREEN_PIN, led_green_on ? LED_PWM_WRAP : 0);

        // --- Atualiza o Display SSD1306 ---
        int square_x = map_adc_to_coord(adc_x, DISPLAY_WIDTH - SQUARE_SIZE);
        int square_y = map_adc_to_coord(adc_y, DISPLAY_HEIGHT - SQUARE_SIZE);

        ssd1306_clear(&ssd);
        // Desenha a borda do display conforme o estilo selecionado
        if (border_style == 0) {
            ssd1306_draw_rect(&ssd, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, true);
        } else {
            ssd1306_draw_rect(&ssd, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, true);
            ssd1306_draw_rect(&ssd, 2, 2, DISPLAY_WIDTH - 4, DISPLAY_HEIGHT - 4, true);
        }
        // Desenha um quadrado preenchido 8x8 na posição determinada pelo joystick
        ssd1306_fill_rect(&ssd, square_x, square_y, SQUARE_SIZE, SQUARE_SIZE, true);
        ssd1306_show(&ssd);

        sleep_ms(50); // Atualiza a cada 50 ms
    }
    return 0;
}
