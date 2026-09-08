#include <algorithm>
#include <cstdint>
#include <iostream>
#include <memory>
#include <random>
#include <vector>

#include <SFML/Audio.hpp>
#include <SFML/Graphics.hpp>

// Constants
const uint32_t WINDOW_WIDTH = 800;
const uint32_t WINDOW_HEIGHT = 800;
const uint32_t FPS_LIMIT = 60;

// Tube generation
const float TUBE_WIDTH = 80.0f;
const float TUBE_SPACING = 200.0f;
const float GAP_HEIGHT = 200.0f;

// Game physics
const float INITIAL_BIRD_VELOCITY_Y = 0.0f;
const float GRAVITY = 0.3f;
const float JUMP_SPEED = -5.0f;  // (set velocity of bird in the Y direction upon jump)
const float TUBE_SPEED = 3.0f;

const float BIRD_RADIUS = 15.0f;
const sf::Vector2f BIRD_POSITION = {100.0f, 400.0f};
const sf::Color BIRD_COLOR = sf::Color::Yellow;

// ResourceManager just owns all the resources/assets you'd want in your game.
// In an engine, you'd probably want to make this more flexible than what we have here
//  (e.g., by setting up a mapping from resourceName->resource which can be modified at runtime).
// Since our lab is simple, we'll just hardcode it:
struct ResourceManager {
    std::unique_ptr<sf::SoundBuffer> jumpSoundBuffer;
    std::unique_ptr<sf::Sound> jumpSound;
};

struct TubePair {
    sf::RectangleShape topTube;
    sf::RectangleShape bottomTube;
    float x;

    TubePair(float xPos, float gapY, float gapHeight) : x(xPos) {
        // Top tube
        topTube.setPosition({xPos, 0});
        topTube.setSize({TUBE_WIDTH, gapY});
        topTube.setFillColor(sf::Color::Green);

        // Bottom tube
        float bottomTubeHeight = WINDOW_HEIGHT - (gapY + gapHeight);
        bottomTube.setPosition({xPos, gapY + gapHeight});
        bottomTube.setSize({TUBE_WIDTH, bottomTubeHeight});
        bottomTube.setFillColor(sf::Color::Green);
    }

    void update(float speed) {
        x -= speed;
        topTube.setPosition({x, topTube.getPosition().y});
        bottomTube.setPosition({x, bottomTube.getPosition().y});
    }

    bool isOffScreen() const { return x + topTube.getSize().x < 0; }
};

bool isTubeOffScreen(const TubePair& tube) { return tube.isOffScreen(); }

struct BirdState {
    BirdState() : shape(BIRD_RADIUS), velocityY{INITIAL_BIRD_VELOCITY_Y} {
        shape.setPosition(BIRD_POSITION);
        shape.setFillColor(BIRD_COLOR);
    }

    sf::CircleShape shape;
    float velocityY;
};

struct GameState {
    GameState() : rng(0), gapDistribution(100.0f, 500.0f) {
        // Note: bird is automatically initialized by BirdState's default ctor,
        // which you implemented above.
        generateInitialTubes();
    }

    void updateState() {
        applyPhysicsToBird();
        updateTubes();
        checkCollisions();
    }

private:
    void generateInitialTubes() {
        // Generate tubes starting from the right edge
        for (float x = WINDOW_WIDTH + 100.f; x < WINDOW_WIDTH + 1200.f; x += TUBE_SPACING) {
            float gapY = gapDistribution(rng);
            tubes.emplace_back(x, gapY, GAP_HEIGHT);
        }
    }

    void resetTubes() {
        tubes.clear();
        generateInitialTubes();
    }

    void applyPhysicsToBird() {
        bird.velocityY += GRAVITY;
        bird.shape.move({0.0f, bird.velocityY});

        sf::FloatRect birdBounds = bird.shape.getGlobalBounds();
        if (birdBounds.position.y + birdBounds.size.y < 0.0f ||
            birdBounds.position.y > WINDOW_HEIGHT) {
            resetTubes();
            bird.shape.setPosition(BIRD_POSITION);
            bird.velocityY = INITIAL_BIRD_VELOCITY_Y;
        }
    }

    void updateTubes() {
        // Update tube positions
        for (auto& tube : tubes) {
            tube.update(TUBE_SPEED);
        }

        // Remove tubes that are off screen
        tubes.erase(std::remove_if(tubes.begin(), tubes.end(), isTubeOffScreen), tubes.end());

        // Generate new tubes when needed
        if (!tubes.empty()) {
            float rightmostTubeX = tubes.back().x;
            if (rightmostTubeX < WINDOW_WIDTH + TUBE_SPACING) {
                float gapY = gapDistribution(rng);
                tubes.emplace_back(rightmostTubeX + TUBE_SPACING, gapY, GAP_HEIGHT);
            }
        }
    }

    void checkCollisions() {
        sf::FloatRect birdBounds = bird.shape.getGlobalBounds();
        bool collided = false;
        for (const auto& tube : tubes) {
            if (birdBounds.findIntersection(tube.topTube.getGlobalBounds()) ||
                birdBounds.findIntersection(tube.bottomTube.getGlobalBounds())) {
                collided = true;
                break;
            }
        }
        if (collided) {
            resetTubes();
            bird.shape.setPosition(BIRD_POSITION);
            bird.velocityY = INITIAL_BIRD_VELOCITY_Y;
        }
    }

public:
    // game world objects
    std::vector<TubePair> tubes;
    BirdState bird;
    // rng state
    std::mt19937 rng;
    std::uniform_real_distribution<float> gapDistribution;
};

void handleInput(sf::Window& window, GameState& gameState, const ResourceManager& resources,
                 bool& shouldQuit) {
    while (const std::optional<sf::Event> event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
            shouldQuit = true;
        }

        if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            if (keyPressed->scancode == sf::Keyboard::Scan::Space) {
                gameState.bird.velocityY = JUMP_SPEED;
                if (resources.jumpSound) {
                    resources.jumpSound->play();
                }
            }
        }
    }
}

void render(sf::RenderWindow& window, const GameState& gameState) {
    // Clear with blue background (sky)
    window.clear(sf::Color::Blue);
    // Draw tubes
    for (const auto& tube : gameState.tubes) {
        window.draw(tube.topTube);
        window.draw(tube.bottomTube);
    }
    window.draw(gameState.bird.shape);
    window.display();
}

int main() {
    sf::RenderWindow window;
    ResourceManager resources;
    GameState gameState;  // game state initialization happens with ctor call

    try {
        // Initialize window
        window.create(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}), "Flappy Bird");
        window.setFramerateLimit(FPS_LIMIT);
        // Prevent key repeats.
        window.setKeyRepeatEnabled(false);

        resources.jumpSoundBuffer.reset(new sf::SoundBuffer);
        if (!resources.jumpSoundBuffer->loadFromFile("assets/jump.wav")) {
            std::cerr << "Warning: Could not load jump.wav" << std::endl;
        }
        resources.jumpSound.reset(new sf::Sound(*resources.jumpSoundBuffer));

        bool shouldQuit = false;
        // Main game loop
        while (window.isOpen()) {
            handleInput(window, gameState, resources, shouldQuit);
            if (shouldQuit) {
                break;
            }
            gameState.updateState();
            render(window, gameState);
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}
