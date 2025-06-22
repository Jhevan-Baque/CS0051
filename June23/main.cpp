#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <algorithm>
#include <random>
using namespace std;

constexpr int NUM_ROUNDS = 3;
mutex cout_mutex;
mutex input_mutex;

struct Question {
    string text;
    map<char, string> options;
    char correct;
};

struct Player {
    int id;
    string name;
    int score = 0;
    bool active = false;
    char lastAnswer = ' ';
    bool used5050 = false;
    bool usedCall = false;
    bool usedAsk = false;
};

vector<Question> questions = {
    { "What is the capital of France?",
      {{'A', "London"}, {'B', "Paris"}, {'C', "Berlin"}, {'D', "Madrid"}}, 'B' },
    { "Which is the Red Planet?",
      {{'A', "Venus"}, {'B', "Earth"}, {'C', "Mars"}, {'D', "Jupiter"}}, 'C' },
    { "Who wrote Romeo and Juliet?",
      {{'A', "Shakespeare"}, {'B', "Hemingway"}, {'C', "Tolkien"}, {'D', "Rowling"}}, 'A' }
};

void playerLogin(Player& player, vector<Player*>& activePlayers, mutex& activeMutex) {
    {
        scoped_lock input_lock(input_mutex);
        cout << "Player " << player.id << ", enter your name: ";
        getline(cin, player.name);

        string response;
        cout << "[" << player.name << "] Do you want to join the quiz? (yes/no): ";
        getline(cin, response);

        if (response != "yes") {
            scoped_lock lock(cout_mutex);
            cout << "[" << player.name << "] chose not to join.\n";
            return;
        }

        player.active = true;
        {
            scoped_lock lock(cout_mutex);
            cout << "[" << player.name << "] has joined the quiz!\n";
        }

        scoped_lock lockActive(activeMutex);
        activePlayers.push_back(&player);
    }
}

void showOptions(const map<char, string>& options, const vector<char>& hide = {}) {
    for (const auto& [key, text] : options) {
        if (find(hide.begin(), hide.end(), key) == hide.end()) {
            cout << "[" << key << "] " << text << "\n";
        }
    }
}

void use5050(const Question& q, Player& player, vector<char>& hiddenOptions) {
    if (player.used5050) {
        cout << "You already used 50/50.\n";
        return;
    }

    player.used5050 = true;
    vector<char> wrongChoices;
    for (const auto& [opt, _] : q.options)
        if (opt != q.correct)
            wrongChoices.push_back(opt);

    shuffle(wrongChoices.begin(), wrongChoices.end(), mt19937{ random_device{}() });
    hiddenOptions.push_back(wrongChoices[0]);
    hiddenOptions.push_back(wrongChoices[1]);

    cout << "[50/50] Two wrong options removed!\n";
}

void useCallFriend(const Question& q, Player& player) {
    if (player.usedCall) {
        cout << "You already used Call a Friend.\n";
        return;
    }

    player.usedCall = true;
    bool friendCorrect = (rand() % 100 < 80);
    char suggestion = friendCorrect ? q.correct : 'A' + rand() % 4;
    cout << "[Call a Friend] Your friend thinks the answer is: " << suggestion << "\n";
}

void useAskAudience(const Question& q, Player& player) {
    if (player.usedAsk) {
        cout << "You already used Ask the Audience.\n";
        return;
    }

    player.usedAsk = true;
    map<char, int> poll = {
        {'A', 10}, {'B', 10}, {'C', 10}, {'D', 10}
    };
    poll[q.correct] += 60;

    cout << "[Ask the Audience] Audience Poll Results:\n";
    for (const auto& [opt, votes] : poll) {
        cout << "  " << opt << ": " << votes << "%\n";
    }
}

void playRound(int roundNumber, const Question& q, vector<Player*>& players) {
    cout << "\nRound " << roundNumber << " \n";
    cout << q.text << "\n";
    showOptions(q.options);
    cout << "=========================\n";

    for (Player* player : players) {
        char finalAnswer = ' ';
        vector<char> hiddenOptions;

        while (true) {
            scoped_lock input_lock(input_mutex);
            cout << "[" << player->name << "] Choose A/B/C/D or type lifeline (5050 / call / ask): ";
            string input;
            getline(cin, input);

            transform(input.begin(), input.end(), input.begin(), ::tolower);

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
                     find(hiddenOptions.begin(), hiddenOptions.end(), toupper(input[0])) == hiddenOptions.end()) {
                finalAnswer = toupper(input[0]);
                break;
            }
            else {
                cout << "Invalid input.\n";
            }
        }

        player->lastAnswer = finalAnswer;
    }

    cout << "\nRound Results:\n";
    for (Player* player : players) {
        cout << player->name << " answered: " << player->lastAnswer << " - ";
        if (player->lastAnswer == q.correct) {
            player->score += 10;
            cout << "Correct! +10 points.\n";
        } else {
            cout << "Wrong. No points.\n";
        }
    }

    cout << "\nScores After Round " << roundNumber << ":\n";
    for (Player* player : players)
        cout << player->name << ": " << player->score << " points\n";
}
void declareWinner(const vector<Player*>& players) {
    int highest = 0;
    for (const Player* p : players)
        highest = max(highest, p->score);

    cout << "\nWinner(s): ";
    for (const Player* p : players)
        if (p->score == highest)
            cout << p->name << " ";
    cout << "\n";
}

int main() {
    int maxPlayers = 3;
    vector<Player> players(maxPlayers);
    vector<thread> loginThreads;
    vector<Player*> activePlayers;
    mutex activeMutex;

    for (int i = 0; i < maxPlayers; ++i) {
        players[i].id = i + 1;
        loginThreads.emplace_back(playerLogin, ref(players[i]), ref(activePlayers), ref(activeMutex));
    }

    for (auto& t : loginThreads) t.join();

    if (activePlayers.empty()) {
        cout << "\nNo players joined. Game canceled.\n";
        return 0;
    }

    cout << "\nGame Starting with " << activePlayers.size() << " players...\n";

    for (int round = 0; round < NUM_ROUNDS; ++round) {
        playRound(round + 1, questions[round], activePlayers);
    }

    declareWinner(activePlayers);

    return 0;
}
