#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <algorithm>
#include <random>
#include <future>
#include <latch>
#include <barrier>

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

void showOptions(const map<char, string>& options, const vector<char>& hide = {}) {
    for (const auto& [key, text] : options)
        if (find(hide.begin(), hide.end(), key) == hide.end())
            cout << "[" << key << "] " << text << "\n";
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

    map<char, int> poll = {{'A', 10}, {'B', 10}, {'C', 10}, {'D', 10}};
    poll[q.correct] += 60;

    cout << "[Ask the Audience] Audience Poll Results:\n";
    for (const auto& [opt, votes] : poll)
        cout << "  " << opt << ": " << votes << "%\n";
}

void answerQuestion(Player* player, const Question& q) {
    char finalAnswer = ' ';
    vector<char> hiddenOptions;

    while (true) {
        scoped_lock input_lock(input_mutex);
        cout << "[" << player->name << "] Choose A/B/C/D or lifeline (5050 / call / ask): ";
        string input;
        getline(cin, input);
        transform(input.begin(), input.end(), input.begin(), ::tolower);

        if (input == "5050") {
            use5050(q, *player, hiddenOptions);
            showOptions(q.options, hiddenOptions);
        } else if (input == "call") {
            useCallFriend(q, *player);
        } else if (input == "ask") {
            useAskAudience(q, *player);
        } else if (input.size() == 1 && q.options.count(toupper(input[0])) &&
                   find(hiddenOptions.begin(), hiddenOptions.end(), toupper(input[0])) == hiddenOptions.end()) {
            finalAnswer = toupper(input[0]);
            break;
        } else {
            cout << "Invalid input.\n";
        }
    }

    player->lastAnswer = finalAnswer;
}

void evaluateAnswers(const vector<Player*>& players, const Question& q) {
    vector<future<void>> futures;

    for (Player* player : players) {
        futures.push_back(async(launch::async, [=]() {
            scoped_lock lock(cout_mutex);
            cout << player->name << " answered: " << player->lastAnswer << " - ";
            if (player->lastAnswer == q.correct) {
                player->score += 10;
                cout << "Correct! +10 points.\n";
            } else {
                cout << "Wrong. No points.\n";
            }
        }));
    }

    for (auto& f : futures) f.wait();
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
    vector<Player*> activePlayers;
    mutex activeMutex;

    // Step 1: Login Phase — Collect users
    cout << "Waiting for players to login...\n";
    vector<thread> threads;
    vector<bool> joined(maxPlayers, false);

    latch latch(maxPlayers);

    for (int i = 0; i < maxPlayers; ++i) {
        players[i].id = i + 1;

        threads.emplace_back([&, i]() {
            scoped_lock input_lock(input_mutex);
            cout << "Player " << (i + 1) << ", enter your name: ";
            getline(cin, players[i].name);

            cout << "[" << players[i].name << "] Do you want to join the quiz? (yes/no): ";
            string response;
            getline(cin, response);

            if (response == "yes") {
                players[i].active = true;
                scoped_lock lock(cout_mutex);
                cout << "[" << players[i].name << "] has joined the quiz!\n";
                {
                    scoped_lock lock(activeMutex);
                    activePlayers.push_back(&players[i]);
                }
            } else {
                scoped_lock lock(cout_mutex);
                cout << "[" << players[i].name << "] chose not to join.\n";
            }

            latch.count_down();
        });
    }

    latch.wait();  // Wait until all players responded
    for (auto& t : threads) t.join();

    if (activePlayers.empty()) {
        cout << "No players joined. Game canceled.\n";
        return 0;
    }

    cout << "\nGame Starting with " << activePlayers.size() << " players...\n";

    barrier roundBarrier((int)activePlayers.size());

    // Step 2: Quiz Rounds
    for (int round = 0; round < NUM_ROUNDS; ++round) {
        const Question& q = questions[round];
        cout << "\nRound " << (round + 1) << ": " << q.text << "\n";
        showOptions(q.options);

        vector<thread> roundThreads;

        for (Player* player : activePlayers) {
            roundThreads.emplace_back([&, player]() {
                answerQuestion(player, q);
                roundBarrier.arrive_and_wait();  // Wait for all to answer
            });
        }

        for (auto& t : roundThreads) t.join();

        cout << "\nResults for Round " << (round + 1) << ":\n";
        evaluateAnswers(activePlayers, q);

        cout << "\nScores:\n";
        for (const Player* p : activePlayers)
            cout << p->name << ": " << p->score << " points\n";
    }

    declareWinner(activePlayers);
    return 0;
}
