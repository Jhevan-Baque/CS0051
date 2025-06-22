#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <algorithm>

constexpr int NUM_ROUNDS = 3;
std::mutex cout_mutex;
std::mutex input_mutex;

struct Question {
    std::string text;
    std::map<char, std::string> options;
    char correct;
};

struct Player {
    int id;
    std::string name;
    int score = 0;
    bool active = false;
    char lastAnswer = ' ';
};

std::vector<Question> questions = {
    { "What is the capital of France?",
      {{'A', "London"}, {'B', "Paris"}, {'C', "Berlin"}, {'D', "Madrid"}}, 'B' },
    { "Which is the Red Planet?",
      {{'A', "Venus"}, {'B', "Earth"}, {'C', "Mars"}, {'D', "Jupiter"}}, 'C' },
    { "Who wrote Romeo and Juliet?",
      {{'A', "Shakespeare"}, {'B', "Hemingway"}, {'C', "Tolkien"}, {'D', "Rowling"}}, 'A' }
};

void playerLogin(Player& player, std::vector<Player*>& activePlayers, std::mutex& activeMutex) {
    {
        std::scoped_lock input_lock(input_mutex);
        std::cout << "Player " << player.id << ", enter your name: ";
        std::getline(std::cin, player.name);

        std::string response;
        std::cout << "[" << player.name << "] Do you want to join the quiz? (yes/no): ";
        std::getline(std::cin, response);

        if (response != "yes") {
            std::scoped_lock lock(cout_mutex);
            std::cout << "[" << player.name << "] chose not to join.\n";
            return;
        }

        player.active = true;
        {
            std::scoped_lock lock(cout_mutex);
            std::cout << "[" << player.name << "] has joined the quiz!\n";
        }

        std::scoped_lock lockActive(activeMutex);
        activePlayers.push_back(&player);
    }
}

void playRound(int roundNumber, const Question& q, std::vector<Player*>& players) {
    {
        std::scoped_lock lock(cout_mutex);
        std::cout << "\n🟦 Round " << roundNumber << " 🟦\n";
        std::cout << q.text << "\n";
        for (auto& [opt, text] : q.options) {
            std::cout << "  " << opt << ") " << text << "\n";
        }
        std::cout << "=========================\n";
    }

    for (Player* player : players) {
        std::string input;
        char ans = ' ';

        while (true) {
            std::scoped_lock input_lock(input_mutex);
            std::cout << "[" << player->name << "] Enter your answer (A/B/C/D): ";
            std::getline(std::cin, input);
            if (input.size() == 1 && q.options.count(toupper(input[0]))) {
                ans = toupper(input[0]);
                break;
            }
            std::cout << "Invalid choice. Please enter A, B, C, or D.\n";
        }

        player->lastAnswer = ans;
    }

    {
        std::scoped_lock lock(cout_mutex);
        std::cout << "\n✅ Round Results:\n";
    }

    for (Player* player : players) {
        std::scoped_lock lock(cout_mutex);
        std::cout << player->name << " answered: " << player->lastAnswer << " - ";
        if (player->lastAnswer == q.correct) {
            player->score += 10;
            std::cout << "Correct! +10 points.\n";
        } else {
            std::cout << "Wrong. No points.\n";
        }
    }

    {
        std::scoped_lock lock(cout_mutex);
        std::cout << "\n📊 Scores After Round " << roundNumber << ":\n";
        for (Player* player : players)
            std::cout << player->name << ": " << player->score << " points\n";
    }
}

void declareWinner(const std::vector<Player*>& players) {
    int highest = 0;
    for (const Player* p : players)
        highest = std::max(highest, p->score);

    std::cout << "\n🏆 Winner(s): ";
    for (const Player* p : players)
        if (p->score == highest)
            std::cout << p->name << " ";
    std::cout << "\n";
}

int main() {
    int maxPlayers = 3;
    std::vector<Player> players(maxPlayers);
    std::vector<std::thread> loginThreads;
    std::vector<Player*> activePlayers;
    std::mutex activeMutex;

    for (int i = 0; i < maxPlayers; ++i) {
        players[i].id = i + 1;
        loginThreads.emplace_back(playerLogin, std::ref(players[i]), std::ref(activePlayers), std::ref(activeMutex));
    }

    for (auto& t : loginThreads) t.join();

    if (activePlayers.empty()) {
        std::cout << "\n❌ No players joined. Game canceled.\n";
        return 0;
    }

    std::cout << "\n🎮 Game Starting with " << activePlayers.size() << " players...\n";

    for (int round = 0; round < NUM_ROUNDS; ++round) {
        playRound(round + 1, questions[round], activePlayers);
    }

    declareWinner(activePlayers);

    return 0;
}