#include <SFML/Graphics.hpp>  // Include the SFML graphics library for rendering
#include <SFML/Audio.hpp>     // Include the SFML audio library for sound
#include <cmath>              // Include the math library for mathematical functions
#include <iostream>           // Include the input/output stream library
#include <bits/stdc++.h>      // Include the standard C++ library (all headers)
#include <chrono>             // Include the chrono library for time measurement
#include <omp.h>              // Include the OpenMP library for parallel programming
#include <immintrin.h>        // Include the intrinsics library for SIMD operations

using namespace std;  // Use the standard namespace

// Define constants for the game
const int WINDOW_WIDTH = 800;            // Width of the game window
const int WINDOW_HEIGHT = 600;           // Height of the game window
const float PADDLE_WIDTH = 10.f;         // Width of the paddle
const float PADDLE_HEIGHT = 100.f;       // Height of the paddle
const float BALL_RADIUS = 10.f;          // Radius of the ball
const float PADDLE_SPEED = 5.0f;         // Speed of the player's paddle
const float BALL_SPEED_BASE = 5.0f;      // Base speed of the ball
const float AI_PADDLE_SPEED_BASE = 0.6f; // Base speed of the AI's paddle

// Alternative function for sin using SIMD (commented out)

__m128 sin_simd(__m128 x) {
    __m128 x3 = _mm_mul_ps(x, _mm_mul_ps(x, x)); // x^3
    __m128 x5 = _mm_mul_ps(x3, _mm_mul_ps(x, x)); // x^5
    __m128 x7 = _mm_mul_ps(x5, _mm_mul_ps(x, x)); // x^7
    __m128 x9 = _mm_mul_ps(x7, _mm_mul_ps(x, x)); // x^9

    __m128 sin_result = _mm_add_ps(
        _mm_sub_ps(
            _mm_add_ps(
                _mm_sub_ps(x, _mm_div_ps(x3, _mm_set1_ps(6.0f))),
                _mm_div_ps(x5, _mm_set1_ps(120.0f))
            ),
            _mm_div_ps(x7, _mm_set1_ps(5040.0f))
        ),
        _mm_div_ps(x9, _mm_set1_ps(362880.0f))
    );

    return sin_result;
}

// Function to calculate sine using assembly (inline assembly)

float sin_asm(float x) {
    float result;
    __asm__ __volatile__ (
        "fsin"  // Use the x86 fsin instruction to calculate sine
        : "=t" (result)  // Output result
        : "0" (x)        // Input x
    );
    return result;
}

// Enum to define different ball movement modes
enum BallMode {
    Straight,    // Ball moves in a straight line
    Sinusoidal,  // Ball moves in a sinusoidal pattern
    Parabolic    // Ball moves in a parabolic pattern
};

// Class representing the paddle
class Paddle {
public:
    sf::RectangleShape shape;  // SFML rectangle shape for the paddle
    float speed;               // Speed of the paddle

    // Constructor to initialize the paddle
    Paddle(float x, float y, float mySpeed) {
        shape.setSize(sf::Vector2f(PADDLE_WIDTH, PADDLE_HEIGHT));  // Set paddle size
        shape.setPosition(x, y);  // Set paddle position
        speed = mySpeed;          // Set paddle speed
    }

    // Function to move the paddle
    void move(float offset) {
        shape.move(0, offset * speed);  // Move the paddle
        // Prevent the paddle from moving out of the window
        if (shape.getPosition().y < 0)
            shape.setPosition(shape.getPosition().x, 0);
        if (shape.getPosition().y + PADDLE_HEIGHT > WINDOW_HEIGHT)
            shape.setPosition(shape.getPosition().x, WINDOW_HEIGHT - PADDLE_HEIGHT);
    }
};

// Class representing the ball
class Ball {
public:
    sf::CircleShape shape;          // SFML circle shape for the ball
    sf::Vector2f velocity;          // Velocity of the ball
    sf::RectangleShape crossHorizontal;  // Horizontal cross on the ball
    sf::RectangleShape crossVertical;    // Vertical cross on the ball
    float rotationAngle;            // Rotation angle of the cross

    // Constructor to initialize the ball
    Ball(float x, float y, float speed) {
        shape.setRadius(BALL_RADIUS);  // Set ball radius
        shape.setPosition(x, y);       // Set ball position
        shape.setFillColor(sf::Color::Yellow);  // Set ball color
        velocity = sf::Vector2f(speed, 0);  // Set initial velocity

        // Initialize the horizontal cross
        crossHorizontal.setSize(sf::Vector2f(BALL_RADIUS * 1.5f, 2.f));
        crossHorizontal.setFillColor(sf::Color::Black);
        crossHorizontal.setOrigin(crossHorizontal.getSize().x / 2, crossHorizontal.getSize().y / 2);

        // Initialize the vertical cross
        crossVertical.setSize(sf::Vector2f(2.f, BALL_RADIUS * 1.5f));
        crossVertical.setFillColor(sf::Color::Black);
        crossVertical.setOrigin(crossVertical.getSize().x / 2, crossVertical.getSize().y / 2);

        rotationAngle = 0.f;  // Initialize rotation angle
    }

    // Function to move the ball
    void move() {
        shape.move(velocity);  // Move the ball

        // Bounce the ball off the top and bottom walls
        if (shape.getPosition().y < 0 || shape.getPosition().y + 2 * BALL_RADIUS > WINDOW_HEIGHT) {
            velocity.y = -velocity.y;
        }
    }

    // Function to reset the ball position and velocity
    void reset(float speed) {
        shape.setPosition(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);  // Reset position to center
        velocity = sf::Vector2f(speed, 0);  // Reset velocity
    }

    // Function to update the position of the cross on the ball
    void updateCrossPosition() {
        sf::Vector2f ballCenter = shape.getPosition() + sf::Vector2f(BALL_RADIUS, BALL_RADIUS);  // Calculate ball center
        crossHorizontal.setPosition(ballCenter);  // Set horizontal cross position
        crossVertical.setPosition(ballCenter);    // Set vertical cross position
    }

    // Function to rotate the cross on the ball
    void rotateCross(float angle) {
        crossHorizontal.rotate(angle);  // Rotate horizontal cross
        crossVertical.rotate(angle);    // Rotate vertical cross
    }
};

// Main function
int main() {
    int level = 1;  // Initialize game level
    std::cout << "Select a level (1 to 10): ";  // Prompt user to select a level
    std::cin >> level;  // Read user input for level

    // Clamp the level between 1 and 10
    if (level < 1) level = 1;
    if (level > 10) level = 10;

    // Calculate AI paddle speed based on level
    const float AI_PADDLE_SPEED = AI_PADDLE_SPEED_BASE * level + 5;
    // Calculate ball speed based on level, ensuring it doesn't go below 10
    float temp_speed = (level <= 5) ? 10 : 13;
    const float BALL_SPEED = temp_speed;

    // Create the game window
    sf::RenderWindow window(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "Pong Game");
    window.setFramerateLimit(60);  // Set frame rate limit to 60 FPS

    // Load background texture
    sf::Texture backgroundTexture;
    if (!backgroundTexture.loadFromFile("../pong.png")) {
        std::cerr << "Failed to load background image!" << std::endl;
        return -1;
    }
    sf::Sprite backgroundSprite(backgroundTexture);  // Create background sprite

    // Initialize paddles and ball
    Paddle playerPaddle(10, WINDOW_HEIGHT / 2 - PADDLE_HEIGHT / 2, PADDLE_SPEED);
    Paddle aiPaddle(WINDOW_WIDTH - 20, WINDOW_HEIGHT / 2 - PADDLE_HEIGHT / 2, AI_PADDLE_SPEED);
    Ball ball(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2, BALL_SPEED);

    // Initialize scores
    int playerScore = 0;
    int aiScore = 0;

    // Load font for score display
    sf::Font font;
    if (!font.loadFromFile("../ARIAL.TTF")) {
        std::cerr << "Failed to load font!" << std::endl;
        return -1;
    }

    // Load and play game over music
    sf::Music gameOverMusic;
    if (gameOverMusic.openFromFile("../game.ogg")) {
        gameOverMusic.setVolume(100);
        gameOverMusic.play();
    }

    // Initialize score text
    sf::Text scoreText;
    scoreText.setFont(font);
    scoreText.setCharacterSize(30);
    scoreText.setFillColor(sf::Color::White);
    scoreText.setPosition(WINDOW_WIDTH / 2 - 50, 10);

    // Initialize ball movement mode
    BallMode ballMode = Straight;

    // Start time measurement
    auto start = std::chrono::high_resolution_clock::now();
    float counter = 0;  // Counter for time measurement

    // Main game loop
    while (window.isOpen()) {
        counter++;  // Increment counter for time measurement
        sf::Event event;
        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();  // Close the window if the close event is triggered
        }

        // Check if the game is over (either player or AI reaches 5 points)
        if (playerScore >= 5 || aiScore >= 5) {
            std::string winner = (playerScore >= 5) ? "Mobin" : "AI";
            std::cout << "Game Over! " << winner << " wins!" << std::endl;
            break;
        }

        // Parallelize paddle movement using OpenMP
        #pragma omp parallel sections
        {
            #pragma omp section
            {
                // Move player paddle up or down based on keyboard input
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up))
                    playerPaddle.move(-1);
                if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down))
                    playerPaddle.move(1);
            }

            #pragma omp section
            {
                // Move AI paddle based on ball position
                float aiPaddleCenter = aiPaddle.shape.getPosition().y + PADDLE_HEIGHT / 2;
                float ballFutureY = ball.shape.getPosition().y + ball.velocity.y * (aiPaddle.shape.getPosition().x - ball.shape.getPosition().x) / ball.velocity.x;

                if (aiPaddleCenter < ballFutureY) {
                    aiPaddle.move(1);
                } else {
                    aiPaddle.move(-1);
                }
            }
        }

        // Change ball movement mode based on keyboard input
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Num1)) {
            ballMode = Straight;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Num2)) {
            ballMode = Sinusoidal;
        }
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Num3)) {
            ballMode = Parabolic;
        }

        // Handle ball movement based on the selected mode
        switch (ballMode) {
            case Straight:
                break;
            case Sinusoidal: {
                // Calculate sine value using assembly function
                
                // float input_value = ball.shape.getPosition().x * 0.006f;

                // __m128 input_simd = _mm_set1_ps(input_value);

                // __m128 result_simd = sin_simd(input_simd);

                // float sin_value;
                // _mm_store_ss(&sin_value, result_simd);

                
                float sin_value = sin_asm(ball.shape.getPosition().x * 0.01);
                ball.velocity.y = sin_value * BALL_SPEED;

                // Handle ball collision with top and bottom walls
                if (ball.shape.getPosition().y < 0) {
                    if(level >= 7) ball.shape.setPosition(ball.shape.getPosition().x, WINDOW_HEIGHT - 2 * BALL_RADIUS);
                    else {
                        ball.velocity.y = (ball.velocity.y>0) ? ball.velocity.y : -ball.velocity.y;
                        ball.shape.setPosition(ball.shape.getPosition().x, 0);
                    }
                }
                if (ball.shape.getPosition().y + 2 * BALL_RADIUS > WINDOW_HEIGHT) {
                    if(level >= 7) ball.shape.setPosition(ball.shape.getPosition().x, 0);
                    else {
                        ball.shape.setPosition(ball.shape.getPosition().x, WINDOW_HEIGHT - 2 * BALL_RADIUS);
                        ball.velocity.y = (ball.velocity.y<0) ? ball.velocity.y : -ball.velocity.y;
                    }
                }
                ball.rotateCross(2.f);  // Rotate the cross on the ball
                break;
            }
            case Parabolic:
                ball.velocity.y += 0.1f;  // Add gravity effect to the ball
                // Handle ball collision with top and bottom walls
                if (ball.shape.getPosition().y < 0) {
                    if(level >= 7) ball.shape.setPosition(ball.shape.getPosition().x, WINDOW_HEIGHT - 2 * BALL_RADIUS);
                    else {
                        ball.velocity.y = (ball.velocity.y>0) ? ball.velocity.y : -ball.velocity.y;
                        ball.shape.setPosition(ball.shape.getPosition().x, 0);
                    }
                }
                if (ball.shape.getPosition().y + 2 * BALL_RADIUS > WINDOW_HEIGHT) {
                    if(level >= 7) ball.shape.setPosition(ball.shape.getPosition().x, 0);
                    else {
                        ball.shape.setPosition(ball.shape.getPosition().x, WINDOW_HEIGHT - 2 * BALL_RADIUS);
                        ball.velocity.y = (ball.velocity.y<0) ? ball.velocity.y : -ball.velocity.y;
                    }
                }
                ball.rotateCross(2.f);  // Rotate the cross on the ball
                break;
        }

        ball.move();  // Move the ball

        // Handle ball collision with paddles
        if (ball.shape.getGlobalBounds().intersects(playerPaddle.shape.getGlobalBounds())) {
            ball.velocity.x = -ball.velocity.x;  // Reverse ball direction
            float hitPoint = (ball.shape.getPosition().y + BALL_RADIUS) - (playerPaddle.shape.getPosition().y + PADDLE_HEIGHT / 2);
            ball.velocity.y = hitPoint * 0.1f;  // Adjust ball velocity based on where it hits the paddle
        }
        if (ball.shape.getGlobalBounds().intersects(aiPaddle.shape.getGlobalBounds())) {
            ball.velocity.x = -ball.velocity.x;  // Reverse ball direction
            float hitPoint = (ball.shape.getPosition().y + BALL_RADIUS) - (aiPaddle.shape.getPosition().y + PADDLE_HEIGHT / 2);
            ball.velocity.y = hitPoint * 0.1f;  // Adjust ball velocity based on where it hits the paddle
        }

        // Handle ball going out of bounds (left or right)
        if (ball.shape.getPosition().x < 0) {
            aiScore++;  // Increment AI score
            ball.reset(BALL_SPEED);  // Reset ball position and velocity
            counter++;
        }
        if (ball.shape.getPosition().x + 2 * BALL_RADIUS > WINDOW_WIDTH) {
            playerScore++;  // Increment player score
            ball.reset(BALL_SPEED);  // Reset ball position and velocity
            counter++;
        }

        // Update score text
        scoreText.setString(std::to_string(playerScore) + " - " + std::to_string(aiScore));

        // Update cross position on the ball
        ball.updateCrossPosition();

        // Clear the window and draw all game objects
        window.clear();
        window.draw(backgroundSprite);
        window.draw(playerPaddle.shape);
        window.draw(aiPaddle.shape);
        window.draw(ball.shape);
        window.draw(ball.crossHorizontal);
        window.draw(ball.crossVertical);
        window.draw(scoreText);
        window.display();
    }

    // Calculate and print the average time taken for ball movement and collision
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()/counter;
    std::cout << "Time taken for ball movement and collision: " << duration << " microseconds" << std::endl;

    return 0;  // End of main function
}