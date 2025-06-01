#include <iostream>
#include <windows.h>
#include <conio.h>
#include <vector>
#include <random>
#include <chrono>
#include <string>
#include <queue>

using namespace std;

// 游戏常量定义
const int HEIGHT = 35;
const int WIDTH = 55;
const int ENEMY_COUNT = 20;  // 减少敌机总数
const int ENEMY_SPAWN_INTERVAL = 30;  // 每隔30帧生成一个新敌机

// 坐标结构体
struct Position {
    int x, y;
    Position(int _x = 0, int _y = 0) : x(_x), y(_y) {}
};

// 游戏对象基类
class GameObject {
protected:
    Position pos;
    char shape[3][5];
public:
    GameObject(int x, int y) : pos(x, y) {
        memset(shape, ' ', sizeof(shape));
    }
    Position getPosition() const { return pos; }
    void setPosition(int x, int y) { pos.x = x; pos.y = y; }
    char getShape(int i, int j) const { return shape[i][j]; }
};

// 玩家飞机类
class PlayerPlane : public GameObject {
public:
    PlayerPlane() : GameObject(WIDTH / 2, HEIGHT - 7) {  // 再往上调整一点初始位置
        // 清空所有位置
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 5; j++)
                shape[i][j] = ' ';

        // 第一行：/=\;
        shape[0][1] = '/';
        shape[0][2] = '=';
        shape[0][3] = '\\';

        // 第二行：<<*>>
        shape[1][0] = '<';
        shape[1][1] = '<';
        shape[1][2] = '*';
        shape[1][3] = '>';
        shape[1][4] = '>';

        // 第三行：* *
        shape[2][1] = '*';
        shape[2][3] = '*';
    }

    void move(char direction) {
        const int speed = 2;
        switch (direction) {
        case 'w': if (pos.y > 4) pos.y -= speed; break;  // 调整上边界
        case 's': if (pos.y < HEIGHT - 7) pos.y += speed; break;  // 调整下边界
        case 'a': if (pos.x > 3) pos.x -= speed; break;
        case 'd': if (pos.x < WIDTH - 6) pos.x += speed; break;
        }
    }
};

// 敌机类
class EnemyPlane : public GameObject {
private:
    float actualY;  // 用于存储精确的Y坐标
    float speed;    // 敌机移动速度
public:
    EnemyPlane(int x, int y) : GameObject(x, y), actualY(y), speed(0.2f + (rand() % 100) / 200.0f) {
        shape[0][0] = '\\';
        shape[0][1] = '+';
        shape[0][2] = '/';
        shape[1][1] = '|';
    }

    void update() {
        actualY += speed;
        pos.y = static_cast<int>(actualY);
    }

    bool isOffScreen() const {
        return pos.y >= HEIGHT - 1;
    }
};

// 子弹类
class Bullet {
    Position pos;
public:
    Bullet(int x, int y) : pos(x, y) {}

    void move() {
        pos.y--;
    }

    Position getPosition() const { return pos; }
};

// 游戏类
class Game {
private:
    PlayerPlane player;
    vector<EnemyPlane> enemies;
    vector<Bullet> bullets;
    queue<EnemyPlane> enemyQueue;
    int score;
    bool gameOver;
    bool hadScore;
    random_device rd;
    mt19937 gen;
    char buffer[HEIGHT][WIDTH];
    char prevBuffer[HEIGHT][WIDTH];
    int frameCount;
    HANDLE consoleHandle;

    void initializeEnemyQueue() {
        uniform_int_distribution<> disX(2, WIDTH - 3);
        uniform_int_distribution<> disY(-20, -1);  // 敌机从屏幕上方开始

        for (int i = 0; i < ENEMY_COUNT; i++) {
            enemyQueue.emplace(disX(gen), disY(gen));
        }
    }

    void clearBuffer() {
        memset(buffer, ' ', sizeof(buffer));
    }

    void drawToBuffer(int x, int y, char ch) {
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) {
            buffer[y][x] = ch;
        }
    }

    void renderBuffer() {
        COORD coord = { 0, 0 };
        SetConsoleCursorPosition(consoleHandle, coord);

        // 只更新发生变化的字符
        for (int i = 0; i < HEIGHT; i++) {
            for (int j = 0; j < WIDTH; j++) {
                if (buffer[i][j] != prevBuffer[i][j]) {
                    coord.X = j;
                    coord.Y = i;
                    SetConsoleCursorPosition(consoleHandle, coord);
                    cout << buffer[i][j];
                }
            }
        }

        // 保存当前帧到前一帧缓冲区
        memcpy(prevBuffer, buffer, sizeof(buffer));
    }

public:
    Game() : score(0), gameOver(false), hadScore(false),
        gen(rd()), frameCount(0) {
        consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
        // 设置控制台缓冲区大小
        COORD bufferSize = { WIDTH, HEIGHT };
        SetConsoleScreenBufferSize(consoleHandle, bufferSize);

        // 设置控制台窗口大小
        SMALL_RECT windowSize = { 0, 0, WIDTH - 1, HEIGHT - 1 };
        SetConsoleWindowInfo(consoleHandle, TRUE, &windowSize);

        // 初始化缓冲区
        memset(buffer, ' ', sizeof(buffer));
        memset(prevBuffer, ' ', sizeof(prevBuffer));

        // 初始化边框
        drawBorder();
        memcpy(prevBuffer, buffer, sizeof(buffer));  // 复制初始边框到前一帧缓冲区

        initializeEnemyQueue();
    }

    void run() {
        while (!gameOver) {
            auto frameStart = chrono::high_resolution_clock::now();

            clearBuffer();
            drawScore();
            drawBorder();  // 先画边框

            // 处理玩家输入
            while (_kbhit()) {
                char key = _getch();
                handleInput(key);
            }

            // 生成新敌机
            if (frameCount % ENEMY_SPAWN_INTERVAL == 0 && !enemyQueue.empty()) {
                enemies.push_back(enemyQueue.front());
                enemyQueue.pop();
            }

            updateGame();

            // 检查是否所有敌机都已用完且当前场上没有敌机
            if (enemyQueue.empty() && enemies.empty()) {
                gameOver = true;
            }

            // 最后绘制所有游戏对象
            drawObjects();
            renderBuffer();

            frameCount++;

            // 控制帧率
            auto frameEnd = chrono::high_resolution_clock::now();
            auto frameDuration = chrono::duration_cast<chrono::milliseconds>(frameEnd - frameStart);
            if (frameDuration.count() < 33) {
                Sleep(33 - frameDuration.count());
            }
        }

        // 游戏结束显示
        clearBuffer();
        string gameOverText;
        if (score == 0 && hadScore) {
            gameOverText = "游戏结束！最终得分：0";
        }
        else {
            gameOverText = "游戏结束！最终得分：" + to_string(score);
        }

        int x = (WIDTH - gameOverText.length()) / 2;
        int y = HEIGHT / 2;
        for (size_t i = 0; i < gameOverText.length(); i++) {
            buffer[y][x + i] = gameOverText[i];
        }
        renderBuffer();
    }

private:
    void updateGame() {
        // 更新子弹位置
        for (auto it = bullets.begin(); it != bullets.end();) {
            it->move();
            if (it->getPosition().y < 1) {
                it = bullets.erase(it);
            }
            else {
                ++it;
            }
        }

        // 更新敌机位置
        for (auto it = enemies.begin(); it != enemies.end();) {
            it->update();
            if (it->isOffScreen()) {
                it = enemies.erase(it);
            }
            else {
                ++it;
            }
        }

        // 检测碰撞
        checkCollisions();
    }

    void checkCollisions() {
        // 检查子弹碰撞
        for (auto bulletIt = bullets.begin(); bulletIt != bullets.end();) {
            bool bulletHit = false;
            Position bulletPos = bulletIt->getPosition();

            // 检查是否击中敌机
            for (auto enemyIt = enemies.begin(); enemyIt != enemies.end();) {
                Position enemyPos = enemyIt->getPosition();
                if (abs(bulletPos.x - enemyPos.x) <= 2 && abs(bulletPos.y - enemyPos.y) <= 2) {
                    enemyIt = enemies.erase(enemyIt);
                    bulletHit = true;
                    score++;  // 击中敌机加分
                    hadScore = true;  // 标记已经获得过分数
                    break;
                }
                else {
                    ++enemyIt;
                }
            }

            if (bulletHit) {
                bulletIt = bullets.erase(bulletIt);
            }
            else {
                ++bulletIt;
            }
        }

        // 检查玩家与敌机碰撞
        Position playerPos = player.getPosition();
        for (auto it = enemies.begin(); it != enemies.end();) {
            Position enemyPos = it->getPosition();
            if (abs(playerPos.x - enemyPos.x) <= 2 && abs(playerPos.y - enemyPos.y) <= 2) {
                it = enemies.erase(it);
                if (score > 0) {  // 只有当分数大于0时才减分
                    score--;
                    if (score == 0 && hadScore) {  // 只有曾经得分且分数归零时才结束游戏
                        gameOver = true;
                    }
                }
            }
            else {
                ++it;
            }
        }
    }

    void drawObjects() {
        // 先绘制子弹
        for (const auto& bullet : bullets) {
            Position bulletPos = bullet.getPosition();
            if (bulletPos.x >= 1 && bulletPos.x < WIDTH - 1 && bulletPos.y >= 1 && bulletPos.y < HEIGHT - 2) {
                drawToBuffer(bulletPos.x, bulletPos.y, '^');
            }
        }

        // 再绘制敌机
        for (const auto& enemy : enemies) {
            Position enemyPos = enemy.getPosition();
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 5; j++) {
                    if (enemy.getShape(i, j) != ' ') {
                        int x = enemyPos.x + j - 2;
                        int y = enemyPos.y + i - 1;
                        if (x >= 1 && x < WIDTH - 1 && y >= 1 && y < HEIGHT - 2) {
                            drawToBuffer(x, y, enemy.getShape(i, j));
                        }
                    }
                }
            }
        }

        // 最后绘制玩家飞机
        Position playerPos = player.getPosition();
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 5; j++) {
                if (player.getShape(i, j) != ' ') {
                    drawToBuffer(playerPos.x + j - 2, playerPos.y + i - 1, player.getShape(i, j));
                }
            }
        }
    }

    void drawBorder() {
        // 绘制边框，从第1行开始
        for (int i = 0; i < WIDTH; i++) {
            buffer[HEIGHT - 2][i] = '#';  // 将底部边框上移一行
        }
        // 左右边框
        for (int i = 1; i < HEIGHT - 1; i++) {  // 调整边框高度
            buffer[i][0] = '#';
            buffer[i][WIDTH - 1] = '#';
        }
    }

    void drawScore() {
        // 在左上角显示简单的分数
        string scoreText = "Score:" + to_string(score);
        for (size_t i = 0; i < scoreText.length(); i++) {
            buffer[0][i] = scoreText[i];
        }
    }

    void handleInput(char key) {
        if (key == ' ') {
            // 发射子弹
            Position playerPos = player.getPosition();
            bullets.emplace_back(playerPos.x, playerPos.y - 1);
        }
        else {
            player.move(key);
        }
    }
};

int main() {
    // 设置控制台
    system("mode con cols=55 lines=35");
    system("title 飞机大战");

    // 隐藏光标
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO CursorInfo;
    GetConsoleCursorInfo(handle, &CursorInfo);
    CursorInfo.bVisible = false;
    SetConsoleCursorInfo(handle, &CursorInfo);

    Game game;
    game.run();

    return 0;
}