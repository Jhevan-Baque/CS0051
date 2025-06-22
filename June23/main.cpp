#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <random>
#include <latch>
#include <barrier>
#include <future>
#include <mutex>
#include <map>
#include <algorithm>

std::mutex cout_mutex;
constexpr int NUM_PLAYERS = 3;
constexpr int NUM_ROUNDS = 3;

struct Player {
    int id;
    int score = 0;
    bool used5050 = false;
    bool usedCallFriend = false;
};

struct Question {
    std::string questionText;
    std::map<char, std::string> options;
    char correctAnswer;
};

std::vector<Question> questions = {
    {"What is the capital of France?",
     {{'A', "Berlin"}, {'B', "Paris"}, {'C', "Rome"}, {'D', "Madrid"}},
     'B'},
    {"Which planet is known as the Red Planet?",
     {{'A', "Mars"}, {'B', "Earth"}, {'C', "Jupiter"}, {'D', "Venus"}},
     'A'},
    {"Who wrote 'Hamlet'?",
     {{'A', "Charles Dickens"}, {'B', "Mark Twain"}, {'C', "William Shakespeare"}, {'D', "Jane Austen"}},
     'C'}
};

char useLifeline5050(const Question& q, Player& player) {
    player.used5050 = true;
    std::vector<char> wrongChoices;
    for (const auto& [opt, text] : q.options)
        if (opt != q.correctAnswer)
            wrongChoices.push_back(opt);
    std::shuffle(wrongChoices.begin(), wrongChoices.end(), std::mt19937{std::random_device{}()});
    std::scoped_lock lock(cout_mutex);
    std::cout << "[Player " << player.id << "] used 50/50! Options left: "
              << q.correctAnswer << ", " << wrongChoices[0] << "\n";
    return (rand() % 2 == 0) ? q.correctAnswer : wrongChoices[0];
}

char useCallFriend(const Question& q, Player& player) {
    player.usedCallFriend = true;
    std::scoped_lock lock(cout_mutex);
    std::cout << "[Player " << player.id << "] used Call a Friend! They suggest: " << q.correctAnswer << "\n";
    return (rand() % 4 == 0) ? q.correctAnswer : 'A' + rand() % 4;
}

char simulateAnswer(Player& player, const Question& q) {
    int lifeline = rand() % 5;
    if (!player.used5050 && lifeline == 0) return useLifeline5050(q, player);
    if (!player.usedCallFriend && lifeline == 1) return useCallFriend(q, player);
    char randomAnswer = 'A' + rand() % 4;
    return randomAnswer;
}

void quizGame(std::latch& loginLatch, std::barrier<>& roundBarrier, Player& player) {
    {
        std::scoped_lock lock(cout_mutex);
        std::cout << "[Player " << player.id << "] has joined.\n";
    }
    loginLatch.count_down();
    loginLatch.wait();

    for (int round = 0; round < NUM_ROUNDS; ++round) {
        const Question& q = questions[round];
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // simulate thinking
        char answer = simulateAnswer(player, q);

        roundBarrier.arrive_and_wait();

        auto evalTask = std::async(std::launch::async, [&]() {
            std::scoped_lock lock(cout_mutex);
            std::cout << "[Player " << player.id << "] answered: " << answer << "\n";
            std::cout << "Evaluating answer for Player " << player.id << "...\n";
            if (answer == q.correctAnswer) {
                player.score += 10;
                std::cout << "Correct! +10 points to Player " << player.id << "\n";
            } else {
                std::cout << "Wrong answer by Player " << player.id << "\n";
            }
        });
        evalTask.wait();

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
        threads.emplace_back(quizGame, std::ref(loginLatch), std::ref(roundBarrier), std::ref(players[i]));
    }

    for (auto& t : threads) t.join();

    std::cout << "\n=== Final Scores ===\n";
    int highScore = 0;
    for (const auto& p : players) {
        std::cout << "Player " << p.id << ": " << p.score << " points\n";
        highScore = std::max(highScore, p.score);
    }

    std::cout << "\n🏆 Winner(s): ";
    for (const auto& p : players)
        if (p.score == highScore)
            std::cout << "Player " << p.id << " ";
    std::cout << "\n";
    return 0;
}