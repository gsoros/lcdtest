#include <Arduino.h>

#include <SPI.h>
#include <U8g2lib.h>

//#include <Arduino_GFX_Library.h>
#include "databus/Arduino_HWSPI.h"
#include "display/Arduino_SSD1283A.h"
#include "canvas/Arduino_Canvas.h"

#define CS_PIN 5
#define RST_PIN 26
#define A0_PIN 27   // DC
#define SDA_PIN 23  // MOSI
#define SCK_PIN 18
#define LED_PIN 25

class Canvas : public Arduino_Canvas {
   public:
    struct Pixel {
        int16_t x, y;

        Pixel() : x(0), y(0) {}
        Pixel(int16_t x, int16_t y)
            : x(x), y(y) {}
    };

    struct Area {  // TODO convert to {x, y, w, h}
        Pixel topLeft;
        Pixel bottomRight;

        Area()
            : topLeft(0, 0), bottomRight(0, 0) {}

        Area(Pixel tl, Pixel br)
            : topLeft(tl), bottomRight(br) {}

        Area(int16_t tlx, int16_t tly,
             int16_t brx, int16_t bry)
            : topLeft(Pixel(tlx, tly)),
              bottomRight(Pixel(brx, bry)) {}

        uint16_t w() {
            int i = bottomRight.x - topLeft.x;
            return 0 < i ? (int16_t)i : 0;
        }

        uint16_t h() {
            int i = bottomRight.y - topLeft.y;
            return 0 < i ? (int16_t)i : 0;
        }

        bool contains(const int16_t x, const int16_t y) const {
            return topLeft.x <= x &&
                   x <= bottomRight.x &&
                   topLeft.y <= y &&
                   y <= bottomRight.y;
        }

        bool contains(Pixel *p) const {
            return contains(p->x, p->y);
        }

        bool contains(Area *a) const {
            return contains(&a->topLeft) && contains(&a->bottomRight);
        }

        bool touches(Area *a) const {
            return contains(&a->topLeft) ||
                   contains(a->bottomRight.x, a->topLeft.y) ||  // topRight
                   contains(a->topLeft.x, a->bottomRight.y) ||  // bottomLeft
                   contains(&a->bottomRight) ||
                   a->contains(topLeft.x, topLeft.y);
        }

        void clipTo(const Area *a) {
            if (topLeft.x < a->topLeft.x) topLeft.x = a->topLeft.x;
            if (topLeft.y < a->topLeft.y) topLeft.y = a->topLeft.y;
            if (a->bottomRight.x < bottomRight.x) bottomRight.x = a->bottomRight.x;
            if (a->bottomRight.y < bottomRight.y) bottomRight.y = a->bottomRight.y;
        }

        void moveBy(int16_t x, int16_t y) {
            topLeft.x += x;
            bottomRight.x += x;
            topLeft.y += y;
            bottomRight.y += y;
        }
    };

    Area clipArea;

    Canvas(int16_t w, int16_t h, Arduino_G *output, int16_t output_x = 0, int16_t output_y = 0)
        : Arduino_Canvas(w, h, output, output_x, output_y) {
        setMaxClipArea();
    };

    void writePixelPreclipped(int16_t x, int16_t y, uint16_t color) override {
        log_i("%d %d %d", x, y, color);
        if (!clipArea.contains(x, y)) return;
        Arduino_Canvas::writePixelPreclipped(x, y, color);
    }

    void writeFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) override {
        if (!_ordered_in_range(x, clipArea.topLeft.x, clipArea.bottomRight.x))
            return;

        if (y < clipArea.topLeft.y) {
            if (y + h < clipArea.topLeft.y) return;
            h -= clipArea.topLeft.y - y;
            y = clipArea.topLeft.y;
        }

        if (clipArea.bottomRight.y < y + h) {
            if (clipArea.bottomRight.y < y) return;
            h = clipArea.bottomRight.y - y;
        }

        Arduino_Canvas::writeFastVLine(x, y, h, color);
    }

    void writeFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) override {
        if (!_ordered_in_range(y, clipArea.topLeft.y, clipArea.bottomRight.y))
            return;

        if (x < clipArea.topLeft.x) {
            if (x + w < clipArea.topLeft.x) return;
            w -= clipArea.topLeft.x - x;
            x = clipArea.topLeft.x;
        }

        if (clipArea.bottomRight.x < x + w) {
            if (clipArea.bottomRight.x < x) return;
            w = clipArea.bottomRight.x - x;
        }

        Arduino_Canvas::writeFastHLine(x, y, w, color);
    }

    void writeFillRectPreclipped(int16_t x, int16_t y,
                                 int16_t w, int16_t h, uint16_t color) override {
        Area a = Area(x, y, x + w, y + h);

        if (!clipArea.touches(&a))
            return;

        if (clipArea.contains(&a))
            Arduino_Canvas::writeFillRectPreclipped(x, y, w, h, color);

        a.clipTo(&clipArea);
        Arduino_Canvas::writeFillRectPreclipped(a.topLeft.x, a.topLeft.y,
                                                a.w(), a.h(), color);
    }

    void fillScreen(uint16_t color) {
        startWrite();
        Arduino_Canvas::writeFillRectPreclipped(0, 0, _width, _height, color);
        endWrite();
    }

    void setClipArea(Area a) {
        clipArea = a;
    }

    void setMaxClipArea() {
        setClipArea(Area(0, 0, width(), height()));
    }

    Area *area() {
        static Area a = Area(0, 0, width(), height());
        return &a;
    }
};

Arduino_DataBus *bus = new Arduino_HWSPI(A0_PIN, CS_PIN);
Arduino_SSD1283A *lcd = new Arduino_SSD1283A(bus, RST_PIN, 2);
Canvas *canvas = new Canvas(130, 130, lcd);

void colorTest() {
    uint16_t colors[] = {RED, GREEN, BLUE, CYAN, MAGENTA, YELLOW};
    for (uint8_t i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) {
        canvas->fillScreen(colors[i]);
        canvas->flush();
        delay(200);
    }
}

void randomDigits(char *buf, size_t size) {
    static const char pool[] = "0123456789";
    for (uint8_t i = 0; i < size; i++)
        buf[i] = pool[random(0, sizeof(pool))];
}

uint16_t randomColor() {
    return canvas->color565(
        random(0, UINT8_MAX + 1),
        random(0, UINT8_MAX + 1),
        random(0, UINT8_MAX + 1));
}

void setup() {
    SPI.begin(SCK_PIN, -1, SDA_PIN, CS_PIN);
    Serial.begin(115200);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

    canvas->begin();
    colorTest();
    canvas->setTextWrap(false);
    canvas->setFont(u8g2_font_logisoso32_tn);
    canvas->setTextColor(WHITE);
}

void loop() {
    static uint16_t bgColor = BLACK;
    static uint16_t textColor = WHITE;
    static uint16_t shadowColor = canvas->color565(192, 192, 192);
    static uint16_t ballColor = RED;  // randomColor();

    static uint8_t ballSize = 10;
    static Canvas::Area ball(canvas->width() / 2 - ballSize / 2,
                             canvas->height() / 2 - ballSize / 2,
                             canvas->width() / 2 + ballSize / 2,
                             canvas->height() / 2 + ballSize / 2);
    static int8_t velMax = 3;
    static int8_t ballVelX = random(0, 2) < 1 ? random(1, velMax + 1) : random(-velMax, 0);
    static int8_t ballVelY = random(0, 2) < 1 ? random(1, velMax + 1) : random(-velMax, 0);

    static int8_t clipVel[4] = {0, 0, 0, 0};  // trbl
    static int16_t clip[4] = {0, 0, 0, 0};    // trbl
    static int16_t clipMin = 0;
    static int16_t clipMax[4] = {0, 0, 0, 0};  // trbl
    static int16_t clipLimit = min(canvas->width(), canvas->height()) / 2 - ballSize / 2;
    static Canvas::Area clipArea = Canvas::Area();

    canvas->fillScreen(bgColor);

    for (uint8_t i = 0; i < 4; i++) {
        clip[i] += clipVel[i];
        if (clipMax[i] <= clip[i])
            clipVel[i] = clipVel[i] ? -abs(clipVel[i]) : random(-velMax, 0);
        if (clip[i] <= clipMin) {
            clipVel[i] = 0;
            clipMax[i] = 0;
        }
    }

    clipArea.topLeft = {clip[0], clip[3]};
    clipArea.bottomRight = {(int16_t)(canvas->width() - clip[2]),
                            (int16_t)(canvas->height() - clip[1])};
    canvas->setClipArea(clipArea);

    // log_i("clip(t%dm%dv%d r%dm%dv%d b%dm%dv%d l%dm%dv%d)",
    //       clip[0], clipMax[0], clipVel[0],
    //       clip[1], clipMax[1], clipVel[1],
    //       clip[2], clipMax[2], clipVel[2],
    //       clip[3], clipMax[3], clipVel[3]);

    ball.moveBy(ballVelX, ballVelY);

    if (ball.topLeft.x <= clipArea.topLeft.x) {
        log_i("left");
        if (clipVel[3] <= 0)
            clipVel[3] = random(1, velMax + 1);
        ballVelX = clipVel[3] + 1;
        if (!clipMax[3])
            clipMax[3] = random(1, clipLimit + 1);
        // ballColor = randomColor();
    } else if (clipArea.bottomRight.x <= ball.bottomRight.x) {
        log_i("right");
        if (clipVel[1] <= 0)
            clipVel[1] = random(1, velMax + 1);
        ballVelX = -clipVel[1] - 1;
        if (!clipMax[1])
            clipMax[1] = random(1, clipLimit + 1);
        // ballColor = randomColor();
    }
    if (ball.topLeft.y <= clipArea.topLeft.y) {
        log_i("top");
        if (clipVel[0] <= 0)
            clipVel[0] = random(1, velMax + 1);
        ballVelY = clipVel[0] + 1;
        if (!clipMax[0])
            clipMax[0] = random(1, clipLimit + 1);
        // ballColor = randomColor();
    } else if (clipArea.bottomRight.y <= ball.bottomRight.y) {
        log_i("bottom");
        if (clipVel[2] <= 0)
            clipVel[2] = random(1, velMax + 1);
        ballVelY = -clipVel[2] - 1;
        if (!clipMax[2])
            clipMax[2] = random(1, clipLimit + 1);
        // ballColor = randomColor();
    }

    canvas->setCursor(0, 32);
    static char buf[4][10] = {"         ", "         ", "         ", "         "};
    static uint16_t colors[4];
    for (uint8_t i = 0; i < sizeof(buf) / sizeof(buf[0]); i++) {
        // if (random(0, 9) < 1)
        //     randomDigits(buf[i] + random(0, sizeof(buf[i])), 1);
        // canvas->println(buf[i]);
        for (uint8_t j = 0; j < sizeof(buf[i]); j++) {
            int16_t x = canvas->getCursorX();
            int16_t y = canvas->getCursorY();
            Canvas::Area textBox(x, y - 32, x + 16, y);
            if (textBox.touches(&ball)) {
                // canvas->drawRect(x, y - 32, 16, 32, YELLOW);
                canvas->setTextColor(shadowColor);
                randomDigits(buf[i] + j, 1);
            }
            canvas->print(buf[i][j]);
            canvas->setTextColor(textColor);
        }
        canvas->println();
    }
    canvas->fillCircle(ball.topLeft.x + ballSize / 2,
                       ball.bottomRight.y - ballSize / 2,
                       ballSize / 2, ballColor);

    canvas->flush();
}