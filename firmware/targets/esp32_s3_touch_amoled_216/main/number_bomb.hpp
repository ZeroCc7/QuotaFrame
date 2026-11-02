#pragma once

#include <cstdint>

namespace usage_panel::amoled {

enum class BombPhase : uint8_t {
    Lobby, Select, Order, Ready, Input, Safe, Boom, RoundResult,
    Final, EndConfirm,
};

enum class BombAction : uint8_t {
    Open, ToggleRole, Start, Continue, Digit, DeleteDigit, Submit,
    Next, Rematch, Lobby, End, CancelEnd, ConfirmEnd, ToggleSound,
};

struct BombEvent {
    BombAction action;
    uint8_t value = 0;
};

struct NumberBomb {
    BombPhase phase = BombPhase::Lobby;
    BombPhase before_confirm = BombPhase::Lobby;
    BombPhase before_lobby = BombPhase::Lobby;
    uint8_t selected_mask = 0x07;
    uint8_t order[6]{};
    uint8_t scores[6]{};
    uint8_t count = 0;
    uint8_t turn = 0;
    uint8_t loser = 0xff;
    uint8_t last_player = 0xff;
    uint8_t bomb = 0;
    uint8_t low = 1;
    uint8_t high = 100;
    uint8_t guess = 0;
    uint8_t input = 0;
    uint8_t input_digits = 0;
    uint16_t round = 1;
    bool active = false;
    bool sound = true;

    uint8_t current_player() const { return count ? order[turn] : 0xff; }
    bool valid_guess() const { return input_digits && input > low && input < high; }
    bool apply(BombEvent event, uint32_t random);

private:
    void new_round(uint32_t random);
    void begin_match(uint32_t random);
};

}  // namespace usage_panel::amoled
