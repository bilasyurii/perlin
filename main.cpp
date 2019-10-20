#include <SFML/Graphics.hpp>
#include <iostream>
#include <math.h>
#include <time.h>
#include <set>
#include <queue>

class PerlinNoise2D
{
private:
    unsigned octaves;
    unsigned long seed;

    double value(long x, long y) const
    {
        unsigned long n = x + y * 563;
        n = ((n + seed) << 13) ^ n;
        return (1.0 - ((n * (n * n * 15731 + 789221) + seed) & 0x7fffffff) / 1073741824.0);
    }

    double value2(long x, long y) const
    {
        unsigned long n = y + x * 367;
        n = ((n + seed) << 11) ^ n;
        return (1.0 - ((n * (n * n * 20183 + 815279) + seed) & 0x7fffffff) / 1073741824.0);
    }

    float LinearInterpolate(float a, float b, float c) const
    {
        return a + c * (b - a);
    }

    inline int fastFloor(float x) const
    {
        return x > 0 ? (int) x : (int) x - 1;
    }

    inline float dot(float gx, float gy, float x, float y) const
    {
        return gx * x + gy * y;
    }

    float interpolatedNoise(float x, float y) const
    {
        long integerX = fastFloor(x);
        long integerY = fastFloor(y);
        float fx = x - integerX;
        float fy = y - integerY;
        float tx = fx * fx * fx * (fx * (fx * 6 - 15) + 10);
        float ty = fy * fy * fy * (fy * (fy * 6 - 15) + 10);
        return LinearInterpolate(LinearInterpolate(dot(value(integerX, integerY),
                                                       value2(integerX, integerY),
                                                       fx    , fy),
                                                   dot(value(integerX + 1, integerY),
                                                       value2(integerX + 1, integerY),
                                                       fx - 1, fy),
                                                   tx),
                                 LinearInterpolate(dot(value(integerX, integerY + 1),
                                                       value2(integerX, integerY + 1),
                                                       fx    , fy - 1),
                                                   dot(value(integerX + 1, integerY + 1),
                                                       value2(integerX + 1, integerY + 1),
                                                       fx - 1, fy - 1),
                                                   tx),
                                 ty);
    }
public:
    static const unsigned MAX_OCTAVES;

    PerlinNoise2D() : octaves(1)
    {
        srand(time(NULL));
        seed = rand();
    }

    PerlinNoise2D(unsigned _octaves) : octaves(_octaves)
    {
        srand(time(NULL));
        seed = rand();
        //seed = 17811;
        //seed = 27728;
        //seed = 3977;
    }

    PerlinNoise2D(unsigned _octaves, unsigned long _seed) : octaves(_octaves), seed(_seed) {}

    float get(float x, float y) const
    {
        float frequency = 0.05f, amplitude = 1.0f, scale = 0.0f, total = 0.0f;
        for(unsigned i = 0; i < octaves; ++i)
        {
            total += interpolatedNoise(x * frequency, y * frequency) * amplitude;
            scale += amplitude;
            frequency *= 2.0f;
            amplitude *= 0.5f;
        }
        return total / scale;
    }

    void setOctaves(float _octaves)
    {
        octaves = _octaves;
        if(_octaves < 1)
            octaves = 1;
        else if(_octaves > MAX_OCTAVES)
            octaves = MAX_OCTAVES;
    }

    unsigned getOctaves() const
    {
        return octaves;
    }

    unsigned long getSeed() const
    {
        return seed;
    }
};

const unsigned PerlinNoise2D::MAX_OCTAVES = 7;

const unsigned int WIDTH = 600, HEIGHT = 600;

typedef std::tuple<float, unsigned, unsigned> flowmapNode;

class World
{
public:
    enum BIOME
    {
        OCEAN,
        BEACH,
        STEPPE,
        GRASSLAND,
        FOREST,
        MOUNTAINS,
        SNOW,
        LAKE,
        COAST,
        RIVER
    };

    BIOME **tiles;

    PerlinNoise2D *noise;

    float a = 0.1f, b = 0.55f, c = 1.4f;
    //best so far: (a;b;c)=(0.15;0.5;1.4)
    //best so far: (a;b;c)=(0.1;0.55;1.4)
    //best so far: (a;b;c)=(0.0;0.6;4.0)

    World()
    {
        unsigned i;
        tiles = new BIOME*[HEIGHT];
        for(i = 0; i < HEIGHT; ++i)
            tiles[i] = new BIOME[WIDTH];
        heightmap = new float*[HEIGHT];
        for(i = 0; i < HEIGHT; ++i)
            heightmap[i] = new float[WIDTH];
        checked = new bool*[HEIGHT];
        coastBackup = new bool*[HEIGHT];
        for(i = 0; i < HEIGHT; ++i)
            checked[i] = new bool[WIDTH];
        for(i = 0; i < HEIGHT; ++i)
            coastBackup[i] = new bool[WIDTH];
        flowMap = new DIRECTION*[HEIGHT];
        for(i = 0; i < HEIGHT; ++i)
            flowMap[i] = new DIRECTION[WIDTH];
    }

    ~World()
    {
        unsigned i;
        for(i = 0; i < HEIGHT; ++i)
            delete tiles[i];
        delete tiles;
        for(i = 0; i < HEIGHT; ++i)
            delete checked[i];
        delete checked;
        for(i = 0; i < HEIGHT; ++i)
            delete coastBackup[i];
        delete coastBackup;
        for(i = 0; i < HEIGHT; ++i)
            delete heightmap[i];
        delete heightmap;
    }

    void generate(bool adjust)
    {
        float height, moisture, waterFactor, riverLevel, factor;
        unsigned long waterCount = 0;
        unsigned riverCount, startX, startY, tx, ty;
        unsigned sourceCoords[2][MAX_RIVERS] = {0};
        float sourceFactors[MAX_RIVERS] = {0.0f};

        for(unsigned y = 0; y < HEIGHT; ++y)
            for(unsigned x = 0; x < WIDTH; ++x)
            {
                float dx = 2.0f * (float) x / WIDTH - 1.0f;
                float dy = 2.0f * (float) y / HEIGHT - 1.0f;
                float d2 = dx * dx + dy * dy;                height = remap(noise->get(x * scale, y * scale));
                height = height + a - b * pow(d2, c);
                if(height < -1.0f)
                    height = -1.0f;
                heightmap[y][x] = height;
                moisture = remap(noise->get((x + 53) * 0.0625f, (y + 71) * 0.0625f));
                tiles[y][x] = biome(height, moisture);
                if((coastBackup[y][x] = (tiles[y][x] == BIOME::COAST)))
                    tiles[y][x] = BIOME::OCEAN;
                if(tiles[y][x] == BIOME::OCEAN)
                    ++waterCount;
                if((x % 16) == 0 && (y % 16) == 0) // TODO use poisson disk sampling
                {
                    factor = (height + 1.0f) * HEIGHT_FACTOR +
                             (moisture + 1.0f) * MOISTURE_FACTOR;
                    int i = MAX_RIVERS - 1;
                    if(factor > sourceFactors[i])
                    {
                        for(--i; i >= 0; --i)
                        {
                            if(factor < sourceFactors[i])
                            {
                                sourceFactors[i + 1] = factor;
                                sourceCoords[0][i + 1] = x;
                                sourceCoords[1][i + 1] = y;
                                break;
                            }
                            else
                            {
                                sourceFactors[i + 1] = sourceFactors[i];
                                sourceCoords[0][i + 1] = sourceCoords[0][i];
                                sourceCoords[1][i + 1] = sourceCoords[1][i];
                                if(i == 0)
                                {
                                    sourceFactors[0] = factor;
                                    sourceCoords[0][0] = x;
                                    sourceCoords[1][0] = y;
                                }
                            }
                        }
                    }
                }
            }
        if(adjust)
        {
            for(unsigned b = BIOME::OCEAN; b <= BIOME::SNOW; ++b)
                adjustBiome(b);
            for(unsigned y = 0; y < HEIGHT; ++y)
                for(unsigned x = 0; x < WIDTH; ++x)
                    if(coastBackup[y][x] && tiles[y][x] == BIOME::OCEAN)
                        tiles[y][x] = BIOME::COAST;
            adjustBiome(BIOME::COAST);
            adjustBiome(BIOME::OCEAN, true);
        }
        riverLevel = 0.1f / MAX_RIVERS;
        waterFactor = ((float) waterCount) / (WIDTH * HEIGHT);
        if(waterFactor <= 0.7f + riverLevel)
            riverCount = MAX_RIVERS;
        else if(waterFactor >= 0.8f - riverLevel)
            riverCount = 1;
        else
            riverCount = ((int)((0.8f - waterFactor) / riverLevel)) + 1;
        for(unsigned i = 0; i < riverCount; ++i)
        {
            for(unsigned y = 0; y < HEIGHT; ++y)
                for(unsigned x = 0; x < WIDTH; ++x)
                    flowMap[y][x] = DIRECTION::NONE;
            std::priority_queue<flowmapNode, std::vector<flowmapNode>,
                                FlowmapNodeCompare> opened;
            /*
            TODO:
            try not opening points and not using flowmap but create river in realtime

            TODO:
            try creating river in realtime and change terrain under it
            */
            startX = sourceCoords[0][i];
            startY = sourceCoords[1][i];
            flowMap[startY][startX] = DIRECTION::TOP;
            opened.push(std::make_tuple(heightmap[startY][startX], startX, startY));
            while(true)
            {
                auto lowest = opened.top();
                tx = std::get<1>(lowest);
                ty = std::get<2>(lowest);
                if(tiles[ty][tx] == BIOME::OCEAN ||
                   tiles[ty][tx] == BIOME::COAST ||
                   tiles[ty][tx] == BIOME::LAKE ||
                   tiles[ty][tx] == BIOME::RIVER)
                    break;
                opened.pop();
                if(ty > 0 && flowMap[ty - 1][tx] == DIRECTION::NONE)
                {
                    opened.push(std::make_tuple(heightmap[ty - 1][tx], tx, ty - 1));
                    flowMap[ty - 1][tx] = DIRECTION::BOTTOM;
                }
                if(ty < HEIGHT - 1 && flowMap[ty + 1][tx] == DIRECTION::NONE)
                {
                    opened.push(std::make_tuple(heightmap[ty + 1][tx], tx, ty + 1));
                    flowMap[ty + 1][tx] = DIRECTION::TOP;
                }
                if(tx > 0 && flowMap[ty][tx - 1] == DIRECTION::NONE)
                {
                    opened.push(std::make_tuple(heightmap[ty][tx - 1], tx - 1, ty));
                    flowMap[ty][tx - 1] = DIRECTION::RIGHT;
                }
                if(tx < WIDTH - 1 && flowMap[ty][tx + 1] == DIRECTION::NONE)
                {
                    opened.push(std::make_tuple(heightmap[ty][tx + 1], tx + 1, ty));
                    flowMap[ty][tx + 1] = DIRECTION::LEFT;
                }
            }
            while(true)
            {
                if(ty == startY && tx == startX)
                    break;
                switch(flowMap[ty][tx])
                {
                case TOP: --ty; break;
                case BOTTOM: ++ty; break;
                case LEFT: --tx; break;
                case RIGHT: ++tx; break;
                default: ;
                }
                tiles[ty][tx] = BIOME::RIVER;
                tiles[ty + 1][tx] = BIOME::RIVER;
                tiles[ty - 1][tx] = BIOME::RIVER;
                tiles[ty][tx + 1] = BIOME::RIVER;
                tiles[ty][tx - 1] = BIOME::RIVER;
            }
        }
        std::cout << riverCount << "\n";

        //TODO go through river and modify neighbours to become
        //beach if there is very little level of moisture
        //forest if there is very big level of moisture
    }
private:
    enum DIRECTION
    {
        TOP,
        BOTTOM,
        LEFT,
        RIGHT,
        NONE
    };

    struct FlowmapNodeCompare
    {
        bool operator()(const flowmapNode &left, const flowmapNode &right)
        {
            return std::get<0>(left) > std::get<0>(right);
        }
    };

    static const float scale;
    static const unsigned BIOME_COUNT;
    static const unsigned MAX_RIVERS;
    static const unsigned HEIGHT_FACTOR;
    static const unsigned MOISTURE_FACTOR;
    bool **checked;
    bool **coastBackup;
    float **heightmap;
    DIRECTION **flowMap;

    void adjustBiome(unsigned b, bool special = false)
    {
        unsigned long neighbourCount[BIOME_COUNT];
        unsigned long index;
        unsigned newBiome;
        unsigned tx, ty;
        for(unsigned y = 0; y < HEIGHT; ++y)
            for(unsigned x = 0; x < WIDTH; ++x)
                checked[y][x] = false;
        for(unsigned y = 0; y < HEIGHT; ++y)
            for(unsigned x = 0; x < WIDTH; ++x)
            {
                if(tiles[y][x] == b && !checked[y][x])
                {
                    bool touchesEdge = false;
                    std::set<unsigned long> closed;
                    std::set<unsigned long> opened;
                    std::set<unsigned long> neighbours;
                    opened.insert(getIndex(x, y));
                    do
                    {
                        std::set<unsigned long>::iterator it = opened.begin();
                        while(it != opened.end())
                        {
                            index = *it;
                            ty = index / WIDTH;
                            tx = index % WIDTH;
                            ++it;
                            opened.erase(index);
                            closed.insert(index);
                            checked[ty][tx] = true;
                            if(ty > 0)
                            {
                                index = getIndex(tx, ty - 1);
                                if(b == tiles[ty - 1][tx])
                                {
                                    if(closed.find(index) == closed.end() &&
                                       opened.find(index) == opened.end())
                                    {
                                        opened.insert(index);
                                        checked[ty - 1][tx] = true;
                                    }
                                }
                                else
                                    neighbours.insert(index);
                            }
                            else
                                touchesEdge = true;
                            if(tx < WIDTH - 1)
                            {
                                index = getIndex(tx + 1, ty);
                                if(b == tiles[ty][tx + 1])
                                {
                                    if(closed.find(index) == closed.end() &&
                                       opened.find(index) == opened.end())
                                    {
                                        opened.insert(index);
                                        checked[ty][tx + 1] = true;
                                    }
                                }
                                else
                                    neighbours.insert(index);
                            }
                            else
                                touchesEdge = true;
                            if(ty < HEIGHT - 1)
                            {
                                index = getIndex(tx, ty + 1);
                                if(b == tiles[ty + 1][tx])
                                {
                                    if(closed.find(index) == closed.end() &&
                                       opened.find(index) == opened.end())
                                    {
                                        opened.insert(index);
                                        checked[ty + 1][tx] = true;
                                    }
                                }
                                else
                                    neighbours.insert(index);
                            }
                            else
                                touchesEdge = true;
                            if(tx > 0)
                            {
                                index = getIndex(tx - 1, ty);
                                if(b == tiles[ty][tx - 1])
                                {
                                    if(closed.find(index) == closed.end() &&
                                       opened.find(index) == opened.end())
                                    {
                                        opened.insert(index);
                                        checked[ty][tx - 1] = true;
                                    }
                                }
                                else
                                    neighbours.insert(index);
                            }
                            else
                                touchesEdge = true;
                            if(b == BIOME::BEACH)
                            {
                                if(tx > 0 && ty > 0 && b == tiles[ty - 1][tx - 1])
                                {
                                    index = getIndex(tx - 1, ty - 1);
                                    if(closed.find(index) == closed.end() &&
                                       opened.find(index) == opened.end())
                                    {
                                        opened.insert(index);
                                        checked[ty - 1][tx - 1] = true;
                                    }
                                }
                                if(tx > 0 && ty < HEIGHT - 1 && b == tiles[ty + 1][tx - 1])
                                {
                                    index = getIndex(tx - 1, ty + 1);
                                    if(closed.find(index) == closed.end() &&
                                       opened.find(index) == opened.end())
                                    {
                                        opened.insert(index);
                                        checked[ty + 1][tx - 1] = true;
                                    }
                                }
                                if(tx < WIDTH - 1 && ty > 0 && b == tiles[ty - 1][tx + 1])
                                {
                                    index = getIndex(tx + 1, ty - 1);
                                    if(closed.find(index) == closed.end() &&
                                       opened.find(index) == opened.end())
                                    {
                                        opened.insert(index);
                                        checked[ty - 1][tx + 1] = true;
                                    }
                                }
                                if(tx < WIDTH - 1 && ty < HEIGHT - 1 && b == tiles[ty + 1][tx + 1])
                                {
                                    index = getIndex(tx + 1, ty + 1);
                                    if(closed.find(index) == closed.end() &&
                                       opened.find(index) == opened.end())
                                    {
                                        opened.insert(index);
                                        checked[ty + 1][tx + 1] = true;
                                    }
                                }
                            }
                        }
                    } while(opened.size() != 0);

                    if((b != BIOME::OCEAN && closed.size() < 100) ||
                       (b == BIOME::OCEAN && closed.size() < 50) ||
                       (b == BIOME::OCEAN && special && closed.size() < 300))
                    {
                        for(unsigned n = 0; n < BIOME_COUNT; ++n)
                            neighbourCount[n] = 0;
                        for(auto n : neighbours)
                        {
                            ty = n / WIDTH;
                            tx = n % WIDTH;
                            ++neighbourCount[tiles[ty][tx]];
                        }
                        newBiome = 0;
                        for(unsigned i = 1; i < BIOME_COUNT; ++i)
                            if(neighbourCount[i] > neighbourCount[newBiome])
                                newBiome = i;
                    }
                    else if(b == BIOME::OCEAN && !touchesEdge &&
                            !special && closed.size() >= 50)
                        newBiome = BIOME::LAKE;
                    else
                        continue;
                    for(auto i : closed)
                        tiles[i / WIDTH][i % WIDTH] = (BIOME) newBiome;
                }
            }
    }

    inline long getIndex(unsigned x, unsigned y) const
    {
        return y * WIDTH + x;
    }

    static float remap(float height)
    {
        if(height <= 0.5f && height >= -0.5)
            return height * 1.8f;
        else if(height > 0)
            return (height - 0.5f) * 0.2f + 0.9f;
        else
            return (height + 0.5f) * 0.2f - 0.9f;
        return height;
    }

    static BIOME biome(float height, float moisture)
    {
        if(height < -0.1f)
            return OCEAN;
        else if(height < 0.0f)
            return COAST;
        else if(height < 0.02f)
        {
            if(moisture > -0.1f)
                return GRASSLAND;
            else
                return BEACH;
        }
        else if(height < 0.2f)
        {
            if(moisture < -0.4)
                return STEPPE;
            else
                return GRASSLAND;
        }
        else if(height < 0.3f)
        {
            if(moisture > 0.0f)
                return FOREST;
            else
                return GRASSLAND;
        }
        else if(height < 0.4f)
            return FOREST;
        else if(height < 0.5f)
            return MOUNTAINS;
        else
        {
            if(moisture < 0.1f)
                return MOUNTAINS;
            else
                return SNOW;
        }
    }
};

const float World::scale = 0.125f;
const unsigned World::BIOME_COUNT = 9;
const unsigned World::MAX_RIVERS = 5;
const unsigned World::HEIGHT_FACTOR = 3;
const unsigned World::MOISTURE_FACTOR = 1;

void getImage(World &w, sf::Image &i)
{
    static const sf::Color colormap[] = {
        sf::Color(0, 30, 100), // ocean
        sf::Color(255, 255, 20), // beach
        sf::Color(120, 170, 0), // steppe
        sf::Color(0, 150, 20), // grassland
        sf::Color(0, 120, 50), // forest
        sf::Color(120, 120, 120), // mountains
        sf::Color::White, // snow
        sf::Color(0, 150, 255), // lake
        sf::Color(0, 60, 150), // coast
        sf::Color(0, 150, 255) // river
    };
    for(unsigned y = 0; y < HEIGHT; ++y)
        for(unsigned x = 0; x < WIDTH; ++x)
            i.setPixel(x, y, colormap[w.tiles[y][x]]);
}

int main()
{
    srand(time(NULL));
    sf::RenderWindow window(sf::VideoMode(WIDTH, HEIGHT), "Noise!");
    unsigned short octaves = 5;
    bool adjust = true;
    PerlinNoise2D *noise = new PerlinNoise2D(octaves);
    World world;
    world.noise = noise;
    sf::Image image;
    image.create(WIDTH, HEIGHT);
    sf::Texture texture;
    sf::Sprite sprite;
    sprite.setTexture(texture);
    sprite.setPosition(0, 0);
    sprite.setTextureRect(sf::IntRect(0, 0, WIDTH, HEIGHT));
    world.generate(adjust);
    getImage(world, image);
    texture.loadFromImage(image);

    while(window.isOpen())
    {
        sf::Event event;
        while(window.pollEvent(event))
        {
            if(event.type == sf::Event::Closed)
                window.close();
            else if(event.type == sf::Event::KeyPressed)
            {
                if(event.key.code == sf::Keyboard::Space)
                {
                    delete noise;
                    noise = new PerlinNoise2D(octaves);
                    world.noise = noise;
                    world.generate(adjust);
                    getImage(world, image);
                    texture.loadFromImage(image);
                }
                else if(event.key.code == sf::Keyboard::A)
                {
                    adjust = !adjust;
                    world.generate(adjust);
                    getImage(world, image);
                    texture.loadFromImage(image);
                }
                else if(event.key.code == sf::Keyboard::S)
                {
                    std::cout << "seed: " << noise->getSeed() << "\n";
                }
                else if(event.key.code == sf::Keyboard::Up)
                {
                    if(octaves < PerlinNoise2D::MAX_OCTAVES)
                    {
                        ++octaves;
                        noise->setOctaves(octaves);
                        world.generate(adjust);
                        getImage(world, image);
                        texture.loadFromImage(image);
                    }
                }
                else if(event.key.code == sf::Keyboard::Down)
                {
                    if(octaves > 1)
                    {
                        --octaves;
                        noise->setOctaves(octaves);
                        world.generate(adjust);
                        getImage(world, image);
                        texture.loadFromImage(image);
                    }
                }
            }
            else if(event.type == sf::Event::MouseWheelScrolled)
            {
                if(sf::Keyboard::isKeyPressed(sf::Keyboard::LControl))
                    world.b += 0.05f * (int)event.mouseWheelScroll.delta;
                else if(sf::Keyboard::isKeyPressed(sf::Keyboard::LShift))
                    world.c += 0.05f * (int)event.mouseWheelScroll.delta;
                else
                    world.a += 0.05f * (int)event.mouseWheelScroll.delta;
                std::cout << "a = " << world.a <<
                            " b = " << world.b <<
                            " c = " << world.c << "\n";
                world.generate(adjust);
                getImage(world, image);
                texture.loadFromImage(image);
            }
        }

        window.clear();
        window.draw(sprite);
        window.display();
    }

    delete noise;

    return 0;
}
