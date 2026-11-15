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
| V | Toggle per-color evaluation components while keeping the AI timer visible |
| H | Ask for a suggested move, marked `?` |
| U | Undo; against AI, return to the previous human turn |
| S / L | Save / load `gomoku.save` in the current working directory |
| [ / ] | View previous / next position in the recorded game |
| P | Pause / resume automatic AI turns; an in-progress search finishes first |
| N | Return to the new-game menu |
| Q | Quit |

For AI-versus-AI replay or saving, pause first. Board mutations are disabled while the AI is thinking. Replay does not change the live game; advance to the last position to resume. Loading preserves the selected human/AI mode and restores the saved rules, opening phase, color ownership, and actions. Saving replaces the previous `gomoku.save`.

The side panel shows captures, current player, outcome, running/last AI calculation time, completed search depth (with selective depth), visited nodes (all threads), cache hits, score, and an example principal variation (PV).

## Rules

- Play on empty intersections of a 19×19 board.
- Five or more consecutive stones can win, horizontally, vertically, or diagonally.
- `X O O X`, formed by the newly placed X, captures the two O stones. All eight directions are checked; simultaneous captures are possible.
- Only pairs are captured. Placing your own stone between existing enemies does not capture your own stones.
- Capturing ten enemy stones wins immediately.
- A five is provisional if the opponent can answer with a capture that removes all winning alignments, or can win immediately by reaching ten captures elsewhere. If the opponent fails to do so on the next move, the original five wins.
- A non-capturing move creating open threes on two different axes is forbidden. Both straight and broken threes count: they can become an open four with one additional stone. Board edges and opponent stones close an end. Capturing moves are exempt, as specified in the subject appendix.
- A full board without a winner is a draw.

Ready-made positions demonstrating each rule are in [demo/](demo/README.md) (`./demo/play.sh 03`, then `2` and `L`).

See [architecture and defense notes](docs/ARCHITECTURE.md) and the [grading checklist](docs/GRADING.md) for the exact implementation and suggested division between two teammates.

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

Negamax is the symmetric form of Minimax. The engine combines:

- alpha-beta pruning with principal variation search (PVS) and iterative deepening;
- a shared, lock-free transposition table (Zobrist keys, 2^20 entries) that persists between moves;
- several search threads (Lazy SMP, up to 4, never more than the machine reports);
- a threat-space search (VCF, victory by continuous fours) before the main search;
- quiescence search over fives, fours and answers to open threes at the horizon;
- killer and history move ordering and late move reductions;
- wins scored as `Win − ply`, so the AI prefers the fastest win and the longest defense.

Search runs on `Board`, an incremental padded board with make/unmake. For every cell and axis it keeps a 2-bit code of the eight neighbors, mapped through a 65,536-entry table to the shape a stone would create there: five, open four, four, open three, and so on. Move generation, ordering, threat search and evaluation all read these per-cell shapes.

The evaluation scores every empty cell near the stones for both colors, by what a stone there would create: Lines (per-axis shapes), Figures (one move making two threats, or two completion squares), Potential captures (worth more as captures accumulate; the tenth captured stone is decisive), Captured pairs (0 / 6k / 14k / 30k / 70k), a small dynamic History term learned from recent real moves, and Center. Press **V** to inspect each component for X and O.

**Time and depth.** The target is **420 ms** per move. Depth 10 is always completed if possible within a hard limit of 472 ms. After that, a new iteration starts only if its predicted cost fits in 420 ms. The panel shows the last fully completed depth of the main thread, the selective depth (deepest ply including quiescence), and `VCF` when a forced win was found by the threat search.

**The search is selective, not exhaustive.** Candidates lie within two intersections of existing stones. At the root up to 24 quiet moves are searched; at a node with remaining depth `d`, `3 + d/2`. The engine's own fours, the table move and every answer to a five threat are searched beyond that limit. Late quiet moves are reduced by one or two plies and re-searched if they turn out better. Against a five threat only blocks and captures are searched; against an open three, blocks, counter-fours and captures. These are standard gomoku prunings, not mathematically safe ones.

## Memory failure handling

Candidate lists and the principal variation use fixed capacity; the recursive engine does not allocate from the heap. If the transposition table cannot be allocated, search continues without it; if a helper thread cannot start, search continues with fewer threads. If history analysis fails to allocate, it uses a neutral profile. The UI marks this as `Memory fallback active`.

History edits, undo, and load preserve the current game if allocation fails. The UI returns to the live position in hotseat mode and keeps accepting input; starting a new game releases history. The real UI is tested with persistent C++ allocation failures as well as recovery. These tests cover C++ allocation paths, not every possible OS or ncurses failure.

## Verification

```sh
make test             # Rules, tactics, persistence; rules checked against a slow reference
make sanitize         # Rule/tactic/reference tests under ASan and UBSan
make ui-test          # Real ncurses interaction through a PTY; requires Python 3
make memory-test      # Inject C++ allocation failures into core and real UI
make performance-test # 90 deterministic sampled positions; depth/time regression gate
make arena-test       # Eight matches against 1-ply and 3-ply automated opponents
make demo             # Rebuild and re-check demo/*.save, one position per rule
./Gomoku --bench
```

The benchmark uses the same history-aware `AI::act()` path as the UI and plays 12 AI moves from each of two deterministic openings and prints actual depth, runtime, nodes, and average time. Positions where the search proved a win or loss are marked `proven`: such a search stops early, so the minimum depth is reported over the other positions. It is a reproducible sample, not a guarantee for every position.

See [validation results](docs/VALIDATION.md) for the measured sample and [defense preparation](docs/DEFENSE.md) for explanations and a demonstration sequence.

Additional features beyond the mandatory modes: configurable rules, save/load, history replay, undo, and AI-versus-AI with pause. Standard, Pro, Swap, and Swap2 openings are implemented. Bonus acceptance depends on the evaluator and requires the mandatory part to pass first.
