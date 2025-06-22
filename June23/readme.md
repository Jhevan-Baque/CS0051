Problem 1

You are to design and implement a multiplayer quiz game similar to “Who Wants to Be a Millionaire”, where multiple players (simulated via threads) participate in a synchronized, competitive quiz round. The system must ensure fair start, synchronized rounds, and parallel evaluation of answers using:

std::latch — for waiting until all players have joined

std::barrier — to synchronize all players per quiz round

std::async — to evaluate answers without blocking the main game loop

Requirements:
Login Phase

Each player runs on a separate thread.

All threads (players) must wait until everyone has logged in before the game begins using std::latch.

Quiz Phase (3 Rounds)

Each round has 1 multiple-choice question with options A/B/C/D.

All players must “submit” answers (simulated input and random answer).

No player may proceed until all have submitted (std::barrier).

Answers are evaluated asynchronously using std::async, and scored individually.

Show each player's result after each round.

Scoring

Players get +10 points per correct answer.

After 3 rounds, show the final scores and declare the winner(s).

Output Requirements

Display per-player messages clearly.

Use std::mutex to avoid messy console output.

- each player runs in diff threads
- they must log in
- player 1 has joined. etc
- each round has multiple questions (a,b,c,d)

- evaluating answer from player 1,2, etc.
- And the correct answer goes to player (1).

- Show the points later

10points per correct answer

After the rounds
declare the winner


add features:
- call a friend
- 50/50
