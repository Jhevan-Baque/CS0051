#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <algorithm>
#include <random>

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
    bool used5050 = false;
    bool usedCall = false;
    bool usedAsk = false;
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

void showOptions(const std::map<char, std::string>& options, const std::vector<char>& hide = {}) {
    for (const auto& [key, text] : options) {
        if (std::find(hide.begin(), hide.end(), key) == hide.end())
            std::cout << "  " << key << ") " << text << "\n";
    }
}

void use5050(const Question& q, Player& player, std::vector<char>& hiddenOptions) {
    if (player.used5050) {
        std::cout << "You already used 50/50.\n";
        return;
    }

    player.used5050 = true;
    std::vector<char> wrongChoices;
    for (const auto& [opt, _] : q.options)
        if (opt != q.correct)
            wrongChoices.push_back(opt);

    std::shuffle(wrongChoices.begin(), wrongChoices.end(), std::mt19937{ std::random_device{}() });
    hiddenOptions.push_back(wrongChoices[0]);
    hiddenOptions.push_back(wrongChoices[1]);

    std::cout << "[50/50] Two wrong options removed!\n";
}

void useCallFriend(const Question& q, Player& player) {
    if (player.usedCall) {
        std::cout << "You already used Call a Friend.\n";
        return;
    }

    player.usedCall = true;
    bool friendCorrect = (rand() % 100 < 80);
    char suggestion = friendCorrect ? q.correct : 'A' + rand() % 4;
    std::cout << "[Call a Friend] Your friend thinks the answer is: " << suggestion << "\n";
}

void useAskAudience(const Question& q, Player& player) {
    if (player.usedAsk) {
        std::cout << "You already used Ask the Audience.\n";
        return;
    }

    player.usedAsk = true;
    std::map<char, int> poll = {
        {'A', 10}, {'B', 10}, {'C', 10}, {'D', 10}
    };
    poll[q.correct] += 60;

    std::cout << "[Ask the Audience] Audience Poll Results:\n";
    for (const auto& [opt, votes] : poll) {
        std::cout << "  " << opt << ": " << votes << "%\n";
    }
}

void playRound(int roundNumber, const Question& q, std::vector<Player*>& players) {
    std::cout << "\n🟦 Round " << roundNumber << " 🟦\n";
    std::cout << q.text << "\n";
    showOptions(q.options);
    std::cout << "=========================\n";

    for (Player* player : players) {
        char finalAnswer = ' ';
        std::vector<char> hiddenOptions;

        while (true) {
            std::scoped_lock input_lock(input_mutex);
            std::cout << "[" << player->name << "] Choose A/B/C/D or type lifeline (5050 / call / ask): ";
            std::string input;
            std::getline(std::cin, input);

            std::transform(input.begin(), input.end(), input.begin(), ::tolower);

            if (input == "5050") {
                use5050(q, *player, hiddenOptions);
                showOptions(q.options, hiddenOptions);
            }
            else if (input == "call") {
                useCallFriend(q, *player);
            }
            else if (input == "ask") {
                useAskAudience(q, *player);
            }
            else if (input.size() == 1 && q.options.count(toupper(input[0])) &&
                     std::find(hiddenOptions.begin(), hiddenOptions.end(), toupper(input[0])) == hiddenOptions.end()) {
                finalAnswer = toupper(input[0]);
                break;
            }
            else {
                std::cout << "Invalid input.\n";
            }
        }

        player->lastAnswer = finalAnswer;
    }

    std::cout << "\n✅ Round Results:\n";
    for (Player* player : players) {
        std::cout << player->name << " answered: " << player->lastAnswer << " - ";
        if (player->lastAnswer == q.correct) {
            player->score += 10;
            std::cout << "Correct! +10 points.\n";
        } else {
            std::cout << "Wrong. No points.\n";
        }
    }

    std::cout << "\n📊 Scores After Round " << roundNumber << ":\n";
    for (Player* player : players)
        std::cout << player->name << ": " << player->score << " points\n";
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