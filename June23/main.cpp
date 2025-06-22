#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <latch>
#include <barrier>
#include <future>
#include <random>

constexpr int NUM_PLAYERS = 3;
constexpr int NUM_ROUNDS = 3;

std::mutex cout_mutex;
std::mutex input_mutex; // To avoid mixed inputs

struct Question {
    std::string text;
    std::map<char, std::string> options;
    char correct;
};

struct Player {
    int id;
    std::string name;
    int score = 0;
    bool used5050 = false;
    bool usedCallFriend = false;
};

std::vector<Question> questions = {
    { "What is the capital of France?",
      {{'A', "London"}, {'B', "Paris"}, {'C', "Berlin"}, {'D', "Madrid"}}, 'B' },
    { "Which is the Red Planet?",
      {{'A', "Venus"}, {'B', "Earth"}, {'C', "Mars"}, {'D', "Jupiter"}}, 'C' },
    { "Who wrote Romeo and Juliet?",
      {{'A', "Shakespeare"}, {'B', "Hemingway"}, {'C', "Tolkien"}, {'D', "Rowling"}}, 'A' }
};

char use5050(const Question& q, Player& player) {
    player.used5050 = true;
    std::vector<char> options = { q.correct };
    for (const auto& [key, _] : q.options) {
        if (key != q.correct) {
            options.push_back(key);
            break;
        }
    }
    std::scoped_lock lock(cout_mutex);
    std::cout << "[" << player.name << "] used 50/50. Choices: " << options[0] << " and " << options[1] << "\n";
    return options[rand() % 2];
}

char useCallFriend(const Question& q, Player& player) {
    player.usedCallFriend = true;
    std::scoped_lock lock(cout_mutex);
    std::cout << "[" << player.name << "] used Call a Friend. Friend suggests: " << q.correct << "\n";
    return (rand() % 4 == 0) ? q.correct : 'A' + rand() % 4;
}

char simulateAnswer(Player& player, const Question& q) {
    int lifeline = rand() % 5;
    if (!player.used5050 && lifeline == 0)
        return use5050(q, player);
    if (!player.usedCallFriend && lifeline == 1)
        return useCallFriend(q, player);
    return 'A' + rand() % 4;
}

void playerThread(Player& player, std::latch& loginLatch, std::barrier<>& roundBarrier) {
    {
        std::scoped_lock input_lock(input_mutex);
        std::cout << "Player " << player.id << ", enter your name: ";
        std::getline(std::cin, player.name);

        std::string confirm;
        do {
            std::cout << "[" << player.name << "] type 'join' to join the quiz: ";
            std::getline(std::cin, confirm);
        } while (confirm != "join");

        std::scoped_lock lock(cout_mutex);
        std::cout << "[" << player.name << "] has joined the game.\n";
    }

    loginLatch.count_down();
    loginLatch.wait();

    for (int round = 0; round < NUM_ROUNDS; ++round) {
        const Question& q = questions[round];

        {
            std::scoped_lock lock(cout_mutex);
            std::cout << "\n[Round " << (round + 1) << "] " << player.name << ", thinking...\n";
        }

        char answer = simulateAnswer(player, q);

        roundBarrier.arrive_and_wait();

        std::future<void> eval = std::async(std::launch::async, [&]() {
            std::scoped_lock lock(cout_mutex);
            std::cout << "[" << player.name << "] answered: " << answer << "\n";
            if (answer == q.correct) {
                player.score += 10;
                std::cout << "Correct! +10 points to " << player.name << "\n";
            } else {
                std::cout << "Wrong answer.\n";
            }
        });

        eval.wait();
        roundBarrier.arrive_and_wait();
    }
}

int main() {
    std::latch loginLatch(NUM_PLAYERS);
    std::barrier roundBarrier(NUM_PLAYERS);

    std::vector<Player> players(NUM_PLAYERS);
    std::vector<std::thread> threads;

    for (int i = 0; i < NUM_PLAYERS; ++i) {
        players[i].id = i + 1;
        threads.emplace_back(playerThread, std::ref(players[i]), std::ref(loginLatch), std::ref(roundBarrier));
    }

    for (auto& t : threads) t.join();

    std::cout << "\n=== Final Scores ===\n";
    int maxScore = 0;
    for (const auto& p : players) {
        std::cout << p.name << ": " << p.score << " points\n";
        maxScore = std::max(maxScore, p.score);
    }

    std::cout << "\n🏆 Winner(s): ";
    for (const auto& p : players)
        if (p.score == maxScore)
            std::cout << p.name << " ";
    std::cout << "\n";

    return 0;
}