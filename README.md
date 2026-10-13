# Gomoku

C++17 implementation of 19×19 capture Gomoku with a timed Minimax AI and an ncurses interface.

## Build and run

```sh
make -j
./Gomoku
```

Requires a C++17 compiler, Make, ncurses development headers/library, and a terminal of at least **76 columns × 28 rows**. On macOS the Command Line Tools SDK provides ncurses; this project was built and tested with Apple Clang on arm64 macOS. On Debian/Ubuntu install `build-essential` and `libncurses-dev`. Linux support is intended but has not been verified here. No network access or package download occurs during the build.

The executable is `Gomoku`. `make clean`, `make fclean`, and `make re` are available. Repeated `make` does not relink unchanged files.

## Play

Choose player 1 versus AI, player 2 versus AI, two humans, or AI versus AI from the opening menu. In Standard/Pro, player 1 owns Black and player 2 owns White. In Swap/Swap2, player identities stay fixed and colors are assigned during the opening. Black moves first. Mandatory rules are enabled by default; the menu can disable captures or the double-three restriction for custom games.

| Key | Action |
| --- | --- |
| Arrow keys | Move the board cursor |
| Enter / Space | Place a stone |
| Mouse click | Select and play an intersection, when the terminal supports it |
| O (menu) | Cycle Standard / Pro / Swap / Swap2 |
| B / W (color choice) | Choose Black / White |
| E (first Swap2 choice) | Add a White and a Black stone, then let the other player choose |
| H | Ask for a suggested move, marked `?` |
| U | Undo; against AI, return to the previous human turn |
| S / L | Save / load `gomoku.save` in the current working directory |
| [ / ] | View previous / next position in the recorded game |
| P | Pause / resume automatic AI turns; an in-progress search finishes first |
| N | Return to the new-game menu |
| Q | Quit |

For AI-versus-AI replay or saving, pause first. Board mutations are disabled while the AI is thinking. Replay does not change the live game; advance to the last position to resume. Loading preserves the selected human/AI mode and restores the saved rules, opening phase, color ownership, and actions. Saving replaces the previous `gomoku.save`.

The side panel shows captures, current player, outcome, running/last AI calculation time, completed search depth, visited nodes, cache hits, score, and an example principal variation (PV).

## Rules

- Play on empty intersections of a 19×19 board.
- Five or more consecutive stones can win, horizontally, vertically, or diagonally.
- `X O O X`, formed by the newly placed X, captures the two O stones. All eight directions are checked; simultaneous captures are possible.
- Only pairs are captured. Placing your own stone between existing enemies does not capture your own stones.
- Capturing ten enemy stones wins immediately.
- A five is provisional if the opponent can answer with a capture that removes all winning alignments, or can win immediately by reaching ten captures elsewhere. If the opponent fails to do so on the next move, the original five wins.
- A non-capturing move creating open threes on two different axes is forbidden. Both straight and broken threes count: they can become an open four with one additional stone. Board edges and opponent stones close an end. Capturing moves are exempt, as specified in the subject appendix.
- A full board without a winner is a draw.

See [architecture and defense notes](docs/ARCHITECTURE.md) for the exact implementation and suggested division between two teammates.

## Opening bonuses

Press **O in the starting menu** before selecting a game mode:

- **Standard**: ordinary alternating moves without opening restrictions; the default mandatory mode.
- **Pro**: the first Black stone must be at the center (J10). White may play anywhere. Black's second stone must be outside the central 5×5 square (H8–L12). Subsequent moves are unrestricted by the opening.
- **Swap**: player 1 places Black, White, Black. Player 2 chooses Black (`B`) or White (`W`). The player who owns White makes the next move.
- **Swap2**: the same initial setup, but player 2 may instead press `E` and place White, Black. Player 1 then chooses a color. There is no second extension; White moves next.

Setup placements do not trigger captures: they construct the opening position. After color assignment, the selected capture and double-three rules apply normally. These are opening protocols layered on this project's rules, not a switch to unrelated tournament win conditions.

The panel shows the current player, setup stone, available choices, and final color ownership. The AI can place setup stones and choose either color. It builds a balanced setup using a static heuristic; when offered a color it uses timed search and picks the preferred side. It currently chooses a color directly rather than requesting the optional Swap2 extension, but handles an extension requested by a human. Opening setup depth is displayed as zero because it is not a Minimax search.

Undo/replay count **actions**: placements and color/extension choices. Version 2 saves record all of them; legacy version 1 saves still load as Standard games.

## AI

Negamax is the symmetric form of Minimax. The engine uses alpha-beta pruning, iterative deepening, a fixed-size transposition table, and attack/defense move ordering. Evaluation rewards open lines, broken lines, captures, and central placement.

The default search budget is **420 ms**, with a maximum depth of **12 plies** (individual moves). The last fully completed iteration supplies the move; a legal fallback exists if time runs out before depth one. Immediate wins terminate early.

**The search is selective, not an exhaustive search of every legal move to depth 12.** Candidates normally lie within two intersections of existing stones. Quiet continuation limits are 16 at the root, 6 at plies 1–2, 4 at plies 3–5, and 2 afterward. Immediate line/capture threats and replies to a pending five are retained beyond these limits. This trades tactical coverage for depth and speed; the UI reports the completed selective depth, not a guaranteed full-width depth. Hard positions or slower machines may complete fewer than ten plies. Acceptance and playing strength must still be checked on the evaluation machine.

## Verification

```sh
make test       # Rules, tactical AI, replay/save validation, randomized undo invariants
make sanitize   # The same tests with AddressSanitizer and UndefinedBehaviorSanitizer
make ui-test    # Real ncurses interaction through a PTY; requires Python 3
./Gomoku --bench
```

The benchmark plays 12 AI moves from each of two deterministic openings and prints actual depth, runtime, nodes, and average time. It is a reproducible sample, not a guarantee for every position.

Additional features beyond the mandatory modes: configurable rules, save/load, history replay, undo, and AI-versus-AI with pause. Standard, Pro, Swap, and Swap2 openings are implemented. Bonus acceptance depends on the evaluator and requires the mandatory part to pass first.
