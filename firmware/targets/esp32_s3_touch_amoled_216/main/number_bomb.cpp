#include "number_bomb.hpp"

namespace usage_panel::amoled {
namespace {
uint8_t player_count(uint8_t mask)
{
    uint8_t result = 0;
    for (uint8_t i = 0; i < 6; ++i) result += (mask >> i) & 1;
    return result;
}

uint32_t mix(uint32_t value)
{
    value ^= value >> 16;
    value *= 0x7feb352dU;
    value ^= value >> 15;
    value *= 0x846ca68bU;
    return value ^ (value >> 16);
}
}  // namespace

void NumberBomb::new_round(uint32_t random)
{
    low = 1;
    high = 100;
    bomb = static_cast<uint8_t>(2 + random % 98);
    input = 0;
    input_digits = 0;
    guess = 0;
    phase = BombPhase::Ready;
}

void NumberBomb::begin_match(uint32_t random)
{
    count = 0;
    for (uint8_t i = 0; i < 6; ++i) {
        scores[i] = 0;
        if (selected_mask & (1U << i)) order[count++] = i;
    }
    uint32_t state = random;
    for (uint8_t i = count - 1; i > 0; --i) {
        state = mix(state + i);
        const uint8_t j = state % (i + 1);
        const uint8_t temp = order[i];
        order[i] = order[j];
        order[j] = temp;
    }
    turn = 0;
    round = 1;
    loser = 0xff;
    last_player = 0xff;
    active = true;
    new_round(mix(state));
    phase = BombPhase::Order;
}

bool NumberBomb::apply(BombEvent event, uint32_t random)
{
    switch (event.action) {
    case BombAction::Open:
        if (phase == BombPhase::Lobby)
            phase = active ? before_lobby : BombPhase::Select;
        return true;
    case BombAction::ToggleSound:
        sound = !sound;
        return true;
    case BombAction::Lobby:
        if (phase != BombPhase::Lobby) before_lobby = phase;
        phase = BombPhase::Lobby;
        return true;
    case BombAction::End:
        if (!active) return false;
        before_confirm = phase;
        phase = BombPhase::EndConfirm;
        return true;
    case BombAction::CancelEnd:
        if (phase != BombPhase::EndConfirm) return false;
        phase = before_confirm;
        return true;
    case BombAction::ConfirmEnd:
        if (phase != BombPhase::EndConfirm) return false;
        active = false;
        phase = BombPhase::Lobby;
        return true;
    case BombAction::ToggleRole:
        if (phase != BombPhase::Select || event.value >= 6) return false;
        selected_mask ^= 1U << event.value;
        return true;
    case BombAction::Start:
        if (phase != BombPhase::Select || player_count(selected_mask) < 2) return false;
        begin_match(random);
        return true;
    case BombAction::Rematch:
        if (phase != BombPhase::Final) return false;
        begin_match(random);
        return true;
    case BombAction::Continue:
        if (phase == BombPhase::Order) phase = BombPhase::Ready;
        else if (phase == BombPhase::Ready) phase = BombPhase::Input;
        else return false;
        return true;
    case BombAction::Digit:
        if (phase != BombPhase::Input || event.value > 9 || input_digits >= 3) return false;
        if (input * 10 + event.value > 100) return false;
        input = input * 10 + event.value;
        ++input_digits;
        return true;
    case BombAction::DeleteDigit:
        if (phase != BombPhase::Input || !input_digits) return false;
        input /= 10;
        --input_digits;
        return true;
    case BombAction::Submit:
        if (phase != BombPhase::Input || !valid_guess()) return false;
        guess = input;
        input = 0;
        input_digits = 0;
        last_player = current_player();
        if (guess == bomb) {
            ++scores[last_player];
            phase = BombPhase::Boom;
        } else {
            if (guess < bomb) low = guess;
            else high = guess;
            phase = BombPhase::Safe;
        }
        return true;
    case BombAction::Next:
        if (phase == BombPhase::Safe) {
            turn = (turn + 1) % count;
            phase = BombPhase::Ready;
        } else if (phase == BombPhase::Boom) {
            phase = BombPhase::RoundResult;
        } else if (phase == BombPhase::RoundResult) {
            if (scores[last_player] >= 3) {
                loser = last_player;
                active = false;
                phase = BombPhase::Final;
            } else {
                turn = (turn + 1) % count;
                ++round;
                new_round(random);
            }
        } else return false;
        return true;
    }
    return false;
}

}  // namespace usage_panel::amoled
