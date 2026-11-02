#include "number_bomb.hpp"

#include <cstdio>
#include <cstdlib>

using namespace usage_panel::amoled;

namespace {
void check(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "number_bomb: %s\n", message);
        std::exit(1);
    }
}

void enter(NumberBomb& game, uint8_t value)
{
    if (value >= 10) check(game.apply({BombAction::Digit, uint8_t(value / 10)}, 0), "tens digit");
    check(game.apply({BombAction::Digit, uint8_t(value % 10)}, 0), "units digit");
}

void ready(NumberBomb& game)
{
    check(game.apply({BombAction::Continue}, 0), "order to ready");
    check(game.apply({BombAction::Continue}, 0), "ready to input");
}
}

int main()
{
    NumberBomb too_few;
    too_few.phase = BombPhase::Select;
    too_few.selected_mask = 1;
    check(!too_few.apply({BombAction::Start}, 7), "one player rejected");

    for (uint8_t count = 2; count <= 6; ++count) {
        for (uint32_t seed = 0; seed < 2000; seed += 17) {
            NumberBomb game;
            game.phase = BombPhase::Select;
            game.selected_mask = uint8_t((1U << count) - 1);
            check(game.apply({BombAction::Start}, seed), "valid player count starts");
            check(game.count == count, "player count retained");
            check(game.bomb >= 2 && game.bomb <= 99, "secret inside 2..99");
            uint8_t seen = 0;
            for (uint8_t i = 0; i < count; ++i) seen |= 1U << game.order[i];
            check(seen == game.selected_mask, "turn order is a permutation");
        }
    }

    NumberBomb safe;
    safe.phase = BombPhase::Select;
    safe.selected_mask = 0x03;
    check(safe.apply({BombAction::Start}, 123), "start safe game");
    ready(safe);
    const uint8_t first = safe.current_player();
    safe.bomb = 70;
    enter(safe, 1);
    check(!safe.apply({BombAction::Submit}, 0), "lower endpoint rejected");
    check(safe.apply({BombAction::DeleteDigit}, 0), "delete input");
    enter(safe, 50);
    check(safe.apply({BombAction::Submit}, 0), "safe guess accepted");
    check(safe.phase == BombPhase::Safe && safe.low == 50 && safe.high == 100,
          "safe guess narrows lower bound");
    check(safe.apply({BombAction::Next}, 0), "next player");
    check(safe.current_player() != first, "turn advances after safe guess");
    check(safe.apply({BombAction::Continue}, 0), "handoff finishes");
    enter(safe, 50);
    check(!safe.apply({BombAction::Submit}, 0), "new endpoint rejected");
    check(safe.apply({BombAction::DeleteDigit}, 0), "delete units");
    check(safe.apply({BombAction::DeleteDigit}, 0), "delete tens");
    enter(safe, 80);
    check(safe.apply({BombAction::Submit}, 0) && safe.high == 80,
          "safe guess narrows upper bound");

    NumberBomb loss;
    loss.phase = BombPhase::Select;
    loss.selected_mask = 0x03;
    check(loss.apply({BombAction::Start}, 12345), "start loss game");
    ready(loss);
    const uint8_t loser = loss.current_player();
    for (uint8_t strike = 1; strike <= 3; ++strike) {
        loss.bomb = 42;
        enter(loss, 42);
        check(loss.apply({BombAction::Submit}, 0), "bomb guess accepted");
        check(loss.phase == BombPhase::Boom && loss.scores[loser] == strike,
              "bomb score increments");
        check(loss.apply({BombAction::Next}, 0) && loss.phase == BombPhase::RoundResult,
              "round result shown");
        check(loss.apply({BombAction::Next}, strike), "round result advances");
        if (strike < 3) {
            check(loss.phase == BombPhase::Ready && loss.low == 1 && loss.high == 100,
                  "new round resets range");
            check(loss.current_player() != loser, "next person begins next round");
            loss.turn = 0;
            check(loss.apply({BombAction::Continue}, 0), "prepare repeated strike");
        } else {
            check(loss.phase == BombPhase::Final && loss.loser == loser,
                  "third bomb ends match");
        }
    }
    check(loss.apply({BombAction::Rematch}, 456), "rematch starts");
    check(loss.phase == BombPhase::Order && loss.active, "rematch order shown");
    for (uint8_t score : loss.scores) check(score == 0, "rematch clears scores");

    NumberBomb resume;
    resume.phase = BombPhase::Select;
    check(resume.apply({BombAction::Start}, 999), "start resume game");
    ready(resume);
    enter(resume, 45);
    check(resume.apply({BombAction::Lobby}, 0) && resume.phase == BombPhase::Lobby,
          "lobby keeps match");
    check(resume.apply({BombAction::Open}, 0) && resume.phase == BombPhase::Input &&
          resume.input == 45, "lobby resumes input");
    check(resume.apply({BombAction::End}, 0), "end asks confirmation");
    check(resume.apply({BombAction::CancelEnd}, 0) && resume.input == 45,
          "cancel restores input");
    check(resume.apply({BombAction::End}, 0) &&
          resume.apply({BombAction::ConfirmEnd}, 0) && !resume.active,
          "confirmed end clears match");
    return 0;
}
